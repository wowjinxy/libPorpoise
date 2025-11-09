#ifndef DOLPHIN_OSMUTEX_H
#define DOLPHIN_OSMUTEX_H

#include <dolphin/types.h>
#include <dolphin/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OSMutex
{
    OSThreadQueue   queue;
    OSThread*       thread;
    s32             count;
    OSMutexLink     link;
};

struct OSCond
{
    OSThreadQueue   queue;
};

void OSInitMutex   (OSMutex* mutex);
void OSLockMutex   (OSMutex* mutex);
void OSUnlockMutex (OSMutex* mutex);
BOOL OSTryLockMutex(OSMutex* mutex);
void OSInitCond    (OSCond* cond);
void OSWaitCond    (OSCond* cond, OSMutex* mutex);
void OSSignalCond  (OSCond* cond);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSMUTEX_H */

