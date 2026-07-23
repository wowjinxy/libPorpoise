/*---------------------------------------------------------------------------*
  OSSemaphore.c - Counting Semaphores
  
  SEMAPHORE CONCEPT:
  ==================
  
  A semaphore is a synchronization primitive with a counter:
  
  **Counting Semaphore:**
  ```
  Counter = 3
  ┌────────────────┐
  │  Count: 3      │  ← OSSignalSemaphore() increments
  │  Queue: []     │
  └────────────────┘
         │
         │  Thread A: OSWaitSemaphore()  → Count becomes 2, thread continues
         │  Thread B: OSWaitSemaphore()  → Count becomes 1, thread continues
         │  Thread C: OSWaitSemaphore()  → Count becomes 0, thread continues
         │  Thread D: OSWaitSemaphore()  → Count is 0, thread BLOCKS
         │
         ▼
  ┌────────────────┐
  │  Count: 0      │
  │  Queue: [D]    │  ← Thread D waiting
  └────────────────┘
         │
         │  OSSignalSemaphore()  → Count becomes 1, wakes Thread D
         │
         ▼
  ┌────────────────┐
  │  Count: 0      │  ← Thread D consumed the signal
  │  Queue: []     │
  └────────────────┘
  ```
  
  **Key Properties:**
  - **Counter**: Can be 0 or positive (negative not allowed)
  - **Wait**: Decrements counter, blocks if 0
  - **Signal**: Increments counter, wakes waiting thread
  - **Binary Semaphore**: Counter limited to 0-1 (similar to mutex)
  - **Counting Semaphore**: Counter can be any value (resource pool)
  
  COMMON USE CASES:
  =================
  
  **1. Resource Pool (Counting Semaphore):**
  ```c
  // 4 audio buffers available
  OSSemaphore audioBuffers;
  OSInitSemaphore(&audioBuffers, 4);
  
  void UseAudioBuffer() {
      OSWaitSemaphore(&audioBuffers);  // Wait for available buffer
      Buffer* buf = GetFreeBuffer();
      UseBuffer(buf);
      ReleaseBuffer(buf);
      OSSignalSemaphore(&audioBuffers);  // Return buffer to pool
  }
  ```
  
  **2. Producer-Consumer (Event Counting):**
  ```c
  OSSemaphore itemsAvailable;
  OSInitSemaphore(&itemsAvailable, 0);  // Start with 0
  
  // Producer
  void Produce() {
      Item* item = CreateItem();
      AddToQueue(item);
      OSSignalSemaphore(&itemsAvailable);  // Signal new item
  }
  
  // Consumer
  void Consume() {
      OSWaitSemaphore(&itemsAvailable);  // Wait for item
      Item* item = RemoveFromQueue();
      ProcessItem(item);
  }
  ```
  
  **3. Thread Barrier (Synchronization Point):**
  ```c
  OSSemaphore barrier;
  OSInitSemaphore(&barrier, 0);
  
  void WorkerThread() {
      DoWork();
      OSSignalSemaphore(&barrier);  // Signal completion
  }
  
  void MainThread() {
      // Wait for 4 workers to complete
      for (int i = 0; i < 4; i++) {
          OSWaitSemaphore(&barrier);
      }
      AllWorkersComplete();
  }
  ```
  
  **4. Rate Limiting:**
  ```c
  OSSemaphore requestLimit;
  OSInitSemaphore(&requestLimit, 10);  // Max 10 concurrent requests
  
  void MakeRequest() {
      if (OSTryWaitSemaphore(&requestLimit) >= 0) {
          ProcessRequest();
          OSSignalSemaphore(&requestLimit);
      } else {
          RequestDenied();  // Too many concurrent requests
      }
  }
  ```
  
  SEMAPHORE vs MUTEX:
  ===================
  
  **Mutex (Mutual Exclusion):**
  - Ownership: Thread that locks must unlock
  - Recursive: Same thread can lock multiple times
  - Priority inheritance: Prevents priority inversion
  - Use for: Protecting shared data
  
  **Semaphore (Signaling):**
  - No ownership: Any thread can signal
  - Not recursive: Count doesn't track per-thread
  - No priority inheritance
  - Use for: Resource counting, event signaling
  
  IMPLEMENTATION ON PC:
  =====================
  
  Works almost identically to original hardware:
  - Counter management same
  - Thread queuing same
  - Wake-up semantics same
  
  Difference:
  - OSDisableInterrupts doesn't truly disable interrupts on PC
  - Still thread-safe via OS scheduler guarantees
  - For production, consider platform semaphores (CreateSemaphore, sem_init)
  
  THREAD SAFETY:
  ==============
  
  All operations are atomic via OSDisableInterrupts/OSRestoreInterrupts:
  - Multiple threads can wait/signal safely
  - Counter updates are atomic
  - Queue manipulation is protected
  - No race conditions
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>

/*---------------------------------------------------------------------------*
  Name:         OSInitSemaphore

  Description:  Initializes a semaphore with an initial count.
                
                The count represents available resources or events.
                - count > 0: That many threads can wait without blocking
                - count = 0: First wait will block
                - count = 1: Binary semaphore (like simplified mutex)

  Arguments:    sem   - Pointer to semaphore structure
                count - Initial count (typically 0 or resource count)

  Returns:      None
  
  Example:
    // Resource pool with 5 slots
    OSSemaphore pool;
    OSInitSemaphore(&pool, 5);
    
    // Event counter (start empty)
    OSSemaphore events;
    OSInitSemaphore(&events, 0);
 *---------------------------------------------------------------------------*/
void OSInitSemaphore(OSSemaphore* sem, s32 count) {
    BOOL enabled;
    
    if (!sem) return;
    
    /* Critical section for initialization */
    enabled = OSDisableInterrupts();
    
    /* Initialize wait queue */
    OSInitThreadQueue(&sem->queue);
    
    /* Set initial count */
    sem->count = count;
    
    OSRestoreInterrupts(enabled);
}

/*---------------------------------------------------------------------------*
  Name:         OSWaitSemaphore

  Description:  Waits on a semaphore (P operation / down / acquire).
                
                Behavior:
                - If count > 0: Decrement count and return immediately
                - If count = 0: Block thread until someone signals
                
                This is a blocking call - thread sleeps until signaled.

  Arguments:    sem - Pointer to semaphore

  Returns:      Count BEFORE decrement (original count)
                Returns -1 if sem is NULL
  
  Thread Safety: Safe to call from multiple threads
  
  Example:
    // Wait for available resource
    s32 prevCount = OSWaitSemaphore(&resourceSem);
    UseResource();
    OSSignalSemaphore(&resourceSem);
 *---------------------------------------------------------------------------*/
s32 OSWaitSemaphore(OSSemaphore* sem) {
    BOOL enabled;
    s32 count;
    
    if (!sem) return -1;
    
    /* Critical section */
    enabled = OSDisableInterrupts();
    
    /* Wait until count > 0 */
    while ((count = ((volatile OSSemaphore*)sem)->count) <= 0) {
        /* Block thread on wait queue */
        /* Note: OSSleepThread will restore interrupts internally */
        OSSleepThread(&sem->queue);
        
        /* When we wake up, re-enter critical section */
        enabled = OSDisableInterrupts();
    }
    
    /* Decrement count (consume one resource) */
    --sem->count;
    
    OSRestoreInterrupts(enabled);
    
    /* Return count BEFORE decrement */
    return count;
}

/*---------------------------------------------------------------------------*
  Name:         OSTryWaitSemaphore

  Description:  Non-blocking wait on semaphore (try P operation).
                
                Behavior:
                - If count > 0: Decrement count and return success
                - If count = 0: Return immediately without blocking
                
                Use when you don't want to block if resource unavailable.

  Arguments:    sem - Pointer to semaphore

  Returns:      Count BEFORE decrement if successful (count > 0)
                0 if semaphore was already 0 (unsuccessful)
                -1 if sem is NULL
  
  Example:
    // Try to acquire resource without blocking
    if (OSTryWaitSemaphore(&resourceSem) > 0) {
        UseResource();
        OSSignalSemaphore(&resourceSem);
    } else {
        // Resource unavailable, do something else
        DoAlternativeWork();
    }
 *---------------------------------------------------------------------------*/
s32 OSTryWaitSemaphore(OSSemaphore* sem) {
    BOOL enabled;
    s32 count;
    
    if (!sem) return -1;
    
    /* Critical section */
    enabled = OSDisableInterrupts();
    
    count = sem->count;
    
    /* Only decrement if count > 0 */
    if (count > 0) {
        --sem->count;
    }
    
    OSRestoreInterrupts(enabled);
    
    /* Return count BEFORE decrement (or 0 if already 0) */
    return count;
}

/*---------------------------------------------------------------------------*
  Name:         OSSignalSemaphore

  Description:  Signals a semaphore (V operation / up / release).
                
                Behavior:
                - Increment count
                - Wake up one waiting thread (if any)
                
                No ownership - any thread can signal any semaphore.
                Use for: Releasing resources, notifying events.

  Arguments:    sem - Pointer to semaphore

  Returns:      Count BEFORE increment (original count)
                Returns -1 if sem is NULL
  
  Thread Safety: Safe to call from any thread (no ownership required)
  
  Example:
    // Producer signals new item available
    AddItemToQueue(item);
    OSSignalSemaphore(&itemsAvailable);
    
    // Consumer waits for item
    OSWaitSemaphore(&itemsAvailable);
    item = RemoveItemFromQueue();
 *---------------------------------------------------------------------------*/
s32 OSSignalSemaphore(OSSemaphore* sem) {
    BOOL enabled;
    s32 count;
    
    if (!sem) return -1;
    
    /* Critical section */
    enabled = OSDisableInterrupts();
    
    /* Save count before increment */
    count = sem->count;
    
    /* Increment count (release one resource) */
    ++sem->count;
    
    /* Wake up one waiting thread (if any) */
    /* If no threads waiting, this is a no-op */
    OSWakeupThread(&sem->queue);
    
    OSRestoreInterrupts(enabled);
    
    /* Return count BEFORE increment */
    return count;
}

/*---------------------------------------------------------------------------*
  Name:         OSGetSemaphoreCount

  Description:  Returns the current semaphore count.
                
                Useful for debugging and monitoring:
                - Positive count: Available resources
                - Zero count: No resources, threads may be waiting
                
                Note: Value may change immediately after reading
                in a multithreaded environment.

  Arguments:    sem - Pointer to semaphore

  Returns:      Current count, or -1 if sem is NULL
  
  Example:
    if (OSGetSemaphoreCount(&pool) > 0) {
        OSReport("Resources available: %d\n", 
                 OSGetSemaphoreCount(&pool));
    }
 *---------------------------------------------------------------------------*/
s32 OSGetSemaphoreCount(OSSemaphore* sem) {
    if (!sem) return -1;
    
    /* Read count (atomic on most platforms) */
    return sem->count;
}

/*===========================================================================*
  ADDITIONAL UTILITIES (Not in original SDK, but useful)
 *===========================================================================*/

#ifdef _DEBUG

/*---------------------------------------------------------------------------*
  Name:         OSIsSemaphoreLocked (Debug utility)

  Description:  Checks if semaphore count is zero.
                Not in original SDK.

  Arguments:    sem - Pointer to semaphore

  Returns:      TRUE if count is 0, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSIsSemaphoreLocked(OSSemaphore* sem) {
    return (sem && sem->count == 0) ? TRUE : FALSE;
}

#endif /* _DEBUG */

/*===========================================================================*
  ADVANCED PATTERNS
  
  These are examples of advanced semaphore usage patterns.
  Not implemented as functions, but documented for reference.
  
  **Pattern 1: Multiple Resource Types**
  ```c
  OSSemaphore cpuSlots;
  OSSemaphore memorySlots;
  
  OSInitSemaphore(&cpuSlots, 4);      // 4 CPU slots
  OSInitSemaphore(&memorySlots, 8);   // 8 memory slots
  
  void ProcessTask() {
      OSWaitSemaphore(&cpuSlots);
      OSWaitSemaphore(&memorySlots);
      
      // Use CPU and memory
      DoWork();
      
      OSSignalSemaphore(&memorySlots);
      OSSignalSemaphore(&cpuSlots);
  }
  ```
  
  **Pattern 2: Reader-Writer with Semaphores**
  ```c
  // Not recommended (use OSMutex instead), but possible:
  OSSemaphore writeLock;
  OSInitSemaphore(&writeLock, 1);  // Binary semaphore
  
  void WriteData() {
      OSWaitSemaphore(&writeLock);  // Exclusive access
      ModifyData();
      OSSignalSemaphore(&writeLock);
  }
  ```
  
  **Pattern 3: Throttling with Timeout**
  ```c
  OSSemaphore throttle;
  OSInitSemaphore(&throttle, MAX_CONCURRENT);
  
  BOOL TryProcess() {
      if (OSTryWaitSemaphore(&throttle) > 0) {
          ProcessRequest();
          OSSignalSemaphore(&throttle);
          return TRUE;
      }
      return FALSE;  // Too busy
  }
  ```
 *===========================================================================*/
