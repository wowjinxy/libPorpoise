#ifndef LIBPORPOISE_OS_HOST_MODULE_H
#define LIBPORPOISE_OS_HOST_MODULE_H

#include <stddef.h>

typedef void* OSHostModuleHandle;

OSHostModuleHandle __OSHostModuleOpen(const char* path, char* error, size_t errorSize);
void* __OSHostModuleFindSymbol(
	OSHostModuleHandle module, const char* symbol, char* error, size_t errorSize);
void __OSHostModuleClose(OSHostModuleHandle module);
const char* __OSHostModuleExtension(void);
int __OSHostModuleGetExecutableDirectory(
	char* directory, size_t directorySize, char* error, size_t errorSize);

#endif
