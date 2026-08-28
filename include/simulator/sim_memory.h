#ifndef SIM_MEMORY_H
#define SIM_MEMORY_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void * SIM_Memory_GetExeStart();
void * SIM_Memory_GetExeEnd();

u32 SIM_Memory_CreateMemoryHandle(void * address);
void * SIM_Memory_MemoryHandleToAddress(u32 handle);

#ifdef __cplusplus
}
#endif

#endif