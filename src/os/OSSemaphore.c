#include <dolphin/os.h>

void OSInitSemaphore(OSSemaphore* sem, s32 count) {
    if (!sem) return;
    
    sem->count = count;
    sem->queue.head = NULL;
    sem->queue.tail = NULL;
}

s32 OSWaitSemaphore(OSSemaphore* sem) {
    if (!sem) return -1;
    
    while (sem->count <= 0) {
        OSSleepThread(&sem->queue);
    }
    
    sem->count--;
    return sem->count;
}

s32 OSTryWaitSemaphore(OSSemaphore* sem) {
    if (!sem) return -1;
    
    if (sem->count > 0) {
        sem->count--;
        return sem->count;
    }
    
    return -1;
}

s32 OSSignalSemaphore(OSSemaphore* sem) {
    if (!sem) return -1;
    
    sem->count++;
    OSWakeupThread(&sem->queue);
    return sem->count;
}

s32 OSGetSemaphoreCount(OSSemaphore* sem) {
    return sem ? sem->count : -1;
}

