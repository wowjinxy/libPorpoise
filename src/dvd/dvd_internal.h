/**
 * @file dvd_internal.h
 * @brief Internal DVD module definitions
 * 
 * Shared between DVD subsystem implementation files.
 * Not part of public API.
 */

#ifndef DVD_INTERNAL_H
#define DVD_INTERNAL_H

#include <dolphin/dvd.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/*---------------------------------------------------------------------------*
    Internal Command Block Structure
 *---------------------------------------------------------------------------*/

// Full command block definition (used by queue system)
struct DVDCommandBlock {
    DVDFileInfo*  fileInfo;         // Associated file info
    void*         addr;             // Destination buffer
    s32           length;           // Bytes to read
    s32           offset;           // File offset
    DVDCallback   callback;         // User callback
    s32           state;            // Current state
    s32           result;           // Result code
    FILE*         file;             // OS file handle
    
    // Queue links
    struct DVDCommandBlock* next;   // Next in queue
    struct DVDCommandBlock* prev;   // Previous in queue
    
    // For async operations
#ifdef _WIN32
    HANDLE        thread;
#else
    pthread_t     thread;
#endif
};

#endif // DVD_INTERNAL_H

