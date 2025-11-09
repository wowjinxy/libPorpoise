#include <dolphin/os.h>

int main(void) {
    OSReport("Initializing Porpoise SDK...\n");
    
    // Initialize the OS
    OSInit();
    
    // Get current time
    OSTime time = OSGetTime();
    OSReport("Current time: %lld ticks\n", (long long)time);
    
    // Get console type
    u32 consoleType = OSGetConsoleType();
    OSReport("Console type: 0x%08X\n", consoleType);
    
    // Convert time to calendar
    u16 year;
    u8 month, day, hour, minute, second;
    OSTicksToCalendarTime(time, &year, &month, &day, &hour, &minute, &second);
    OSReport("Calendar time: %04d-%02d-%02d %02d:%02d:%02d\n",
             year, month, day, hour, minute, second);
    
    OSReport("Simple example completed successfully!\n");
    
    return 0;
}

