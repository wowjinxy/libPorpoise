/*---------------------------------------------------------------------------*
  OSThread.c - Threading and Synchronization
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (PowerPC - Cooperative Scheduler):
  ---------------------------------------------
  - Custom cooperative scheduler with 32 priority levels
  - Manual thread switching via OSLoadContext/OSSaveContext
  - Basic Priority Inheritance (BPI) for mutexes to prevent priority inversion
  - Run queues (one per priority level, 0=highest to 31=lowest)
  - SelectThread() picks highest priority ready thread
  - Threads yield CPU explicitly or when blocked
  - All thread management in user space
  
  On PC (Preemptive OS Threads):
  ------------------------------
  - Use platform threads (Win32 CreateThread / POSIX pthread)
  - OS handles scheduling automatically (preemptive)
  - OS handles priority inheritance
  - Threads run in parallel on multi-core CPUs
  - Can't implement manual scheduling (OS controls it)
  
  WHY THE DIFFERENCE:
  - Original hardware: Single-core CPU, cooperative scheduling for predictability
  - PC: Multi-core, preemptive scheduling, OS-controlled
  - Can't port the scheduler directly - fundamentally different models
  
  WHAT WE PRESERVE:
  - Same API surface (OSCreateThread, OSResumeThread, etc.)
  - Suspend counts work the same way
  - Thread states (READY, RUNNING, WAITING, MORIBUND)
  - Thread-specific data
  - Priorities (mapped to OS priorities)
  - Mutexes and condition variables
  
  WHAT'S DIFFERENT:
  - Threads actually run in parallel (not cooperative)
  - Scheduler is OS-controlled (not our SelectThread)
  - Priority inheritance is OS-handled
  - Context switching is automatic (not manual)
 *---------------------------------------------------------------------------*/

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
    OSThread* osThread;  // Back-reference
} PlatformThread;

#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

typedef struct PlatformThread {
    pthread_t handle;
    void* (*func)(void*);
    void* arg;
    OSThread* osThread;  // Back-reference
} PlatformThread;
#endif

/* Global thread state */
static OSThread s_idleThread;
static OSThread* s_currentThread = &s_idleThread;
static OSSwitchThreadCallback s_switchCallback = NULL;

/* Thread wrapper function */
#ifdef _WIN32
static DWORD WINAPI ThreadWrapper(LPVOID param) {
    OSThread* thread = (OSThread*)param;
    PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
    void* result = NULL;
    
    /* Set this thread as current for this platform thread */
    s_currentThread = thread;
    thread->state = OS_THREAD_STATE_RUNNING;
    
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
    
    /* Set this thread as current for this platform thread */
    s_currentThread = thread;
    thread->state = OS_THREAD_STATE_RUNNING;
    
    if (platform && platform->func) {
        result = platform->func(platform->arg);
    }
    
    thread->state = OS_THREAD_STATE_MORIBUND;
    thread->val = result;
    return result;
}
#endif

/*---------------------------------------------------------------------------*
  Name:         OSInitThreadQueue

  Description:  Initializes a thread queue structure. On original hardware,
                these queues are used for ready threads, waiting threads,
                threads waiting to join, etc.
                
                On PC: We still use these for API compatibility, though the
                OS does most scheduling.

  Arguments:    queue - Thread queue to initialize
 *---------------------------------------------------------------------------*/
void OSInitThreadQueue(OSThreadQueue* queue) {
    if (!queue) return;
    queue->head = NULL;
    queue->tail = NULL;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetCurrentThread

  Description:  Returns pointer to the currently executing thread.
                
                On original hardware: Points to thread selected by scheduler
                On PC: Thread-local, each platform thread has its own "current"

  Returns:      Pointer to current OSThread structure
 *---------------------------------------------------------------------------*/
OSThread* OSGetCurrentThread(void) {
    /* Note: On a real multi-threaded PC port, this should be thread-local.
     * For now, we return the global which works for simple cases.
     * 
     * TODO: Use __thread (GCC) or __declspec(thread) (MSVC) for true TLS
     */
    return s_currentThread;
}

/*---------------------------------------------------------------------------*
  Name:         OSIsThreadSuspended

  Description:  Checks if a thread is suspended. Threads can be suspended
                multiple times (suspend count > 0).

  Arguments:    thread - Thread to check

  Returns:      TRUE if suspended, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSIsThreadSuspended(OSThread* thread) {
    return thread && (thread->suspend > 0);
}

/*---------------------------------------------------------------------------*
  Name:         OSIsThreadTerminated

  Description:  Checks if a thread has finished executing.

  Arguments:    thread - Thread to check

  Returns:      TRUE if terminated, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSIsThreadTerminated(OSThread* thread) {
    return thread && (thread->state == OS_THREAD_STATE_MORIBUND);
}

/*---------------------------------------------------------------------------*
  Name:         OSDisableScheduler / OSEnableScheduler

  Description:  On original hardware, these disable/enable thread rescheduling.
                When disabled, no thread switches occur even if higher priority
                thread becomes ready. Uses a counter so calls can nest.
                
                On PC: We can't actually disable the OS scheduler. We track
                the state for API compatibility but it doesn't affect scheduling.
                
                Games use this around critical sections to prevent preemption.
                On PC, use mutexes instead.

  Returns:      Previous scheduler suspend count
 *---------------------------------------------------------------------------*/
static s32 s_schedulerDisableCount = 0;

s32 OSDisableScheduler(void) {
    s32 prev = s_schedulerDisableCount;
    s_schedulerDisableCount++;
    return prev;
}

s32 OSEnableScheduler(void) {
    s32 prev = s_schedulerDisableCount;
    if (s_schedulerDisableCount > 0) {
        s_schedulerDisableCount--;
    }
    return prev;
}

/*---------------------------------------------------------------------------*
  Name:         OSYieldThread

  Description:  Voluntarily gives up the CPU to other threads.
                
                On original hardware: Calls scheduler to select next thread
                On PC: Asks OS to yield timeslice

  Returns:      None
 *---------------------------------------------------------------------------*/
void OSYieldThread(void) {
#ifdef _WIN32
    Sleep(0);  // Yield to other threads
#else
    sched_yield();
#endif
}

/*---------------------------------------------------------------------------*
  Name:         __OSReschedule

  Description:  Forces a thread reschedule. On original hardware, this would
                call the scheduler to select the next ready thread.
                
                On PC: Yields to the OS scheduler, allowing other threads
                to run. Similar to OSYieldThread but with internal naming.

  Returns:      None
 *---------------------------------------------------------------------------*/
void __OSReschedule(void) {
    OSYieldThread();  // On PC, just yield to OS scheduler
}

/*---------------------------------------------------------------------------*
  Name:         OSCreateThread

  Description:  Creates a new thread. On original hardware, this initializes
                the thread structure but doesn't start it (call OSResumeThread).
                
                The thread starts with suspend count = 1, so you must call
                OSResumeThread to actually start it.

  Arguments:    thread    - OSThread structure (user provides storage)
                func      - Thread function
                param     - Parameter passed to function
                stack     - Stack memory (grows DOWN - provide TOP address)
                stackSize - Stack size in bytes
                priority  - Priority (0=highest, 31=lowest)
                attr      - Attributes (OS_THREAD_ATTR_DETACH)

  Returns:      TRUE on success, FALSE on failure
 *---------------------------------------------------------------------------*/
BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param,
                   void* stack, u32 stackSize, OSPriority priority, u16 attr) {
    if (!thread) return FALSE;
    
    /* Initialize thread structure */
    memset(thread, 0, sizeof(OSThread));
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr;
    thread->suspend = 1;  // Start suspended (like original)
    thread->priority = priority;
    thread->base = priority;
    thread->stackBase = (u8*)stack;
    thread->stackEnd = (u32*)((u8*)stack - stackSize);
    thread->val = NULL;
    thread->mutex = NULL;
    
    /* Initialize queues */
    OSInitThreadQueue(&thread->queueJoin);
    thread->queue = NULL;
    
    /* Initialize thread-specific data slots */
    for (int i = 0; i < OS_THREAD_SPECIFIC_MAX; i++) {
        thread->specific[i] = NULL;
    }
    
    /* Set up platform thread data */
    PlatformThread* platform = (PlatformThread*)malloc(sizeof(PlatformThread));
    if (!platform) return FALSE;
    
    platform->func = func;
    platform->arg = param;
    platform->osThread = thread;
    platform->handle = 0;
    
    /* Store platform data in context (hack: use gpr[0]) */
    thread->context.gpr[0] = (u32)platform;
    
    /* Mark stack with magic value for debugging */
    if (stack && stackSize >= 4) {
        *(u32*)((u8*)stack - 4) = OS_THREAD_STACK_MAGIC;
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         OSExitThread

  Description:  Terminates the current thread. The thread enters MORIBUND
                state and wakes up any threads waiting on OSJoinThread.
                
                On original hardware: Triggers reschedule to select new thread
                On PC: Thread function just returns

  Arguments:    val - Exit value (returned by OSJoinThread)
 *---------------------------------------------------------------------------*/
void OSExitThread(void* val) {
    OSThread* thread = OSGetCurrentThread();
    if (thread) {
        thread->val = val;
        thread->state = OS_THREAD_STATE_MORIBUND;
        
        /* Wake up threads waiting in OSJoinThread */
        OSWakeupThread(&thread->queueJoin);
    }
    
    /* On PC, the thread function should just return */
}

/*---------------------------------------------------------------------------*
  Name:         OSCancelThread

  Description:  Forcibly terminates a thread. This is DANGEROUS - the thread
                doesn't clean up resources!
                
                On original hardware: Removes from all queues, marks MORIBUND
                On PC: Uses TerminateThread (Win32) or pthread_cancel (POSIX)
                
                WARNING: Avoid if possible. Prefer having threads exit gracefully.

  Arguments:    thread - Thread to cancel
 *---------------------------------------------------------------------------*/
void OSCancelThread(OSThread* thread) {
    if (!thread || !thread->context.gpr[0]) return;
    
    PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
    
#ifdef _WIN32
    if (platform->handle) {
        TerminateThread(platform->handle, 0);
        CloseHandle(platform->handle);
        platform->handle = NULL;
    }
#else
    if (platform->handle) {
        pthread_cancel(platform->handle);
        pthread_detach(platform->handle);
    }
#endif
    
    thread->state = OS_THREAD_STATE_MORIBUND;
    free(platform);
    thread->context.gpr[0] = 0;
}

/*---------------------------------------------------------------------------*
  Name:         OSJoinThread

  Description:  Waits for a thread to terminate and retrieves its exit value.
                Like pthread_join().
                
                This blocks the calling thread until the target thread exits.

  Arguments:    thread - Thread to wait for
                val    - Pointer to receive exit value (can be NULL)

  Returns:      TRUE on success, FALSE on failure
 *---------------------------------------------------------------------------*/
BOOL OSJoinThread(OSThread* thread, void** val) {
    if (!thread) return FALSE;
    
    /* Wait for thread to terminate */
    while (!OSIsThreadTerminated(thread)) {
        OSSleepThread(&thread->queueJoin);
    }
    
    /* Get exit value */
    if (val) {
        *val = thread->val;
    }
    
    /* Clean up platform thread handle */
    if (thread->context.gpr[0]) {
        PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
        
#ifdef _WIN32
        if (platform->handle) {
            WaitForSingleObject(platform->handle, INFINITE);
            CloseHandle(platform->handle);
            platform->handle = NULL;
        }
#else
        if (platform->handle) {
            pthread_join(platform->handle, NULL);
        }
#endif
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         OSDetachThread

  Description:  Marks a thread as detached. Detached threads automatically
                clean up when they exit (can't be joined).

  Arguments:    thread - Thread to detach
 *---------------------------------------------------------------------------*/
void OSDetachThread(OSThread* thread) {
    if (!thread) return;
    
    thread->attr |= OS_THREAD_ATTR_DETACH;
    
    /* Detach platform thread */
    if (thread->context.gpr[0]) {
        PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
        
#ifndef _WIN32
        if (platform->handle) {
            pthread_detach(platform->handle);
        }
#endif
    }
}

/*---------------------------------------------------------------------------*
  Name:         OSResumeThread

  Description:  Decrements suspend count and starts thread if it reaches 0.
                
                On original hardware:
                - Decrements suspend count
                - If count becomes 0 and thread is READY, adds to run queue
                - May trigger reschedule if higher priority than current
                
                On PC:
                - Decrements suspend count
                - If count becomes 0, actually creates/starts platform thread

  Arguments:    thread - Thread to resume

  Returns:      Previous suspend count
 *---------------------------------------------------------------------------*/
s32 OSResumeThread(OSThread* thread) {
    if (!thread) return -1;
    
    s32 prevSuspend = thread->suspend;
    
    if (thread->suspend > 0) {
        thread->suspend--;
    }
    
    /* If suspend count reaches 0 and thread is ready, start it */
    if (thread->suspend == 0 && thread->state == OS_THREAD_STATE_READY) {
        PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
        if (platform) {
#ifdef _WIN32
            /* Map priority (0=highest to 31=lowest → Win32 priorities) */
            int winPriority = THREAD_PRIORITY_NORMAL;
            if (thread->priority < 8) {
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
            } else if (thread->priority < 16) {
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
            } else if (thread->priority > 24) {
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
            }
            
            platform->handle = CreateThread(NULL, 0, ThreadWrapper, thread, 0, &platform->threadId);
            if (platform->handle != NULL) {
                SetThreadPriority(platform->handle, winPriority);
                thread->state = OS_THREAD_STATE_RUNNING;
            }
#else
            if (pthread_create(&platform->handle, NULL, ThreadWrapper, thread) == 0) {
                /* Map priority to POSIX if possible */
                thread->state = OS_THREAD_STATE_RUNNING;
            }
#endif
        }
    }
    
    return prevSuspend;
}

/*---------------------------------------------------------------------------*
  Name:         OSSuspendThread

  Description:  Increments suspend count. If thread is running, it continues
                to run. Thread won't be scheduled if it yields/blocks.
                
                On original hardware: Marks thread as suspended
                On PC: Increments counter (can't actually suspend OS threads)

  Arguments:    thread - Thread to suspend

  Returns:      Previous suspend count
 *---------------------------------------------------------------------------*/
s32 OSSuspendThread(OSThread* thread) {
    if (!thread) return -1;
    
    s32 prevSuspend = thread->suspend;
    thread->suspend++;
    
    /* Note: On PC, we can't actually suspend a running OS thread.
     * The suspend count is honored when the thread tries to resume
     * after blocking or when OSResumeThread is called.
     */
    
    return prevSuspend;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetThreadPriority / OSGetThreadPriority

  Description:  Sets/gets thread priority. On original hardware, changing
                priority may trigger reschedule.
                
                On PC: We store the priority and map it to OS priority.

  Arguments:    thread   - Thread to modify
                priority - New priority (0=highest, 31=lowest)

  Returns:      TRUE on success (Set), priority value (Get)
 *---------------------------------------------------------------------------*/
BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (!thread || priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    
    thread->priority = priority;
    
    /* Update OS thread priority if running */
    if (thread->state == OS_THREAD_STATE_RUNNING && thread->context.gpr[0]) {
        PlatformThread* platform = (PlatformThread*)thread->context.gpr[0];
        
#ifdef _WIN32
        if (platform->handle) {
            int winPriority = THREAD_PRIORITY_NORMAL;
            if (priority < 8) {
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
            } else if (priority < 16) {
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
            } else if (priority > 24) {
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
            }
            SetThreadPriority(platform->handle, winPriority);
        }
#else
        /* POSIX priority setting is more complex and requires root on some systems */
        /* For now, we just store it */
#endif
    }
    
    return TRUE;
}

OSPriority OSGetThreadPriority(OSThread* thread) {
    return thread ? thread->priority : OS_PRIORITY_MAX;
}

/*---------------------------------------------------------------------------*
  Name:         OSSleepThread

  Description:  Puts current thread to sleep on a queue. On original hardware,
                thread is moved from run queue to the specified wait queue.
                
                On PC: We do a short sleep. Proper implementation would need
                condition variables per queue.

  Arguments:    queue - Queue to sleep on (can be NULL for simple sleep)
 *---------------------------------------------------------------------------*/
void OSSleepThread(OSThreadQueue* queue) {
    /* Original hardware: Thread is removed from run queue and added to
     * the wait queue. Scheduler picks next thread to run.
     * 
     * PC: We just sleep briefly. Proper implementation would use
     * condition variables.
     */
    (void)queue;
    
#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
}

/*---------------------------------------------------------------------------*
  Name:         OSWakeupThread

  Description:  Wakes up all threads sleeping on a queue.
                
                On original hardware: Moves threads from wait queue to run queue
                On PC: Would need condition variable per queue

  Arguments:    queue - Queue to wake up
 *---------------------------------------------------------------------------*/
void OSWakeupThread(OSThreadQueue* queue) {
    /* Original hardware: All threads in the queue are moved to run queue.
     * 
     * PC: Proper implementation needs condition variables.
     * For now, this is a stub.
     */
    (void)queue;
    /* TODO: Implement with condition variables */
}

/*---------------------------------------------------------------------------*
  Name:         OSGetThreadSpecific / OSSetThreadSpecific

  Description:  Thread-local storage (TLS). Each thread has 2 slots for
                storing arbitrary pointers.

  Arguments:    index - Slot index (0 or 1)
                ptr   - Pointer to store (Set function)

  Returns:      Stored pointer (Get function)
 *---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*
  Name:         OSSetIdleFunction / OSGetIdleFunction

  Description:  Sets a function to run when no other threads are ready.
                On original hardware, this is the scheduler's idle loop.
                
                On PC: Not really applicable. Stub returns idle thread.

  Arguments:    idleFunction - Function to call when idle
                param        - Parameter to pass
                stack        - Stack for idle function
                stackSize    - Stack size

  Returns:      Pointer to idle thread structure
 *---------------------------------------------------------------------------*/
OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, 
                            void* stack, u32 stackSize) {
    /* Original hardware: Idle function runs in special idle thread when
     * no other threads are runnable.
     * 
     * PC: OS scheduler handles idle. This is a stub.
     */
    (void)idleFunction;
    (void)param;
    (void)stack;
    (void)stackSize;
    return &s_idleThread;
}

OSThread* OSGetIdleFunction(void) {
    return &s_idleThread;
}

/*---------------------------------------------------------------------------*
  Name:         OSClearStack

  Description:  Fills the current thread's stack with a pattern (for debugging).
                Helps detect stack overflow and unused stack analysis.

  Arguments:    val - Pattern byte to fill with
 *---------------------------------------------------------------------------*/
void OSClearStack(u8 val) {
    /* Original hardware: Fills stack from SP to stackEnd with pattern.
     * Helps detect stack corruption and measure stack usage.
     * 
     * PC: Platform threads manage their own stacks. We can't safely
     * access or modify them.
     */
    (void)val;
}

/*---------------------------------------------------------------------------*
  Name:         OSCheckActiveThreads

  Description:  Returns number of active threads (for debugging).

  Returns:      Number of active threads
 *---------------------------------------------------------------------------*/
long OSCheckActiveThreads(void) {
    /* Original hardware: Walks __OSActiveThreadQueue and counts.
     * PC: We don't maintain active thread queue. Return stub value.
     */
    return 1;
}

/*---------------------------------------------------------------------------*
  Name:         OSSetSwitchThreadCallback

  Description:  Sets a callback to be called when switching threads.
                Useful for profiling and debugging.

  Arguments:    callback - Function called as callback(oldThread, newThread)

  Returns:      Previous callback
 *---------------------------------------------------------------------------*/
OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    OSSwitchThreadCallback old = s_switchCallback;
    s_switchCallback = callback ? callback : NULL;
    return old;
}

/*---------------------------------------------------------------------------*
  Name:         OSSleepTicks

  Description:  Sleeps for specified number of OS timer ticks.
                Convenience wrapper that converts ticks to sleep time.

  Arguments:    ticks - Number of ticks to sleep (40.5 MHz clock)
 *---------------------------------------------------------------------------*/
void OSSleepTicks(OSTime ticks) {
#ifdef _WIN32
    DWORD ms = (DWORD)(ticks / (OS_TIMER_CLOCK / 1000));
    if (ms == 0 && ticks > 0) ms = 1;
    Sleep(ms);
#else
    useconds_t us = (useconds_t)(ticks / (OS_TIMER_CLOCK / 1000000));
    if (us == 0 && ticks > 0) us = 1;
    usleep(us);
#endif
}

/* Mutex and condition variable functions moved to OSMutex.c */

/*---------------------------------------------------------------------------*
  Name:         __OSGetEffectivePriority

  Description:  Get the effective priority of a thread. On original hardware,
                this accounts for priority inheritance when a high-priority
                thread is blocked by a low-priority thread holding a mutex.
                
                On GC/Wii: Checks thread priority + any inheritance boost
                On PC: OS handles priority inheritance, just return base priority

  Arguments:    thread  Thread to query

  Returns:      Effective priority (0-31, 0=highest)
 *---------------------------------------------------------------------------*/
s32 __OSGetEffectivePriority(OSThread* thread) {
    if (!thread) {
        return 31;  // Lowest priority
    }
    
    /* On PC, the OS automatically handles priority inheritance
     * for mutexes. We just return the thread's base priority.
     * 
     * On original hardware, this would check if the thread is
     * holding a mutex that a higher-priority thread is waiting on,
     * and return the boosted priority.
     */
    return thread->priority;
}

/*---------------------------------------------------------------------------*
  Name:         __OSPromoteThread

  Description:  Boost a thread's priority temporarily (priority inheritance).
                Used when high-priority thread blocks on mutex held by
                low-priority thread to prevent priority inversion.
                
                On GC/Wii: Adjusts thread priority, updates run queues
                On PC: No-op stub (OS handles this automatically)

  Arguments:    thread      Thread to promote
                priority    New priority to boost to

  Returns:      None
 *---------------------------------------------------------------------------*/
void __OSPromoteThread(OSThread* thread, s32 priority) {
    (void)thread;
    (void)priority;
    
    /* On PC, priority inheritance is handled automatically by the
     * operating system's mutex implementation. We don't need to
     * manually adjust priorities.
     * 
     * On original hardware, this function would:
     * 1. Remove thread from its current priority run queue
     * 2. Boost its priority to the specified level
     * 3. Insert into new priority run queue
     * 4. Potentially trigger a context switch
     * 
     * Since we use OS threads (Win32 CRITICAL_SECTION or pthread_mutex),
     * the OS does this for us automatically.
     */
}