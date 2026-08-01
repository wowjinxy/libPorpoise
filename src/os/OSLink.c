#include <dolphin/os/OSHostEndian.h>
#include <dolphin/os/OSLink.h>
#include <dolphin/os/OSError.h>
#include <porpoise/native_module.h>

#if defined(LIBPORPOISE_PORT)

#include "OSHostModule.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OS_NATIVE_MODULE_LIMIT 64
#define OS_MODULE_PATH_SIZE    1024
#define OS_MODULE_ERROR_SIZE   1024
#define OS_REL_BSS_SIZE_OFFSET 0x20

typedef struct OSLinkedNativeModule {
	OSModuleInfo* module;
	OSHostModuleHandle handle;
	const PorpoiseNativeModuleDescriptor* descriptor;
} OSLinkedNativeModule;

extern OSModuleQueue __OSModuleInfoList;
extern const void* __OSStringTable;

static OSLinkedNativeModule NativeModules[OS_NATIVE_MODULE_LIMIT];
static const PorpoiseNativeModuleDescriptor* RegisteredNativeModules[OS_NATIVE_MODULE_LIMIT];
static char ModuleError[OS_MODULE_ERROR_SIZE];
static const char* const NativeRelSuffixes[] = { ".rel.szs", ".rel", ".szs" };

static void ClearModuleError(void)
{
	ModuleError[0] = '\0';
}

static void SetModuleError(const char* format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(ModuleError, sizeof(ModuleError), format, arguments);
	va_end(arguments);
	ModuleError[sizeof(ModuleError) - 1] = '\0';
	OSReport("OSLink: %s\n", ModuleError);
}

static OSLinkedNativeModule* FindLinkedModule(const OSModuleInfo* module)
{
	int index;
	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (NativeModules[index].module == module) {
			return &NativeModules[index];
		}
	}
	return NULL;
}

static OSLinkedNativeModule* FindFreeModuleSlot(void)
{
	int index;
	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (NativeModules[index].module == NULL) {
			return &NativeModules[index];
		}
	}
	return NULL;
}

static BOOL EndsWith(const char* text, const char* suffix)
{
	size_t textLength = strlen(text);
	size_t suffixLength = strlen(suffix);
	return textLength >= suffixLength &&
	       strcmp(text + textLength - suffixLength, suffix) == 0;
}

static BOOL HasNativeExtension(const char* path)
{
	const char* extension = __OSHostModuleExtension();
	return extension[0] != '\0' && EndsWith(path, extension);
}

static BOOL IsAbsoluteHostPath(const char* path)
{
	if (path[0] == '/' || path[0] == '\\') {
		return TRUE;
	}
	return path[0] != '\0' && path[1] == ':' &&
	       (path[2] == '/' || path[2] == '\\');
}

static const char* FindFileName(const char* path)
{
	const char* slash = strrchr(path, '/');
	const char* backslash = strrchr(path, '\\');
	const char* fileName = path;

	if (slash != NULL) {
		fileName = slash + 1;
	}
	if (backslash != NULL && backslash + 1 > fileName) {
		fileName = backslash + 1;
	}
	return fileName;
}

static BOOL GetLogicalModuleName(const char* moduleName, char* logicalName, size_t logicalNameSize)
{
	const char* fileName;
	size_t nameLength;
	size_t suffixIndex;

	fileName = FindFileName(moduleName);
	nameLength = strlen(fileName);
	if (nameLength == 0) {
		SetModuleError("module name has no file name");
		return FALSE;
	}

	for (suffixIndex = 0;
	     suffixIndex < sizeof(NativeRelSuffixes) / sizeof(NativeRelSuffixes[0]);
	     ++suffixIndex) {
		size_t suffixLength = strlen(NativeRelSuffixes[suffixIndex]);
		if (nameLength >= suffixLength &&
		    strcmp(fileName + nameLength - suffixLength, NativeRelSuffixes[suffixIndex]) == 0) {
			nameLength -= suffixLength;
			break;
		}
	}
	if (nameLength == 0) {
		SetModuleError("module name has no native base name");
		return FALSE;
	}
	if (nameLength >= logicalNameSize) {
		SetModuleError("native module logical name is too long");
		return FALSE;
	}

	memcpy(logicalName, fileName, nameLength);
	logicalName[nameLength] = '\0';
	return TRUE;
}

static BOOL FormatModulePath(char* path, size_t pathSize, const char* format, ...)
{
	int length;
	va_list arguments;

	va_start(arguments, format);
	length = vsnprintf(path, pathSize, format, arguments);
	va_end(arguments);
	if (length < 0 || (size_t)length >= pathSize) {
		SetModuleError("native module path is too long");
		return FALSE;
	}
	return TRUE;
}

static BOOL BuildNativeModulePath(const char* moduleName, char* path, size_t pathSize)
{
	char executableDirectory[OS_MODULE_PATH_SIZE];
	char pathError[OS_MODULE_ERROR_SIZE];
	const char* fileName;
	const char* extension;
	size_t nameLength;
	size_t suffixIndex;

	if (HasNativeExtension(moduleName) && IsAbsoluteHostPath(moduleName)) {
		return FormatModulePath(path, pathSize, "%s", moduleName);
	}

	pathError[0] = '\0';
	if (!__OSHostModuleGetExecutableDirectory(
		    executableDirectory, sizeof(executableDirectory), pathError, sizeof(pathError))) {
		SetModuleError("could not find the executable directory: %s", pathError);
		return FALSE;
	}

	if (HasNativeExtension(moduleName)) {
		return FormatModulePath(path, pathSize, "%s/%s", executableDirectory, moduleName);
	}

	fileName = FindFileName(moduleName);
	nameLength = strlen(fileName);
	if (nameLength == 0) {
		SetModuleError("module name has no file name");
		return FALSE;
	}

	for (suffixIndex = 0;
	     suffixIndex < sizeof(NativeRelSuffixes) / sizeof(NativeRelSuffixes[0]);
	     ++suffixIndex) {
		size_t suffixLength = strlen(NativeRelSuffixes[suffixIndex]);
		if (nameLength >= suffixLength &&
		    strcmp(fileName + nameLength - suffixLength, NativeRelSuffixes[suffixIndex]) == 0) {
			nameLength -= suffixLength;
			break;
		}
	}
	if (nameLength == 0) {
		SetModuleError("module name has no native base name");
		return FALSE;
	}

	extension = __OSHostModuleExtension();
	return FormatModulePath(
		path, pathSize, "%s/%.*s%s", executableDirectory, (int)nameLength, fileName, extension);
}

static BOOL ValidateDescriptorShape(const PorpoiseNativeModuleDescriptor* descriptor)
{
	if (descriptor == NULL) {
		SetModuleError("native module returned a null descriptor");
		return FALSE;
	}
	if (descriptor->abiVersion != PORPOISE_NATIVE_MODULE_ABI_VERSION) {
		SetModuleError(
			"native module uses ABI version %u, but the loader requires version %u",
			(unsigned int)descriptor->abiVersion,
			(unsigned int)PORPOISE_NATIVE_MODULE_ABI_VERSION);
		return FALSE;
	}
	if (descriptor->structSize < sizeof(PorpoiseNativeModuleDescriptor)) {
		SetModuleError("native module descriptor is too small");
		return FALSE;
	}
	if (descriptor->logicalName == NULL || descriptor->logicalName[0] == '\0') {
		SetModuleError("native module descriptor has no logical name");
		return FALSE;
	}
	return TRUE;
}

static BOOL ValidateDescriptor(
	const PorpoiseNativeModuleDescriptor* descriptor, const OSModuleInfo* module)
{
	u32 moduleId;

	if (!ValidateDescriptorShape(descriptor)) {
		return FALSE;
	}

	moduleId = OSReadBigEndian32(module);
	if (descriptor->moduleId != 0 && descriptor->moduleId != moduleId) {
		SetModuleError(
			"native module ID %u does not match REL module ID %u",
			(unsigned int)descriptor->moduleId,
			(unsigned int)moduleId);
		return FALSE;
	}
	return TRUE;
}

static const PorpoiseNativeModuleDescriptor* FindRegisteredNativeModule(
	const char* logicalName)
{
	int index;

	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		const PorpoiseNativeModuleDescriptor* descriptor = RegisteredNativeModules[index];
		if (descriptor != NULL && strcmp(descriptor->logicalName, logicalName) == 0) {
			return descriptor;
		}
	}
	return NULL;
}

static BOOL ValidateNoLinkedDescriptorConflict(
	const PorpoiseNativeModuleDescriptor* descriptor)
{
	int index;

	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		const PorpoiseNativeModuleDescriptor* linkedDescriptor;
		if (NativeModules[index].module == NULL) {
			continue;
		}
		linkedDescriptor = NativeModules[index].descriptor;
		if (strcmp(linkedDescriptor->logicalName, descriptor->logicalName) == 0) {
			SetModuleError(
				"native module logical name '%s' is already linked",
				descriptor->logicalName);
			return FALSE;
		}
		if (descriptor->moduleId != 0 && linkedDescriptor->moduleId == descriptor->moduleId) {
			SetModuleError(
				"native module ID %u is already linked",
				(unsigned int)descriptor->moduleId);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL PorpoiseRegisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor)
{
	int freeIndex = -1;
	int index;

	ClearModuleError();
	if (!ValidateDescriptorShape(descriptor) ||
	    !ValidateNoLinkedDescriptorConflict(descriptor)) {
		return FALSE;
	}

	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		const PorpoiseNativeModuleDescriptor* registered = RegisteredNativeModules[index];
		if (registered == NULL) {
			if (freeIndex < 0) {
				freeIndex = index;
			}
			continue;
		}
		if (strcmp(registered->logicalName, descriptor->logicalName) == 0) {
			SetModuleError(
				"native module logical name '%s' is already registered",
				descriptor->logicalName);
			return FALSE;
		}
		if (descriptor->moduleId != 0 && registered->moduleId == descriptor->moduleId) {
			SetModuleError(
				"native module ID %u is already registered",
				(unsigned int)descriptor->moduleId);
			return FALSE;
		}
	}

	if (freeIndex < 0) {
		SetModuleError("native module registry is full");
		return FALSE;
	}
	RegisteredNativeModules[freeIndex] = descriptor;
	return TRUE;
}

BOOL PorpoiseUnregisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor)
{
	int index;
	int registeredIndex = -1;

	ClearModuleError();
	if (descriptor == NULL) {
		SetModuleError("cannot unregister a null native module descriptor");
		return FALSE;
	}

	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (RegisteredNativeModules[index] == descriptor) {
			registeredIndex = index;
			break;
		}
	}
	if (registeredIndex < 0) {
		SetModuleError("native module descriptor is not registered");
		return FALSE;
	}

	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (NativeModules[index].module != NULL &&
		    NativeModules[index].descriptor == descriptor) {
			SetModuleError(
				"native module '%s' is still linked", descriptor->logicalName);
			return FALSE;
		}
	}

	RegisteredNativeModules[registeredIndex] = NULL;
	return TRUE;
}

void OSNotifyLink(void)
{
}

void OSNotifyUnlink(void)
{
}

void OSSetStringTable(const void* stringTable)
{
	__OSStringTable = stringTable;
}

const char* OSGetModuleLastError(void)
{
	return ModuleError;
}

u32 OSGetModuleBssSize(const OSModuleInfo* module)
{
	if (module == NULL) {
		return 0;
	}
	return OSReadBigEndian32((const u8*)module + OS_REL_BSS_SIZE_OFFSET);
}

BOOL OSLinkByName(const char* moduleName, OSModuleInfo* newModule, void* bss)
{
	char logicalName[OS_MODULE_PATH_SIZE];
	char path[OS_MODULE_PATH_SIZE];
	char loaderError[OS_MODULE_ERROR_SIZE];
	OSHostModuleHandle handle;
	PorpoiseGetNativeModuleProc query;
	const PorpoiseNativeModuleDescriptor* descriptor;
	OSLinkedNativeModule* slot;

	(void)bss;
	ClearModuleError();

	if (moduleName == NULL || moduleName[0] == '\0' || newModule == NULL) {
		SetModuleError("invalid module name or REL header");
		return FALSE;
	}
	if (FindLinkedModule(newModule) != NULL) {
		SetModuleError("module is already linked");
		return FALSE;
	}
	if (!GetLogicalModuleName(moduleName, logicalName, sizeof(logicalName))) {
		return FALSE;
	}

	slot = FindFreeModuleSlot();
	if (slot == NULL) {
		SetModuleError("native module table is full");
		return FALSE;
	}

	descriptor = FindRegisteredNativeModule(logicalName);
	if (descriptor != NULL) {
		if (!ValidateDescriptor(descriptor, newModule) ||
		    !ValidateNoLinkedDescriptorConflict(descriptor)) {
			return FALSE;
		}
		slot->module = newModule;
		slot->handle = NULL;
		slot->descriptor = descriptor;
		OSNotifyLink();
		return TRUE;
	}

	if (!BuildNativeModulePath(moduleName, path, sizeof(path))) {
		return FALSE;
	}

	loaderError[0] = '\0';
	handle = __OSHostModuleOpen(path, loaderError, sizeof(loaderError));
	if (handle == NULL) {
		SetModuleError("could not load native module '%s': %s", path, loaderError);
		return FALSE;
	}

	loaderError[0] = '\0';
	query = (PorpoiseGetNativeModuleProc)__OSHostModuleFindSymbol(
		handle, PORPOISE_NATIVE_MODULE_QUERY_SYMBOL, loaderError, sizeof(loaderError));
	if (query == NULL) {
		SetModuleError(
			"native module '%s' has no %s entry point: %s",
			path,
			PORPOISE_NATIVE_MODULE_QUERY_SYMBOL,
			loaderError);
		__OSHostModuleClose(handle);
		return FALSE;
	}

	descriptor = query();
	if (!ValidateDescriptor(descriptor, newModule) ||
	    !ValidateNoLinkedDescriptorConflict(descriptor)) {
		__OSHostModuleClose(handle);
		return FALSE;
	}

	slot->module = newModule;
	slot->handle = handle;
	slot->descriptor = descriptor;
	OSNotifyLink();
	return TRUE;
}

BOOL OSLink(OSModuleInfo* newModule, void* bss)
{
	(void)bss;
	ClearModuleError();
	if (newModule == NULL) {
		SetModuleError("invalid REL header");
		return FALSE;
	}

	SetModuleError("host builds require OSLinkByName to map a REL name to a native module");
	return FALSE;
}

BOOL OSLinkFixed(OSModuleInfo* newModule, void* bss)
{
	return OSLink(newModule, bss);
}

BOOL OSRunModuleProlog(OSModuleInfo* module)
{
	OSLinkedNativeModule* linked;
	ClearModuleError();
	linked = FindLinkedModule(module);
	if (linked == NULL) {
		SetModuleError("cannot run the prolog for an unlinked module");
		return FALSE;
	}
	if (linked->descriptor->prolog != NULL) {
		linked->descriptor->prolog();
	}
	return TRUE;
}

BOOL OSRunModuleEpilog(OSModuleInfo* module)
{
	OSLinkedNativeModule* linked;
	ClearModuleError();
	linked = FindLinkedModule(module);
	if (linked == NULL) {
		SetModuleError("cannot run the epilog for an unlinked module");
		return FALSE;
	}
	if (linked->descriptor->epilog != NULL) {
		linked->descriptor->epilog();
	}
	return TRUE;
}

void OSRunModuleUnresolved(OSModuleInfo* module)
{
	OSLinkedNativeModule* linked;
	ClearModuleError();
	linked = FindLinkedModule(module);
	if (linked == NULL) {
		SetModuleError("cannot run the unresolved handler for an unlinked module");
		return;
	}
	if (linked->descriptor->unresolved != NULL) {
		linked->descriptor->unresolved();
	}
}

BOOL OSUnlink(OSModuleInfo* oldModule)
{
	OSLinkedNativeModule* linked;
	ClearModuleError();
	linked = FindLinkedModule(oldModule);
	if (linked == NULL) {
		SetModuleError("cannot unlink a module that is not linked");
		return FALSE;
	}

	OSNotifyUnlink();
	if (linked->handle != NULL) {
		__OSHostModuleClose(linked->handle);
	}
	memset(linked, 0, sizeof(*linked));
	return TRUE;
}

OSModuleInfo* OSSearchModule(void* ptr, u32* section, u32* offset)
{
	int index;
	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (NativeModules[index].module == ptr) {
			if (section != NULL) {
				*section = 0;
			}
			if (offset != NULL) {
				*offset = 0;
			}
			return NativeModules[index].module;
		}
	}
	return NULL;
}

void __OSModuleInit(void)
{
	int index;
	for (index = 0; index < OS_NATIVE_MODULE_LIMIT; ++index) {
		if (NativeModules[index].handle != NULL) {
			__OSHostModuleClose(NativeModules[index].handle);
		}
	}
	memset(NativeModules, 0, sizeof(NativeModules));
	__OSModuleInfoList.head = NULL;
	__OSModuleInfoList.tail = NULL;
	__OSStringTable = NULL;
	ClearModuleError();
}

#else

/*
 * Keep the GameCube route separate from host dynamic loading. These are the
 * original SDK-facing entry points; OSLinkByName delegates to OSLink so a
 * complete PowerPC linker can be supplied without changing callers.
 */
#define OS_MODULE_LIST_ADDR  0x800030C8
#define OS_STRING_TABLE_ADDR 0x800030D0

extern OSModuleQueue __OSModuleInfoList AT_ADDRESS(OS_MODULE_LIST_ADDR);
extern const void* __OSStringTable AT_ADDRESS(OS_STRING_TABLE_ADDR);

static const char ModuleError[] = "native modules are unavailable on GameCube builds";

BOOL PorpoiseRegisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor)
{
	(void)descriptor;
	return FALSE;
}

BOOL PorpoiseUnregisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor)
{
	(void)descriptor;
	return FALSE;
}

void OSNotifyLink(void)
{
	TRAP_UNIMPLEMENTED;
}

void OSNotifyUnlink(void)
{
	TRAP_UNIMPLEMENTED;
}

void OSSetStringTable(const void* stringTable)
{
	__OSStringTable = stringTable;
}

void Relocate(void)
{
	TRAP_UNIMPLEMENTED;
}

BOOL OSLink(OSModuleInfo* newModule, void* bss)
{
	(void)newModule;
	(void)bss;
	TRAP_UNIMPLEMENTED;
	return FALSE;
}

BOOL OSLinkFixed(OSModuleInfo* newModule, void* bss)
{
	return OSLink(newModule, bss);
}

void Undo(void)
{
	TRAP_UNIMPLEMENTED;
}

BOOL OSUnlink(OSModuleInfo* oldModule)
{
	(void)oldModule;
	TRAP_UNIMPLEMENTED;
	return FALSE;
}

OSModuleInfo* OSSearchModule(void* ptr, u32* section, u32* offset)
{
	(void)ptr;
	(void)section;
	(void)offset;
	TRAP_UNIMPLEMENTED;
	return NULL;
}

BOOL OSLinkByName(const char* moduleName, OSModuleInfo* newModule, void* bss)
{
	(void)moduleName;
	return OSLink(newModule, bss);
}

u32 OSGetModuleBssSize(const OSModuleInfo* module)
{
	return module != NULL ? ((const OSModuleHeader*)module)->bssSize : 0;
}

BOOL OSRunModuleProlog(OSModuleInfo* module)
{
	(void)module;
	return FALSE;
}

BOOL OSRunModuleEpilog(OSModuleInfo* module)
{
	(void)module;
	return FALSE;
}

void OSRunModuleUnresolved(OSModuleInfo* module)
{
	(void)module;
}

const char* OSGetModuleLastError(void)
{
	return ModuleError;
}

void __OSModuleInit(void)
{
	__OSModuleInfoList.head = NULL;
	__OSModuleInfoList.tail = NULL;
	__OSStringTable = NULL;
}

#endif
