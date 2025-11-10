/*---------------------------------------------------------------------------*
  OSMutex.c - Mutex and Condition Variable Implementation
  
  Moved from OSThread.c to match original SDK structure.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>

/*===========================================================================*
  MUTEX IMPLEMENTATION
 *===========================================================================*/

void OSInitMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    mutex->queue.head = NULL;
    mutex->queue.tail = NULL;
    mutex->thread = NULL;
    mutex->count = 0;
}

void OSLockMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    OSThread* current = OSGetCurrentThread();
    
    /* Recursive locking - same thread can lock multiple times */
    if (mutex->thread == current) {
        mutex->count++;
        return;
    }
    
    /* Lock is held by another thread - wait */
    while (mutex->thread != NULL) {
        OSSleepThread(&mutex->queue);
    }
    
    /* Acquire lock */
    mutex->thread = current;
    mutex->count = 1;
}

void OSUnlockMutex(OSMutex* mutex) {
    if (!mutex) return;
    
    OSThread* current = OSGetCurrentThread();
    
    /* Only owner can unlock */
    if (mutex->thread != current) {
        return;
    }
    
    /* Decrement recursion count */
    mutex->count--;
    if (mutex->count > 0) {
        return;  /* Still locked (recursive) */
    }
    
    /* Release lock */
    mutex->thread = NULL;
    
    /* Wake one waiting thread */
    OSWakeupThread(&mutex->queue);
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    if (!mutex) return FALSE;
    
    OSThread* current = OSGetCurrentThread();
    
    /* Recursive locking */
    if (mutex->thread == current) {
        mutex->count++;
        return TRUE;
    }
    
    /* Try to acquire - fail if held */
    if (mutex->thread != NULL) {
        return FALSE;
    }
    
    mutex->thread = current;
    mutex->count = 1;
    return TRUE;
}

/*===========================================================================*
  CONDITION VARIABLE IMPLEMENTATION
 *===========================================================================*/

void OSInitCond(OSCond* cond) {
    if (!cond) return;
    cond->queue.head = NULL;
    cond->queue.tail = NULL;
}

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
    if (!cond || !mutex) return;
    
    /* Atomic unlock-and-wait pattern */
    OSUnlockMutex(mutex);
    OSSleepThread(&cond->queue);
    OSLockMutex(mutex);
}

void OSSignalCond(OSCond* cond) {
    if (!cond) return;

    /* Wake one waiting thread */
    OSWakeupThread(&cond->queue);
}

