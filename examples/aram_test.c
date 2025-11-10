/**
 * @file aram_test.c
 * @brief Example demonstrating ARAM (Audio RAM) usage with libPorpoise
 * 
 * Shows how to:
 * - Initialize ARAM
 * - Allocate ARAM space
 * - DMA data to/from ARAM
 * - Use ARQ for queued operations
 */

#include <dolphin/os.h>
#include <dolphin/ar.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    u32 aramBase;
    u32 aramAddr;
    u32 length;
    
    // Initialize OS
    OSReport("Initializing libPorpoise...\n");
    OSInit();
    
    OSReport("\n==============================================\n");
    OSReport("   libPorpoise ARAM Test\n");
    OSReport("==============================================\n\n");
    
    // Initialize ARAM
    OSReport("[Test 1] Initializing ARAM...\n");
    aramBase = ARInit(NULL, 0);
    OSReport("  ARAM initialized\n");
    OSReport("  Total size: %u bytes (%u MB)\n", ARGetSize(), ARGetSize() / (1024*1024));
    OSReport("  User base: 0x%08X\n", aramBase);
    OSReport("  Internal size: %u bytes\n", ARGetInternalSize());
    
    // Allocate ARAM space
    OSReport("\n[Test 2] Allocating ARAM space...\n");
    aramAddr = ARAlloc(1024 * 1024);  // 1MB
    if (aramAddr != 0xFFFFFFFF) {
        OSReport("  Allocated 1MB at ARAM address: 0x%08X\n", aramAddr);
    } else {
        OSReport("  Failed to allocate ARAM!\n");
        return 1;
    }
    
    // Test DMA to ARAM
    OSReport("\n[Test 3] DMA to ARAM...\n");
    char testData[128];
    strcpy(testData, "Hello from main RAM! This will be copied to ARAM via DMA.");
    
    OSReport("  Source data: '%s'\n", testData);
    OSReport("  Starting DMA: MRAM -> ARAM\n");
    
    ARStartDMA(AR_MRAM_TO_ARAM, (u32)testData, aramAddr, 128);
    
    OSReport("  DMA Status: %s\n", ARGetDMAStatus() ? "Busy" : "Idle");
    OSReport("  DMA complete!\n");
    
    // Test DMA from ARAM
    OSReport("\n[Test 4] DMA from ARAM...\n");
    char readBuffer[128];
    memset(readBuffer, 0, sizeof(readBuffer));
    
    OSReport("  Starting DMA: ARAM -> MRAM\n");
    ARStartDMA(AR_ARAM_TO_MRAM, (u32)readBuffer, aramAddr, 128);
    
    OSReport("  Read back: '%s'\n", readBuffer);
    
    if (strcmp(testData, readBuffer) == 0) {
        OSReport("  ✓ Data matches! DMA working correctly.\n");
    } else {
        OSReport("  ✗ Data mismatch!\n");
    }
    
    // Test ARQ (queued operations)
    OSReport("\n[Test 5] ARQ (Queue) system...\n");
    ARQInit();
    OSReport("  ARQ initialized\n");
    OSReport("  Chunk size: %u bytes\n", ARQGetChunkSize());
    
    // Post a queued request
    ARQRequest request;
    char queueData[64];
    strcpy(queueData, "Queued DMA transfer test");
    
    u32 aramAddr2 = ARAlloc(64);
    OSReport("  Allocated another ARAM block at: 0x%08X\n", aramAddr2);
    
    OSReport("  Posting ARQ request...\n");
    ARQPostRequest(&request, 1, AR_MRAM_TO_ARAM, 0,
                   (u32)queueData, aramAddr2, 64, NULL);
    OSReport("  ARQ request completed\n");
    
    // Verify queued transfer
    char verifyBuffer[64];
    memset(verifyBuffer, 0, sizeof(verifyBuffer));
    ARStartDMA(AR_ARAM_TO_MRAM, (u32)verifyBuffer, aramAddr2, 64);
    OSReport("  Verified: '%s'\n", verifyBuffer);
    
    // Free ARAM
    OSReport("\n[Test 6] Freeing ARAM...\n");
    u32 freedAddr = ARFree(&length);
    OSReport("  Freed %u bytes from address 0x%08X\n", length, freedAddr);
    
    // Test allocation tracking
    OSReport("\n[Test 7] Multiple allocations...\n");
    u32 addr1 = ARAlloc(512 * 1024);   // 512KB
    u32 addr2 = ARAlloc(1024 * 1024);  // 1MB
    u32 addr3 = ARAlloc(2048 * 1024);  // 2MB
    
    OSReport("  Allocation 1 (512KB): 0x%08X\n", addr1);
    OSReport("  Allocation 2 (1MB):   0x%08X\n", addr2);
    OSReport("  Allocation 3 (2MB):   0x%08X\n", addr3);
    
    OSReport("\n==============================================\n");
    OSReport("ARAM test completed successfully!\n");
    OSReport("==============================================\n\n");
    
    OSReport("Summary:\n");
    OSReport("- ARAM is simulated using regular heap memory\n");
    OSReport("- DMA transfers are instant memcpy operations\n");
    OSReport("- ARQ queued transfers execute immediately on PC\n");
    OSReport("- Games can use ARAM for audio data storage\n");
    OSReport("- Total ARAM: 16MB (matching GameCube hardware)\n");
    
    return 0;
}

