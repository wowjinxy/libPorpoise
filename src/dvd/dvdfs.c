#include <dolphin/dvd.h>
#include <dolphin/os.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

typedef struct FSTEntry FSTEntry;

struct FSTEntry {
	uint isDirAndStringOff;
	uint parentOrPosition;
	uint nextEntryOrLength;
};

static OSBootInfo* BootInfo;
static FSTEntry* FstStart;
static char* FstStringStart;
static u32 MaxEntryNum;
static u32 currentDirectory = 0;
OSThreadQueue __DVDThreadQueue;
u32 __DVDLongFileNameFlag = FALSE;

#ifdef LIBPORPOISE_PORT
#define HOST_DVD_PATH_MAX 1024
#define HOST_DVD_MAX_OPEN_FILES 64
#define HOST_DVD_MAX_OPEN_DIRS 32
#define HOST_DVD_MAX_ENTRIES 512

typedef struct HostDVDFile {
	BOOL used;
	DVDFileInfo* info;
	FILE* file;
} HostDVDFile;

typedef struct HostDVDEntry {
	BOOL used;
	BOOL isDir;
	char path[HOST_DVD_PATH_MAX];
} HostDVDEntry;

typedef struct HostDVDDir {
	BOOL used;
	DVDDir* dir;
	char dvdPath[HOST_DVD_PATH_MAX];
	char name[HOST_DVD_PATH_MAX];
#ifdef _WIN32
	HANDLE findHandle;
	WIN32_FIND_DATAA findData;
	BOOL firstPending;
#else
	DIR* handle;
#endif
} HostDVDDir;

static char HostDVDRoot[HOST_DVD_PATH_MAX] = "files";
static char HostDVDCurrentDir[HOST_DVD_PATH_MAX] = "/";
static HostDVDFile HostDVDFiles[HOST_DVD_MAX_OPEN_FILES];
static HostDVDDir HostDVDDirs[HOST_DVD_MAX_OPEN_DIRS];
static HostDVDEntry HostDVDEntries[HOST_DVD_MAX_ENTRIES];

static BOOL hostDVDNormalizePath(const char* path, char* normalized, size_t normalizedSize);
static BOOL hostDVDBuildPath(const char* dvdPath, char* hostPath, size_t hostPathSize);
static BOOL hostDVDPathInfo(const char* dvdPath, BOOL* isDir, u32* length);
static s32 hostDVDRegisterEntry(const char* dvdPath, BOOL isDir);
static HostDVDFile* hostDVDFindFile(DVDFileInfo* fileInfo);
static HostDVDDir* hostDVDFindDir(DVDDir* dir);
static BOOL hostDVDOpenDirHandle(HostDVDDir* slot);
static void hostDVDCloseDirHandle(HostDVDDir* slot);
#endif

static void cbForReadAsync(s32 result, DVDCommandBlock* block);
static void cbForReadSync(s32 result, DVDCommandBlock* block);
static void cbForSeekAsync(s32 result, DVDCommandBlock* block);
static void cbForSeekSync(s32 result, DVDCommandBlock* block);
static void cbForPrepareStreamAsync(s32 result, DVDCommandBlock* block);
static void cbForPrepareStreamSync(s32 result, DVDCommandBlock* block);

#ifdef LIBPORPOISE_PORT
static BOOL hostDVDNormalizePath(const char* path, char* normalized, size_t normalizedSize)
{
	char combined[HOST_DVD_PATH_MAX];
	char component[HOST_DVD_PATH_MAX];
	size_t componentLength = 0;
	size_t outputLength = 1;
	const char* source;

	if (path == NULL || normalized == NULL || normalizedSize < 2) {
		return FALSE;
	}

	if (path[0] == '/' || path[0] == '\\') {
		snprintf(combined, sizeof(combined), "%s", path);
	} else if (strcmp(HostDVDCurrentDir, "/") == 0) {
		snprintf(combined, sizeof(combined), "/%s", path);
	} else {
		snprintf(combined, sizeof(combined), "%s/%s", HostDVDCurrentDir, path);
	}

	normalized[0] = '/';
	normalized[1] = '\0';
	source = combined;

	while (1) {
		char ch = *source++;
		if (ch == '\\') {
			ch = '/';
		}

		if (ch != '/' && ch != '\0') {
			if (componentLength + 1 >= sizeof(component)) {
				return FALSE;
			}
			component[componentLength++] = ch;
			continue;
		}

		if (componentLength != 0) {
			component[componentLength] = '\0';
			if (strcmp(component, ".") == 0) {
				/* Nothing to append. */
			} else if (strcmp(component, "..") == 0) {
				if (outputLength > 1) {
					size_t cursor = outputLength - 1;
					while (cursor > 0 && normalized[cursor - 1] != '/') {
						cursor--;
					}
					outputLength = cursor > 1 ? cursor - 1 : 1;
					normalized[outputLength] = '\0';
				}
			} else {
				size_t required = outputLength + componentLength + (outputLength > 1 ? 1 : 0) + 1;
				if (required > normalizedSize) {
					return FALSE;
				}
				if (outputLength > 1) {
					normalized[outputLength++] = '/';
				}
				memcpy(normalized + outputLength, component, componentLength);
				outputLength += componentLength;
				normalized[outputLength] = '\0';
			}
			componentLength = 0;
		}

		if (ch == '\0') {
			break;
		}
	}

	return TRUE;
}

static BOOL hostDVDBuildPath(const char* dvdPath, char* hostPath, size_t hostPathSize)
{
	char normalized[HOST_DVD_PATH_MAX];
	size_t i;

	if (!hostDVDNormalizePath(dvdPath, normalized, sizeof(normalized))) {
		return FALSE;
	}

	if (strcmp(normalized, "/") == 0) {
		if (snprintf(hostPath, hostPathSize, "%s", HostDVDRoot) >= (int)hostPathSize) {
			return FALSE;
		}
	} else if (snprintf(hostPath, hostPathSize, "%s/%s", HostDVDRoot, normalized + 1) >= (int)hostPathSize) {
		return FALSE;
	}

#ifdef _WIN32
	for (i = 0; hostPath[i] != '\0'; i++) {
		if (hostPath[i] == '/') {
			hostPath[i] = '\\';
		}
	}
#else
	(void)i;
#endif
	return TRUE;
}

static BOOL hostDVDPathInfo(const char* dvdPath, BOOL* isDir, u32* length)
{
	char hostPath[HOST_DVD_PATH_MAX];
	struct stat status;

	if (!hostDVDBuildPath(dvdPath, hostPath, sizeof(hostPath)) || stat(hostPath, &status) != 0) {
		return FALSE;
	}

	if (isDir != NULL) {
		isDir[0] = (status.st_mode & S_IFDIR) != 0 ? TRUE : FALSE;
	}
	if (length != NULL) {
		length[0] = (u32)status.st_size;
	}
	return TRUE;
}

static s32 hostDVDRegisterEntry(const char* dvdPath, BOOL isDir)
{
	char normalized[HOST_DVD_PATH_MAX];
	s32 freeEntry = -1;
	s32 i;

	if (!hostDVDNormalizePath(dvdPath, normalized, sizeof(normalized))) {
		return -1;
	}

	for (i = 0; i < HOST_DVD_MAX_ENTRIES; i++) {
		if (HostDVDEntries[i].used) {
			if (HostDVDEntries[i].isDir == isDir && strcmp(HostDVDEntries[i].path, normalized) == 0) {
				return i;
			}
		} else if (freeEntry < 0) {
			freeEntry = i;
		}
	}

	if (freeEntry < 0) {
		return -1;
	}

	HostDVDEntries[freeEntry].used = TRUE;
	HostDVDEntries[freeEntry].isDir = isDir;
	snprintf(HostDVDEntries[freeEntry].path, sizeof(HostDVDEntries[freeEntry].path), "%s", normalized);
	return freeEntry;
}

static HostDVDFile* hostDVDFindFile(DVDFileInfo* fileInfo)
{
	s32 i;
	for (i = 0; i < HOST_DVD_MAX_OPEN_FILES; i++) {
		if (HostDVDFiles[i].used && HostDVDFiles[i].info == fileInfo) {
			return &HostDVDFiles[i];
		}
	}
	return NULL;
}

static HostDVDDir* hostDVDFindDir(DVDDir* dir)
{
	s32 i;
	for (i = 0; i < HOST_DVD_MAX_OPEN_DIRS; i++) {
		if (HostDVDDirs[i].used && HostDVDDirs[i].dir == dir) {
			return &HostDVDDirs[i];
		}
	}
	return NULL;
}

static BOOL hostDVDOpenDirHandle(HostDVDDir* slot)
{
	char hostPath[HOST_DVD_PATH_MAX];
	if (slot == NULL || !hostDVDBuildPath(slot->dvdPath, hostPath, sizeof(hostPath))) {
		return FALSE;
	}

#ifdef _WIN32
	{
		char pattern[HOST_DVD_PATH_MAX];
		if (snprintf(pattern, sizeof(pattern), "%s\\*", hostPath) >= (int)sizeof(pattern)) {
			return FALSE;
		}
		slot->findHandle = FindFirstFileA(pattern, &slot->findData);
		if (slot->findHandle == INVALID_HANDLE_VALUE) {
			return FALSE;
		}
		slot->firstPending = TRUE;
	}
#else
	slot->handle = opendir(hostPath);
	if (slot->handle == NULL) {
		return FALSE;
	}
#endif
	return TRUE;
}

static void hostDVDCloseDirHandle(HostDVDDir* slot)
{
	if (slot == NULL) {
		return;
	}
#ifdef _WIN32
	if (slot->findHandle != NULL && slot->findHandle != INVALID_HANDLE_VALUE) {
		FindClose(slot->findHandle);
	}
	slot->findHandle = INVALID_HANDLE_VALUE;
	slot->firstPending = FALSE;
#else
	if (slot->handle != NULL) {
		closedir(slot->handle);
	}
	slot->handle = NULL;
#endif
}

BOOL DVDSetRootDirectory(const char* path)
{
	size_t length;
	if (path == NULL || path[0] == '\0') {
		return FALSE;
	}
	length = strlen(path);
	if (length >= sizeof(HostDVDRoot)) {
		return FALSE;
	}
	memcpy(HostDVDRoot, path, length + 1);
	while (length > 1 && (HostDVDRoot[length - 1] == '/' || HostDVDRoot[length - 1] == '\\')) {
		HostDVDRoot[--length] = '\0';
	}
	snprintf(HostDVDCurrentDir, sizeof(HostDVDCurrentDir), "/");
	return TRUE;
}
#endif

/**
 * @TODO: Documentation
 */
void __DVDFSInit()
{
#ifdef LIBPORPOISE_PORT
	struct stat status;
	memset(HostDVDFiles, 0, sizeof(HostDVDFiles));
	memset(HostDVDDirs, 0, sizeof(HostDVDDirs));
	memset(HostDVDEntries, 0, sizeof(HostDVDEntries));
	HostDVDEntries[0].used = TRUE;
	HostDVDEntries[0].isDir = TRUE;
	snprintf(HostDVDEntries[0].path, sizeof(HostDVDEntries[0].path), "/");
	snprintf(HostDVDCurrentDir, sizeof(HostDVDCurrentDir), "/");

	if (stat(HostDVDRoot, &status) != 0) {
#ifdef _WIN32
		_mkdir(HostDVDRoot);
#else
		mkdir(HostDVDRoot, 0755);
#endif
	}
	OSReport("DVD: host root is '%s'\n", HostDVDRoot);
#else
	BootInfo = (OSBootInfo*)OSPhysicalToCached(0);
	FstStart = (FSTEntry*)BootInfo->FSTLocation;

	if (FstStart) {
		MaxEntryNum    = FstStart[0].nextEntryOrLength;
		FstStringStart = (char*)&(FstStart[MaxEntryNum]);
	}
#endif
}

/* For convenience */
#define entryIsDir(i)   (((FstStart[i].isDirAndStringOff & 0xff000000) == 0) ? FALSE : TRUE)
#define stringOff(i)    (FstStart[i].isDirAndStringOff & ~0xff000000)
#define parentDir(i)    (FstStart[i].parentOrPosition)
#define nextDir(i)      (FstStart[i].nextEntryOrLength)
#define filePosition(i) (FstStart[i].parentOrPosition)
#define fileLength(i)   (FstStart[i].nextEntryOrLength)

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00009C (Matching by size)
 */
static BOOL isSame(const char* path, const char* string)
{
	while (*string != '\0') {
		if (tolower(*path++) != tolower(*string++)) {
			return FALSE;
		}
	}

	if ((*path == '/') || (*path == '\0')) {
		return TRUE;
	}
	return FALSE;
}

/**
 * @TODO: Documentation
 */
s32 DVDConvertPathToEntrynum(const char* pathPtr)
{
	const char* ptr;
	char* stringPtr;
	BOOL isDir;
	u32 length;
	u32 dirLookAt;
	u32 i;
	const char* origPathPtr = pathPtr;
	const char* extentionStart;
	BOOL illegal;
	BOOL extention;

#ifdef LIBPORPOISE_PORT
	if (!hostDVDPathInfo(pathPtr, &isDir, NULL)) {
		return -1;
	}
	return hostDVDRegisterEntry(pathPtr, isDir);
#endif

	if (pathPtr == NULL || FstStart == NULL || FstStringStart == NULL ||
	    MaxEntryNum == 0) {
		return -1;
	}

	dirLookAt = currentDirectory;

	while (1) {
		if (*pathPtr == '\0') {
			return (s32)dirLookAt;
		} else if (*pathPtr == '/') {
			dirLookAt = 0;
			pathPtr++;
			continue;
		} else if (*pathPtr == '.') {
			if (*(pathPtr + 1) == '.') {
				if (*(pathPtr + 2) == '/') {
					dirLookAt = parentDir(dirLookAt);
					pathPtr += 3;
					continue;
				} else if (*(pathPtr + 2) == '\0') {
					return (s32)parentDir(dirLookAt);
				}
			} else if (*(pathPtr + 1) == '/') {
				pathPtr += 2;
				continue;
			} else if (*(pathPtr + 1) == '\0') {
				return (s32)dirLookAt;
			}
		}

		if (!__DVDLongFileNameFlag) {
			extention = FALSE;
			illegal   = FALSE;

			for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
				if (*ptr == '.') {
					if ((ptr - pathPtr > 8) || (extention == TRUE)) {
						illegal = TRUE;
						break;
					}
					extention      = TRUE;
					extentionStart = ptr + 1;

				} else if (*ptr == ' ') {
					illegal = TRUE;
				}
			}

			if ((extention == TRUE) && (ptr - extentionStart > 3)) {
				illegal = TRUE;
			}

			if (illegal) {
#if OS_BUILD_VERSION >= 20011002L
				OSErrorLine(376,
				            "DVDConvertEntrynumToPath(possibly DVDOpen or DVDChangeDir or DVDOpenDir): specified directory or file "
				            "(%s) doesn't match standard 8.3 format. This is a temporary restriction and will be removed soon\n",
				            origPathPtr);
#else
				OSErrorLine(373,
				            "DVDConvertEntrynumToPath(possibly DVDOpen or DVDChangeDir or DVDOpenDir): specified directory or file "
				            "(%s) doesn't match standard 8.3 format. This is a temporary restriction and will be removed soon\n",
				            origPathPtr);
#endif
			}
		} else {
			for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++)
				;
		}

		isDir  = (*ptr == '\0') ? FALSE : TRUE;
		length = (u32)(ptr - pathPtr);

		ptr = pathPtr;

		for (i = dirLookAt + 1; i < nextDir(dirLookAt); i = entryIsDir(i) ? nextDir(i) : (i + 1)) {
			if ((entryIsDir(i) == FALSE) && (isDir == TRUE)) {
				continue;
			}

			stringPtr = FstStringStart + stringOff(i);

			if (isSame(ptr, stringPtr) == TRUE) {
				goto next_hier;
			}
		}

		return -1;

	next_hier:
		if (!isDir) {
			return (s32)i;
		}

		dirLookAt = i;
		pathPtr += length + 1;
	}
}

/**
 * @TODO: Documentation
 */
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo)
{
#ifdef LIBPORPOISE_PORT
	if (entrynum < 0 || entrynum >= HOST_DVD_MAX_ENTRIES ||
	    !HostDVDEntries[entrynum].used || HostDVDEntries[entrynum].isDir) {
		return FALSE;
	}
	return DVDOpen(HostDVDEntries[entrynum].path, fileInfo);
#else
	if (FstStart == NULL || fileInfo == NULL) {
		return FALSE;
	}

	if ((entrynum < 0) || (entrynum >= MaxEntryNum) || entryIsDir(entrynum)) {
		return FALSE;
	}

	fileInfo->startAddr    = filePosition(entrynum);
	fileInfo->length       = fileLength(entrynum);
	fileInfo->callback     = (DVDCallback)NULL;
	fileInfo->cBlock.state = DVD_STATE_END;

	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 */
BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo)
{
#ifdef LIBPORPOISE_PORT
	char hostPath[HOST_DVD_PATH_MAX];
	FILE* file;
	long size;
	s32 i;

	OSCheckAlarmQueue();
	if (fileName == NULL || fileInfo == NULL ||
	    !hostDVDBuildPath(fileName, hostPath, sizeof(hostPath))) {
		return FALSE;
	}

	file = fopen(hostPath, "rb");
	if (file == NULL) {
		OSReport("Warning: DVDOpen(): host file '%s' was not found.\n", hostPath);
		return FALSE;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return FALSE;
	}
	OSCheckAlarmQueue();

	for (i = 0; i < HOST_DVD_MAX_OPEN_FILES; i++) {
		if (!HostDVDFiles[i].used) {
			memset(fileInfo, 0, sizeof(*fileInfo));
			HostDVDFiles[i].used = TRUE;
			HostDVDFiles[i].info = fileInfo;
			HostDVDFiles[i].file = file;
			fileInfo->startAddr = 0;
			fileInfo->length = (u32)size;
			fileInfo->callback = NULL;
			fileInfo->cBlock.state = DVD_STATE_END;
			hostDVDRegisterEntry(fileName, FALSE);
			return TRUE;
		}
	}

	fclose(file);
	OSReport("Warning: DVDOpen(): host file table is full.\n");
	return FALSE;
#else
	s32 entry;
	char currentDir[128];

	entry = DVDConvertPathToEntrynum(fileName);

	if (0 > entry) {
		DVDGetCurrentDir(currentDir, sizeof(currentDir));
		OSReport("Warning: DVDOpen(): file '%s' was not found under %s.\n", fileName, currentDir);
		return FALSE;
	}

	if (entryIsDir(entry)) {
		return FALSE;
	}

	fileInfo->startAddr    = filePosition(entry);
	fileInfo->length       = fileLength(entry);
	fileInfo->callback     = (DVDCallback)NULL;
	fileInfo->cBlock.state = DVD_STATE_END;

	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 */
BOOL DVDClose(DVDFileInfo* fileInfo)
{
#ifdef LIBPORPOISE_PORT
	HostDVDFile* slot = hostDVDFindFile(fileInfo);
	OSCheckAlarmQueue();
	if (slot == NULL) {
		return FALSE;
	}
	fclose(slot->file);
	OSCheckAlarmQueue();
	memset(slot, 0, sizeof(*slot));
	fileInfo->cBlock.state = DVD_STATE_END;
	return TRUE;
#else
	DVDCancel(&(fileInfo->cBlock));
	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000038
 */
static u32 myStrncpy(char* dest, const char* src, u32 maxlen)
{
	u32 i = maxlen;

	while ((i > 0) && (*src != 0)) {
		*dest++ = *src++;
		i--;
	}

	return (maxlen - i);
}

/**
 * @TODO: Documentation
 */
static u32 entryToPath(u32 entry, char* path, u32 maxlen)
{
	char* name;
	u32 loc;

	if (entry == 0) {
		return 0;
	}

	name = FstStringStart + stringOff(entry);

	loc = entryToPath(parentDir(entry), path, maxlen);

	if (loc == maxlen) {
		return loc;
	}

	*(path + loc++) = '/';

	loc += myStrncpy(path + loc, name, maxlen - loc);

	return loc;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000154
 */
static BOOL DVDConvertEntrynumToPath(s32 entrynum, char* path, u32 maxlen)
{
	u32 loc;

	loc = entryToPath((u32)entrynum, path, maxlen);

	if (loc == maxlen) {
		path[maxlen - 1] = '\0';
		return FALSE;
	}

	if (entryIsDir(entrynum)) {
		if (loc == maxlen - 1) {
			path[loc] = '\0';
			return FALSE;
		}

		path[loc++] = '/';
	}

	path[loc] = '\0';
	return TRUE;
}

/**
 * @TODO: Documentation
 */
BOOL DVDGetCurrentDir(char* path, u32 maxlen)
{
	if (path == NULL || maxlen == 0) {
		return FALSE;
	}
#ifdef LIBPORPOISE_PORT
	if (strlen(HostDVDCurrentDir) + 1 > maxlen) {
		snprintf(path, maxlen, "%s", HostDVDCurrentDir);
		return FALSE;
	}
	snprintf(path, maxlen, "%s", HostDVDCurrentDir);
	return TRUE;
#else
	if (FstStart == NULL) {
		path[0] = '/';
		if (maxlen > 1) {
			path[1] = '\0';
			return TRUE;
		}
		path[0] = '\0';
		return FALSE;
	}

	return DVDConvertEntrynumToPath((s32)currentDirectory, path, maxlen);
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000060
 */
BOOL DVDChangeDir(const char* dirName)
{
#ifdef LIBPORPOISE_PORT
	char normalized[HOST_DVD_PATH_MAX];
	BOOL isDir;
	if (!hostDVDNormalizePath(dirName, normalized, sizeof(normalized)) ||
	    !hostDVDPathInfo(dirName, &isDir, NULL) || !isDir) {
		return FALSE;
	}
	snprintf(HostDVDCurrentDir, sizeof(HostDVDCurrentDir), "%s", normalized);
	hostDVDRegisterEntry(normalized, TRUE);
	return TRUE;
#else
	s32 entry;
	if (FstStart == NULL) {
		return FALSE;
	}
	entry = DVDConvertPathToEntrynum(dirName);
	if ((entry < 0) || (entryIsDir(entry) == FALSE)) {
		return FALSE;
	}

	currentDirectory = (u32)entry;

	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 */
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32 prio)
{
#ifdef LIBPORPOISE_PORT
	s32 result;
	(void)prio;
	result = DVDReadPrio(fileInfo, addr, length, offset, prio);
	if (callback != NULL) {
		callback(result, fileInfo);
	}
	return result >= 0 ? TRUE : FALSE;
#else
	if (!((0 <= offset) && (offset < fileInfo->length))) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(739, "DVDReadAsync(): specified area is out of the file  ");
#else
		OSErrorLine(735, "DVDReadAsync(): specified area is out of the file  ");
#endif
	}

	if (!((0 <= offset + length) && (offset + length < fileInfo->length + DVD_MIN_TRANSFER_SIZE))) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(745, "DVDReadAsync(): specified area is out of the file  ");
#else
		OSErrorLine(741, "DVDReadAsync(): specified area is out of the file  ");
#endif
	}

	fileInfo->callback = callback;
	DVDReadAbsAsyncPrio(&(fileInfo->cBlock), addr, length, (s32)(fileInfo->startAddr + offset), cbForReadAsync, prio);

	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 */
static void cbForReadAsync(s32 result, DVDCommandBlock* block)
{
	DVDFileInfo* fileInfo;

	fileInfo = (DVDFileInfo*)((char*)block - offsetof(DVDFileInfo, cBlock));
	if (fileInfo->callback) {
		(fileInfo->callback)(result, fileInfo);
	}
}

/**
 * @TODO: Documentation
 */
s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio)
{
#ifdef LIBPORPOISE_PORT
	HostDVDFile* slot = hostDVDFindFile(fileInfo);
	size_t bytesRead;
	(void)prio;
	OSCheckAlarmQueue();
	if (slot == NULL || addr == NULL || length < 0 || offset < 0 ||
	    (u32)offset > fileInfo->length) {
		return DVD_RESULT_FATAL_ERROR;
	}
	if (fseek(slot->file, offset, SEEK_SET) != 0) {
		fileInfo->cBlock.state = DVD_STATE_FATAL_ERROR;
		return DVD_RESULT_FATAL_ERROR;
	}
	fileInfo->cBlock.state = DVD_STATE_BUSY;
	bytesRead = fread(addr, 1, (size_t)length, slot->file);
	OSCheckAlarmQueue();
	fileInfo->cBlock.currTransferSize = (u32)bytesRead;
	fileInfo->cBlock.transferredSize = (u32)bytesRead;
	fileInfo->cBlock.state = DVD_STATE_END;
	if (bytesRead < (size_t)length && ferror(slot->file)) {
		clearerr(slot->file);
		return DVD_RESULT_FATAL_ERROR;
	}
	return (s32)bytesRead;
#else
	BOOL result;
	DVDCommandBlock* block;
	s32 state;
	BOOL enabled;
	s32 retVal;

	if (!((0 <= offset) && (offset < fileInfo->length))) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(809, "DVDRead(): specified area is out of the file  ");
#else
		OSErrorLine(805, "DVDRead(): specified area is out of the file  ");
#endif
	}

	if (!((0 <= offset + length) && (offset + length < fileInfo->length + DVD_MIN_TRANSFER_SIZE))) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(815, "DVDRead(): specified area is out of the file  ");
#else
		OSErrorLine(811, "DVDRead(): specified area is out of the file  ");
#endif
	}

	block = &(fileInfo->cBlock);

	result = DVDReadAbsAsyncPrio(block, addr, length, (s32)(fileInfo->startAddr + offset), cbForReadSync, prio);

	if (result == FALSE) {
		return DVD_RESULT_FATAL_ERROR;
	}

	enabled = OSDisableInterrupts();

	while (TRUE) {
		state = ((volatile DVDCommandBlock*)block)->state;

		if (state == DVD_STATE_END) {
			retVal = (s32)block->transferredSize;
			break;
		}
		if (state == DVD_STATE_FATAL_ERROR) {
			retVal = DVD_RESULT_FATAL_ERROR;
			break;
		}
		if (state == DVD_STATE_CANCELED) {
			retVal = DVD_RESULT_CANCELED;
			break;
		}

		OSSleepThread(&__DVDThreadQueue);
	}

	OSRestoreInterrupts(enabled);
	return retVal;
#endif
}

/**
 * @TODO: Documentation
 */
static void cbForReadSync(s32 result, DVDCommandBlock* block)
{
	OSWakeupThread(&__DVDThreadQueue);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000098
 */
BOOL DVDSeekAsyncPrio(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback, s32 prio)
{
#ifdef LIBPORPOISE_PORT
	s32 result = DVDSeekPrio(fileInfo, offset, prio);
	if (callback != NULL) {
		callback(result, fileInfo);
	}
	return result >= 0 ? TRUE : FALSE;
#else
	if (!((0 <= offset) && (offset < fileInfo->length))) {
		OSErrorLine(881, "DVDSeek(): offset is out of the file  ");
	}

	fileInfo->callback = callback;
	DVDSeekAbsAsyncPrio(&fileInfo->cBlock, (char*)fileInfo->startAddr + offset, cbForSeekAsync, prio);
	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000030
 */
static void cbForSeekAsync(s32 result, DVDCommandBlock* block)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000AC
 */
s32 DVDSeekPrio(DVDFileInfo* fileInfo, s32 offset, s32 prio)
{
#ifdef LIBPORPOISE_PORT
	HostDVDFile* slot = hostDVDFindFile(fileInfo);
	(void)prio;
	OSCheckAlarmQueue();
	if (slot == NULL || offset < 0 || (u32)offset > fileInfo->length) {
		return DVD_RESULT_FATAL_ERROR;
	}
	if (fseek(slot->file, offset, SEEK_SET) != 0) {
		return DVD_RESULT_FATAL_ERROR;
	}
	OSCheckAlarmQueue();
	fileInfo->cBlock.offset = (u32)offset;
	fileInfo->cBlock.state = DVD_STATE_END;
	return offset;
#else
	TRAP_UNIMPLEMENTED;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000024
 */
static void cbForSeekSync(s32 result, DVDCommandBlock* block)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000020
 */
s32 DVDGetFileInfoStatus(DVDFileInfo* fileInfo)
{
#ifdef LIBPORPOISE_PORT
	if (hostDVDFindFile(fileInfo) == NULL) {
		return DVD_STATE_FATAL_ERROR;
	}
	return fileInfo->cBlock.state;
#else
	TRAP_UNIMPLEMENTED;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000084 (OS_BUILD_VERSION <  20011002L) (Matching by size)
 * @note UNUSED Size: 0000c0 (OS_BUILD_VERSION >= 20011002L) (Matching by size)
 */
BOOL DVDOpenDir(const char* dirName, DVDDir* dir)
{
#ifdef LIBPORPOISE_PORT
	BOOL isDir;
	char normalized[HOST_DVD_PATH_MAX];
	s32 entry;
	s32 i;

	if (dir == NULL || !hostDVDNormalizePath(dirName, normalized, sizeof(normalized)) ||
	    !hostDVDPathInfo(dirName, &isDir, NULL) || !isDir) {
		return FALSE;
	}

	entry = hostDVDRegisterEntry(normalized, TRUE);
	if (entry < 0) {
		return FALSE;
	}

	for (i = 0; i < HOST_DVD_MAX_OPEN_DIRS; i++) {
		HostDVDDir* slot = &HostDVDDirs[i];
		if (!slot->used) {
			memset(slot, 0, sizeof(*slot));
			slot->used = TRUE;
			slot->dir = dir;
			snprintf(slot->dvdPath, sizeof(slot->dvdPath), "%s", normalized);
			if (!hostDVDOpenDirHandle(slot)) {
				memset(slot, 0, sizeof(*slot));
				return FALSE;
			}
			dir->entryNum = (u32)entry;
			dir->location = 0;
			dir->next = 0;
			return TRUE;
		}
	}
	return FALSE;
#else
	s32 entry;
	char currentDir[128];

	if (FstStart == NULL || dir == NULL) {
		return FALSE;
	}

	entry = DVDConvertPathToEntrynum(dirName);
#if OS_BUILD_VERSION >= 20011002L
	if (entry < 0) {
		DVDGetCurrentDir(currentDir, sizeof(currentDir));
		OSReport("Warning: DVDOpenDir(): file '%s' was not found under %s.\n", dirName, currentDir);
		return FALSE;
	}
	if (!entryIsDir(entry)) {
		return FALSE;
	}
#else
	if (entry < 0 || !entryIsDir(entry)) {
		return FALSE;
	}
#endif
	dir->entryNum = entry;
	dir->location = entry + 1;
	dir->next     = nextDir(entry);
	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 0000A4
 */
BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirent)
{
#ifdef LIBPORPOISE_PORT
	HostDVDDir* slot = hostDVDFindDir(dir);
	if (slot == NULL || dirent == NULL) {
		return FALSE;
	}

	while (TRUE) {
		const char* name;
		BOOL isDir;
		char childPath[HOST_DVD_PATH_MAX];
#ifdef _WIN32
		if (slot->firstPending) {
			slot->firstPending = FALSE;
		} else if (!FindNextFileA(slot->findHandle, &slot->findData)) {
			return FALSE;
		}
		name = slot->findData.cFileName;
		isDir = (slot->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? TRUE : FALSE;
#else
		struct dirent* entry = readdir(slot->handle);
		if (entry == NULL) {
			return FALSE;
		}
		name = entry->d_name;
		if (snprintf(childPath, sizeof(childPath), "%s/%s", slot->dvdPath, name) >= (int)sizeof(childPath) ||
		    !hostDVDPathInfo(childPath, &isDir, NULL)) {
			continue;
		}
#endif
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}

		if (strcmp(slot->dvdPath, "/") == 0) {
			snprintf(childPath, sizeof(childPath), "/%s", name);
		} else {
			snprintf(childPath, sizeof(childPath), "%s/%s", slot->dvdPath, name);
		}
		{
			s32 entryNum = hostDVDRegisterEntry(childPath, isDir);
			const char* stableName;
			if (entryNum < 0) {
				return FALSE;
			}
			stableName = strrchr(HostDVDEntries[entryNum].path, '/');
			dirent->entryNum = (u32)entryNum;
			dirent->name = stableName != NULL
				? stableName + 1
				: HostDVDEntries[entryNum].path;
		}
		dirent->isDir = isDir;
		dir->location++;
		return TRUE;
	}
#else
	TRAP_UNIMPLEMENTED;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008
 */
BOOL DVDCloseDir(DVDDir* dir)
{
#ifdef LIBPORPOISE_PORT
	HostDVDDir* slot = hostDVDFindDir(dir);
	if (slot == NULL) {
		return FALSE;
	}
	hostDVDCloseDirHandle(slot);
	memset(slot, 0, sizeof(*slot));
	return TRUE;
#else
	TRAP_UNIMPLEMENTED;
#endif
}

void DVDRewindDir(DVDDir* dir)
{
#ifdef LIBPORPOISE_PORT
	HostDVDDir* slot = hostDVDFindDir(dir);
	if (slot == NULL) {
		return;
	}
	hostDVDCloseDirHandle(slot);
	hostDVDOpenDirHandle(slot);
	dir->location = 0;
#else
	if (dir != NULL) {
		dir->location = dir->entryNum + 1;
	}
#endif
}

BOOL DVDFastOpenDir(s32 entryNum, DVDDir* dir)
{
#ifdef LIBPORPOISE_PORT
	if (entryNum < 0 || entryNum >= HOST_DVD_MAX_ENTRIES ||
	    !HostDVDEntries[entryNum].used || !HostDVDEntries[entryNum].isDir) {
		return FALSE;
	}
	return DVDOpenDir(HostDVDEntries[entryNum].path, dir);
#else
	if (entryNum < 0 || entryNum >= (s32)MaxEntryNum || !entryIsDir(entryNum)) {
		return FALSE;
	}
	dir->entryNum = (u32)entryNum;
	dir->location = (u32)entryNum + 1;
	dir->next = nextDir(entryNum);
	return TRUE;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00000C
 */
void* DVDGetFSTLocation()
{
#ifdef LIBPORPOISE_PORT
	return HostDVDEntries;
#else
	TRAP_UNIMPLEMENTED;
#endif
}

#define RoundUp32KB(x)   (((u32)(x) + 32 * 1024 - 1) & ~(32 * 1024 - 1))
#define Is32KBAligned(x) (((u32)(x) & (32 * 1024 - 1)) == 0)

/**
 * @TODO: Documentation
 */
BOOL DVDPrepareStreamAsync(DVDFileInfo* fileInfo, u32 length, u32 offset, DVDCallback callback)
{
	u32 start;

	start = fileInfo->startAddr + offset;

	if (!Is32KBAligned(start)) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(1186,
		            "DVDPrepareStreamAsync(): Specified start address (filestart(0x%x) + offset(0x%x)) is "
		            "not 32KB aligned",
		            fileInfo->startAddr, offset);
#else
		OSErrorLine(1150,
		            "DVDPrepareStreamAsync(): Specified start address (filestart(0x%x) + offset(0x%x)) is "
		            "not 32KB aligned",
		            fileInfo->startAddr, offset);
#endif
	}

	if (length == 0)
		length = fileInfo->length - offset;

	if (!Is32KBAligned(length)) {
#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(1196, "DVDPrepareStreamAsync(): Specified length (0x%x) is not a multiple of 32768(32*1024)", length);
#else
		OSErrorLine(1160, "DVDPrepareStreamAsync(): Specified length (0x%x) is not a multiple of 32768(32*1024)", length);
#endif
	}

	if (!((offset < fileInfo->length) && (offset + length <= fileInfo->length))) {

#if OS_BUILD_VERSION >= 20011002L
		OSErrorLine(1204,
		            "DVDPrepareStreamAsync(): The area specified (offset(0x%x), length(0x%x)) is out of "
		            "the file",
		            offset, length);
#else
		OSErrorLine(1168,
		            "DVDPrepareStreamAsync(): The area specified (offset(0x%x), length(0x%x)) is out of "
		            "the file",
		            offset, length);
#endif
	}

	fileInfo->callback = callback;
	return DVDPrepareStreamAbsAsync(&(fileInfo->cBlock), length, fileInfo->startAddr + offset, cbForPrepareStreamAsync);
}

/**
 * @TODO: Documentation
 */
static void cbForPrepareStreamAsync(s32 result, DVDCommandBlock* block)
{
	struct DVDFileInfo* fileInfo;

	fileInfo = (struct DVDFileInfo*)&block->next;
	if (fileInfo->callback) {
		(fileInfo->callback)(result, fileInfo);
	}
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000144 (Matching by size)
 */
s32 DVDPrepareStream(DVDFileInfo* fileInfo, u32 length, u32 offset)
{
	BOOL result;
	DVDCommandBlock* block;
	s32 state;
	BOOL enabled;
	s32 retVal;
	u32 start;

	start = fileInfo->startAddr + offset;

	if (!Is32KBAligned(start)) {
		OSErrorLine(0x4BF,
		            "DVDPrepareStream(): Specified start address (filestart(0x%x) + offset(0x%x)) is not "
		            "32KB aligned",
		            fileInfo->startAddr, offset);
	}

	if (length == 0)
		length = fileInfo->length - offset;

	if (!Is32KBAligned(length)) {
		OSErrorLine(0x4C9, "DVDPrepareStream(): Specified length (0x%x) is not a multiple of 32768(32*1024)", length);
	}

	if (!((offset < fileInfo->length) && (offset + length <= fileInfo->length))) {
		OSErrorLine(0x4D1, "DVDPrepareStream(): The area specified (offset(0x%x), length(0x%x)) is out of the file", offset, length);
	}

	block  = &(fileInfo->cBlock);
	result = DVDPrepareStreamAbsAsync(block, length, start, cbForPrepareStreamSync);

	if (result == FALSE) {
		return -1;
	}

	enabled = OSDisableInterrupts();

	while (1) {
		state = ((volatile DVDCommandBlock*)block)->state;

		if (state == DVD_STATE_END) {
			retVal = 0;
			break;
		}
		if (state == DVD_STATE_FATAL_ERROR) {
			retVal = DVD_RESULT_FATAL_ERROR;
			break;
		}
		if (state == DVD_STATE_CANCELED) {
			retVal = DVD_RESULT_CANCELED;
			break;
		}

		OSSleepThread(&__DVDThreadQueue);
	}

	OSRestoreInterrupts(enabled);
	return retVal;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000024
 */
static void cbForPrepareStreamSync(s32 result, DVDCommandBlock* block)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00006C
 */
s32 DVDGetTransferredSize(DVDFileInfo* fileinfo)
{
#ifdef LIBPORPOISE_PORT
	if (hostDVDFindFile(fileinfo) == NULL) {
		return DVD_RESULT_FATAL_ERROR;
	}
	return (s32)fileinfo->cBlock.transferredSize;
#else
	TRAP_UNIMPLEMENTED;
#endif
}
