/*---------------------------------------------------------------------------*
  DVD.c - Disc File System
  
  ARCHITECTURAL DIFFERENCES: GC/Wii vs PC
  ========================================
  
  On GC/Wii (DVD Drive Hardware):
  --------------------------------
  - Physical DVD drive with proprietary format
  - Direct sector reads via DI (Drive Interface) hardware
  - FST (File System Table) stored at start of disc
  - Files accessed by entry number or path lookup
  - Asynchronous DMA transfers from disc to RAM
  - Hardware read queue with priority system
  - Typical read latency: 40-100ms (seek time + read time)
  - Files fragmented across disc for optimal streaming
  - Maximum file size: 1.46GB (single-layer DVD)
  
  On PC (Local File System):
  ---------------------------
  - Regular filesystem (NTFS, ext4, APFS, etc.)
  - Standard fopen/fread/fseek operations
  - Virtual FST built from directory scanning
  - Files in normal directory structure
  - Synchronous reads (simulated async with threading)
  - No real queue - OS handles file I/O
  - Instant access (no seek latency)
  - Files stored contiguously
  - No practical size limit
  
  WHY THE DIFFERENCE:
  - Original: Custom disc format optimized for streaming
  - PC: Standard filesystem more convenient for development
  - Can't read GameCube disc format on PC without extraction
  - PC filesystem is faster and more flexible
  
  WHAT WE PRESERVE:
  - Same API surface (DVDOpen, DVDRead, DVDReadAsync, etc.)
  - Path-based file access
  - Directory operations
  - Asynchronous callback system
  - Priority concept (though not enforced)
  - File info structures
  
  WHAT'S DIFFERENT:
  - No FST parsing - we scan directory on init
  - No DMA hardware - we use fread()
  - Async operations use background threads (not DI interrupts)
  - No disc spin-up time - instant access
  - Root path configurable ("files/" by default)
  - Can access files outside disc image for modding
  
  DIRECTORY STRUCTURE:
  - Game files placed in "files/" folder
  - Example: "files/data/stage1.bin"
  - Accessed as: DVDOpen("data/stage1.bin", ...)
  - Preserves original disc directory structure
 *---------------------------------------------------------------------------*/

#include <dolphin/dvd.h>
#include <dolphin/dvd_internal.h>
#include <dolphin/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#else
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#define PATH_SEPARATOR '/'
#endif

/*---------------------------------------------------------------------------*
    Internal Structures
 *---------------------------------------------------------------------------*/

// Internal file entry (virtual FST)
typedef struct DVDEntry {
    char*   path;                   // Full path
    u32     offset;                 // Offset (always 0 for PC files)
    u32     length;                 // File size
    BOOL    isDir;                  // TRUE if directory
} DVDEntry;

// Internal directory state
typedef struct DVDDirState {
    char    path[512];              // Full path to directory
#ifdef _WIN32
    HANDLE  handle;                 // Windows FindFirstFile handle
    WIN32_FIND_DATAA findData;      // Current find data
    BOOL    firstRead;              // TRUE if haven't read first entry yet
#else
    DIR*    handle;                 // POSIX directory handle
    struct dirent* entry;           // Current entry
#endif
} DVDDirState;

// Directory state pool
#define MAX_OPEN_DIRS 8
static DVDDirState s_dirStates[MAX_OPEN_DIRS];
static BOOL s_dirStateUsed[MAX_OPEN_DIRS];

/*---------------------------------------------------------------------------*
    Internal State
 *---------------------------------------------------------------------------*/

static BOOL s_initialized = FALSE;
static char s_rootPath[256] = "files/";        // Default root directory
static char s_currentDir[256] = "/";           // Current directory (relative to root)

// Virtual FST (File System Table)
static DVDEntry* s_fst = NULL;
static u32 s_fstSize = 0;
static u32 s_fstCapacity = 0;

// Disc ID (fake for PC)
static DVDDiskID s_diskID = {
    .gameName = {'P', 'O', 'R', 'P'},
    .company = {'0', '1'},
    .diskNumber = 0,
    .gameVersion = 0,
    .streaming = 0,
    .streamBufSize = 0,
    .rvlMagic = 0x5D1C9EA3,
    .gcMagic = 0xC2339F3D,
};

// Command block pool
#define MAX_COMMAND_BLOCKS 16
static DVDCommandBlock s_commandBlocks[MAX_COMMAND_BLOCKS];
static BOOL s_commandBlockUsed[MAX_COMMAND_BLOCKS];

/*---------------------------------------------------------------------------*
  Name:         AllocCommandBlock

  Description:  Allocate a command block from the pool.

  Arguments:    None

  Returns:      Pointer to command block, or NULL if pool exhausted
 *---------------------------------------------------------------------------*/
static DVDCommandBlock* AllocCommandBlock(void) {
    for (int i = 0; i < MAX_COMMAND_BLOCKS; i++) {
        if (!s_commandBlockUsed[i]) {
            s_commandBlockUsed[i] = TRUE;
            memset(&s_commandBlocks[i], 0, sizeof(DVDCommandBlock));
            s_commandBlocks[i].state = DVD_STATE_END;
            return &s_commandBlocks[i];
        }
    }
    return NULL;
}

/*---------------------------------------------------------------------------*
  Name:         FreeCommandBlock

  Description:  Free a command block back to the pool.

  Arguments:    cb  Command block to free

  Returns:      None
 *---------------------------------------------------------------------------*/
static void FreeCommandBlock(DVDCommandBlock* cb) {
    if (!cb) return;
    
    for (int i = 0; i < MAX_COMMAND_BLOCKS; i++) {
        if (&s_commandBlocks[i] == cb) {
            s_commandBlockUsed[i] = FALSE;
            return;
        }
    }
}

/*---------------------------------------------------------------------------*
  Name:         BuildPath

  Description:  Build absolute filesystem path from DVD path.
                Combines root path + current dir + relative path.

  Arguments:    dvdPath  DVD path (may be absolute or relative)
                outPath  Buffer to receive full path
                maxLen   Buffer size

  Returns:      None
 *---------------------------------------------------------------------------*/
static void BuildPath(const char* dvdPath, char* outPath, size_t maxLen) {
    // Start with root
    strncpy(outPath, s_rootPath, maxLen - 1);
    outPath[maxLen - 1] = '\0';
    
    // Add current directory if path is relative
    if (dvdPath[0] != '/') {
        if (strcmp(s_currentDir, "/") != 0) {
            strncat(outPath, s_currentDir + 1, maxLen - strlen(outPath) - 1);
            strncat(outPath, "/", maxLen - strlen(outPath) - 1);
        }
    } else {
        dvdPath++;  // Skip leading slash
    }
    
    // Add the requested path
    strncat(outPath, dvdPath, maxLen - strlen(outPath) - 1);
    
    // Convert slashes to platform separator
#ifdef _WIN32
    for (char* p = outPath; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif
}

/*---------------------------------------------------------------------------*
  Name:         AsyncReadThread

  Description:  Background thread for async read operations.
                Simulates disc DMA by reading file in background.

  Arguments:    arg  Pointer to DVDCommandBlock

  Returns:      0 (thread return value)
 *---------------------------------------------------------------------------*/
#ifdef _WIN32
static DWORD WINAPI AsyncReadThread(LPVOID arg)
#else
static void* AsyncReadThread(void* arg)
#endif
{
    DVDCommandBlock* cb = (DVDCommandBlock*)arg;
    
    // Perform the read
    if (cb->file) {
        fseek(cb->file, cb->offset, SEEK_SET);
        size_t bytesRead = fread(cb->addr, 1, cb->length, cb->file);
        cb->result = (s32)bytesRead;
        cb->state = DVD_STATE_END;
    } else {
        cb->result = DVD_RESULT_FATAL_ERROR;
        cb->state = DVD_STATE_END;
    }
    
    // Call user callback
    if (cb->callback) {
        ((DVDCallback)cb->callback)(cb->result, cb->fileInfo);
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/*---------------------------------------------------------------------------*
  Name:         DVDInit

  Description:  Initialize the DVD subsystem. Creates the "files/" directory
                if it doesn't exist and prepares the file system.
                
                On GC/Wii: Initializes DI hardware, reads FST from disc
                On PC: Creates/verifies "files/" directory exists

  Arguments:    None

  Returns:      TRUE if initialized successfully
 *---------------------------------------------------------------------------*/
BOOL DVDInit(void) {
    if (s_initialized) {
        return TRUE;
    }
    
    OSReport("DVD: Initializing disc file system...\n");
    OSReport("DVD: Root directory: %s\n", s_rootPath);
    
    // Create files directory if it doesn't exist
    struct stat st;
    if (stat(s_rootPath, &st) != 0) {
        OSReport("DVD: Creating root directory...\n");
        if (mkdir(s_rootPath, 0755) != 0) {
            OSReport("DVD: Warning - could not create root directory\n");
        }
    }
    
    // Initialize command block pool
    memset(s_commandBlocks, 0, sizeof(s_commandBlocks));
    memset(s_commandBlockUsed, 0, sizeof(s_commandBlockUsed));
    
    // Initialize directory state pool
    memset(s_dirStates, 0, sizeof(s_dirStates));
    memset(s_dirStateUsed, 0, sizeof(s_dirStateUsed));
    
    // Initialize current directory
    strcpy(s_currentDir, "/");
    
    s_initialized = TRUE;
    OSReport("DVD: Initialization complete\n");
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDOpen

  Description:  Open a file from the disc file system.
                
                On GC/Wii: Looks up file in FST, gets offset/length
                On PC: Opens file using fopen() with path relative to "files/"

  Arguments:    fileName  Path to file (relative to DVD root)
                fileInfo  Pointer to DVDFileInfo to fill

  Returns:      TRUE if file opened successfully, FALSE if not found
 *---------------------------------------------------------------------------*/
BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo) {
    if (!s_initialized || !fileName || !fileInfo) {
        return FALSE;
    }
    
    // Build full path
    char fullPath[512];
    BuildPath(fileName, fullPath, sizeof(fullPath));
    
    // Try to open file
    FILE* file = fopen(fullPath, "rb");
    if (!file) {
        OSReport("DVD: Failed to open file: %s\n", fullPath);
        return FALSE;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate command block
    DVDCommandBlock* cb = AllocCommandBlock();
    if (!cb) {
        fclose(file);
        OSReport("DVD: No command blocks available\n");
        return FALSE;
    }
    
    // Fill in file info
    cb->file = file;
    cb->state = DVD_STATE_END;
    fileInfo->cb = cb;
    fileInfo->startAddr = 0;
    fileInfo->length = (u32)size;
    fileInfo->callback = NULL;
    
    cb->fileInfo = fileInfo;
    
    OSReport("DVD: Opened %s (%u bytes)\n", fileName, fileInfo->length);
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDClose

  Description:  Close an open file.
                
                On GC/Wii: Cancels pending operations, releases resources
                On PC: Closes FILE* handle, frees command block

  Arguments:    fileInfo  Pointer to open file

  Returns:      TRUE if closed successfully
 *---------------------------------------------------------------------------*/
BOOL DVDClose(DVDFileInfo* fileInfo) {
    if (!fileInfo || !fileInfo->cb) {
        return FALSE;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    
    // Close file handle
    if (cb->file) {
        fclose(cb->file);
        cb->file = NULL;
    }
    
    // Free command block
    FreeCommandBlock(cb);
    fileInfo->cb = NULL;
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadPrio

  Description:  Read from file synchronously with priority.
                
                On GC/Wii: Queues read command with priority, waits for completion
                On PC: Reads immediately using fread() (priority ignored)

  Arguments:    fileInfo  Pointer to open file
                addr      Destination buffer
                length    Number of bytes to read
                offset    Offset in file
                prio      Priority (ignored on PC)

  Returns:      Number of bytes read, or -1 on error
 *---------------------------------------------------------------------------*/
s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    (void)prio;  // Priority not used on PC
    
    if (!fileInfo || !fileInfo->cb || !addr || length < 0) {
        return -1;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    if (!cb->file) {
        return -1;
    }
    
    // Clamp to file size
    if (offset < 0) offset = 0;
    if (offset + length > (s32)fileInfo->length) {
        length = fileInfo->length - offset;
    }
    
    if (length <= 0) {
        return 0;
    }
    
    // Seek and read
    fseek(cb->file, offset, SEEK_SET);
    size_t bytesRead = fread(addr, 1, length, cb->file);
    
    return (s32)bytesRead;
}

/*---------------------------------------------------------------------------*
  Name:         DVDRead

  Description:  Read from file synchronously (convenience wrapper).

  Arguments:    fileInfo  Pointer to open file
                addr      Destination buffer
                length    Number of bytes to read
                offset    Offset in file

  Returns:      Number of bytes read, or -1 on error
 *---------------------------------------------------------------------------*/
s32 DVDRead(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset) {
    return DVDReadPrio(fileInfo, addr, length, offset, DVD_PRIO_MEDIUM);
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadAsyncPrio

  Description:  Read from file asynchronously with priority.
                
                On GC/Wii: Queues read command, returns immediately, callback
                           called from DI interrupt when complete
                On PC: Spawns background thread to simulate async operation

  Arguments:    fileInfo  Pointer to open file
                addr      Destination buffer
                length    Number of bytes to read
                offset    Offset in file
                callback  Callback when complete (or NULL)
                prio      Priority (ignored on PC)

  Returns:      TRUE if read started successfully
 *---------------------------------------------------------------------------*/
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio) {
    (void)prio;
    
    if (!fileInfo || !fileInfo->cb || !addr || length < 0) {
        return FALSE;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    if (!cb->file) {
        return FALSE;
    }
    
    // Set up command block
    cb->addr = addr;
    cb->length = length;
    cb->offset = offset;
    cb->callback = (void*)callback;
    cb->state = DVD_STATE_BUSY;
    
    // Start async read thread
#ifdef _WIN32
    cb->thread = CreateThread(NULL, 0, AsyncReadThread, cb, 0, NULL);
    if (!cb->thread) {
        cb->state = DVD_STATE_END;
        return FALSE;
    }
#else
    if (pthread_create(&cb->thread, NULL, AsyncReadThread, cb) != 0) {
        cb->state = DVD_STATE_END;
        return FALSE;
    }
#endif
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadAsync

  Description:  Read from file asynchronously (convenience wrapper).

  Arguments:    fileInfo  Pointer to open file
                addr      Destination buffer
                length    Number of bytes to read
                offset    Offset in file
                callback  Callback when complete (or NULL)

  Returns:      TRUE if read started successfully
 *---------------------------------------------------------------------------*/
BOOL DVDReadAsync(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                  DVDCallback callback) {
    return DVDReadAsyncPrio(fileInfo, addr, length, offset, callback, DVD_PRIO_MEDIUM);
}

/*---------------------------------------------------------------------------*
  Name:         DVDSeek

  Description:  Seek to position in file (for sequential reads).
                
                On GC/Wii: Records seek position for next read
                On PC: Uses fseek() on FILE handle

  Arguments:    fileInfo  Pointer to open file
                offset    Offset to seek to

  Returns:      Current position in file
 *---------------------------------------------------------------------------*/
s32 DVDSeek(DVDFileInfo* fileInfo, s32 offset) {
    if (!fileInfo || !fileInfo->cb) {
        return -1;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    if (!cb->file) {
        return -1;
    }
    
    if (offset < 0) offset = 0;
    if (offset > (s32)fileInfo->length) offset = fileInfo->length;
    
    fseek(cb->file, offset, SEEK_SET);
    return offset;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetFileInfoStatus

  Description:  Get current state of file operation.

  Arguments:    fileInfo  Pointer to file

  Returns:      DVD_STATE_* constant
 *---------------------------------------------------------------------------*/
s32 DVDGetFileInfoStatus(const DVDFileInfo* fileInfo) {
    if (!fileInfo || !fileInfo->cb) {
        return DVD_STATE_END;
    }
    
    return fileInfo->cb->state;
}

/*---------------------------------------------------------------------------*
  Name:         DVDOpenDir

  Description:  Open a directory for reading.
                
                On GC/Wii: Looks up directory in FST
                On PC: Opens directory using platform APIs

  Arguments:    dirName  Path to directory
                dir      Pointer to DVDDir structure to fill

  Returns:      TRUE if directory opened successfully
 *---------------------------------------------------------------------------*/
BOOL DVDOpenDir(const char* dirName, DVDDir* dir) {
    if (!s_initialized || !dirName || !dir) {
        return FALSE;
    }
    
    // Find free directory state
    int stateIndex = -1;
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (!s_dirStateUsed[i]) {
            stateIndex = i;
            break;
        }
    }
    
    if (stateIndex < 0) {
        OSReport("DVD: No free directory slots\n");
        return FALSE;
    }
    
    // Build full path
    char fullPath[512];
    BuildPath(dirName, fullPath, sizeof(fullPath));
    
    DVDDirState* state = &s_dirStates[stateIndex];
    strncpy(state->path, fullPath, sizeof(state->path) - 1);
    
#ifdef _WIN32
    // Windows: Use FindFirstFile
    char searchPath[520];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", fullPath);
    
    state->handle = FindFirstFileA(searchPath, &state->findData);
    if (state->handle == INVALID_HANDLE_VALUE) {
        OSReport("DVD: Failed to open directory: %s\n", fullPath);
        return FALSE;
    }
    state->firstRead = TRUE;
#else
    // POSIX: Use opendir
    state->handle = opendir(fullPath);
    if (!state->handle) {
        OSReport("DVD: Failed to open directory: %s\n", fullPath);
        return FALSE;
    }
#endif
    
    // Mark state as used and store index in DVDDir
    s_dirStateUsed[stateIndex] = TRUE;
    dir->entryNum = stateIndex;
    dir->location = 0;
    dir->next = 0;
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadDir

  Description:  Read next entry from directory.
                
                On GC/Wii: Reads from FST
                On PC: Uses FindNextFile (Windows) or readdir (POSIX)

  Arguments:    dir      Pointer to open directory
                dirent   Pointer to DVDDirEntry to fill

  Returns:      TRUE if entry read, FALSE if end of directory
 *---------------------------------------------------------------------------*/
BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) {
    if (!dir || !dirent) {
        return FALSE;
    }
    
    int stateIndex = dir->entryNum;
    if (stateIndex < 0 || stateIndex >= MAX_OPEN_DIRS || !s_dirStateUsed[stateIndex]) {
        return FALSE;
    }
    
    DVDDirState* state = &s_dirStates[stateIndex];
    
#ifdef _WIN32
    // Windows implementation
    BOOL found = FALSE;
    
    if (state->firstRead) {
        // First entry already loaded by FindFirstFile
        state->firstRead = FALSE;
        found = TRUE;
    } else {
        // Read next entry
        found = FindNextFileA(state->handle, &state->findData);
    }
    
    if (!found) {
        return FALSE;  // End of directory
    }
    
    // Skip "." and ".." entries
    while (strcmp(state->findData.cFileName, ".") == 0 ||
           strcmp(state->findData.cFileName, "..") == 0) {
        if (!FindNextFileA(state->handle, &state->findData)) {
            return FALSE;
        }
    }
    
    // Fill in entry info
    dirent->isDir = (state->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    dirent->name = state->findData.cFileName;  // Points to state (valid until next read)
    dirent->entryNum = dir->location++;
    
#else
    // POSIX implementation
    state->entry = readdir(state->handle);
    if (!state->entry) {
        return FALSE;  // End of directory
    }
    
    // Skip "." and ".." entries
    while (strcmp(state->entry->d_name, ".") == 0 ||
           strcmp(state->entry->d_name, "..") == 0) {
        state->entry = readdir(state->handle);
        if (!state->entry) {
            return FALSE;
        }
    }
    
    // Check if entry is directory
    char entryPath[1024];
    snprintf(entryPath, sizeof(entryPath), "%s/%s", state->path, state->entry->d_name);
    struct stat st;
    dirent->isDir = (stat(entryPath, &st) == 0 && S_ISDIR(st.st_mode));
    
    // Fill in entry info
    dirent->name = state->entry->d_name;  // Points to state (valid until next read)
    dirent->entryNum = dir->location++;
#endif
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCloseDir

  Description:  Close an open directory.

  Arguments:    dir  Pointer to open directory

  Returns:      TRUE if closed successfully
 *---------------------------------------------------------------------------*/
BOOL DVDCloseDir(DVDDir* dir) {
    if (!dir) {
        return FALSE;
    }
    
    int stateIndex = dir->entryNum;
    if (stateIndex < 0 || stateIndex >= MAX_OPEN_DIRS || !s_dirStateUsed[stateIndex]) {
        return FALSE;
    }
    
    DVDDirState* state = &s_dirStates[stateIndex];
    
#ifdef _WIN32
    if (state->handle != INVALID_HANDLE_VALUE) {
        FindClose(state->handle);
        state->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (state->handle) {
        closedir(state->handle);
        state->handle = NULL;
    }
#endif
    
    s_dirStateUsed[stateIndex] = FALSE;
    memset(dir, 0, sizeof(DVDDir));
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDRewindDir

  Description:  Rewind directory to beginning.

  Arguments:    dir  Pointer to open directory

  Returns:      None
 *---------------------------------------------------------------------------*/
void DVDRewindDir(DVDDir* dir) {
    if (!dir) {
        return;
    }
    
    int stateIndex = dir->entryNum;
    if (stateIndex < 0 || stateIndex >= MAX_OPEN_DIRS || !s_dirStateUsed[stateIndex]) {
        return;
    }
    
    DVDDirState* state = &s_dirStates[stateIndex];
    
#ifdef _WIN32
    // Close and reopen to rewind
    if (state->handle != INVALID_HANDLE_VALUE) {
        FindClose(state->handle);
    }
    
    char searchPath[520];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", state->path);
    state->handle = FindFirstFileA(searchPath, &state->findData);
    state->firstRead = TRUE;
#else
    // POSIX: rewinddir
    if (state->handle) {
        rewinddir(state->handle);
    }
#endif
    
    dir->location = 0;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetCurrentDir

  Description:  Get current directory path.

  Arguments:    path     Buffer to receive path
                maxlen   Buffer size

  Returns:      TRUE if successful
 *---------------------------------------------------------------------------*/
BOOL DVDGetCurrentDir(char* path, u32 maxlen) {
    if (!path || maxlen == 0) {
        return FALSE;
    }
    
    strncpy(path, s_currentDir, maxlen - 1);
    path[maxlen - 1] = '\0';
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDChangeDir

  Description:  Change current directory.

  Arguments:    dirName  Path to new directory

  Returns:      TRUE if successful
 *---------------------------------------------------------------------------*/
BOOL DVDChangeDir(const char* dirName) {
    if (!s_initialized || !dirName) {
        return FALSE;
    }
    
    // Build full path to verify it exists
    char fullPath[512];
    BuildPath(dirName, fullPath, sizeof(fullPath));
    
    struct stat st;
    if (stat(fullPath, &st) != 0 || !(st.st_mode & S_IFDIR)) {
        return FALSE;
    }
    
    // Update current directory
    if (dirName[0] == '/') {
        strncpy(s_currentDir, dirName, sizeof(s_currentDir) - 1);
    } else {
        // Append to current dir
        if (strcmp(s_currentDir, "/") != 0) {
            strncat(s_currentDir, "/", sizeof(s_currentDir) - strlen(s_currentDir) - 1);
        }
        strncat(s_currentDir, dirName, sizeof(s_currentDir) - strlen(s_currentDir) - 1);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetDiskID

  Description:  Get disc ID information.
                
                On GC/Wii: Returns disc ID from disc header
                On PC: Returns fake disc ID

  Arguments:    None

  Returns:      Pointer to DVDDiskID structure
 *---------------------------------------------------------------------------*/
DVDDiskID* DVDGetDiskID(void) {
    return &s_diskID;
}

/*---------------------------------------------------------------------------*
  Name:         DVDSetRootDirectory

  Description:  PC-specific extension: Set the root directory for DVD access.
                Default is "files/"
                
                Useful for:
                - Loading assets from different locations
                - Supporting multiple game directories
                - Development with assets in separate folders

  Arguments:    path  New root directory path

  Returns:      TRUE if path is valid
 *---------------------------------------------------------------------------*/
BOOL DVDSetRootDirectory(const char* path) {
    if (!path) {
        return FALSE;
    }
    
    // Verify directory exists
    struct stat st;
    if (stat(path, &st) != 0) {
        OSReport("DVD: Root directory does not exist: %s\n", path);
        return FALSE;
    }
    
    // Update root path
    strncpy(s_rootPath, path, sizeof(s_rootPath) - 1);
    s_rootPath[sizeof(s_rootPath) - 1] = '\0';
    
    // Ensure trailing slash
    size_t len = strlen(s_rootPath);
    if (len > 0 && s_rootPath[len - 1] != '/' && s_rootPath[len - 1] != '\\') {
        strncat(s_rootPath, "/", sizeof(s_rootPath) - len - 1);
    }
    
    OSReport("DVD: Root directory changed to: %s\n", s_rootPath);
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetRootDirectory

  Description:  PC-specific extension: Get current DVD root directory.

  Arguments:    None

  Returns:      Current root directory path
 *---------------------------------------------------------------------------*/
const char* DVDGetRootDirectory(void) {
    return s_rootPath;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancel

  Description:  Cancel a pending asynchronous operation.
                
                On GC/Wii: Cancels DMA transfer, clears DI queue
                On PC: Waits for thread to complete (can't truly cancel)

  Arguments:    fileInfo  Pointer to file with pending operation

  Returns:      TRUE if canceled successfully
 *---------------------------------------------------------------------------*/
BOOL DVDCancel(DVDFileInfo* fileInfo) {
    if (!fileInfo || !fileInfo->cb) {
        return FALSE;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    
    // If operation is busy, wait for it to complete
    if (cb->state == DVD_STATE_BUSY) {
#ifdef _WIN32
        if (cb->thread) {
            WaitForSingleObject(cb->thread, INFINITE);
            CloseHandle(cb->thread);
            cb->thread = NULL;
        }
#else
        if (cb->thread) {
            pthread_join(cb->thread, NULL);
        }
#endif
        cb->state = DVD_STATE_CANCELED;
        cb->result = DVD_RESULT_CANCELED;
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancelAsync

  Description:  Cancel asynchronously with callback.

  Arguments:    fileInfo  Pointer to file with pending operation
                callback  Callback when cancel completes

  Returns:      TRUE if cancel initiated
 *---------------------------------------------------------------------------*/
BOOL DVDCancelAsync(DVDFileInfo* fileInfo, DVDCallback callback) {
    BOOL result = DVDCancel(fileInfo);
    
    if (callback) {
        callback(result ? DVD_RESULT_CANCELED : DVD_RESULT_FATAL_ERROR, fileInfo);
    }
    
    return result;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetTransferredSize

  Description:  Get number of bytes transferred so far for async operation.
                
                On GC/Wii: Reads DI transfer counter
                On PC: Returns full size (operations complete quickly)

  Arguments:    fileInfo  Pointer to file with operation

  Returns:      Number of bytes transferred
 *---------------------------------------------------------------------------*/
s32 DVDGetTransferredSize(DVDFileInfo* fileInfo) {
    if (!fileInfo || !fileInfo->cb) {
        return 0;
    }
    
    DVDCommandBlock* cb = fileInfo->cb;
    
    // If operation complete, return result
    if (cb->state == DVD_STATE_END) {
        return cb->result;
    }
    
    // If busy, return 0 (can't track partial progress easily)
    return 0;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetCommandBlockStatus

  Description:  Get status of a command block.

  Arguments:    block  Pointer to command block

  Returns:      DVD_STATE_* constant
 *---------------------------------------------------------------------------*/
s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) {
    if (!block) {
        return DVD_STATE_END;
    }
    return block->state;
}

/*---------------------------------------------------------------------------*
  Name:         DVDConvertPathToEntrynum

  Description:  Convert a path to entry number in virtual FST.
                Entry numbers are used for fast file access.
                
                On GC/Wii: Searches FST for matching path
                On PC: Simulated - we don't build a real FST

  Arguments:    pathPtr  Path to file or directory

  Returns:      Entry number, or -1 if not found
 *---------------------------------------------------------------------------*/
s32 DVDConvertPathToEntrynum(const char* pathPtr) {
    if (!s_initialized || !pathPtr) {
        return -1;
    }
    
    // For PC implementation, we could build an FST by scanning directories
    // For now, return -1 (not implemented - use DVDOpen instead)
    // This is acceptable since most games use path-based access
    return -1;
}

/*---------------------------------------------------------------------------*
  Name:         DVDFastOpen

  Description:  Open file by entry number (faster than path lookup).
                
                On GC/Wii: Directly indexes into FST
                On PC: Not implemented (use DVDOpen with path instead)

  Arguments:    entrynum  Entry number from DVDConvertPathToEntrynum
                fileInfo  Pointer to DVDFileInfo to fill

  Returns:      TRUE if opened successfully
 *---------------------------------------------------------------------------*/
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    (void)entrynum;
    (void)fileInfo;
    
    /* Not implemented on PC.
     * Entry numbers require a pre-built FST which we don't generate.
     * Use DVDOpen() with file paths instead.
     */
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadDiskID

  Description:  Read disc ID information.
                
                On GC/Wii: Reads disc ID sector from DVD
                On PC: Returns fake disc ID

  Arguments:    block   Command block (unused on PC)
                diskID  Pointer to DVDDiskID to fill
                callback Callback when complete

  Returns:      TRUE always (instant on PC)
 *---------------------------------------------------------------------------*/
BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCallback callback) {
    (void)block;
    
    if (diskID) {
        memcpy(diskID, &s_diskID, sizeof(DVDDiskID));
    }
    
    if (callback) {
        callback(DVD_RESULT_GOOD, NULL);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDGetDriveStatus

  Description:  Get current drive status.
                
                On GC/Wii: Returns drive state (cover open, spinning, etc.)
                On PC: Always returns "disc ready"

  Arguments:    None

  Returns:      DVD_STATE_COVER_CLOSED (always on PC)
 *---------------------------------------------------------------------------*/
s32 DVDGetDriveStatus(void) {
    return s_initialized ? DVD_STATE_COVER_CLOSED : DVD_STATE_NO_DISK;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCheckDisk

  Description:  Check if disc is inserted and readable.
                
                On GC/Wii: Checks drive status, verifies disc
                On PC: Checks if "files/" directory exists

  Arguments:    None

  Returns:      DVD_RESULT_GOOD if ready, negative on error
 *---------------------------------------------------------------------------*/
s32 DVDCheckDisk(void) {
    if (!s_initialized) {
        return DVD_RESULT_FATAL_ERROR;
    }
    
    struct stat st;
    if (stat(s_rootPath, &st) == 0 && (st.st_mode & S_IFDIR)) {
        return DVD_RESULT_GOOD;
    }
    
    return DVD_RESULT_FATAL_ERROR;
}

/*---------------------------------------------------------------------------*
  Name:         DVDResume

  Description:  Resume DVD operations after pause.
                
                On GC/Wii: Restarts disc motor and operations
                On PC: No-op (no motor to restart)

  Arguments:    None

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDResume(void) {
    // No-op on PC - no motor to resume
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDSeekAsyncPrio

  Description:  Seek to position asynchronously with priority.
                
                On GC/Wii: Commands disc head to seek, returns immediately
                On PC: Seeks immediately, calls callback

  Arguments:    fileInfo  Pointer to open file
                offset    Offset to seek to
                callback  Callback when complete
                prio      Priority (ignored on PC)

  Returns:      TRUE if seek started
 *---------------------------------------------------------------------------*/
BOOL DVDSeekAsyncPrio(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback, s32 prio) {
    (void)prio;
    
    s32 result = DVDSeek(fileInfo, offset);
    
    if (callback) {
        callback(result >= 0 ? DVD_RESULT_GOOD : DVD_RESULT_FATAL_ERROR, fileInfo);
    }
    
    return (result >= 0);
}

/*---------------------------------------------------------------------------*
  Name:         DVDSeekAsync

  Description:  Seek to position asynchronously (convenience wrapper).

  Arguments:    fileInfo  Pointer to open file
                offset    Offset to seek to
                callback  Callback when complete

  Returns:      TRUE if seek started
 *---------------------------------------------------------------------------*/
BOOL DVDSeekAsync(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback) {
    return DVDSeekAsyncPrio(fileInfo, offset, callback, DVD_PRIO_MEDIUM);
}

/*---------------------------------------------------------------------------*
  Name:         DVDSeekPrio

  Description:  Seek to position with priority (synchronous).

  Arguments:    fileInfo  Pointer to open file
                offset    Offset to seek to
                prio      Priority (ignored on PC)

  Returns:      Current position, or -1 on error
 *---------------------------------------------------------------------------*/
s32 DVDSeekPrio(DVDFileInfo* fileInfo, s32 offset, s32 prio) {
    (void)prio;
    return DVDSeek(fileInfo, offset);
}

/*---------------------------------------------------------------------------*
  Name:         DVDInquiry

  Description:  Query DVD drive information.
                
                On GC/Wii: Sends INQUIRY command to drive
                On PC: Returns fake drive info

  Arguments:    block     Command block
                info      Pointer to DVDDriveInfo to fill
                callback  Callback when complete

  Returns:      DVD_RESULT_GOOD
 *---------------------------------------------------------------------------*/
s32 DVDInquiry(DVDCommandBlock* block, void* info, DVDCBCallback callback) {
    (void)block;
    (void)info;
    
    if (callback) {
        callback(DVD_RESULT_GOOD, block);
    }
    
    return DVD_RESULT_GOOD;
}

/*---------------------------------------------------------------------------*
  Name:         DVDInquiryAsync

  Description:  Query DVD drive information asynchronously.

  Arguments:    block     Command block
                info      Pointer to DVDDriveInfo to fill
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDInquiryAsync(DVDCommandBlock* block, void* info, DVDCBCallback callback) {
    DVDInquiry(block, info, callback);
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDChangeDisk

  Description:  Change disc (eject and wait for new disc).
                
                On GC/Wii: Waits for user to change disc
                On PC: No-op (no disc to change)

  Arguments:    block     Command block
                diskID    Expected new disc ID

  Returns:      DVD_RESULT_GOOD
 *---------------------------------------------------------------------------*/
s32 DVDChangeDisk(DVDCommandBlock* block, DVDDiskID* diskID) {
    (void)block;
    (void)diskID;
    
    OSReport("DVD: ChangeDisk called (no-op on PC)\n");
    return DVD_RESULT_GOOD;
}

/*---------------------------------------------------------------------------*
  Name:         DVDChangeDiskAsync

  Description:  Change disc asynchronously.

  Arguments:    block     Command block
                diskID    Expected new disc ID
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback) {
    s32 result = DVDChangeDisk(block, diskID);
    
    if (callback) {
        callback(result, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDStopMotor

  Description:  Stop disc motor.
                
                On GC/Wii: Sends motor stop command
                On PC: No-op (no motor)

  Arguments:    block  Command block

  Returns:      DVD_RESULT_GOOD
 *---------------------------------------------------------------------------*/
s32 DVDStopMotor(DVDCommandBlock* block) {
    (void)block;
    return DVD_RESULT_GOOD;
}

/*---------------------------------------------------------------------------*
  Name:         DVDStopMotorAsync

  Description:  Stop disc motor asynchronously.

  Arguments:    block     Command block
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDStopMotorAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    s32 result = DVDStopMotor(block);
    
    if (callback) {
        callback(result, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReset

  Description:  Reset DVD system.
                
                On GC/Wii: Resets DI hardware, disc state
                On PC: Reinitializes file system

  Arguments:    block     Command block
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDResetAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    DVDInit();  // Reinitialize
    
    if (callback) {
        callback(DVD_RESULT_GOOD, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDResetRequired

  Description:  Check if DVD reset is required.
                
                On GC/Wii: Checks if error state requires reset
                On PC: Always returns FALSE

  Arguments:    None

  Returns:      FALSE always on PC
 *---------------------------------------------------------------------------*/
BOOL DVDResetRequired(void) {
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDSetAutoInvalidation

  Description:  Enable/disable automatic cache invalidation for reads.
                
                On GC/Wii: Controls whether cache is invalidated on DMA reads
                On PC: No-op (no DMA or cache concerns)

  Arguments:    autoInval  TRUE to enable, FALSE to disable

  Returns:      Previous state (always TRUE on PC)
 *---------------------------------------------------------------------------*/
BOOL DVDSetAutoInvalidation(BOOL autoInval) {
    (void)autoInval;
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancelAllAsync

  Description:  Cancel all pending DVD operations.

  Arguments:    callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDCancelAllAsync(DVDCBCallback callback) {
    /* On PC, operations complete quickly so nothing to cancel.
     * Call callback immediately.
     */
    if (callback) {
        callback(DVD_RESULT_GOOD, NULL);
    }
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancelAll

  Description:  Cancel all pending DVD operations (synchronous).

  Arguments:    None

  Returns:      DVD_RESULT_GOOD
 *---------------------------------------------------------------------------*/
s32 DVDCancelAll(void) {
    return DVD_RESULT_GOOD;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCheckDiskAsync

  Description:  Check disc asynchronously.

  Arguments:    block     Command block
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    s32 result = DVDCheckDisk();
    
    if (callback) {
        callback(result, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDPrepareStreamAbsAsync

  Description:  Prepare DVD for audio streaming from absolute offset.
                
                On GC/Wii: Configures hardware audio streaming DMA
                On PC: No-op (no hardware streaming)

  Arguments:    block     Command block
                length    Stream length
                offset    Disc offset
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDPrepareStreamAbsAsync(DVDCommandBlock* block, u32 length, u32 offset,
                               DVDCBCallback callback) {
    (void)block;
    (void)length;
    (void)offset;
    
    /* Hardware audio streaming not supported on PC.
     * Games should use regular DVDRead for audio data.
     */
    if (callback) {
        callback(DVD_RESULT_GOOD, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancelStreamAsync

  Description:  Cancel audio streaming.

  Arguments:    block     Command block
                callback  Callback when complete

  Returns:      TRUE always
 *---------------------------------------------------------------------------*/
BOOL DVDCancelStreamAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    (void)block;
    
    if (callback) {
        callback(DVD_RESULT_GOOD, block);
    }
    
    return TRUE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDCancelStream

  Description:  Cancel audio streaming (synchronous).

  Arguments:    block  Command block

  Returns:      DVD_RESULT_GOOD
 *---------------------------------------------------------------------------*/
s32 DVDCancelStream(DVDCommandBlock* block) {
    (void)block;
    return DVD_RESULT_GOOD;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadAbsAsyncPrio

  Description:  Read from absolute disc offset with priority.
                
                On GC/Wii: Reads raw sectors from disc
                On PC: Not supported (use DVDOpen/DVDRead instead)

  Arguments:    block     Command block
                addr      Destination buffer
                length    Bytes to read
                offset    Absolute disc offset
                callback  Callback when complete
                prio      Priority

  Returns:      FALSE (not supported on PC)
 *---------------------------------------------------------------------------*/
BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length,
                         u32 offset, DVDCBCallback callback, s32 prio) {
    (void)block;
    (void)addr;
    (void)length;
    (void)offset;
    (void)prio;
    
    /* Absolute sector reads not supported on PC.
     * Games should use DVDOpen() + DVDRead() for file-based access.
     */
    if (callback) {
        callback(DVD_RESULT_FATAL_ERROR, block);
    }
    
    return FALSE;
}

/*---------------------------------------------------------------------------*
  Name:         DVDReadAbsAsyncForBS

  Description:  Read from absolute disc offset for boot sector operations.

  Arguments:    block     Command block
                addr      Destination buffer
                length    Bytes to read
                offset    Absolute disc offset
                callback  Completion callback

  Returns:      TRUE if started
 *---------------------------------------------------------------------------*/
BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length,
                          u32 offset, DVDCBCallback callback) {
    return DVDReadAbsAsyncPrio(block, addr, length, offset, callback, 0);
}

