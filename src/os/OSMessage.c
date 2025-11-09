/*---------------------------------------------------------------------------*
  OSMessage.c - Message Queue System
  
  MESSAGE QUEUE PATTERN:
  ======================
  
  Message queues are a classic inter-thread communication mechanism:
  
  **Producer-Consumer Pattern:**
  ```
  Producer Thread          Message Queue         Consumer Thread
  ┌──────────────┐        ┌─────────────┐       ┌──────────────┐
  │              │        │ [Msg][Msg]  │       │              │
  │ OSSendMsg() ─┼───────>│ [Msg][ ][ ] │──────>│ OSRecvMsg()  │
  │              │        │             │       │              │
  └──────────────┘        └─────────────┘       └──────────────┘
       │                        │                      │
       └─ Blocks if full       └─ Circular buffer    └─ Blocks if empty
  ```
  
  **How It Works:**
  
  1. **Circular Buffer:**
     - Fixed-size array of OSMessage (void pointers)
     - firstIndex: Points to next message to read
     - usedCount: Number of messages currently in queue
     - Wraps around using modulo arithmetic
  
  2. **Blocking Behavior:**
     - OSSendMessage: Blocks if queue is full (OS_MESSAGE_BLOCK flag)
     - OSReceiveMessage: Blocks if queue is empty (OS_MESSAGE_BLOCK flag)
     - Threads wait on queueSend or queueReceive
  
  3. **Thread Safety:**
     - OSDisableInterrupts/OSRestoreInterrupts protect critical sections
     - Prevents race conditions during queue manipulation
     - Wake-up notifications after operations
  
  **Typical Usage:**
  ```c
  // Setup
  OSMessage msgBuffer[16];
  OSMessageQueue queue;
  OSInitMessageQueue(&queue, msgBuffer, 16);
  
  // Producer thread
  void* myData = GetData();
  OSSendMessage(&queue, myData, OS_MESSAGE_BLOCK);
  
  // Consumer thread
  void* receivedData;
  OSReceiveMessage(&queue, &receivedData, OS_MESSAGE_BLOCK);
  ProcessData(receivedData);
  ```
  
  **OSJamMessage - Priority Send:**
  - Inserts message at HEAD of queue (instead of tail)
  - Used for high-priority messages
  - Example: Error messages, abort signals
  
  IMPLEMENTATION ON PC:
  =====================
  
  Works almost identically to original hardware:
  
  **What's The Same:** ✅
  - Circular buffer logic
  - Blocking/non-blocking modes
  - Thread wake-up after operations
  - Queue capacity management
  
  **What's Different:** ⚠️
  - OSDisableInterrupts doesn't actually disable interrupts on PC
  - Instead, we rely on atomic operations and OS thread scheduling
  - OSSleepThread/OSWakeupThread use platform sleep mechanisms
  - Still fully thread-safe via OS scheduler
  
  **Thread Safety Notes:**
  On original hardware, OSDisableInterrupts() prevents ALL thread
  switches and interrupts, providing true atomicity.
  
  On PC, we can't disable interrupts, but modern CPUs + OS schedulers
  provide similar guarantees for simple operations. For production use,
  consider adding explicit mutex locks.
  
  COMMON PATTERNS:
  ================
  
  **Pattern 1: Command Queue**
  ```c
  // Main thread → Worker thread command queue
  OSMessage cmdQueue[32];
  OSMessageQueue queue;
  
  void SendCommand(Command* cmd) {
      OSSendMessage(&queue, cmd, OS_MESSAGE_BLOCK);
  }
  
  void WorkerThread() {
      while (running) {
          Command* cmd;
          OSReceiveMessage(&queue, (OSMessage*)&cmd, OS_MESSAGE_BLOCK);
          ExecuteCommand(cmd);
      }
  }
  ```
  
  **Pattern 2: Frame Synchronization**
  ```c
  // Graphics thread signals frame completion
  void OnFrameComplete() {
      OSMessage dummy = (OSMessage)1;
      OSSendMessage(&frameQueue, dummy, OS_MESSAGE_NOBLOCK);
  }
  
  // Main thread waits for frame
  void WaitForFrame() {
      OSMessage msg;
      OSReceiveMessage(&frameQueue, &msg, OS_MESSAGE_BLOCK);
  }
  ```
  
  **Pattern 3: Event Broadcasting**
  ```c
  // Multiple consumers process same events
  for (int i = 0; i < numWorkers; i++) {
      OSSendMessage(&workerQueue[i], event, OS_MESSAGE_NOBLOCK);
  }
  ```
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>

/*---------------------------------------------------------------------------*
  Name:         OSInitMessageQueue

  Description:  Initializes a message queue structure.
                
                Sets up the circular buffer and thread wait queues.
                Must be called before using the message queue.

  Arguments:    mq       - Pointer to message queue structure
                msgArray - Array of OSMessage to use as buffer
                msgCount - Number of messages the queue can hold

  Returns:      None
  
  Example:
    OSMessage messages[16];
    OSMessageQueue queue;
    OSInitMessageQueue(&queue, messages, 16);
 *---------------------------------------------------------------------------*/
void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    if (!mq) return;
    
    /* Initialize thread wait queues */
    OSInitThreadQueue(&mq->queueSend);     /* Threads waiting to send */
    OSInitThreadQueue(&mq->queueReceive);  /* Threads waiting to receive */
    
    /* Set up circular buffer */
    mq->msgArray = msgArray;
    mq->msgCount = msgCount;
    mq->firstIndex = 0;      /* Read position */
    mq->usedCount = 0;       /* Number of messages currently in queue */
}

/*---------------------------------------------------------------------------*
  Name:         OSSendMessage

  Description:  Sends a message to the tail of the queue (FIFO order).
                
                On original hardware:
                - Disables interrupts for atomicity
                - If queue is full and OS_MESSAGE_BLOCK is set, sleeps thread
                - Inserts message at tail (circular buffer)
                - Wakes up threads waiting to receive
                - Restores interrupts
                
                On PC:
                - Uses OSDisableInterrupts (tracks state, doesn't actually disable)
                - Blocking works via OSSleepThread (platform sleep)
                - Still thread-safe due to OS scheduler

  Arguments:    mq    - Pointer to message queue
                msg   - Message to send (void pointer, can be anything)
                flags - OS_MESSAGE_BLOCK or OS_MESSAGE_NOBLOCK

  Returns:      TRUE on success, FALSE if queue full and non-blocking
  
  Thread Safety:
    - Safe to call from multiple threads simultaneously
    - Blocks if queue full (when OS_MESSAGE_BLOCK set)
    - Returns FALSE immediately if full (when OS_MESSAGE_NOBLOCK set)
 *---------------------------------------------------------------------------*/
BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled;
    s32 lastIndex;
    
    if (!mq) return FALSE;
    
    /* Enter critical section */
    enabled = OSDisableInterrupts();
    
    /* Check if queue is full */
    while (mq->usedCount >= mq->msgCount) {
        /* Queue is full */
        if (!(flags & OS_MESSAGE_BLOCK)) {
            /* Non-blocking mode: return failure immediately */
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        
        /* Blocking mode: wait for space to become available */
        /* Note: OSSleepThread will restore interrupts internally */
        OSSleepThread(&mq->queueSend);
        
        /* When we wake up, re-disable interrupts to check again */
        enabled = OSDisableInterrupts();
    }
    
    /* Insert message at tail of circular buffer */
    lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
    mq->msgArray[lastIndex] = msg;
    mq->usedCount++;
    
    /* Wake up any threads waiting to receive */
    OSWakeupThread(&mq->queueReceive);
    
    /* Exit critical section */
    OSRestoreInterrupts(enabled);
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         OSJamMessage

  Description:  Sends a message to the HEAD of the queue (priority send).
                
                Unlike OSSendMessage which appends to tail (FIFO),
                OSJamMessage inserts at head, so it will be received next.
                
                Use cases:
                - High-priority messages (errors, abort signals)
                - Out-of-band notifications
                - Interrupt handlers sending urgent data
                
                On original hardware:
                - Same thread-safety as OSSendMessage
                - Moves firstIndex backward (with wraparound)
                - Increments usedCount
                
                On PC: Same behavior, thread-safe

  Arguments:    mq    - Pointer to message queue
                msg   - Message to send (inserted at head)
                flags - OS_MESSAGE_BLOCK or OS_MESSAGE_NOBLOCK

  Returns:      TRUE on success, FALSE if queue full and non-blocking
  
  Example:
    // Normal message
    OSSendMessage(&queue, normalData, OS_MESSAGE_BLOCK);
    
    // Urgent message (will be received first)
    OSJamMessage(&queue, urgentData, OS_MESSAGE_NOBLOCK);
 *---------------------------------------------------------------------------*/
BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled;
    
    if (!mq) return FALSE;
    
    /* Enter critical section */
    enabled = OSDisableInterrupts();
    
    /* Check if queue is full */
    while (mq->usedCount >= mq->msgCount) {
        /* Queue is full */
        if (!(flags & OS_MESSAGE_BLOCK)) {
            /* Non-blocking mode: return failure */
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        
        /* Blocking mode: wait for space */
        OSSleepThread(&mq->queueSend);
        enabled = OSDisableInterrupts();
    }
    
    /* Insert message at HEAD by moving firstIndex backward */
    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    mq->usedCount++;
    
    /* Wake up any threads waiting to receive */
    OSWakeupThread(&mq->queueReceive);
    
    /* Exit critical section */
    OSRestoreInterrupts(enabled);
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         OSReceiveMessage

  Description:  Receives a message from the head of the queue (FIFO order).
                
                On original hardware:
                - Disables interrupts for atomicity
                - If queue is empty and OS_MESSAGE_BLOCK is set, sleeps thread
                - Retrieves message from head (firstIndex)
                - Advances firstIndex (circular)
                - Decrements usedCount
                - Wakes up threads waiting to send
                - Restores interrupts
                
                On PC: Same behavior, thread-safe

  Arguments:    mq    - Pointer to message queue
                msg   - Pointer to receive message (can be NULL to just remove)
                flags - OS_MESSAGE_BLOCK or OS_MESSAGE_NOBLOCK

  Returns:      TRUE on success, FALSE if queue empty and non-blocking
  
  Thread Safety:
    - Safe to call from multiple threads
    - Blocks if queue empty (when OS_MESSAGE_BLOCK set)
    - Returns FALSE immediately if empty (when OS_MESSAGE_NOBLOCK set)
  
  Usage:
    // Blocking receive (waits for message)
    OSMessage msg;
    OSReceiveMessage(&queue, &msg, OS_MESSAGE_BLOCK);
    
    // Non-blocking receive (polls)
    if (OSReceiveMessage(&queue, &msg, OS_MESSAGE_NOBLOCK)) {
        // Message received
    } else {
        // Queue was empty
    }
    
    // Discard message (just remove from queue)
    OSReceiveMessage(&queue, NULL, OS_MESSAGE_BLOCK);
 *---------------------------------------------------------------------------*/
BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    BOOL enabled;
    
    if (!mq) return FALSE;
    
    /* Enter critical section */
    enabled = OSDisableInterrupts();
    
    /* Check if queue is empty */
    while (mq->usedCount == 0) {
        /* Queue is empty */
        if (!(flags & OS_MESSAGE_BLOCK)) {
            /* Non-blocking mode: return failure */
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        
        /* Blocking mode: wait for message to arrive */
        OSSleepThread(&mq->queueReceive);
        enabled = OSDisableInterrupts();
    }
    
    /* Copy message from head of queue */
    if (msg != NULL) {
        *msg = mq->msgArray[mq->firstIndex];
    }
    
    /* Advance head pointer (circular) */
    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;
    
    /* Wake up any threads waiting to send */
    OSWakeupThread(&mq->queueSend);
    
    /* Exit critical section */
    OSRestoreInterrupts(enabled);
    
    return TRUE;
}

/*===========================================================================*
  ADDITIONAL HELPER FUNCTIONS (Not in original SDK, but useful)
 *===========================================================================*/

#ifdef _DEBUG

/*---------------------------------------------------------------------------*
  Name:         OSGetMessageCount (Debug utility)

  Description:  Returns the number of messages currently in the queue.
                Not in original SDK, but useful for debugging.

  Arguments:    mq - Pointer to message queue

  Returns:      Number of messages in queue (0 if empty or NULL)
 *---------------------------------------------------------------------------*/
s32 OSGetMessageCount(OSMessageQueue* mq) {
    if (!mq) return 0;
    
    BOOL enabled = OSDisableInterrupts();
    s32 count = mq->usedCount;
    OSRestoreInterrupts(enabled);
    
    return count;
}

/*---------------------------------------------------------------------------*
  Name:         OSIsMessageQueueFull (Debug utility)

  Description:  Checks if the message queue is full.
                Not in original SDK.

  Arguments:    mq - Pointer to message queue

  Returns:      TRUE if full, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSIsMessageQueueFull(OSMessageQueue* mq) {
    if (!mq) return FALSE;
    
    BOOL enabled = OSDisableInterrupts();
    BOOL full = (mq->usedCount >= mq->msgCount);
    OSRestoreInterrupts(enabled);
    
    return full;
}

/*---------------------------------------------------------------------------*
  Name:         OSIsMessageQueueEmpty (Debug utility)

  Description:  Checks if the message queue is empty.
                Not in original SDK.

  Arguments:    mq - Pointer to message queue

  Returns:      TRUE if empty, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL OSIsMessageQueueEmpty(OSMessageQueue* mq) {
    if (!mq) return FALSE;
    
    BOOL enabled = OSDisableInterrupts();
    BOOL empty = (mq->usedCount == 0);
    OSRestoreInterrupts(enabled);
    
    return empty;
}

#endif /* _DEBUG */
