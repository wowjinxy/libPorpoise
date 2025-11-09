#include <dolphin/os.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static BOOL s_osInitialized = FALSE;

/* Arena management variables
 * On GC/Wii, "arenas" define ranges of memory available for allocation
 * MEM1 = 24MB main RAM, MEM2 = 64MB external RAM (Wii only)
 */
static void* s_arenaHi = NULL;
static void* s_arenaLo = NULL;
static void* s_mem1ArenaHi = NULL;
static void* s_mem1ArenaLo = NULL;
static void* s_mem2ArenaHi = NULL;
static void* s_mem2ArenaLo = NULL;

/* Simulated memory regions - allocated on first init */
static void* s_mem1Base = NULL;
static void* s_mem2Base = NULL;

#define SIMULATED_MEM1_SIZE (24 * 1024 * 1024)  // 24 MB like GameCube/Wii
#define SIMULATED_MEM2_SIZE (64 * 1024 * 1024)  // 64 MB like Wii

/*---------------------------------------------------------------------------*
  Name:         OSInit

  Description:  Initializes the operating system. This is the first OS
                function that should be called. On the original hardware,
                this sets up exception handlers, interrupts, caches, and
                various hardware subsystems.
                
                On PC, we simulate the memory layout and initialize our
                platform-specific implementations of threading, timing, etc.

  Arguments:    None.

  Returns:      None.
 *---------------------------------------------------------------------------*/
void OSInit(void) {
    if (s_osInitialized) {
        return;
    }
    
    s_osInitialized = TRUE;
    
    // Initialize simulated memory arenas
    // Many games expect these to be valid memory ranges they can subdivide
    if (s_mem1Base == NULL) {
        s_mem1Base = malloc(SIMULATED_MEM1_SIZE);
        if (s_mem1Base == NULL) {
            OSPanic(__FILE__, __LINE__, "Failed to allocate simulated MEM1");
        }
        
        s_mem1ArenaLo = s_mem1Base;
        s_mem1ArenaHi = (u8*)s_mem1Base + SIMULATED_MEM1_SIZE;
        
        // Default arena points to MEM1
        s_arenaLo = s_mem1ArenaLo;
        s_arenaHi = s_mem1ArenaHi;
        
        // Clear the memory (original hardware does this)
        memset(s_mem1Base, 0, SIMULATED_MEM1_SIZE);
    }
    
    if (s_mem2Base == NULL) {
        s_mem2Base = malloc(SIMULATED_MEM2_SIZE);
        if (s_mem2Base == NULL) {
            OSPanic(__FILE__, __LINE__, "Failed to allocate simulated MEM2");
        }
        
        s_mem2ArenaLo = s_mem2Base;
        s_mem2ArenaHi = (u8*)s_mem2Base + SIMULATED_MEM2_SIZE;
        
        // Clear the memory
        memset(s_mem2Base, 0, SIMULATED_MEM2_SIZE);
    }
    
    // Report initialization
    OSReport("==================================\n");
    OSReport("Porpoise SDK v0.1.0\n");
    OSReport("GC/Wii SDK PC Port\n");
    OSReport("==================================\n");
    OSReport("Initialized: %s %s\n", __DATE__, __TIME__);
    OSReport("MEM1 Arena: %p - %p (%d MB)\n", 
             s_mem1ArenaLo, s_mem1ArenaHi, SIMULATED_MEM1_SIZE / (1024*1024));
    OSReport("MEM2 Arena: %p - %p (%d MB)\n", 
             s_mem2ArenaLo, s_mem2ArenaHi, SIMULATED_MEM2_SIZE / (1024*1024));
    OSReport("==================================\n");
}

/*---------------------------------------------------------------------------*
  Name:         OSGetConsoleType

  Description:  Returns the console type identifier. On original hardware,
                this returns values like OS_CONSOLE_RVL_RETAIL1 (retail Wii),
                OS_CONSOLE_RVL_NDEV2_0 (dev kit), etc.
                
                On PC, we return a custom identifier to indicate this is a
                PC port. Games may check this value to enable/disable features
                or adjust behavior.

  Arguments:    None.

  Returns:      Console type identifier. 0x10000000 = PC port
 *---------------------------------------------------------------------------*/
#define OS_CONSOLE_PC_PORT 0x10000000

u32 OSGetConsoleType(void) {
    return OS_CONSOLE_PC_PORT;
}

/*---------------------------------------------------------------------------*
  Name:         OSReport

  Description:  Debug output function similar to printf. On retail hardware
                this typically does nothing, but on dev kits it outputs to
                the debugger or USB Gecko device.
                
                On PC, we simply output to stdout. This is used extensively
                by games for debug logging.

  Arguments:    fmt  - printf-style format string
                ...  - variable arguments

  Returns:      None.
 *---------------------------------------------------------------------------*/
void OSReport(const char* fmt, ...) {
    va_list args;
    
    // Add timestamp for easier debugging
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", tm_info);
    printf("[%s] ", timeStr);
    
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

/*---------------------------------------------------------------------------*
  Name:         OSPanic

  Description:  Fatal error handler. This is called when the game encounters
                an unrecoverable error. On original hardware, this displays
                an error screen and halts execution.
                
                On PC, we print the error and abort the program.

  Arguments:    file - Source file where panic occurred
                line - Line number where panic occurred  
                fmt  - printf-style format string
                ...  - variable arguments

  Returns:      Does not return (aborts program).
 *---------------------------------------------------------------------------*/
void OSPanic(const char* file, int line, const char* fmt, ...) {
    va_list args;
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "         PANIC - FATAL ERROR\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Location: %s:%d\n", file, line);
    fprintf(stderr, "Message:  ");
    
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fflush(stderr);
    
    abort();
}

/*---------------------------------------------------------------------------*
  Arena Management Functions
  
  These functions manage memory arenas - contiguous ranges of memory that
  games can subdivide and allocate from. The original hardware has:
  - MEM1: 24MB main RAM (NAPA)
  - MEM2: 64MB external RAM (GDDR3, Wii only)
  
  Games typically get the arena bounds and set up their own allocators
  within those ranges. We simulate this by allocating large blocks from
  the system heap.
 *---------------------------------------------------------------------------*/

/* Default arena (usually points to MEM1) */
void* OSGetArenaHi(void) { return s_arenaHi; }
void* OSGetArenaLo(void) { return s_arenaLo; }
void  OSSetArenaHi(void* addr) { s_arenaHi = addr; }
void  OSSetArenaLo(void* addr) { s_arenaLo = addr; }

/* MEM1 arena (24MB main RAM) */
void* OSGetMEM1ArenaHi(void) { return s_mem1ArenaHi; }
void* OSGetMEM1ArenaLo(void) { return s_mem1ArenaLo; }
void  OSSetMEM1ArenaHi(void* addr) { s_mem1ArenaHi = addr; }
void  OSSetMEM1ArenaLo(void* addr) { s_mem1ArenaLo = addr; }

/* MEM2 arena (64MB external RAM, Wii only) */
void* OSGetMEM2ArenaHi(void) { return s_mem2ArenaHi; }
void* OSGetMEM2ArenaLo(void) { return s_mem2ArenaLo; }
void  OSSetMEM2ArenaHi(void* addr) { s_mem2ArenaHi = addr; }
void  OSSetMEM2ArenaLo(void* addr) { s_mem2ArenaLo = addr; }
