/*---------------------------------------------------------------------------*
  OSThread.cpp - Threading and Synchronization (Aurora-based, C++ Port)
  
  This implementation is based on Aurora's OSThread.c with adaptations for:
  - C++ compilation
  - PC platform (non-PowerPC)
  - Modern C++ features while maintaining C API compatibility
  
  The core scheduler logic remains true to Aurora's cooperative scheduler design.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/os/OSContext.h>
#include <dolphin/os/OSAlarm.h>
#include <dolphin/os/OSInterrupt.h>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <climits>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
extern OSErrorHandler __OSErrorTable[];

// Functions from OSMutex.c (will be properly defined when OSMutex is converted to C++)
extern void __OSUnlockAllMutex(OSThread* thread);
extern BOOL __OSCheckMutexes(OSThread* thread);
extern BOOL __OSCheckDeadLock(OSThread* thread);

// Helper to count leading zeros (equivalent to PowerPC __cntlzw)
static inline u32 countLeadingZeros(u32 value) {
    if (value == 0) return 32;
    
#ifdef _WIN32
    unsigned long index;
    if (_BitScanReverse(&index, value)) {
        return 31 - index;
    }
    return 32;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clz(value);
#else
    // Fallback implementation
    u32 count = 0;
    if ((value & 0xFFFF0000) == 0) { count += 16; value <<= 16; }
    if ((value & 0xFF000000) == 0) { count += 8; value <<= 8; }
    if ((value & 0xF0000000) == 0) { count += 4; value <<= 4; }
    if ((value & 0xC0000000) == 0) { count += 2; value <<= 2; }
    if ((value & 0x80000000) == 0) { count += 1; }
    return count;
#endif
}

#define __cntlzw(x) countLeadingZeros(x)

// Stack alignment (8 bytes for EABI)
#define ALIGNMENT 8

// Helper macros for alignment
#define OFFSET(n, a) (((u32)(n)) & ((a) - 1))
#define TRUNC(n, a)  (((u32)(n)) & ~((a) - 1))
#define ROUND(n, a)  (((u32)(n) + (a) - 1) & ~((a) - 1))

// Scheduler state
static volatile u32  RunQueueBits = 0;      // Bit set if corresponding RunQueue is not empty
static OSThreadQueue RunQueue[32];
static volatile BOOL RunQueueHint = FALSE;  // Hint that scheduler should check run queue
static volatile s32  Reschedule = 0;        // Suspended if greater than zero
static OSThread      IdleThread;
static OSThread      DefaultThread;
static OSContext     IdleContext;

// Forward declaration: must be before SwitchThreadCallback init (used as default callback)
static void DefaultSwitchThreadCallback(OSThread* from, OSThread* to);

/* Must be DefaultSwitchThreadCallback; nullptr causes crash when OSSetCurrentThread invokes it before OSSetSwitchThreadCallback */
static OSSwitchThreadCallback SwitchThreadCallback = DefaultSwitchThreadCallback;

// Global thread variables (these would be in low memory on original hardware)
OSThread*            __OSCurrentThread = nullptr;
OSThreadQueue        __OSActiveThreadQueue;
volatile OSContext*  __OSCurrentContext = nullptr;
volatile OSContext*  __OSFPUContext = nullptr;

// Forward declarations for internal functions
static BOOL __OSIsThreadActive(OSThread* thread);
static void SetRun(OSThread* thread);
static void UnsetRun(OSThread* thread);
static OSThread* SelectThread(BOOL yield);
static OSThread* SetEffectivePriority(OSThread* thread, OSPriority priority);
static void UpdatePriority(OSThread* thread);

// Queue manipulation macros (matching Aurora's implementation)
#define EnqueueTail(queue, thread, link) do { \
    OSThread* __prev = (queue)->tail; \
    if (__prev == nullptr) (queue)->head = (thread); \
    else __prev->link.next = (thread); \
    (thread)->link.prev = __prev; \
    (thread)->link.next = nullptr; \
    (queue)->tail = (thread); \
} while(0)

#define EnqueuePrio(queue, thread, link) do { \
    OSThread* __prev; \
    OSThread* __next; \
    for (__next = (queue)->head; \
         __next && __next->priority <= (thread)->priority; \
         __next = __next->link.next) ; \
    if (__next == nullptr) { \
        EnqueueTail(queue, thread, link); \
    } else { \
        (thread)->link.next = __next; \
        __prev = __next->link.prev; \
        __next->link.prev = (thread); \
        (thread)->link.prev = __prev; \
        if (__prev == nullptr) (queue)->head = (thread); \
        else __prev->link.next = (thread); \
    } \
} while(0)

#define DequeueItem(queue, thread, link) do { \
    OSThread* __next = (thread)->link.next; \
    OSThread* __prev = (thread)->link.prev; \
    if (__next == nullptr) (queue)->tail = __prev; \
    else __next->link.prev = __prev; \
    if (__prev == nullptr) (queue)->head = __next; \
    else __prev->link.next = __next; \
} while(0)

#define DequeueHead(queue, thread, link) do { \
    (thread) = (queue)->head; \
    OSThread* __next = (thread)->link.next; \
    if (__next == nullptr) (queue)->tail = nullptr; \
    else __next->link.prev = nullptr; \
    (queue)->head = __next; \
} while(0)

#define IsSuspended(suspendCount) (0 < (suspendCount))

static void DefaultSwitchThreadCallback(OSThread* from, OSThread* to) {
    (void)from;
    (void)to;
}

static inline void OSSetCurrentThread(OSThread* thread) {
    SwitchThreadCallback(__OSCurrentThread, thread);
    __OSCurrentThread = thread;
}

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    OSSwitchThreadCallback prev;
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    prev = SwitchThreadCallback;
    SwitchThreadCallback = callback ? callback : DefaultSwitchThreadCallback;
    OSRestoreInterrupts(enabled);
    return (prev == DefaultSwitchThreadCallback) ? nullptr : prev;
}

/*---------------------------------------------------------------------------*
  Name:         __OSThreadInit
  
  Description:  Performs thread initialization. Called during OSInit().
 *---------------------------------------------------------------------------*/
extern u8* _stack_addr;  /* Top of default thread stack (grows down) */
extern u8* _stack_end;   /* Bottom of default thread stack */

void __OSThreadInit(void) {
    OSThread* thread = &DefaultThread;
    int prio;
    
    thread->state = OS_THREAD_STATE_RUNNING;
    thread->attr = OS_THREAD_ATTR_DETACH;
    thread->priority = thread->base = 16;
    thread->suspend = 0;
    thread->val = (void*)(uintptr_t)-1;
    thread->mutex = nullptr;
    OSInitThreadQueue(&thread->queueJoin);
    thread->queueMutex.head = nullptr;
    thread->queueMutex.tail = nullptr;
    
    OSClearContext(&thread->context);
    OSSetCurrentContext(&thread->context);
    
    thread->stackBase = (u8*)_stack_addr;
    thread->stackEnd = (u32*)_stack_end;
    *(thread->stackEnd) = OS_THREAD_STACK_MAGIC;
    
    OSSetCurrentThread(thread);
    
    OSClearStack(0);
    
    RunQueueBits = 0;
    RunQueueHint = FALSE;
    for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; prio++) {
        OSInitThreadQueue(&RunQueue[prio]);
    }
    
    OSInitThreadQueue(&__OSActiveThreadQueue);
    EnqueueTail(&__OSActiveThreadQueue, thread, linkActive);
    
    OSClearContext(&IdleContext);
    Reschedule = 0;
}

void OSInitThreadQueue(OSThreadQueue* queue) {
    if (queue) {
        queue->head = queue->tail = nullptr;
    }
}

OSThread* OSGetCurrentThread(void) {
    return __OSCurrentThread;
}

/*---------------------------------------------------------------------------*
  Name:         __OSSwitchThread
  
  Description:  Switches to the next thread. On PowerPC, this uses
                OSLoadContext to restore registers. On PC, we can't do
                true context switching, but we maintain the API.
 *---------------------------------------------------------------------------*/
static void __OSSwitchThread(OSThread* nextThread) {
    OSSetCurrentThread(nextThread);
    OSSetCurrentContext(&nextThread->context);
    // On PC, actual context switching is handled by OS threads
    // OSLoadContext(&nextThread->context);  // Not fully supported on PC
}

BOOL OSIsThreadSuspended(OSThread* thread) {
    return IsSuspended(thread->suspend) ? TRUE : FALSE;
}

BOOL OSIsThreadTerminated(OSThread* thread) {
    return ((thread->state == OS_THREAD_STATE_MORIBUND) ||
            (thread->state == 0)) ? TRUE : FALSE;
}

static BOOL __OSIsThreadActive(OSThread* thread) {
    OSThread* active;
    
    if (thread->state == 0) {
        return FALSE;
    }
    for (active = __OSActiveThreadQueue.head;
         active;
         active = active->linkActive.next) {
        if (thread == active) {
            return TRUE;
        }
    }
    return FALSE;
}

s32 OSDisableScheduler(void) {
    BOOL enabled;
    s32 count;
    
    enabled = OSDisableInterrupts();
    count = Reschedule++;
    OSRestoreInterrupts(enabled);
    return count;
}

s32 OSEnableScheduler(void) {
    BOOL enabled;
    s32 count;
    
    enabled = OSDisableInterrupts();
    count = Reschedule--;
    OSRestoreInterrupts(enabled);
    return count;
}

static void SetRun(OSThread* thread) {
    if (IsSuspended(thread->suspend)) return;
    if (thread->state != OS_THREAD_STATE_READY) return;
    
    thread->queue = &RunQueue[thread->priority];
    EnqueueTail(thread->queue, thread, link);
    RunQueueBits |= 1u << (OS_PRIORITY_MAX - thread->priority);
    RunQueueHint = TRUE;
}

static void UnsetRun(OSThread* thread) {
    OSThreadQueue* queue;
    
    queue = thread->queue;
    DequeueItem(queue, thread, link);
    if (queue->head == nullptr) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - thread->priority));
    }
    thread->queue = nullptr;
}

OSPriority __OSGetEffectivePriority(OSThread* thread) {
    OSPriority priority;
    OSMutex* mutex;
    OSThread* blocked;
    
    priority = thread->base;
    for (mutex = thread->queueMutex.head;
         mutex;
         mutex = mutex->link.next) {
        blocked = mutex->queue.head;
        if (blocked && blocked->priority < priority) {
            priority = blocked->priority;
        }
    }
    return priority;
}

static OSThread* SetEffectivePriority(OSThread* thread, OSPriority priority) {
    if (IsSuspended(thread->suspend)) {
        return nullptr;
    }
    
    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        UnsetRun(thread);
        thread->priority = priority;
        SetRun(thread);
        break;
    case OS_THREAD_STATE_WAITING:
        DequeueItem(thread->queue, thread, link);
        thread->priority = priority;
        EnqueuePrio(thread->queue, thread, link);
        if (thread->mutex) {
            return thread->mutex->thread;
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        thread->priority = priority;
        break;
    }
    return nullptr;
}

static void UpdatePriority(OSThread* thread) {
    OSPriority priority;
    OSThread* nextThread;
    
    do {
        if (IsSuspended(thread->suspend)) {
            break;
        }
        priority = __OSGetEffectivePriority(thread);
        if (thread->priority == priority) {
            break;
        }
        nextThread = SetEffectivePriority(thread, priority);
        thread = nextThread;
    } while (thread != nullptr);
}

void __OSPromoteThread(OSThread* thread, OSPriority priority) {
    OSThread* nextThread;
    
    do {
        if (IsSuspended(thread->suspend) ||
            thread->priority <= priority) {
            break;
        }
        nextThread = SetEffectivePriority(thread, priority);
        thread = nextThread;
    } while (thread != nullptr);
}

static OSThread* SelectThread(BOOL yield) {
    OSContext* currentContext;
    OSThread* currentThread;
    OSThread* nextThread;
    OSPriority priority;
    OSThreadQueue* queue;
    
    if (IsSuspended(Reschedule)) {
        return nullptr;
    }
    
    currentContext = OSGetCurrentContext();
    currentThread = OSGetCurrentThread();
    if (currentContext != &currentThread->context) {
        // Stacking context - can't switch
        return nullptr;
    }
    
    if (currentThread) {
        if (currentThread->state == OS_THREAD_STATE_RUNNING) {
            if (!yield) {
                priority = __cntlzw(RunQueueBits);
                if (currentThread->priority <= priority) {
                    return nullptr;
                }
            }
            currentThread->state = OS_THREAD_STATE_READY;
            SetRun(currentThread);
        }
        
        // On PC, we can't actually save/restore context like PowerPC
        // OSSaveContext would need to be implemented differently
        // For now, we skip the actual context save
    }
    
    if (RunQueueBits == 0) {
        OSSetCurrentThread(nullptr);
        OSSetCurrentContext(&IdleContext);
        
        // Idle loop - wait for threads to become ready
        do {
            OSEnableInterrupts();
            while (RunQueueBits == 0) {
#ifdef _WIN32
                Sleep(1);  /* Yield CPU; cooperative scheduler has no other threads to run */
#else
                usleep(1000);
#endif
            }
            OSDisableInterrupts();
        } while (RunQueueBits == 0);
        
        OSClearContext(&IdleContext);
    }
    
    RunQueueHint = FALSE;
    
    priority = __cntlzw(RunQueueBits);
    queue = &RunQueue[priority];
    DequeueHead(queue, nextThread, link);
    if (queue->head == nullptr) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - priority));
    }
    nextThread->queue = nullptr;
    nextThread->state = OS_THREAD_STATE_RUNNING;
    __OSSwitchThread(nextThread);
    // NOT REACHED HERE on original hardware
    return nextThread;
}

void __OSReschedule(void) {
    if (RunQueueHint) {
        SelectThread(FALSE);
    }
}

void OSYieldThread(void) {
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    SelectThread(TRUE);
    OSRestoreInterrupts(enabled);
}


// Helper for mutex queue
static inline void OSInitMutexQueue(OSMutexQueue* queue) {
    queue->head = queue->tail = nullptr;
}

BOOL OSCreateThread(
    OSThread*  thread,
    void*    (*func)(void*),
    void*      param,
    void*      stack,
    u32        stackSize,
    OSPriority priority,
    u16        attr)
{
    BOOL enabled;
    u32  sp;
    int  i;
    
    if (priority < OS_PRIORITY_MIN || OS_PRIORITY_MAX < priority) {
        return FALSE;
    }
    
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = (u16)(attr & OS_THREAD_ATTR_DETACH);
    thread->priority = thread->base = priority;
    thread->suspend = 1;    // suspended by default
    thread->val = (void*)(uintptr_t)-1;
    thread->mutex = nullptr;
    OSInitThreadQueue(&thread->queueJoin);
    OSInitMutexQueue(&thread->queueMutex);
    
    {
        uintptr_t sp_addr = (uintptr_t)stack;
        sp_addr &= ~(ALIGNMENT - 1);
        sp_addr -= ALIGNMENT;
        ((u32*)sp_addr)[0] = 0;  // stack back trace
        ((u32*)sp_addr)[1] = 0;  // LR save area
        sp = (u32)sp_addr;  // truncated for OSInitContext (PowerPC u32 ABI)
    }
    OSInitContext(&thread->context, (u32)(uintptr_t)func, sp);
    thread->context.lr = (u32)(uintptr_t)OSExitThread;
    thread->context.gpr[3] = (u32)(uintptr_t)param;
    thread->stackBase = (u8*)stack;
    thread->stackEnd = (u32*)((char*)stack - stackSize);
    
    // write magic value into end of stack (use real pointer, not truncated)
    *(thread->stackEnd) = OS_THREAD_STACK_MAGIC;
    
    thread->error = 0;
    for (i = 0; i < OS_THREAD_SPECIFIC_MAX; ++i) {
        thread->specific[i] = nullptr;
    }
    
    enabled = OSDisableInterrupts();
    // Note: FPU context initialization skipped for PC port
    if (__OSErrorTable && __OSErrorTable[OS_ERROR_FPE] != nullptr) {
        // On PC, we don't initialize PowerPC FPU context
        // This would be PowerPC-specific MSR and FPSCR setup
    }
    if (__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    EnqueueTail(&__OSActiveThreadQueue, thread, linkActive);
    OSRestoreInterrupts(enabled);
    
    return TRUE;
}

void OSExitThread(void* val) {
    BOOL      enabled;
    OSThread* currentThread;
    
    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();
    if (!currentThread) {
        OSRestoreInterrupts(enabled);
        return;
    }
    
    if (currentThread->state != OS_THREAD_STATE_RUNNING) {
        OSRestoreInterrupts(enabled);
        return;
    }
    
    if (!__OSIsThreadActive(currentThread)) {
        OSRestoreInterrupts(enabled);
        return;
    }
    
    OSClearContext(&currentThread->context);
    if (currentThread->attr & OS_THREAD_ATTR_DETACH) {
        DequeueItem(&__OSActiveThreadQueue, currentThread, linkActive);
        currentThread->state = 0;
    } else {
        currentThread->state = OS_THREAD_STATE_MORIBUND;
        currentThread->val = val;
    }
    
    // Release all the locked mutexes by current thread
    __OSUnlockAllMutex(currentThread);
    
    // Wakeup threads waiting to be joined
    OSWakeupThread(&currentThread->queueJoin);
    
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

void OSCancelThread(OSThread* thread) {
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return;
    }
    
    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        if (!IsSuspended(thread->suspend)) {
            UnsetRun(thread);
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        break;
    case OS_THREAD_STATE_WAITING:
        DequeueItem(thread->queue, thread, link);
        thread->queue = nullptr;
        if (!IsSuspended(thread->suspend) && thread->mutex) {
            UpdatePriority(thread->mutex->thread);
        }
        break;
    default:
        OSRestoreInterrupts(enabled);
        return;
    }
    
    OSClearContext(&thread->context);
    if (thread->attr & OS_THREAD_ATTR_DETACH) {
        DequeueItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    } else {
        thread->state = OS_THREAD_STATE_MORIBUND;
    }
    
    // Release all the mutexes locked by the thread
    __OSUnlockAllMutex(thread);
    
    // Wakeup threads waiting to be joined
    OSWakeupThread(&thread->queueJoin);
    
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

BOOL OSJoinThread(OSThread* thread, void** val) {
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    
    if (!(thread->attr & OS_THREAD_ATTR_DETACH) &&
        thread->state != OS_THREAD_STATE_MORIBUND &&
        thread->queueJoin.head == nullptr) {
        OSSleepThread(&thread->queueJoin);
        if (!__OSIsThreadActive(thread)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
    }
    
    if (((volatile OSThread*)thread)->state == OS_THREAD_STATE_MORIBUND) {
        if (val) {
            *val = thread->val;
        }
        
        DequeueItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
        OSRestoreInterrupts(enabled);
        return TRUE;
    }
    
    OSRestoreInterrupts(enabled);
    return FALSE;
}

void OSDetachThread(OSThread* thread) {
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return;
    }
    thread->attr |= OS_THREAD_ATTR_DETACH;
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        DequeueItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    }
    // Wakeup threads waiting to be joined
    OSWakeupThread(&thread->queueJoin);
    OSRestoreInterrupts(enabled);
}

s32 OSResumeThread(OSThread* thread) {
    BOOL enabled;
    s32  suspendCount;
    
    enabled = OSDisableInterrupts();
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return -1;
    }
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        OSRestoreInterrupts(enabled);
        return -1;
    }
    suspendCount = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_READY:
            thread->priority = __OSGetEffectivePriority(thread);
            SetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            if (thread->queue) {
                DequeueItem(thread->queue, thread, link);
                thread->priority = __OSGetEffectivePriority(thread);
                EnqueuePrio(thread->queue, thread, link);
                if (thread->mutex) {
                    UpdatePriority(thread->mutex->thread);
                }
            }
            break;
        }
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

s32 OSSuspendThread(OSThread* thread) {
    BOOL enabled;
    s32  suspendCount;
    
    enabled = OSDisableInterrupts();
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return -1;
    }
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        OSRestoreInterrupts(enabled);
        return -1;
    }
    suspendCount = thread->suspend++;
    if (suspendCount == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_RUNNING:
            RunQueueHint = TRUE;
            thread->state = OS_THREAD_STATE_READY;
            break;
        case OS_THREAD_STATE_READY:
            UnsetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            // Move thread at the tail of the queue
            DequeueItem(thread->queue, thread, link);
            thread->priority = 32;
            EnqueueTail(thread->queue, thread, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

void OSSleepThread(OSThreadQueue* queue) {
    BOOL      enabled;
    OSThread* currentThread;
    
    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();
    
    if (!currentThread) {
        OSRestoreInterrupts(enabled);
        return;
    }
    if (!__OSIsThreadActive(currentThread)) {
        OSRestoreInterrupts(enabled);
        return;
    }
    if (currentThread->state != OS_THREAD_STATE_RUNNING) {
        OSRestoreInterrupts(enabled);
        return;
    }
    if (IsSuspended(currentThread->suspend)) {
        OSRestoreInterrupts(enabled);
        return;
    }
    
    currentThread->state = OS_THREAD_STATE_WAITING;
    currentThread->queue = queue;
    EnqueuePrio(queue, currentThread, link);
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

void OSWakeupThread(OSThreadQueue* queue) {
    BOOL      enabled;
    OSThread* thread;
    
    enabled = OSDisableInterrupts();
    while (queue->head) {
        DequeueHead(queue, thread, link);
        if (!__OSIsThreadActive(thread)) continue;
        if (thread->state == OS_THREAD_STATE_MORIBUND) continue;
        if (thread->queue != queue) continue;
        
        thread->state = OS_THREAD_STATE_READY;
        if (!IsSuspended(thread->suspend)) {
            SetRun(thread);
        }
    }
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    BOOL enabled;
    
    if (priority < OS_PRIORITY_MIN || OS_PRIORITY_MAX < priority) {
        return FALSE;
    }
    enabled = OSDisableInterrupts();
    if (!__OSIsThreadActive(thread)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    if (thread->base != priority) {
        thread->base = priority;
        UpdatePriority(thread);
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return TRUE;
}

OSPriority OSGetThreadPriority(OSThread* thread) {
    return thread ? thread->base : OS_PRIORITY_MAX;
}

OSThread* OSSetIdleFunction(
    OSIdleFunction idleFunction,
    void*          param,
    void*          stack,
    u32            stackSize)
{
    if (idleFunction) {
        if (IdleThread.state == 0) {
            OSCreateThread(&IdleThread,
                          (void* (*)(void*))idleFunction,
                          param, stack, stackSize,
                          OS_PRIORITY_IDLE,
                          OS_THREAD_ATTR_DETACH);
            OSResumeThread(&IdleThread);
            return &IdleThread;
        }
    } else if (IdleThread.state != 0) {
        OSCancelThread(&IdleThread);
    }
    return nullptr;
}

OSThread* OSGetIdleFunction(void) {
    return (IdleThread.state != 0) ? &IdleThread : nullptr;
}

long OSCheckActiveThreads(void) {
    OSThread* thread;
    OSPriority prio;
    long cThread = 0;
    BOOL enabled;
    
    enabled = OSDisableInterrupts();
    
    // Check run queue bits and run queue consistency
    for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; prio++) {
        if (RunQueueBits & (1u << (OS_PRIORITY_MAX - prio))) {
            if (!(RunQueue[prio].head != nullptr && RunQueue[prio].tail != nullptr)) {
                OSReport("OSCheckActiveThreads: Run queue %d inconsistency\n", prio);
            }
        } else {
            if (!(RunQueue[prio].head == nullptr && RunQueue[prio].tail == nullptr)) {
                OSReport("OSCheckActiveThreads: Run queue %d should be empty\n", prio);
            }
        }
    }
    
    // Check active threads
    for (thread = __OSActiveThreadQueue.head;
         thread;
         thread = thread->linkActive.next) {
        cThread += 1;
        
        // check stack magic value
        if (*(thread->stackEnd) != OS_THREAD_STACK_MAGIC) {
            OSReport("OSCheckActiveThreads: Stack magic value mismatch for thread %p\n", thread);
        }
    }
    
    OSRestoreInterrupts(enabled);
    return cThread;
}

void OSClearStack(u8 val) {
    u32 sp = OSGetStackPointer();
    u32* p;
    u32 pattern;
    
    pattern = ((u32)val << 24) | ((u32)val << 16) | ((u32)val << 8) | (u32)val;
    for (p = __OSCurrentThread->stackEnd + 1; p < (u32*)sp; ++p) {
        *p = pattern;
    }
}

void OSSetThreadSpecific(s32 index, void* ptr) {
    OSThread* thread = __OSCurrentThread;
    
    if (thread && 0 <= index && index < OS_THREAD_SPECIFIC_MAX) {
        thread->specific[index] = ptr;
    }
}

void* OSGetThreadSpecific(s32 index) {
    OSThread* thread = __OSCurrentThread;
    
    if (thread && 0 <= index && index < OS_THREAD_SPECIFIC_MAX) {
        return thread->specific[index];
    }
    return nullptr;
}

static void SleepAlarmHandler(OSAlarm* alarm, OSContext* context) {
    (void)context;
    OSResumeThread((OSThread*)OSGetAlarmUserData(alarm));
}

void OSSleepTicks(OSTime tick) {
    BOOL enabled;
    OSThread* current;
    OSAlarm sleepAlarm;
    
    enabled = OSDisableInterrupts();
    current = OSGetCurrentThread();
    if (!current) {
        OSRestoreInterrupts(enabled);
        return;
    }
    OSCreateAlarm(&sleepAlarm);
    OSSetAlarmTag(&sleepAlarm, (u32)(uintptr_t)current);
    OSSetAlarmUserData(&sleepAlarm, (void*)current);
    OSSetAlarm(&sleepAlarm, tick, SleepAlarmHandler);
    OSSuspendThread(current);
    
    // Suspended - will resume when alarm fires
    
    OSCancelAlarm(&sleepAlarm);
    OSRestoreInterrupts(enabled);
}

#ifdef __cplusplus
}
#endif


