#ifndef DOLPHIN_OSSEMAPHORE_H
#define DOLPHIN_OSSEMAPHORE_H

#include <dolphin/types.h>
#include <dolphin/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSSemaphore
{
    s32           count;
    OSThreadQueue queue;
} OSSemaphore;

void OSInitSemaphore    (OSSemaphore* sem, s32 count);
s32  OSWaitSemaphore    (OSSemaphore* sem);
s32  OSTryWaitSemaphore (OSSemaphore* sem);
s32  OSSignalSemaphore  (OSSemaphore* sem);
s32  OSGetSemaphoreCount(OSSemaphore* sem);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSSEMAPHORE_H */

