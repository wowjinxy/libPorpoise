/**
 * Locked Cache Example
 * 
 * Demonstrates the use of locked cache (scratchpad memory) functionality.
 * 
 * This example only works when built with PORPOISE_USE_GECKO_MEMORY=ON
 * Otherwise, the locked cache functions are no-ops.
 */

#include <dolphin/os.h>

#ifdef PORPOISE_USE_GECKO_MEMORY
#include <dolphin/gecko_memory.h>
#endif

#define LC_BUFFER_ADDR  0xE0000000  // Locked cache base address
#define LC_BUFFER_SIZE  4096        // 4 KB buffer

/* Example: Audio processing in locked cache */
void ProcessAudioInLockedCache(void) {
#ifdef PORPOISE_USE_GECKO_MEMORY
    OSReport("=== Locked Cache Audio Processing Example ===\n\n");
    
    /* Allocate source and destination buffers in main RAM */
    float* sourceBuffer = (float*)OSAlloc(LC_BUFFER_SIZE);
    float* destBuffer = (float*)OSAlloc(LC_BUFFER_SIZE);
    
    if (!sourceBuffer || !destBuffer) {
        OSReport("Failed to allocate buffers\n");
        return;
    }
    
    /* Fill source buffer with test audio data (sine wave) */
    int numSamples = LC_BUFFER_SIZE / sizeof(float);
    for (int i = 0; i < numSamples; i++) {
        sourceBuffer[i] = (float)i / numSamples; // Ramp 0.0 to 1.0
    }
    OSReport("Generated %d audio samples\n", numSamples);
    
    /* Enable locked cache */
    OSReport("Enabling locked cache...\n");
    LCEnable();
    
    /* Allocate region in locked cache */
    OSReport("Allocating %d bytes at 0x%08X\n", LC_BUFFER_SIZE, LC_BUFFER_ADDR);
    LCAlloc((void*)LC_BUFFER_ADDR, LC_BUFFER_SIZE);
    
    /* DMA from main RAM to locked cache (instant on PC) */
    OSReport("Loading data to locked cache (DMA)...\n");
    u32 transactions = LCLoadData((void*)LC_BUFFER_ADDR, sourceBuffer, LC_BUFFER_SIZE);
    OSReport("  %u DMA transactions\n", transactions);
    
    /* Process audio in locked cache (simulated - normally very fast) */
    OSReport("Processing audio in locked cache...\n");
    float* lcBuffer = (float*)LC_BUFFER_ADDR;
    
    // Access locked cache buffer directly
    if (g_geckoMemory) {
        void* lcPtr = GeckoGetPointer(g_geckoMemory, LC_BUFFER_ADDR);
        if (lcPtr) {
            float* samples = (float*)lcPtr;
            
            /* Apply simple gain (2x amplification) */
            for (int i = 0; i < numSamples; i++) {
                samples[i] *= 2.0f;
            }
            
            OSReport("  Processed %d samples\n", numSamples);
        }
    }
    
    /* DMA from locked cache back to main RAM */
    OSReport("Storing data from locked cache (DMA)...\n");
    transactions = LCStoreData(destBuffer, (void*)LC_BUFFER_ADDR, LC_BUFFER_SIZE);
    OSReport("  %u DMA transactions\n", transactions);
    
    /* Verify results */
    OSReport("Verifying results...\n");
    BOOL success = TRUE;
    for (int i = 0; i < 10; i++) {
        float expected = (sourceBuffer[i] * 2.0f);
        if (destBuffer[i] != expected) {
            OSReport("  Mismatch at sample %d: got %f, expected %f\n", 
                     i, destBuffer[i], expected);
            success = FALSE;
        }
    }
    
    if (success) {
        OSReport("  SUCCESS: All samples processed correctly!\n");
        OSReport("  First sample: %f -> %f (2x gain)\n", 
                 sourceBuffer[0], destBuffer[0]);
        OSReport("  Last sample:  %f -> %f (2x gain)\n", 
                 sourceBuffer[numSamples-1], destBuffer[numSamples-1]);
    }
    
    /* Cleanup */
    LCDisable();
    OSFree(sourceBuffer);
    OSFree(destBuffer);
    
    OSReport("\n=== Locked Cache Example Complete ===\n");
    
#else
    OSReport("=== Locked Cache Example ===\n");
    OSReport("This example requires full memory emulation.\n");
    OSReport("Rebuild with: cmake .. -DPORPOISE_USE_GECKO_MEMORY=ON\n");
#endif
}

int main(void) {
    OSReport("Locked Cache Example\n");
    OSReport("=====================\n\n");
    
    /* Initialize OS */
    OSInit();
    
#ifdef PORPOISE_USE_GECKO_MEMORY
    /* Initialize full memory emulation */
    OSReport("Initializing Gecko memory emulation...\n");
    GeckoMemory memory;
    GeckoMemoryInit(&memory, TRUE);  // TRUE = Wii mode
    g_geckoMemory = &memory;
    OSReport("  MEM1: %d MB\n", GECKO_MEM1_SIZE / (1024*1024));
    OSReport("  MEM2: %d MB\n", GECKO_MEM2_SIZE / (1024*1024));
    OSReport("  Locked Cache: %d KB\n\n", GECKO_LOCKED_CACHE_SIZE / 1024);
    
    /* Initialize heap system */
    void* arenaLo = OSGetMEM1ArenaLo();
    void* arenaHi = OSGetMEM1ArenaHi();
    arenaLo = OSInitAlloc(arenaLo, arenaHi, 1);
    
    OSHeapHandle heap = OSCreateHeap(arenaLo, arenaHi);
    OSSetCurrentHeap(heap);
    OSReport("Heap initialized\n\n");
    
    /* Run locked cache demo */
    ProcessAudioInLockedCache();
    
    /* Cleanup */
    GeckoMemoryFree(&memory);
#else
    ProcessAudioInLockedCache();
#endif
    
    return 0;
}

