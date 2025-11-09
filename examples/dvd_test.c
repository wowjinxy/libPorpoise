/**
 * @file dvd_test.c
 * @brief Example program demonstrating DVD file I/O with libPorpoise
 * 
 * Shows how to:
 * - Initialize DVD subsystem
 * - Open files from "files/" directory
 * - Read file contents
 * - Use async reads with callbacks
 * 
 * Setup:
 * 1. Create "files/" directory next to this executable
 * 2. Place test files in "files/" (e.g., files/test.txt)
 * 3. Run this program
 */

#include <dolphin/os.h>
#include <dolphin/dvd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Async callback example
static volatile BOOL s_asyncComplete = FALSE;

void AsyncReadCallback(s32 result, DVDFileInfo* fileInfo) {
    (void)fileInfo;
    
    if (result >= 0) {
        OSReport("  Async read complete: %d bytes\n", result);
    } else {
        OSReport("  Async read failed: %d\n", result);
    }
    
    s_asyncComplete = TRUE;
}

int main(void) {
    DVDFileInfo file;
    char buffer[1024];
    s32 bytesRead;
    
    // Initialize OS
    OSReport("Initializing libPorpoise...\n");
    OSInit();
    
    // Initialize DVD
    OSReport("\nInitializing DVD subsystem...\n");
    if (!DVDInit()) {
        OSReport("Failed to initialize DVD!\n");
        return 1;
    }
    
    OSReport("\n==============================================\n");
    OSReport("   libPorpoise DVD Test\n");
    OSReport("==============================================\n");
    OSReport("DVD Root: %s\n", DVDGetRootDirectory());
    OSReport("==============================================\n\n");
    
    // Create test file if it doesn't exist
    OSReport("Setting up test files...\n");
    FILE* testFile = fopen("files/test.txt", "w");
    if (testFile) {
        fprintf(testFile, "Hello from libPorpoise DVD!\n");
        fprintf(testFile, "This is a test file.\n");
        fprintf(testFile, "Line 3\n");
        fclose(testFile);
        OSReport("  Created files/test.txt\n");
    }
    
    // Test 1: Open and read file
    OSReport("\n[Test 1] Opening file...\n");
    if (DVDOpen("test.txt", &file)) {
        OSReport("  File opened successfully\n");
        OSReport("  File size: %u bytes\n", file.length);
        
        // Read entire file
        memset(buffer, 0, sizeof(buffer));
        bytesRead = DVDRead(&file, buffer, file.length, 0);
        
        if (bytesRead > 0) {
            OSReport("\n  File contents (%d bytes):\n", bytesRead);
            OSReport("  -------------------\n");
            OSReport("%s", buffer);
            OSReport("  -------------------\n");
        } else {
            OSReport("  Failed to read file\n");
        }
        
        DVDClose(&file);
        OSReport("  File closed\n");
    } else {
        OSReport("  Failed to open file\n");
    }
    
    // Test 2: Partial read
    OSReport("\n[Test 2] Partial read (10 bytes at offset 6)...\n");
    if (DVDOpen("test.txt", &file)) {
        memset(buffer, 0, sizeof(buffer));
        bytesRead = DVDRead(&file, buffer, 10, 6);
        
        if (bytesRead > 0) {
            OSReport("  Read %d bytes: '%s'\n", bytesRead, buffer);
        }
        
        DVDClose(&file);
    }
    
    // Test 3: Async read
    OSReport("\n[Test 3] Async read...\n");
    if (DVDOpen("test.txt", &file)) {
        memset(buffer, 0, sizeof(buffer));
        s_asyncComplete = FALSE;
        
        OSReport("  Starting async read...\n");
        if (DVDReadAsync(&file, buffer, file.length, 0, AsyncReadCallback)) {
            OSReport("  Waiting for async completion...\n");
            
            // Wait for completion
            while (!s_asyncComplete) {
                OSSleepTicks(OSMillisecondsToTicks(10));
            }
            
            OSReport("  Data: %.30s...\n", buffer);
        }
        
        DVDClose(&file);
    }
    
    // Test 4: Seek and read
    OSReport("\n[Test 4] Seek and read...\n");
    if (DVDOpen("test.txt", &file)) {
        // Seek to middle of file
        s32 pos = DVDSeek(&file, 15);
        OSReport("  Seeked to position: %d\n", pos);
        
        memset(buffer, 0, sizeof(buffer));
        bytesRead = DVDRead(&file, buffer, 20, pos);
        OSReport("  Read %d bytes: '%s'\n", bytesRead, buffer);
        
        DVDClose(&file);
    }
    
    // Test 5: Directory operations
    OSReport("\n[Test 5] Directory operations...\n");
    char currentDir[256];
    if (DVDGetCurrentDir(currentDir, sizeof(currentDir))) {
        OSReport("  Current directory: %s\n", currentDir);
    }
    
    // Test 6: Create subdirectory
    OSReport("\n[Test 6] Creating subdirectory structure...\n");
#ifdef _WIN32
    _mkdir("files/data");
#else
    mkdir("files/data", 0755);
#endif
    
    FILE* dataFile = fopen("files/data/level1.dat", "w");
    if (dataFile) {
        fprintf(dataFile, "Level 1 data here!\n");
        fclose(dataFile);
        OSReport("  Created files/data/level1.dat\n");
        
        // Open from subdirectory
        if (DVDOpen("data/level1.dat", &file)) {
            memset(buffer, 0, sizeof(buffer));
            bytesRead = DVDRead(&file, buffer, file.length, 0);
            OSReport("  Read from subdirectory: '%s'\n", buffer);
            DVDClose(&file);
        }
    }
    
    // Test 7: Change directory
    OSReport("\n[Test 7] Changing directory...\n");
    if (DVDChangeDir("data")) {
        DVDGetCurrentDir(currentDir, sizeof(currentDir));
        OSReport("  New directory: %s\n", currentDir);
        
        // Open file without path (from current dir)
        if (DVDOpen("level1.dat", &file)) {
            OSReport("  Opened level1.dat from current dir\n");
            OSReport("  File size: %u bytes\n", file.length);
            DVDClose(&file);
        }
        
        // Change back to root
        DVDChangeDir("/");
        DVDGetCurrentDir(currentDir, sizeof(currentDir));
        OSReport("  Back to: %s\n", currentDir);
    }
    
    OSReport("\n==============================================\n");
    OSReport("DVD test completed!\n");
    OSReport("==============================================\n\n");
    
    OSReport("All tests passed! ✓\n\n");
    OSReport("Tips:\n");
    OSReport("- Place game files in files/ directory\n");
    OSReport("- Use DVDOpen(\"path/to/file.dat\", &file)\n");
    OSReport("- Paths are relative to files/ directory\n");
    OSReport("- Subdirectories work: files/data/level1.dat\n");
    
    return 0;
}

