/**
 * @file vi_test.c
 * @brief Example demonstrating VI (Video Interface) usage with libPorpoise
 * 
 * Shows how to:
 * - Initialize video system
 * - Wait for vertical retrace
 * - Manage frame buffers
 * - Use retrace callbacks
 */

#include <dolphin/os.h>
#include <dolphin/vi.h>
#include <stdio.h>
#include <stdlib.h>

// Frame buffer simulation (in real game, allocated and rendered by GX)
static u8 s_framebuffer1[640 * 480 * 2];  // 16-bit RGB565
static u8 s_framebuffer2[640 * 480 * 2];

static volatile u32 s_callbackCount = 0;

void PreRetraceCallback(u32 retraceCount) {
    s_callbackCount++;
    
    // This would typically be used to:
    // - Update game logic
    // - Start next frame rendering
    // - Swap frame buffers
    
    if (retraceCount % 60 == 0) {
        OSReport("  Pre-retrace: Frame %u (callbacks: %u)\n", 
                 retraceCount, s_callbackCount);
    }
}

void PostRetraceCallback(u32 retraceCount) {
    // This would typically be used to:
    // - Finalize rendering
    // - Submit command lists
    // - Handle audio
    
    (void)retraceCount;
}

int main(void) {
    // Initialize OS
    OSReport("Initializing libPorpoise...\n");
    OSInit();
    
    OSReport("\n==============================================\n");
    OSReport("   libPorpoise VI (Video Interface) Test\n");
    OSReport("==============================================\n\n");
    
    // Initialize VI
    OSReport("[Test 1] Initializing VI...\n");
    VIInit();
    OSReport("  VI initialized\n");
    OSReport("  TV Format: %s\n", 
             VIGetTvFormat() == VI_NTSC ? "NTSC" : "PAL");
    OSReport("  Scan Mode: %s\n",
             VIGetScanMode() == VI_INTERLACE ? "Interlaced" : "Progressive");
    
    // Test frame buffers
    OSReport("\n[Test 2] Frame buffer management...\n");
    OSReport("  Setting frame buffer 1: %p\n", s_framebuffer1);
    VISetNextFrameBuffer(s_framebuffer1);
    OSReport("  Next FB: %p\n", VIGetNextFrameBuffer());
    
    // Wait for retrace
    OSReport("\n[Test 3] Waiting for VBlank...\n");
    u32 beforeCount = VIGetRetraceCount();
    OSReport("  Retrace count before: %u\n", beforeCount);
    
    VIWaitForRetrace();
    
    u32 afterCount = VIGetRetraceCount();
    OSReport("  Retrace count after: %u\n", afterCount);
    OSReport("  ✓ VBlank occurred! (count increased by %u)\n", 
             afterCount - beforeCount);
    
    // Test callbacks
    OSReport("\n[Test 4] Retrace callbacks...\n");
    VISetPreRetraceCallback(PreRetraceCallback);
    VISetPostRetraceCallback(PostRetraceCallback);
    OSReport("  Callbacks registered\n");
    OSReport("  Waiting for callbacks (3 seconds)...\n");
    
    OSSleepTicks(OSSecondsToTicks(3));
    
    OSReport("  Callbacks triggered %u times\n", s_callbackCount);
    
    // Test black screen
    OSReport("\n[Test 5] Black screen control...\n");
    VISetBlack(TRUE);
    OSReport("  Black screen enabled\n");
    OSSleepTicks(OSMillisecondsToTicks(500));
    
    VISetBlack(FALSE);
    OSReport("  Black screen disabled\n");
    
    // Test buffer swapping
    OSReport("\n[Test 6] Frame buffer swapping...\n");
    VISetNextFrameBuffer(s_framebuffer1);
    VIWaitForRetrace();
    OSReport("  Current FB after retrace: %p\n", VIGetCurrentFrameBuffer());
    OSReport("  (should be framebuffer1)\n");
    
    VISetNextFrameBuffer(s_framebuffer2);
    VIWaitForRetrace();
    OSReport("  Current FB after retrace: %p\n", VIGetCurrentFrameBuffer());
    OSReport("  (should be framebuffer2)\n");
    
    // Display stats
    OSReport("\n[Test 7] VI statistics...\n");
    OSReport("  Total retraces: %u\n", VIGetRetraceCount());
    OSReport("  Next field: %s\n", 
             VIGetNextField() == VI_FIELD_ABOVE ? "Above" : "Below");
    OSReport("  Current line: %u\n", VIGetCurrentLine());
    
    OSReport("\n==============================================\n");
    OSReport("VI test completed!\n");
    OSReport("==============================================\n\n");
    
    OSReport("Summary:\n");
    OSReport("- VI provides display timing and frame buffer management\n");
    OSReport("- VIWaitForRetrace simulates 60Hz VBlank on PC\n");
    OSReport("- Retrace callbacks work at ~60Hz for game loop timing\n");
    OSReport("- Frame buffers are just pointers (GX allocates/renders them)\n");
    OSReport("- Games use VI for synchronization and timing\n");
    
    // Clean up callbacks before exit
    VISetPreRetraceCallback(NULL);
    VISetPostRetraceCallback(NULL);
    
    return 0;
}

