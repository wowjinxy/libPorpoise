#ifndef LIBPORPOISE_SIM_MEMORY_HPP
#define LIBPORPOISE_SIM_MEMORY_HPP

#include <dolphin/types.h>

#include "simulator/sim_memory.h"

namespace SIM::Memory {

void Init();

void * GetExeStart();
void * GetExeEnd();

u32 CreateMemoryHandle(void * address);
void * MemoryHandleToAddress(u32 handle);

}

#endif
