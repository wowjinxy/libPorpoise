/*---------------------------------------------------------------------------*
  __ppc_eabi_init.c - Hardware and User Initialization for libPorpoise
  
  Provides initialization functions called by __start() before main().
  Replaces PowerPC-specific initialization with PC-compatible versions.
 *---------------------------------------------------------------------------*/

#include <dolphin/os.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
    C++ Constructor/Destructor Support
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus
/* C++ static constructor/destructor arrays (provided by linker) */
typedef void (*voidfunctionptr)(void);
extern voidfunctionptr _ctors[];
extern voidfunctionptr _dtors[];

static void __init_cpp(void);
static void __fini_cpp(void);
#endif

/*---------------------------------------------------------------------------*
  Name:         __init_hardware

  Description:  Initialize hardware (PC-specific stub).
                
                On GC/Wii: Initializes cache, FPU, paired singles via assembly
                On PC: Stub function for compatibility (no hardware to initialize)

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __init_hardware(void) {
    /* On PC, there's no GameCube/Wii hardware to initialize */
    /* This function exists for compatibility with games that expect it */
    
    /* Future: Could initialize libPorpoise-specific things here if needed */
    /* For now, OSInit() handles all necessary initialization */
}

/*---------------------------------------------------------------------------*
  Name:         __init_user

  Description:  User-level initialization before main().
                
                On GC/Wii: Allocates additional heaps, calls C++ constructors
                On PC: Calls C++ constructors if C++ is used

  Arguments:    None

  Returns:      None
 *---------------------------------------------------------------------------*/
void __init_user(void) {
    /*
     * Initialization of static initializers (C++ constructors)
     */
#ifdef __cplusplus
    __init_cpp();
#endif

    /*
     * Add your custom initializations here.
     * Games can override this function or call additional init functions.
     */
}

/*---------------------------------------------------------------------------*
  Name:         __init_cpp

  Description:  Initialize C++ static constructors.
                Iterates through _ctors[] array and calls each constructor.

  Arguments:    None

  Returns:      None
  
  Note:         Only compiled if __cplusplus is defined
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
static void __init_cpp(void) {
    voidfunctionptr* constructor;

    /*
     * Call static initializers
     * _ctors[] array is null-terminated and provided by linker
     */
    if (_ctors) {
        for (constructor = _ctors; *constructor; constructor++) {
            (*constructor)();
        }
    }
}
#endif

/*---------------------------------------------------------------------------*
  Name:         __fini_cpp

  Description:  Finalize C++ static destructors.
                Iterates through _dtors[] array and calls each destructor.

  Arguments:    None

  Returns:      None
  
  Note:         Only compiled if __cplusplus is defined
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
static void __fini_cpp(void) {
    voidfunctionptr* destructor;

    /*
     * Call destructors
     * _dtors[] array is null-terminated and provided by linker
     */
    if (_dtors) {
        for (destructor = _dtors; *destructor; destructor++) {
            (*destructor)();
        }
    }
}
#endif

/*---------------------------------------------------------------------------*
  Name:         __flush_cache

  Description:  Flush data cache and invalidate instruction cache (PC stub).
                
                On GC/Wii: Uses assembly to flush D-cache and invalidate I-cache
                On PC: No-op (CPU/OS handles cache automatically)

  Arguments:    address  Memory address to flush
                size     Size in bytes

  Returns:      None
 *---------------------------------------------------------------------------*/
void __flush_cache(void* address, unsigned int size) {
    (void)address;
    (void)size;
    
    /* On PC, there's no instruction cache to invalidate */
    /* Data cache is handled automatically by the CPU/OS */
    /* This function exists for compatibility with games that call it */
    
    /* Note: Some compilers (MSVC, GCC) have __builtin___clear_cache() but
     * it's not necessary on x86/x64/ARM (most modern CPUs) since they have
     * unified caches or handle instruction/data coherency automatically.
     */
}

#ifdef __cplusplus
}
#endif

