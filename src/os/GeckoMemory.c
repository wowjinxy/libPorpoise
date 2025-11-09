/*---------------------------------------------------------------------------*
  GeckoMemory.c - Full Memory Emulation for GameCube/Wii
  
  Provides complete memory layout emulation including:
  - MEM1/MEM2 with address translation
  - Locked cache scratchpad
  - Big-endian byte order
  - Virtual address mirrors (cached/uncached)
 *---------------------------------------------------------------------------*/

#include <dolphin/gecko_memory.h>
#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORPOISE_USE_GECKO_MEMORY
GeckoMemory* g_geckoMemory = NULL;
#endif

void GeckoMemoryInit(GeckoMemory* mem, BOOL is_wii) {
    if (!mem) return;
    
    memset(mem->mem1, 0, GECKO_MEM1_SIZE);
    mem->is_wii = is_wii;
    mem->mem2_enabled = FALSE;
    mem->aram_enabled = FALSE;
    mem->locked_cache_enabled = FALSE;
    
    /* Allocate MEM2 for Wii */
    if (is_wii) {
        mem->mem2 = (u8*)malloc(GECKO_MEM2_SIZE);
        if (mem->mem2) {
            memset(mem->mem2, 0, GECKO_MEM2_SIZE);
            mem->mem2_enabled = TRUE;
        }
    } else {
        mem->mem2 = NULL;
    }
    
    mem->aram = NULL;
    memset(mem->locked_cache, 0, GECKO_LOCKED_CACHE_SIZE);
}

void GeckoMemoryFree(GeckoMemory* mem) {
    if (!mem) return;
    
    if (mem->mem2) {
        free(mem->mem2);
        mem->mem2 = NULL;
        mem->mem2_enabled = FALSE;
    }
    
    if (mem->aram) {
        free(mem->aram);
        mem->aram = NULL;
        mem->aram_enabled = FALSE;
    }
}

void GeckoMemoryAllocARAM(GeckoMemory* mem) {
    if (!mem || mem->aram_enabled) return;
    
    mem->aram = (u8*)malloc(GECKO_ARAM_SIZE);
    if (mem->aram) {
        memset(mem->aram, 0, GECKO_ARAM_SIZE);
        mem->aram_enabled = TRUE;
    }
}

u32 GeckoTranslateAddress(u32 vaddr) {
    /* Cached mirror of MEM1 (0x80000000 - 0x817FFFFF) */
    if (vaddr >= 0x80000000 && vaddr < 0x81800000) {
        return vaddr & 0x01FFFFFF;
    }
    
    /* Uncached mirror of MEM1 (0xC0000000 - 0xC17FFFFF) */
    if (vaddr >= 0xC0000000 && vaddr < 0xC1800000) {
        return vaddr & 0x01FFFFFF;
    }
    
    /* Cached mirror of MEM2 (0x90000000 - 0x93FFFFFF) */
    if (vaddr >= 0x90000000 && vaddr < 0x94000000) {
        return vaddr | 0x10000000; // Mark as MEM2
    }
    
    /* Uncached mirror of MEM2 (0xD0000000 - 0xD3FFFFFF) */
    if (vaddr >= 0xD0000000 && vaddr < 0xD4000000) {
        return vaddr | 0x10000000; // Mark as MEM2
    }
    
    /* Already physical or unmapped */
    return vaddr;
}

BOOL GeckoIsLockedCacheAddress(u32 addr) {
    return (addr >= GECKO_LOCKED_CACHE_BASE && 
            addr < GECKO_LOCKED_CACHE_BASE + GECKO_LOCKED_CACHE_SIZE);
}

u8 GeckoRead8(const GeckoMemory* mem, u32 vaddr) {
    u32 paddr = GeckoTranslateAddress(vaddr);
    
    /* Locked cache */
    if (GeckoIsLockedCacheAddress(vaddr)) {
        u32 offset = vaddr - GECKO_LOCKED_CACHE_BASE;
        return mem->locked_cache[offset];
    }
    
    /* MEM1 */
    if (paddr < GECKO_MEM1_SIZE) {
        return mem->mem1[paddr];
    }
    
    /* MEM2 (Wii) */
    if (mem->mem2_enabled && (paddr & 0x10000000)) {
        u32 offset = paddr & 0x03FFFFFF;
        if (offset < GECKO_MEM2_SIZE) {
            return mem->mem2[offset];
        }
    }
    
    return 0xFF;
}

void GeckoWrite8(GeckoMemory* mem, u32 vaddr, u8 value) {
    u32 paddr = GeckoTranslateAddress(vaddr);
    
    /* Locked cache */
    if (GeckoIsLockedCacheAddress(vaddr)) {
        u32 offset = vaddr - GECKO_LOCKED_CACHE_BASE;
        mem->locked_cache[offset] = value;
        return;
    }
    
    /* MEM1 */
    if (paddr < GECKO_MEM1_SIZE) {
        mem->mem1[paddr] = value;
        return;
    }
    
    /* MEM2 (Wii) */
    if (mem->mem2_enabled && (paddr & 0x10000000)) {
        u32 offset = paddr & 0x03FFFFFF;
        if (offset < GECKO_MEM2_SIZE) {
            mem->mem2[offset] = value;
        }
    }
}

u16 GeckoRead16(const GeckoMemory* mem, u32 vaddr) {
    u8 b0 = GeckoRead8(mem, vaddr);
    u8 b1 = GeckoRead8(mem, vaddr + 1);
    return ((u16)b0 << 8) | b1;
}

void GeckoWrite16(GeckoMemory* mem, u32 vaddr, u16 value) {
    GeckoWrite8(mem, vaddr, (value >> 8) & 0xFF);
    GeckoWrite8(mem, vaddr + 1, value & 0xFF);
}

u32 GeckoRead32(const GeckoMemory* mem, u32 vaddr) {
    u8 b0 = GeckoRead8(mem, vaddr);
    u8 b1 = GeckoRead8(mem, vaddr + 1);
    u8 b2 = GeckoRead8(mem, vaddr + 2);
    u8 b3 = GeckoRead8(mem, vaddr + 3);
    return ((u32)b0 << 24) | ((u32)b1 << 16) | ((u32)b2 << 8) | b3;
}

void GeckoWrite32(GeckoMemory* mem, u32 vaddr, u32 value) {
    GeckoWrite8(mem, vaddr, (value >> 24) & 0xFF);
    GeckoWrite8(mem, vaddr + 1, (value >> 16) & 0xFF);
    GeckoWrite8(mem, vaddr + 2, (value >> 8) & 0xFF);
    GeckoWrite8(mem, vaddr + 3, value & 0xFF);
}

void* GeckoGetPointer(GeckoMemory* mem, u32 vaddr) {
    u32 paddr = GeckoTranslateAddress(vaddr);
    
    /* Locked cache */
    if (GeckoIsLockedCacheAddress(vaddr)) {
        u32 offset = vaddr - GECKO_LOCKED_CACHE_BASE;
        return &mem->locked_cache[offset];
    }
    
    /* MEM1 */
    if (paddr < GECKO_MEM1_SIZE) {
        return &mem->mem1[paddr];
    }
    
    /* MEM2 (Wii) */
    if (mem->mem2_enabled && (paddr & 0x10000000)) {
        u32 offset = paddr & 0x03FFFFFF;
        if (offset < GECKO_MEM2_SIZE) {
            return &mem->mem2[offset];
        }
    }
    
    return NULL;
}

