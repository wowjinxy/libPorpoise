/*---------------------------------------------------------------------------*
  OSCache.c - Cache Management Operations
  
  On GC/Wii hardware:
  - PowerPC has manual cache control (L1 data, L1 instruction, L2)
  - Games must explicitly flush/invalidate caches before DMA
  - Instruction cache must be invalidated after modifying code
  - "Locked cache" is special 16KB scratchpad memory with DMA
  
  On PC (x86/x64/ARM):
  - CPUs handle cache coherency automatically
  - No manual cache control needed
  - OS/compiler handle instruction cache
  - We simulate the API but most operations are no-ops
  
  TWO MODES:
  1. Simple mode (default): Most operations are no-ops
  2. Full emulation mode (PORPOISE_USE_GECKO_MEMORY): Implements locked cache
  
  Key functions that matter on PC:
  - DCZeroRange: Fast memset (we use actual memset)
  - DCFlushRange: Important if we later add real DMA
  - ICInvalidateRange: Important for JIT or code modification
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <string.h>

#ifdef PORPOISE_USE_GECKO_MEMORY
#include <dolphin/gecko_memory.h>
#endif

#define CACHE_LINE_SIZE 32

/* Locked cache state (for full emulation mode) */
static BOOL s_locked_cache_enabled = FALSE;

/*---------------------------------------------------------------------------*
  L1 Data Cache Operations
  
  On original hardware, these use PowerPC dcbf/dcbi/dcbz/dcbst instructions.
  On PC, most are no-ops since x86/ARM handle cache coherency automatically.
 *---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
  Name:         DCInvalidateRange

  Description:  Invalidates data cache for a memory range. On original hardware,
                this executes 'dcbi' (data cache block invalidate) on each
                32-byte cache line in the range.
                
                On PC: No-op. Modern CPUs handle cache coherency automatically.

  Arguments:    addr   - Address (will be aligned down to 32-byte boundary)
                nBytes - Size (will be aligned up to 32-byte boundary)
 *---------------------------------------------------------------------------*/
void DCInvalidateRange(void* addr, u32 nBytes) {
    // No-op on PC - cache coherency handled by hardware
    (void)addr;
    (void)nBytes;
}

/*---------------------------------------------------------------------------*
  Name:         DCFlushRange

  Description:  Flushes data cache to main memory. On original hardware,
                executes 'dcbf' (data cache block flush) - writes modified
                cache lines to memory and invalidates them.
                
                Critical before DMA operations! Games call this before GPU
                reads data or before DVD writes.
                
                On PC: No-op for now. If we implement real DMA later, we might
                need memory barriers here.
 *---------------------------------------------------------------------------*/
void DCFlushRange(void* addr, u32 nBytes) {
    // No-op on PC - but kept for API compatibility
    // If we add real DMA: insert memory barrier/fence here
    (void)addr;
    (void)nBytes;
}

/*---------------------------------------------------------------------------*
  Name:         DCStoreRange

  Description:  Stores data cache to main memory without invalidating.
                Like DCFlushRange but cache lines stay in cache.
                
                On PC: No-op.
 *---------------------------------------------------------------------------*/
void DCStoreRange(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}

/*---------------------------------------------------------------------------*
  Name:         DCFlushRangeNoSync / DCStoreRangeNoSync

  Description:  Same as above but without sync barrier (for performance when
                batching multiple operations).
                
                On PC: No-op.
 *---------------------------------------------------------------------------*/
void DCFlushRangeNoSync(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}

void DCStoreRangeNoSync(void* addr, u32 nBytes) {
    (void)addr;
    (void)nBytes;
}

/*---------------------------------------------------------------------------*
  Name:         DCZeroRange

  Description:  Zeros a memory range using cache operations. On original
                hardware, executes 'dcbz' (data cache block zero) which
                quickly zeros 32-byte cache lines.
                
                This is faster than memset on PowerPC!
                
                On PC: Use standard memset (compiler may optimize to SSE/AVX).

  Arguments:    addr   - Address (aligned down to 32 bytes)
                nBytes - Size (aligned up to 32 bytes)
 *---------------------------------------------------------------------------*/
void DCZeroRange(void* addr, u32 nBytes) {
    if (addr && nBytes > 0) {
        // Align down to 32-byte boundary
        u32 alignedAddr = ((u32)addr) & ~31;
        // Align up size
        u32 alignedSize = (nBytes + 31) & ~31;
        
        memset((void*)alignedAddr, 0, alignedSize);
    }
}

/*---------------------------------------------------------------------------*
  Name:         DCTouchRange

  Description:  Prefetches data into cache. On original hardware, executes
                'dcbt' (data cache block touch) to load data into L1 cache
                without blocking.
                
                On PC: No-op. Modern CPUs have hardware prefetchers.
 *---------------------------------------------------------------------------*/
void DCTouchRange(void* addr, u32 nBytes) {
    // No-op - modern CPUs prefetch automatically
    (void)addr;
    (void)nBytes;
}

/*---------------------------------------------------------------------------*
  Low-Level Data Cache Operations (single cache line)
 *---------------------------------------------------------------------------*/

void DCFlashInvalidate(void) {
    // Invalidate entire data cache - no-op on PC
}

void DCEnable(void) {
    // Enable data cache - always on for modern CPUs
}

void DCDisable(void) {
    // Disable data cache - not possible/needed on modern CPUs
}

void DCFreeze(void) {
    // Lock cache contents - no equivalent on PC
}

void DCUnfreeze(void) {
    // Unlock cache - no equivalent on PC
}

void DCTouchLoad(void* addr) {
    // Prefetch single cache line - no-op
    (void)addr;
}

void DCBlockZero(void* addr) {
    // Zero single 32-byte block
    if (addr) {
        memset(addr, 0, CACHE_LINE_SIZE);
    }
}

void DCBlockStore(void* addr) {
    // Store single cache line - no-op
    (void)addr;
}

void DCBlockFlush(void* addr) {
    // Flush single cache line - no-op
    (void)addr;
}

void DCBlockInvalidate(void* addr) {
    // Invalidate single cache line - no-op
    (void)addr;
}

/*---------------------------------------------------------------------------*
  L1 Instruction Cache Operations
  
  On original hardware, these manage the instruction cache. Critical when
  modifying code at runtime (JIT compilers, loading overlays, etc.).
  
  On PC: Mostly no-op, but ICInvalidateRange might be needed for JIT.
 *---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
  Name:         ICInvalidateRange

  Description:  Invalidates instruction cache for a range. On original hardware,
                executes 'icbi' (instruction cache block invalidate).
                
                CRITICAL for: JIT compilation, loading code overlays, 
                self-modifying code
                
                On PC: We might need platform-specific code flushing for JIT:
                - Windows: FlushInstructionCache()
                - Linux: __builtin___clear_cache()
                - For now: no-op (most games don't modify code)
 *---------------------------------------------------------------------------*/
void ICInvalidateRange(void* addr, u32 nBytes) {
    // On PC: Would need platform-specific instruction cache flush
    // For now: no-op (games rarely modify code at runtime)
    
#ifdef _WIN32
    // If we implement JIT: FlushInstructionCache(GetCurrentProcess(), addr, nBytes);
#elif defined(__GNUC__)
    // If we implement JIT: __builtin___clear_cache((char*)addr, (char*)addr + nBytes);
#endif
    
    (void)addr;
    (void)nBytes;
}

void ICSync(void) {
    // Sync instruction cache - no-op on PC
}

void ICFlashInvalidate(void) {
    // Invalidate entire instruction cache - no-op on PC
}

void ICEnable(void) {
    // Enable instruction cache - always on
}

void ICDisable(void) {
    // Disable instruction cache - not possible
}

void ICFreeze(void) {
    // Lock instruction cache - no equivalent
}

void ICUnfreeze(void) {
    // Unlock instruction cache - no equivalent
}

void ICBlockInvalidate(void* addr) {
    // Invalidate single cache line - no-op
    (void)addr;
}

/*---------------------------------------------------------------------------*
  L2 Cache Operations
  
  On GC/Wii: 256KB unified L2 cache with manual control
  On PC: Modern CPUs have multi-level caches (L2, L3) managed automatically
 *---------------------------------------------------------------------------*/

void L2Enable(void) {
    // L2 cache always enabled on modern CPUs
}

void L2Disable(void) {
    // Cannot disable L2 on modern CPUs
}

void L2GlobalInvalidate(void) {
    // Invalidate entire L2 - not needed on PC
}

void L2SetDataOnly(BOOL dataOnly) {
    // Configure L2 for data-only mode - not applicable on PC
    (void)dataOnly;
}

void L2SetWriteThrough(BOOL writeThrough) {
    // Configure L2 write policy - not applicable on PC
    (void)writeThrough;
}

/*---------------------------------------------------------------------------*
  Locked Cache (LC) Operations
  
  GameCube/Wii specific feature: "Locked Cache" is 16KB of L1 cache that
  can be used as fast scratchpad memory with DMA capabilities.
  
  Memory range: 0xE0000000 - 0xE0003FFF (16 KB)
  
  Uses:
  - Ultra-fast temporary buffers
  - DMA source/destination for GPU
  - Audio buffer processing
  
  On PC:
  - Simple mode: No-ops (games need modification)
  - Full emulation mode: Uses 16KB buffer from GeckoMemory
 *---------------------------------------------------------------------------*/

void LCEnable(void) {
    s_locked_cache_enabled = TRUE;
    
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory) {
        g_geckoMemory->locked_cache_enabled = TRUE;
        OSReport("Locked cache enabled (16 KB scratchpad at 0xE0000000)\n");
    }
#endif
}

void LCDisable(void) {
    s_locked_cache_enabled = FALSE;
    
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory) {
        g_geckoMemory->locked_cache_enabled = FALSE;
    }
#endif
}

void LCLoadBlocks(void* destTag, void* srcAddr, u32 numBlocks) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        /* DMA from main memory to locked cache */
        u32 destAddr = (u32)destTag;
        if (GeckoIsLockedCacheAddress(destAddr)) {
            u32 offset = destAddr - GECKO_LOCKED_CACHE_BASE;
            u32 size = numBlocks * 32;
            
            if (offset + size <= GECKO_LOCKED_CACHE_SIZE) {
                /* Get source pointer */
                void* src = GeckoGetPointer(g_geckoMemory, (u32)srcAddr);
                if (src) {
                    memcpy(&g_geckoMemory->locked_cache[offset], src, size);
                }
            }
        }
    }
#else
    (void)destTag;
    (void)srcAddr;
    (void)numBlocks;
#endif
}

void LCStoreBlocks(void* destAddr, void* srcTag, u32 numBlocks) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        /* DMA from locked cache to main memory */
        u32 srcAddr = (u32)srcTag;
        if (GeckoIsLockedCacheAddress(srcAddr)) {
            u32 offset = srcAddr - GECKO_LOCKED_CACHE_BASE;
            u32 size = numBlocks * 32;
            
            if (offset + size <= GECKO_LOCKED_CACHE_SIZE) {
                /* Get destination pointer */
                void* dest = GeckoGetPointer(g_geckoMemory, (u32)destAddr);
                if (dest) {
                    memcpy(dest, &g_geckoMemory->locked_cache[offset], size);
                }
            }
        }
    }
#else
    (void)destAddr;
    (void)srcTag;
    (void)numBlocks;
#endif
}

u32 LCLoadData(void* destAddr, void* srcAddr, u32 nBytes) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        u32 numBlocks = (nBytes + 31) / 32;
        u32 numTransactions = (numBlocks + 127) / 128; // Max 128 blocks per transaction
        
        /* Batch DMA operations (max 128 blocks at a time) */
        while (numBlocks > 0) {
            u32 blocksThisTime = (numBlocks < 128) ? numBlocks : 128;
            LCLoadBlocks(destAddr, srcAddr, blocksThisTime);
            
            numBlocks -= blocksThisTime;
            destAddr = (void*)((u32)destAddr + blocksThisTime * 32);
            srcAddr = (void*)((u32)srcAddr + blocksThisTime * 32);
        }
        
        return numTransactions;
    }
#else
    (void)destAddr;
    (void)srcAddr;
    (void)nBytes;
#endif
    return 0;
}

u32 LCStoreData(void* destAddr, void* srcAddr, u32 nBytes) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        u32 numBlocks = (nBytes + 31) / 32;
        u32 numTransactions = (numBlocks + 127) / 128;
        
        while (numBlocks > 0) {
            u32 blocksThisTime = (numBlocks < 128) ? numBlocks : 128;
            LCStoreBlocks(destAddr, srcAddr, blocksThisTime);
            
            numBlocks -= blocksThisTime;
            destAddr = (void*)((u32)destAddr + blocksThisTime * 32);
            srcAddr = (void*)((u32)srcAddr + blocksThisTime * 32);
        }
        
        return numTransactions;
    }
#else
    (void)destAddr;
    (void)srcAddr;
    (void)nBytes;
#endif
    return 0;
}

u32 LCQueueLength(void) {
    /* No DMA queue on PC - all transfers are instant */
    return 0;
}

void LCQueueWait(u32 len) {
    /* Instant on PC */
    (void)len;
}

void LCFlushQueue(void) {
    /* Instant on PC */
}

void LCAlloc(void* addr, u32 nBytes) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory) {
        /* Just enable locked cache - memory already exists in GeckoMemory */
        if (!s_locked_cache_enabled) {
            LCEnable();
        }
        
        /* Invalidate the region to ensure no stale cache data */
        u32 destAddr = (u32)addr;
        if (GeckoIsLockedCacheAddress(destAddr)) {
            u32 offset = destAddr - GECKO_LOCKED_CACHE_BASE;
            if (offset + nBytes <= GECKO_LOCKED_CACHE_SIZE) {
                memset(&g_geckoMemory->locked_cache[offset], 0, nBytes);
            }
        }
    }
#else
    (void)addr;
    (void)nBytes;
#endif
}

void LCAllocNoInvalidate(void* addr, u32 nBytes) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory) {
        /* Same as LCAlloc but don't clear the buffer */
        if (!s_locked_cache_enabled) {
            LCEnable();
        }
    }
#else
    (void)addr;
    (void)nBytes;
#endif
}

void LCAllocOneTag(BOOL invalidate, void* tag) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        u32 addr = (u32)tag;
        if (GeckoIsLockedCacheAddress(addr)) {
            if (invalidate) {
                u32 offset = addr - GECKO_LOCKED_CACHE_BASE;
                memset(&g_geckoMemory->locked_cache[offset], 0, 32);
            }
        }
    }
#else
    (void)invalidate;
    (void)tag;
#endif
}

void LCAllocTags(BOOL invalidate, void* startTag, u32 numBlocks) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    if (g_geckoMemory && s_locked_cache_enabled) {
        u32 addr = (u32)startTag;
        if (GeckoIsLockedCacheAddress(addr)) {
            if (invalidate) {
                u32 offset = addr - GECKO_LOCKED_CACHE_BASE;
                u32 size = numBlocks * 32;
                if (offset + size <= GECKO_LOCKED_CACHE_SIZE) {
                    memset(&g_geckoMemory->locked_cache[offset], 0, size);
                }
            }
        }
    }
#else
    (void)invalidate;
    (void)startTag;
    (void)numBlocks;
#endif
}
