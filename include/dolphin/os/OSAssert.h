/**
 * @file OSAssert.h
 * @brief Assertion Macros for libPorpoise
 * 
 * Provides assertion macros compatible with GameCube/Wii SDK.
 */

#ifndef DOLPHIN_OSASSERT_H
#define DOLPHIN_OSASSERT_H

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*
    Assertion Macros
 *---------------------------------------------------------------------------*/

// Standard assertion (enabled in debug builds)
#ifdef NDEBUG
    #define ASSERT(cond)            ((void)0)
    #define ASSERTMSG(cond, msg)    ((void)0)
#else
    #define ASSERT(cond) \
        do { \
            if (!(cond)) { \
                OSPanic(__FILE__, __LINE__, "Assertion failed: %s", #cond); \
            } \
        } while(0)
    
    #define ASSERTMSG(cond, msg) \
        do { \
            if (!(cond)) { \
                OSPanic(__FILE__, __LINE__, "Assertion failed: %s\n%s", #cond, msg); \
            } \
        } while(0)
#endif

// OSHalt - similar to OSPanic but from original SDK
#define OSHalt(msg) OSPanic(__FILE__, __LINE__, "%s", msg)

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSASSERT_H */

