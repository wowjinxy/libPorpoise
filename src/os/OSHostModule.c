#include "OSHostModule.h"

#include <stdio.h>
#include <string.h>

#if defined(LIBPORPOISE_BUILD_WIN64)
#include <windows.h>
#elif defined(LIBPORPOISE_BUILD_LINUX)
#include <dlfcn.h>
#include <unistd.h>
#endif

static void CopyError(char* destination, size_t destinationSize, const char* message)
{
	if (destination == NULL || destinationSize == 0) {
		return;
	}

	if (message == NULL || message[0] == '\0') {
		message = "unknown dynamic-loader error";
	}

	snprintf(destination, destinationSize, "%s", message);
}

#if defined(LIBPORPOISE_BUILD_WIN64)

static void CopyWindowsError(char* destination, size_t destinationSize, DWORD code)
{
	char message[512];
	DWORD length = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		message,
		(DWORD)sizeof(message),
		NULL);

	if (length == 0) {
		snprintf(message, sizeof(message), "Windows loader error %lu", (unsigned long)code);
	} else {
		while (length > 0 && (message[length - 1] == '\r' || message[length - 1] == '\n')) {
			message[--length] = '\0';
		}
	}

	CopyError(destination, destinationSize, message);
}

OSHostModuleHandle __OSHostModuleOpen(const char* path, char* error, size_t errorSize)
{
	wchar_t widePath[1024];
	int converted;
	HMODULE module;

	if (path == NULL) {
		CopyError(error, errorSize, "native module path is null");
		return NULL;
	}

	converted = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		path,
		-1,
		widePath,
		(int)(sizeof(widePath) / sizeof(widePath[0])));
	if (converted == 0) {
		CopyWindowsError(error, errorSize, GetLastError());
		return NULL;
	}

	module = LoadLibraryW(widePath);
	if (module == NULL) {
		CopyWindowsError(error, errorSize, GetLastError());
	}
	return (OSHostModuleHandle)module;
}

void* __OSHostModuleFindSymbol(
	OSHostModuleHandle module, const char* symbol, char* error, size_t errorSize)
{
	FARPROC address;

	if (module == NULL || symbol == NULL) {
		CopyError(error, errorSize, "invalid module handle or symbol name");
		return NULL;
	}

	SetLastError(ERROR_SUCCESS);
	address = GetProcAddress((HMODULE)module, symbol);
	if (address == NULL) {
		DWORD code = GetLastError();
		if (code == ERROR_SUCCESS) {
			CopyError(error, errorSize, "native module symbol was not found");
		} else {
			CopyWindowsError(error, errorSize, code);
		}
	}
	return (void*)address;
}

void __OSHostModuleClose(OSHostModuleHandle module)
{
	if (module != NULL) {
		FreeLibrary((HMODULE)module);
	}
}

const char* __OSHostModuleExtension(void)
{
	return ".dll";
}

int __OSHostModuleGetExecutableDirectory(
	char* directory, size_t directorySize, char* error, size_t errorSize)
{
	wchar_t widePath[1024];
	DWORD length;
	int converted;
	char* separator;

	if (directory == NULL || directorySize == 0) {
		CopyError(error, errorSize, "executable-directory buffer is invalid");
		return 0;
	}

	SetLastError(ERROR_SUCCESS);
	length = GetModuleFileNameW(NULL, widePath, (DWORD)(sizeof(widePath) / sizeof(widePath[0])));
	if (length == 0 || length >= sizeof(widePath) / sizeof(widePath[0])) {
		DWORD code = GetLastError();
		if (code == ERROR_SUCCESS) {
			CopyError(error, errorSize, "executable path is too long");
		} else {
			CopyWindowsError(error, errorSize, code);
		}
		return 0;
	}

	converted = WideCharToMultiByte(
		CP_UTF8, 0, widePath, -1, directory, (int)directorySize, NULL, NULL);
	if (converted == 0) {
		CopyWindowsError(error, errorSize, GetLastError());
		return 0;
	}

	separator = strrchr(directory, '\\');
	if (separator == NULL) {
		separator = strrchr(directory, '/');
	}
	if (separator == NULL) {
		CopyError(error, errorSize, "executable path has no directory");
		return 0;
	}
	*separator = '\0';
	return 1;
}

#elif defined(LIBPORPOISE_BUILD_LINUX)

OSHostModuleHandle __OSHostModuleOpen(const char* path, char* error, size_t errorSize)
{
	void* module;

	if (path == NULL) {
		CopyError(error, errorSize, "native module path is null");
		return NULL;
	}

	dlerror();
	module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (module == NULL) {
		CopyError(error, errorSize, dlerror());
	}
	return module;
}

void* __OSHostModuleFindSymbol(
	OSHostModuleHandle module, const char* symbol, char* error, size_t errorSize)
{
	void* address;
	const char* loaderError;

	if (module == NULL || symbol == NULL) {
		CopyError(error, errorSize, "invalid module handle or symbol name");
		return NULL;
	}

	dlerror();
	address = dlsym(module, symbol);
	loaderError = dlerror();
	if (loaderError != NULL) {
		CopyError(error, errorSize, loaderError);
		return NULL;
	}
	return address;
}

void __OSHostModuleClose(OSHostModuleHandle module)
{
	if (module != NULL) {
		dlclose(module);
	}
}

const char* __OSHostModuleExtension(void)
{
	return ".so";
}

int __OSHostModuleGetExecutableDirectory(
	char* directory, size_t directorySize, char* error, size_t errorSize)
{
	ssize_t length;
	char* separator;

	if (directory == NULL || directorySize < 2) {
		CopyError(error, errorSize, "executable-directory buffer is too small");
		return 0;
	}

	length = readlink("/proc/self/exe", directory, directorySize - 1);
	if (length < 0) {
		CopyError(error, errorSize, "could not read /proc/self/exe");
		return 0;
	}
	if ((size_t)length >= directorySize - 1) {
		CopyError(error, errorSize, "executable path is too long");
		return 0;
	}
	directory[length] = '\0';

	separator = strrchr(directory, '/');
	if (separator == NULL) {
		CopyError(error, errorSize, "executable path has no directory");
		return 0;
	}
	*separator = '\0';
	return 1;
}

#else

OSHostModuleHandle __OSHostModuleOpen(const char* path, char* error, size_t errorSize)
{
	(void)path;
	CopyError(error, errorSize, "native modules are unavailable on this platform");
	return NULL;
}

void* __OSHostModuleFindSymbol(
	OSHostModuleHandle module, const char* symbol, char* error, size_t errorSize)
{
	(void)module;
	(void)symbol;
	CopyError(error, errorSize, "native modules are unavailable on this platform");
	return NULL;
}

void __OSHostModuleClose(OSHostModuleHandle module)
{
	(void)module;
}

const char* __OSHostModuleExtension(void)
{
	return "";
}

int __OSHostModuleGetExecutableDirectory(
	char* directory, size_t directorySize, char* error, size_t errorSize)
{
	(void)directory;
	(void)directorySize;
	CopyError(error, errorSize, "executable paths are unavailable on this platform");
	return 0;
}

#endif
