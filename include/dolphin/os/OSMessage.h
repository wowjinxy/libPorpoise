#ifndef DOLPHIN_OSMESSAGE_H
#define DOLPHIN_OSMESSAGE_H

#include <dolphin/types.h>
#include <dolphin/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSMessageQueue   OSMessageQueue;
typedef void*                   OSMessage;

struct OSMessageQueue
{
    OSThreadQueue   queueSend;
    OSThreadQueue   queueReceive;
    OSMessage*      msgArray;
    s32             msgCount;
    s32             firstIndex;
    s32             usedCount;
};

// Flags to turn blocking on/off when sending/receiving message
#define OS_MESSAGE_NOBLOCK  0
#define OS_MESSAGE_BLOCK    1

void OSInitMessageQueue (OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount);
BOOL OSSendMessage      (OSMessageQueue* mq, OSMessage msg, s32 flags);
BOOL OSJamMessage       (OSMessageQueue* mq, OSMessage msg, s32 flags);
BOOL OSReceiveMessage   (OSMessageQueue* mq, OSMessage* msg, s32 flags);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSMESSAGE_H */

