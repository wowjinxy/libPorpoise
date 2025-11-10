// Temporary file with missing DVD functions
#include <dolphin/dvd.h>

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length,
                      s32 offset, DVDCallback callback, s32 priority) {
    (void)priority;
    return DVDReadAsync(fileInfo, addr, length, offset, callback);
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length,
                s32 offset, s32 priority) {
    (void)priority;
    return DVDRead(fileInfo, addr, length, offset);
}

BOOL DVDCancelStream(DVDCommandBlock* block) {
    return DVDCancel(block);
}

