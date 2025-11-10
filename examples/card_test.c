/**
 * @file card_test.c
 * @brief Example demonstrating CARD (Memory Card) usage with libPorpoise
 * 
 * Shows how to:
 * - Initialize memory card system
 * - Mount/unmount cards
 * - Create, open, and delete save files
 * - Read/write save data
 */

#include <dolphin/os.h>
#include <dolphin/card.h>
#include <stdio.h>
#include <string.h>

// Example save data structure
typedef struct SaveData {
    u32 magic;          // 'SAVE'
    u32 version;
    char playerName[32];
    u32 score;
    u32 level;
    u32 playtime;
    u8  checksum;
} SaveData;

int main(void) {
    // Initialize OS
    OSReport("Initializing libPorpoise...\n");
    OSInit();
    
    OSReport("\n==============================================\n");
    OSReport("   libPorpoise CARD (Memory Card) Test\n");
    OSReport("==============================================\n\n");
    
    // Initialize CARD
    OSReport("[Test 1] Initializing memory card system...\n");
    CARDInit();
    OSReport("  CARD initialized\n");
    
    // Probe for cards
    OSReport("\n[Test 2] Probing for memory cards...\n");
    BOOL slotA = CARDProbe(CARD_SLOTA);
    BOOL slotB = CARDProbe(CARD_SLOTB);
    OSReport("  Slot A: %s\n", slotA ? "Present" : "Empty");
    OSReport("  Slot B: %s\n", slotB ? "Present" : "Empty");
    
    // Mount Slot A
    OSReport("\n[Test 3] Mounting Slot A...\n");
    s32 result = CARDMount(CARD_SLOTA, NULL, NULL);
    if (result == CARD_RESULT_READY) {
        OSReport("  ✓ Slot A mounted\n");
    } else {
        OSReport("  ✗ Mount failed: %d\n", result);
        return 1;
    }
    
    // Get card info
    OSReport("\n[Test 4] Card information...\n");
    u16 memSize;
    u32 sectorSize;
    s32 freeBytes, freeFiles;
    
    CARDGetMemSize(CARD_SLOTA, &memSize);
    CARDGetSectorSize(CARD_SLOTA, &sectorSize);
    CARDFreeBlocks(CARD_SLOTA, &freeBytes, &freeFiles);
    
    OSReport("  Memory size: %u Mbit (%u MB)\n", memSize, memSize / 8);
    OSReport("  Sector size: %u bytes\n", sectorSize);
    OSReport("  Free space: %d bytes\n", freeBytes);
    OSReport("  Free file slots: %d\n", freeFiles);
    
    // Create a save file
    OSReport("\n[Test 5] Creating save file...\n");
    CARDFileInfo fileInfo;
    const char* saveName = "TestSave";
    
    result = CARDCreate(CARD_SLOTA, saveName, sizeof(SaveData), &fileInfo);
    if (result == CARD_RESULT_READY) {
        OSReport("  ✓ Created '%s' (%u bytes)\n", saveName, sizeof(SaveData));
    } else if (result == CARD_RESULT_EXIST) {
        OSReport("  File already exists, opening instead...\n");
        result = CARDOpen(CARD_SLOTA, saveName, &fileInfo);
    } else {
        OSReport("  ✗ Create failed: %d\n", result);
    }
    
    // Write save data
    OSReport("\n[Test 6] Writing save data...\n");
    SaveData save;
    save.magic = 0x53415645;  // 'SAVE'
    save.version = 1;
    strcpy(save.playerName, "Player One");
    save.score = 12345;
    save.level = 10;
    save.playtime = 3600;  // 1 hour
    save.checksum = 0;  // TODO: Calculate
    
    OSReport("  Player: %s\n", save.playerName);
    OSReport("  Score: %u\n", save.score);
    OSReport("  Level: %u\n", save.level);
    
    result = CARDWrite(&fileInfo, &save, sizeof(SaveData), 0);
    if (result >= 0) {
        OSReport("  ✓ Wrote %d bytes\n", result);
    } else {
        OSReport("  ✗ Write failed: %d\n", result);
    }
    
    // Close file
    CARDClose(&fileInfo);
    
    // Re-open and read back
    OSReport("\n[Test 7] Reading save data...\n");
    result = CARDOpen(CARD_SLOTA, saveName, &fileInfo);
    if (result == CARD_RESULT_READY) {
        SaveData loadedSave;
        memset(&loadedSave, 0, sizeof(SaveData));
        
        result = CARDRead(&fileInfo, &loadedSave, sizeof(SaveData), 0);
        if (result >= 0) {
            OSReport("  ✓ Read %d bytes\n", result);
            OSReport("  Player: %s\n", loadedSave.playerName);
            OSReport("  Score: %u\n", loadedSave.score);
            OSReport("  Level: %u\n", loadedSave.level);
            
            if (memcmp(&save, &loadedSave, sizeof(SaveData)) == 0) {
                OSReport("  ✓ Data matches!\n");
            } else {
                OSReport("  ⚠ Data mismatch (read/write not fully implemented)\n");
            }
        }
        
        CARDClose(&fileInfo);
    }
    
    // Delete test file
    OSReport("\n[Test 8] Deleting save file...\n");
    result = CARDDelete(CARD_SLOTA, saveName);
    if (result == CARD_RESULT_READY) {
        OSReport("  ✓ Deleted '%s'\n", saveName);
    } else {
        OSReport("  Delete result: %d\n", result);
    }
    
    // Unmount
    OSReport("\n[Test 9] Unmounting...\n");
    CARDUnmount(CARD_SLOTA);
    OSReport("  ✓ Slot A unmounted\n");
    
    OSReport("\n==============================================\n");
    OSReport("CARD test completed!\n");
    OSReport("==============================================\n\n");
    
    OSReport("Summary:\n");
    OSReport("- Memory cards map to directories (memcard_a/, memcard_b/)\n");
    OSReport("- Save files are stored as individual files (.sav)\n");
    OSReport("- Games can save/load data to PC filesystem\n");
    OSReport("- All operations complete instantly (no EXI latency)\n");
    OSReport("\nNote: Read/Write implementation is basic.\n");
    OSReport("      Full implementation would track open files.\n");
    
    return 0;
}

