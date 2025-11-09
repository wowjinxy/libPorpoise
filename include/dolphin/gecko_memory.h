/**
 * @file gecko_memory.h
 * @brief Gekko/Broadway CPU Memory Emulation for Porpoise SDK
 * 
 * Optional full memory emulation for games requiring precise hardware behavior.
 * Enable with PORPOISE_USE_GECKO_MEMORY define.
 */

#ifndef DOLPHIN_GECKO_MEMORY_H
#define DOLPHIN_GECKO_MEMORY_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory size constants */
#define GECKO_MEM1_SIZE         (24 * 1024 * 1024)  // 24 MB
#define GECKO_MEM2_SIZE         (64 * 1024 * 1024)  // 64 MB  
#define GECKO_ARAM_SIZE         (16 * 1024 * 1024)  // 16 MB
#define GECKO_LOCKED_CACHE_SIZE (16 * 1024)         // 16 KB

/* Base addresses for virtual memory mirrors */
#define GECKO_PHYS_BASE         0x00000000  // Physical
#define GECKO_CACHED_BASE       0x80000000  // Cached mirror
#define GECKO_UNCACHED_BASE     0xC0000000  // Uncached mirror
#define GECKO_LOCKED_CACHE_BASE 0xE0000000  // Locked cache

/* Hardware register base */
#define GECKO_HWREG_BASE        0xCC000000

/**
 * @brief Complete memory structure for GC/Wii emulation
 */
typedef struct GeckoMemory {
    u8  mem1[GECKO_MEM1_SIZE];          // Main RAM
    u8* mem2;                            // External RAM (Wii only)
    u8* aram;                            // Audio RAM
    u8  locked_cache[GECKO_LOCKED_CACHE_SIZE]; // Scratchpad
    
    BOOL is_wii;
    BOOL mem2_enabled;
    BOOL aram_enabled;
    BOOL locked_cache_enabled;
} GeckoMemory;

/* Initialize memory structure */
void  GeckoMemoryInit(GeckoMemory* mem, BOOL is_wii);
void  GeckoMemoryFree(GeckoMemory* mem);
void  GeckoMemoryAllocARAM(GeckoMemory* mem);

/* Address translation */
u32   GeckoTranslateAddress(u32 vaddr);
BOOL  GeckoIsLockedCacheAddress(u32 addr);

/* Memory access (big-endian) */
u8    GeckoRead8(const GeckoMemory* mem, u32 vaddr);
u16   GeckoRead16(const GeckoMemory* mem, u32 vaddr);
u32   GeckoRead32(const GeckoMemory* mem, u32 vaddr);
void  GeckoWrite8(GeckoMemory* mem, u32 vaddr, u8 value);
void  GeckoWrite16(GeckoMemory* mem, u32 vaddr, u16 value);
void  GeckoWrite32(GeckoMemory* mem, u32 vaddr, u32 value);
void* GeckoGetPointer(GeckoMemory* mem, u32 vaddr);

/* Global memory instance (when full emulation is enabled) */
#ifdef PORPOISE_USE_GECKO_MEMORY
extern GeckoMemory* g_geckoMemory;
#endif

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_GECKO_MEMORY_H */

