#include <dolphin/os.h>
#include <string.h>

void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    if (!mq) return;
    
    mq->queueSend.head = NULL;
    mq->queueSend.tail = NULL;
    mq->queueReceive.head = NULL;
    mq->queueReceive.tail = NULL;
    mq->msgArray = msgArray;
    mq->msgCount = msgCount;
    mq->firstIndex = 0;
    mq->usedCount = 0;
}

BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    if (!mq) return FALSE;
    
    if (mq->usedCount >= mq->msgCount) {
        if (flags == OS_MESSAGE_NOBLOCK) {
            return FALSE;
        }
        // Block until space available
        while (mq->usedCount >= mq->msgCount) {
            OSSleepThread(&mq->queueSend);
        }
    }
    
    s32 index = (mq->firstIndex + mq->usedCount) % mq->msgCount;
    mq->msgArray[index] = msg;
    mq->usedCount++;
    
    OSWakeupThread(&mq->queueReceive);
    return TRUE;
}

BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    if (!mq) return FALSE;
    
    if (mq->usedCount >= mq->msgCount) {
        if (flags == OS_MESSAGE_NOBLOCK) {
            return FALSE;
        }
    }
    
    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    if (mq->usedCount < mq->msgCount) {
        mq->usedCount++;
    }
    
    OSWakeupThread(&mq->queueReceive);
    return TRUE;
}

BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    if (!mq) return FALSE;
    
    if (mq->usedCount == 0) {
        if (flags == OS_MESSAGE_NOBLOCK) {
            return FALSE;
        }
        while (mq->usedCount == 0) {
            OSSleepThread(&mq->queueReceive);
        }
    }
    
    if (msg) {
        *msg = mq->msgArray[mq->firstIndex];
    }
    
    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;
    
    OSWakeupThread(&mq->queueSend);
    return TRUE;
}

