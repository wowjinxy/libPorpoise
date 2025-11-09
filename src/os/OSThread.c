#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef struct PlatformThread {
    HANDLE handle;
    DWORD threadId;
    void* (*func)(void*);
    void* arg;
} PlatformThread;
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
typedef struct PlatformThread {
    pthread_t handle;
    void* (*func)(void*);
    void* arg;
} PlatformThread;
#endif

static OSThread s_idleThread;
static OSThread* s_currentThread = &s_idleThread;
static OSSwitchThreadCallback s_switchCallback = NULL;

#ifdef _WIN32
static DWORD WINAPI ThreadWrapper(LPVOID param) {
    OSThread* thread = (OSThread*)param;
    PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
    void* result = NULL;
    
    if (platform && platform->func) {
        result = platform->func(platform->arg);
    }
    
    thread->state = OS_THREAD_STATE_MORIBUND;
    thread->val = result;
    return 0;
}
#else
static void* ThreadWrapper(void* param) {
    OSThread* thread = (OSThread*)param;
    PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
    void* result = NULL;
    
    if (platform && platform->func) {
        result = platform->func(platform->arg);
    }
    
    thread->state = OS_THREAD_STATE_MORIBUND;
    thread->val = result;
    return result;
}
#endif

void OSInitThreadQueue(OSThreadQueue* queue) {
    if (!queue) return;
    queue->head = NULL;
    queue->tail = NULL;
}

OSThread* OSGetCurrentThread(void) {
    return s_currentThread;
}

BOOL OSIsThreadSuspended(OSThread* thread) {
    return thread && (thread->suspend > 0);
}

BOOL OSIsThreadTerminated(OSThread* thread) {
    return thread && (thread->state == OS_THREAD_STATE_MORIBUND);
}

s32 OSDisableScheduler(void) {
    // Stub: return previous state
    return 1;
}

s32 OSEnableScheduler(void) {
    // Stub: return previous state
    return 0;
}

void OSYieldThread(void) {
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param,
                   void* stack, u32 stackSize, OSPriority priority, u16 attr) {
    if (!thread) return FALSE;
    
    memset(thread, 0, sizeof(OSThread));
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr;
    thread->suspend = 0;
    thread->priority = priority;
    thread->base = priority;
    thread->stackBase = (u8*)stack;
    thread->stackEnd = (u32*)((u8*)stack - stackSize);
    
    // Set up platform thread data
    PlatformThread* platform = (PlatformThread*)malloc(sizeof(PlatformThread));
    if (!platform) return FALSE;
    
    platform->func = func;
    platform->arg = param;
    thread->context.gpr[0] = (u32)platform;
    
    // Mark stack
    if (stack && stackSize >= 4) {
        *(u32*)((u8*)stack - 4) = OS_THREAD_STACK_MAGIC;
    }
    
    return TRUE;
}

void OSExitThread(void* val) {
    OSThread* thread = OSGetCurrentThread();
    if (thread) {
        thread->val = val;
        thread->state = OS_THREAD_STATE_MORIBUND;
    }
}

void OSCancelThread(OSThread* thread) {
    if (!thread || !thread->context.gpr[0]) return;
    
    PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
    
#ifdef _WIN32
    if (platform->handle) {
        TerminateThread(platform->handle, 0);
        CloseHandle(platform->handle);
    }
#else
    pthread_cancel(platform->handle);
    pthread_detach(platform->handle);
#endif
    
    thread->state = OS_THREAD_STATE_MORIBUND;
    free(platform);
    thread->context.gpr[0] = 0;
}

BOOL OSJoinThread(OSThread* thread, void** val) {
    if (!thread) return FALSE;
    
    while (!OSIsThreadTerminated(thread)) {
        OSSleepThread(NULL);
    }
    
    if (val) {
        *val = thread->val;
    }
    return TRUE;
}

void OSDetachThread(OSThread* thread) {
    if (thread) {
        thread->attr |= OS_THREAD_ATTR_DETACH;
    }
}

s32 OSResumeThread(OSThread* thread) {
    if (!thread) return -1;
    
    s32 prevSuspend = thread->suspend;
    
    if (thread->suspend > 0) {
        thread->suspend--;
    }
    
    if (thread->suspend == 0 && thread->state == OS_THREAD_STATE_READY) {
        PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
        if (platform) {
#ifdef _WIN32
            platform->handle = CreateThread(NULL, 0, ThreadWrapper, thread, 0, &platform->threadId);
            if (platform->handle != NULL) {
                thread->state = OS_THREAD_STATE_RUNNING;
            }
#else
            if (pthread_create(&platform->handle, NULL, ThreadWrapper, thread) == 0) {
                thread->state = OS_THREAD_STATE_RUNNING;
            }
#endif
        }
    }
    
    return prevSuspend;
}

s32 OSSuspendThread(OSThread* thread) {
    if (!thread) return -1;
    
    s32 prevSuspend = thread->suspend;
    thread->suspend++;
    
    return prevSuspend;
}

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (!thread || priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    
    thread->priority = priority;
    return TRUE;
}

OSPriority OSGetThreadPriority(OSThread* thread) {
    return thread ? thread->priority : OS_PRIORITY_MAX;
}

void OSSleepThread(OSThreadQueue* queue) {
#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
}

void OSWakeupThread(OSThreadQueue* queue) {
    // Stub: wake up threads in queue
}

void* OSGetThreadSpecific(s32 index) {
    OSThread* thread = OSGetCurrentThread();
    if (thread && index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
        return thread->specific[index];
    }
    return NULL;
}

void OSSetThreadSpecific(s32 index, void* ptr) {
    OSThread* thread = OSGetCurrentThread();
    if (thread && index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
        thread->specific[index] = ptr;
    }
}

OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize) {
    // Stub: return idle thread
    return &s_idleThread;
}

OSThread* OSGetIdleFunction(void) {
    return &s_idleThread;
}

void OSClearStack(u8 val) {
    // Stub: clear stack with pattern
}

long OSCheckActiveThreads(void) {
    // Stub: return active thread count
    return 1;
}

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    OSSwitchThreadCallback old = s_switchCallback;
    s_switchCallback = callback;
    return old;
}

void OSSleepTicks(OSTime ticks) {
#ifdef _WIN32
    Sleep((DWORD)(ticks / (OS_TIMER_CLOCK / 1000)));
#else
    usleep((useconds_t)(ticks / (OS_TIMER_CLOCK / 1000000)));
#endif
}

/* Mutex Functions */
void OSInitMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    mutex->thread = NULL;
    mutex->count = 0;
    mutex->queue.head = NULL;
    mutex->queue.tail = NULL;
}

void OSLockMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    OSThread* current = OSGetCurrentThread();
    if (mutex->thread == current) {
        mutex->count++;
    } else {
        while (mutex->thread != NULL) {
            OSSleepThread(NULL);
        }
        mutex->thread = current;
        mutex->count = 1;
    }
}

void OSUnlockMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    if (mutex->count > 0) {
        mutex->count--;
        if (mutex->count == 0) {
            mutex->thread = NULL;
        }
    }
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    if (!mutex) return FALSE;
    
    if (mutex->thread == NULL) {
        mutex->thread = OSGetCurrentThread();
        mutex->count = 1;
        return TRUE;
    }
    
    return FALSE;
}

void OSInitCond(OSCond* cond) {
    if (!cond) return;
    cond->queue.head = NULL;
    cond->queue.tail = NULL;
}

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
    // Stub: wait on condition variable
    OSUnlockMutex(mutex);
    OSSleepThread(&cond->queue);
    OSLockMutex(mutex);
}

void OSSignalCond(OSCond* cond) {
    // Stub: signal condition variable
    if (cond) {
        OSWakeupThread(&cond->queue);
    }
}
