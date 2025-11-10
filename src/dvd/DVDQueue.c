/*---------------------------------------------------------------------------*
  DVDQueue.c - DVD Command Queue Management
  
  Manages priority-based command queues for DVD operations.
  
  On GC/Wii: Hardware has limited command slots, needs queuing
  On PC: File I/O is instant, but we maintain queues for API compatibility
 *---------------------------------------------------------------------------*/

#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include "dvd_internal.h"
#include <string.h>

/*---------------------------------------------------------------------------*
    Queue System
 *---------------------------------------------------------------------------*/

#define NUM_QUEUES 4  // One for each priority level

typedef struct {
    DVDCommandBlock* next;
    DVDCommandBlock* prev;
} Queue;

static Queue s_waitingQueue[NUM_QUEUES];

/*---------------------------------------------------------------------------*
  Name:         __DVDClearWaitingQueue

  Description:  Initialize/clear all waiting queues.

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __DVDClearWaitingQueue(void) {
    for (u32 i = 0; i < NUM_QUEUES; i++) {
        DVDCommandBlock* q = (DVDCommandBlock*)&s_waitingQueue[i];
        q->next = q;
        q->prev = q;
    }
}

/*---------------------------------------------------------------------------*
  Name:         __DVDPushWaitingQueue

  Description:  Add command block to waiting queue for specified priority.

  Arguments:    prio   Priority level (0-3)
                block  Command block to queue

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block) {
    BOOL enabled = OSDisableInterrupts();
    
    DVDCommandBlock* q = (DVDCommandBlock*)&s_waitingQueue[prio];
    
    // Insert at end of queue
    q->prev->next = block;
    block->prev = q->prev;
    block->next = q;
    q->prev = block;
    
    OSRestoreInterrupts(enabled);
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         PopWaitingQueuePrio

  Description:  Pop command block from specified priority queue.

  Arguments:    prio  Priority level (0-3)

  Returns:      Command block, or NULL if queue empty
 *---------------------------------------------------------------------------*/
static DVDCommandBlock* PopWaitingQueuePrio(s32 prio) {
    DVDCommandBlock* tmp;
    BOOL enabled;
    DVDCommandBlock* q;
    
    enabled = OSDisableInterrupts();
    
    q = (DVDCommandBlock*)&s_waitingQueue[prio];
    
    // Check if queue empty
    if (q->next == q) {
        OSRestoreInterrupts(enabled);
        return NULL;
    }
    
    // Pop from front
    tmp = q->next;
    q->next = tmp->next;
    tmp->next->prev = q;
    
    OSRestoreInterrupts(enabled);
    return tmp;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDPopWaitingQueue

  Description:  Pop highest priority command block from all queues.

  Arguments:    None

  Returns:      Command block with highest priority, or NULL if all empty
 *---------------------------------------------------------------------------*/
DVDCommandBlock* __DVDPopWaitingQueue(void) {
    DVDCommandBlock* block;
    
    // Check queues from highest to lowest priority
    for (s32 prio = 0; prio < NUM_QUEUES; prio++) {
        block = PopWaitingQueuePrio(prio);
        if (block) {
            return block;
        }
    }
    
    return NULL;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDCheckWaitingList

  Description:  Check if any commands are waiting in queues.

  Arguments:    None

  Returns:      TRUE if any queue has commands, FALSE if all empty
 *---------------------------------------------------------------------------*/
BOOL __DVDCheckWaitingList(void) {
    BOOL enabled = OSDisableInterrupts();
    
    for (u32 i = 0; i < NUM_QUEUES; i++) {
        DVDCommandBlock* q = (DVDCommandBlock*)&s_waitingQueue[i];
        if (q->next != q) {
            OSRestoreInterrupts(enabled);
            return TRUE;
        }
    }
    
    OSRestoreInterrupts(enabled);
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDDequeueWaitingQueue

  Description:  Remove a specific command block from waiting queue.

  Arguments:    block  Command block to remove

  Returns:      TRUE if removed, FALSE if not in queue
 *---------------------------------------------------------------------------*/
BOOL __DVDDequeueWaitingQueue(DVDCommandBlock* block) {
    BOOL enabled;
    BOOL inQueue = FALSE;
    
    if (!block) {
        return FALSE;
    }
    
    enabled = OSDisableInterrupts();
    
    // Check if block is actually in a queue
    if (block->next && block->prev) {
        block->prev->next = block->next;
        block->next->prev = block->prev;
        block->next = NULL;
        block->prev = NULL;
        inQueue = TRUE;
    }
    
    OSRestoreInterrupts(enabled);
    return inQueue;
}

/*---------------------------------------------------------------------------*
  Name:         __DVDIsBlockInWaitingQueue

  Description:  Check if command block is in waiting queue.

  Arguments:    block  Command block to check

  Returns:      TRUE if in queue, FALSE otherwise
 *---------------------------------------------------------------------------*/
BOOL __DVDIsBlockInWaitingQueue(DVDCommandBlock* block) {
    if (!block) {
        return FALSE;
    }
    
    // Block is in queue if next/prev are not NULL
    return (block->next != NULL && block->prev != NULL);
}

// Convenience function - pushes to low priority queue
void __DVDPushWaitingQueue(DVDCommandBlock* block) {
    __DVDPushWaitingQueue(2, block);
}

