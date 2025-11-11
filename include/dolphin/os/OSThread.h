#ifndef DOLPHIN_OSTHREAD_H
#define DOLPHIN_OSTHREAD_H

#include <dolphin/types.h>
#include <dolphin/os/OSContext.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_THREAD_SPECIFIC_MAX  2

typedef struct OSThread         OSThread;
typedef struct OSThreadQueue    OSThreadQueue;
typedef struct OSThreadLink     OSThreadLink;
typedef s32                     OSPriority;
typedef struct OSMutex          OSMutex;
typedef struct OSMutexQueue     OSMutexQueue;
typedef struct OSMutexLink      OSMutexLink;
typedef struct OSCond           OSCond;

typedef void (*OSIdleFunction)(void* param);
typedef void (*OSSwitchThreadCallback)(OSThread* from, OSThread* to);

struct OSThreadQueue
{
    OSThread* head;
    OSThread* tail;
};

struct OSThreadLink
{
    OSThread* next;
    OSThread* prev;
};

struct OSMutexQueue
{
    OSMutex* head;
    OSMutex* tail;
};

struct OSMutexLink
{
    OSMutex* next;
    OSMutex* prev;
};

struct OSThread
{
    OSContext       context;
    u16             state;
    u16             attr;
    s32             suspend;
    OSPriority      priority;
    OSPriority      base;
    void*           val;
    OSThreadQueue*  queue;
    OSThreadLink    link;
    OSThreadQueue   queueJoin;
    OSMutex*        mutex;
    OSMutexQueue    queueMutex;
    OSThreadLink    linkActive;
    u8*             stackBase;
    u32*            stackEnd;
    s32             error;
    void*           specific[OS_THREAD_SPECIFIC_MAX];
};

// Thread states
enum OS_THREAD_STATE
{
    OS_THREAD_STATE_READY    = 1,
    OS_THREAD_STATE_RUNNING  = 2,
    OS_THREAD_STATE_WAITING  = 4,
    OS_THREAD_STATE_MORIBUND = 8
};

// Thread priorities
#define OS_PRIORITY_MIN         0
#define OS_PRIORITY_MAX         31
#define OS_PRIORITY_IDLE        OS_PRIORITY_MAX

// Thread attributes
#define OS_THREAD_ATTR_DETACH   0x0001u

// Stack magic value
#define OS_THREAD_STACK_MAGIC   0xDEADBABE

void       OSInitThreadQueue    (OSThreadQueue* queue);
OSThread*  OSGetCurrentThread   (void);
BOOL       OSIsThreadSuspended  (OSThread* thread);
BOOL       OSIsThreadTerminated (OSThread* thread);
s32        OSDisableScheduler   (void);
s32        OSEnableScheduler    (void);
void       OSYieldThread        (void);
BOOL       OSCreateThread       (OSThread* thread, void* (*func)(void*), void* param,
                                void* stack, u32 stackSize, OSPriority priority, u16 attr);
void       OSExitThread         (void* val);
void       OSCancelThread       (OSThread* thread);
BOOL       OSJoinThread         (OSThread* thread, void** val);
void       OSDetachThread       (OSThread* thread);
s32        OSResumeThread       (OSThread* thread);
s32        OSSuspendThread      (OSThread* thread);
BOOL       OSSetThreadPriority  (OSThread* thread, OSPriority priority);
OSPriority OSGetThreadPriority  (OSThread* thread);
void       OSSleepThread        (OSThreadQueue* queue);
void       OSWakeupThread       (OSThreadQueue* queue);
void*      OSGetThreadSpecific  (s32 index);
void       OSSetThreadSpecific  (s32 index, void* ptr);
OSThread*  OSSetIdleFunction    (OSIdleFunction idleFunction, void* param, 
                                void* stack, u32 stackSize);
OSThread*  OSGetIdleFunction    (void);
void       OSClearStack         (u8 val);
long       OSCheckActiveThreads (void);
OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback);
void       OSSleepTicks         (OSTime ticks);

// Internal priority management functions
s32        __OSGetEffectivePriority(OSThread* thread);  // Get effective priority
void       __OSPromoteThread       (OSThread* thread, s32 priority);  // Boost priority
void       __OSReschedule          (void);  // Force thread reschedule

#define OSSleepSeconds(sec)         OSSleepTicks(OSSecondsToTicks((OSTime)sec))
#define OSSleepMilliseconds(msec)   OSSleepTicks(OSMillisecondsToTicks((OSTime)msec))
#define OSSleepMicroseconds(usec)   OSSleepTicks(OSMicrosecondsToTicks((OSTime)usec))
#define OSSleepNanoseconds(nsec)    OSSleepTicks(OSNanosecondsToTicks((OSTime)nsec))

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSTHREAD_H */

