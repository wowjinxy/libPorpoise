#include <dolphin/types.h>
#include <stdint.h>

#include "simulator/sim_memory.hpp"
#include <dolphin/os.h>
#include <unordered_map>
#include <vector>

#ifdef LIBPORPOISE_BUILD_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef LIBPORPOISE_BUILD_LINUX
#include <sys/mman.h>
#include <dlfcn.h>
#include <elf.h>
#endif

static void * sExeImageStart;
static void * sExeImageEnd;

static std::vector<void *> sMemoryHandles = {};
static std::unordered_map<void *, u32> sAddressesToMemoryHandles = {};

extern int main(int argc, char** argv);

namespace SIM::Memory {
void Init() {
#ifdef _WIN32
    {
        HMODULE exe = GetModuleHandle(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((char*)exe + dos->e_lfanew);
        sExeImageStart = (void*)(uintptr_t)exe;
        sExeImageEnd = sExeImageStart + nt->OptionalHeader.SizeOfImage;
    }
#else
    {
        Dl_info dl;
        if (dladdr((void*)main, &dl) && dl.dli_fbase) {
            sExeImageStart = (void*)(uintptr_t)dl.dli_fbase;
            Elf32_Ehdr* ehdr = (Elf32_Ehdr*)dl.dli_fbase;
            Elf32_Phdr* phdr = (Elf32_Phdr*)((char*)dl.dli_fbase + ehdr->e_phoff);
            uintptr_t max_end = 0;
            for (int i = 0; i < ehdr->e_phnum; i++) {
                if (phdr[i].p_type == PT_LOAD) {
                    uintptr_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
                    if (seg_end > max_end) max_end = seg_end;
                }
            }
            /* ET_EXEC: p_vaddr is absolute. ET_DYN (PIE): relative to load address. */
            if (ehdr->e_type == ET_DYN) {
                sExeImageEnd = (void*)((uintptr_t)sExeImageStart + max_end);
            } else {
                sExeImageEnd = (void*)max_end;
            }
        }
    }
#endif
}

void * GetExeStart() {
    return sExeImageStart;
}

void * GetExeEnd() {
    return sExeImageEnd;
}

u32 CreateMemoryHandle(void * address) {
    if(sAddressesToMemoryHandles.count(address) > 0) {
        return sAddressesToMemoryHandles[address];
    }

    if(sMemoryHandles.size() > 0xFFFFFFFF) {
        // At this point you would be using >4GB of memory on memory handles, so you probably have bigger problems
        OSReport("SIM::Memory warning: extremely high memory handle usage. Here be dragons!\n");
    }
    
    u32 handle = sMemoryHandles.size();
    sMemoryHandles.push_back(address);
    sAddressesToMemoryHandles[address] = handle;
    return handle;
}

void * MemoryHandleToAddress(u32 handle) {
    if(handle < sMemoryHandles.size()) {
        return sMemoryHandles[handle];
    } else {
        return nullptr;
    }
}

}

// C APIs
void * SIM_Memory_GetExeStart() {
    return SIM::Memory::GetExeStart();
}

void * SIM_Memory_GetExeEnd() {
    return SIM::Memory::GetExeEnd();
}

u32 SIM_Memory_CreateMemoryHandle(void * address) {
    return SIM::Memory::CreateMemoryHandle(address);
}

void * SIM_Memory_MemoryHandleToAddress(u32 handle) {
    return SIM::Memory::MemoryHandleToAddress(handle);
}