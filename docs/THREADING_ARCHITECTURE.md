# Threading Architecture: GC/Wii vs PC

This document explains the fundamental differences between the original GameCube/Wii threading model and our PC implementation.

---

## Original Hardware (GameCube/Wii)

### Cooperative Scheduler

The GC/Wii uses a **custom cooperative scheduler** implemented entirely in user space:

```
┌─────────────────────────────────────┐
│   32 Priority-Based Run Queues     │
│                                     │
│  [0] Highest ←─ Critical threads    │
│  [1]                                │
│  ...                                │
│  [16] Normal ←─ Most game threads   │
│  ...                                │
│  [31] Lowest ←─ Background tasks    │
└─────────────────────────────────────┘
          ↓
    SelectThread() ← Picks highest priority ready thread
          ↓
    __OSSwitchThread() ← Saves current context, loads new context
          ↓
    OSLoadContext() ← RFI instruction jumps to new thread
```

### Key Characteristics

**Single-Core CPU:**
- Only ONE thread runs at a time
- True cooperative multitasking
- Threads yield CPU explicitly (OSYieldThread) or when blocked

**Manual Thread Switching:**
```c
CurrentThread saves context → SelectThread picks next → LoadContext switches
```

**Priority Inheritance (BPI):**
- Prevents priority inversion
- When high-priority thread blocks on mutex held by low-priority thread
- Low-priority thread's priority is temporarily boosted
- Complex queue management required

**Deterministic:**
- Predictable thread switching
- No preemption (except interrupts)
- Games can rely on execution order

---

## PC Implementation (libPorpoise)

### Preemptive OS Threads

We use **platform threads** (Win32 or POSIX):

```
┌──────────────────────────────────────┐
│        Windows / Linux / macOS       │
│         OS Thread Scheduler          │
│    (Preemptive, Multi-core)          │
└──────────────────────────────────────┘
          ↓
    ┌─────┴─────┬─────┬─────┬─────┐
    │           │     │     │     │
  Thread 1  Thread 2  ...  Thread N
    │           │     │     │     │
  OSThread  OSThread ... OSThread
   (wrapper)
```

### Key Characteristics

**Multi-Core CPUs:**
- Threads run **in parallel** on different cores
- True concurrent execution
- Can't control which core runs which thread

**Automatic Scheduling:**
```c
OS decides when to switch → Preempts running threads → Context switching automatic
```

**OS-Managed Priorities:**
- We map GC/Wii priorities (0-31) to OS priorities
- OS handles priority inheritance
- Less control than original scheduler

**Non-Deterministic:**
- Thread interleaving varies
- Race conditions possible
- Games must use proper synchronization

---

## Comparison Table

| Feature | GC/Wii | PC (Porpoise) |
|---------|--------|---------------|
| **Model** | Cooperative | Preemptive |
| **Cores** | Single-core | Multi-core |
| **Switching** | Manual (OSLoadContext) | Automatic (OS) |
| **Priority Levels** | 32 (0-31) | Mapped to OS levels |
| **Priority Inheritance** | Manual (BPI) | OS-handled |
| **Determinism** | High | Low |
| **Parallelism** | None (one thread at a time) | True parallel |
| **Context** | PowerPC registers | x86/ARM registers |
| **Scheduler** | In SDK | In OS |

---

## API Mapping

### What Works the Same

```c
// These behave identically:
OSCreateThread(&thread, MyFunc, param, stack, stackSize, priority, attr);
OSResumeThread(&thread);        // Start thread
OSSuspendThread(&thread);       // Increment suspend count
OSJoinThread(&thread, &result); // Wait for termination
OSGetThreadPriority(&thread);   // Get priority
OSSetThreadPriority(&thread, newPriority); // Change priority
OSGetThreadSpecific(0);         // TLS slot 0
OSSetThreadSpecific(0, ptr);    // TLS slot 0
```

### What's Different

| Function | GC/Wii Behavior | PC Behavior |
|----------|-----------------|-------------|
| **OSYieldThread** | Switch to next ready thread | OS decides next thread |
| **OSSleepThread** | Sleep on specific queue | Generic sleep |
| **OSWakeupThread** | Wake queue, reschedule | Stub (no queue) |
| **OSDisableScheduler** | Prevent all thread switches | No-op (can't disable OS) |
| **OSEnableScheduler** | Allow thread switches | No-op |

---

## Threading Patterns

### Pattern 1: Basic Thread Creation (✅ Works Identically)

```c
// GC/Wii and PC - same code!
static u8 threadStack[16384];
static OSThread myThread;

void MyThreadFunc(void* arg) {
    OSReport("Thread running!\n");
    return NULL;
}

OSCreateThread(&myThread, MyThreadFunc, NULL,
               threadStack + 16384, 16384, 16, 0);
OSResumeThread(&myThread);
OSJoinThread(&myThread, NULL);
```

### Pattern 2: Thread Priority (⚠️ Different Behavior)

```c
// GC/Wii:
OSSetThreadPriority(&thread, 0); // Immediate effect, may reschedule
// Thread might preempt current thread RIGHT NOW

// PC:
OSSetThreadPriority(&thread, 0); // OS decides when to schedule
// Thread will run soon, but not immediately guaranteed
```

### Pattern 3: Critical Sections (⚠️ Need Different Approach)

```c
// GC/Wii:
OSDisableScheduler();
// Critical section - no thread switches will occur
global_data++;
OSEnableScheduler();

// PC - DON'T DO THIS, use mutex instead:
OSMutex mutex;
OSInitMutex(&mutex);
OSLockMutex(&mutex);
global_data++;
OSUnlockMutex(&mutex);
```

### Pattern 4: Spin-Wait (⚠️ Inefficient on PC)

```c
// GC/Wii:
while (!condition) {
    OSYieldThread(); // Cooperative yield
}

// PC - This works but is inefficient:
while (!condition) {
    OSYieldThread(); // OS may not actually yield
    OSSleepMilliseconds(1); // Better: sleep briefly
}

// PC - Even better, use condition variable:
OSMutex mutex;
OSCond cond;
OSLockMutex(&mutex);
while (!condition) {
    OSWaitCond(&cond, &mutex);
}
OSUnlockMutex(&mutex);
```

---

## Priority Mapping

### GC/Wii → Windows

| GC/Wii Priority | Windows Priority |
|-----------------|------------------|
| 0-7 (Critical) | THREAD_PRIORITY_TIME_CRITICAL |
| 8-15 (High) | THREAD_PRIORITY_ABOVE_NORMAL |
| 16-23 (Normal) | THREAD_PRIORITY_NORMAL |
| 24-31 (Low) | THREAD_PRIORITY_BELOW_NORMAL |

### GC/Wii → Linux

Linux priorities are more complex (require root for real-time priorities):
- Mapped to nice values (-20 to +19)
- Or SCHED_OTHER with default priority
- Real-time priorities (SCHED_FIFO) require special permissions

---

## Common Issues & Solutions

### Issue 1: Race Conditions

**Problem:**
```c
// On GC/Wii (single-core, cooperative):
global++;  // Safe if scheduler disabled

// On PC (multi-core, preemptive):
global++;  // NOT SAFE! Race condition!
```

**Solution:**
```c
// Use mutex
OSLockMutex(&mutex);
global++;
OSUnlockMutex(&mutex);

// Or atomic operations
__atomic_fetch_add(&global, 1, __ATOMIC_SEQ_CST);
```

### Issue 2: Thread Timing

**Problem:**
```c
// Game expects specific thread ordering
Thread1: Set flag
Thread2: Read flag  // Expects to see Thread1's change
```

**Solution:**
```c
// Use synchronization
OSMutex mutex;
OSCond cond;

// Thread 1:
OSLockMutex(&mutex);
flag = TRUE;
OSSignalCond(&cond);
OSUnlockMutex(&mutex);

// Thread 2:
OSLockMutex(&mutex);
while (!flag) OSWaitCond(&cond, &mutex);
OSUnlockMutex(&mutex);
```

### Issue 3: Spin Loops

**Problem:**
```c
// Wastes CPU on PC
while (!ready) {
    OSYieldThread();  // May not actually yield!
}
```

**Solution:**
```c
// Use condition variables or sleep
while (!ready) {
    OSSleepMilliseconds(1);
}
```

---

## Advanced Features NOT Implemented

### 1. Manual Run Queues
- Original has 32 priority queues
- We let OS scheduler handle it
- **Impact:** Can't guarantee exact thread ordering

### 2. Priority Inheritance (BPI)
- Original boosts priority when blocking on mutex
- OS does this automatically
- **Impact:** None - OS handles it better

### 3. Thread Queue Management
- Original: EnqueuePrio, DequeueHead, complex macros
- PC: OS manages thread queues
- **Impact:** OSSleepThread/OSWakeupThread don't work perfectly

### 4. Scheduler Control
- Original: OSDisableScheduler prevents ALL thread switches
- PC: Can't disable OS scheduler
- **Impact:** Games using this need mutex refactoring

---

## Performance Characteristics

### GC/Wii (Cooperative)
- **Thread switch:** ~100-200 cycles (very fast)
- **Predictable:** Always same timing
- **Overhead:** Minimal (no kernel involvement)

### PC (Preemptive)
- **Thread switch:** ~1000-10000 cycles (kernel transition)
- **Variable:** Depends on OS scheduler
- **Overhead:** Higher, but we have more CPU power

**Conclusion:** PC is slower per-thread-switch but has multiple cores and much faster CPUs overall.

---

## Migration Guide

### Easy Ports (Most Games)
Just use the API as-is. Most games will work fine:
```c
OSCreateThread(...);
OSResumeThread(...);
// Works!
```

### Medium Complexity
Replace scheduler-dependent code:
```c
// Old:
OSDisableScheduler();
critical_section();
OSEnableScheduler();

// New:
OSLockMutex(&mutex);
critical_section();
OSUnlockMutex(&mutex);
```

### High Complexity
Games with tight timing requirements:
- Analyze thread interactions
- Add proper synchronization (mutexes, condition variables)
- Test thoroughly for race conditions
- May need architectural changes

---

## Conclusion

**The Good News:**
- ✅ Most thread APIs work identically
- ✅ Platform threads are more powerful (parallel execution)
- ✅ OS handles hard stuff (priority inheritance, deadlock detection)

**The Trade-offs:**
- ⚠️ Can't control exact thread ordering
- ⚠️ Must use proper synchronization (mutexes, not scheduler disable)
- ⚠️ Race conditions possible (test well!)

**Overall:** ~90% of games will just work. The remaining 10% need careful analysis of thread interactions and synchronization.

---

**Last Updated:** 2025-11-09

