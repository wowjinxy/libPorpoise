#ifndef PORPOISE_HOST_STATIC_U32_H
#define PORPOISE_HOST_STATIC_U32_H

#include <stdint.h>

/*
 * PE/COFF has an IMAGE_REL_AMD64_ADDR32 relocation, but GCC rejects a
 * pointer-to-u32 conversion before it can emit that relocation from a C
 * static initializer.  Relocatable pointers preserve two logical u32 values
 * in one pointer-sized initializer so the compiler emits a DIR64 relocation:
 *
 *   low dword  = the eventual second u32
 *   high dword = the first u32 plus a tag
 *
 * pe_static_u32_fixups.py canonicalizes those pairs after linking and removes
 * the now-invalid DIR64 relocation.  Numeric constants, including legacy
 * pointer-typed segmented addresses, are emitted directly in canonical word
 * order and need no relocation.  The executable must use the matching fixed
 * low image base.  C++ does not need this protocol because it can emit a
 * startup initializer for the narrowing store.
 */
#if defined(LIBPORPOISE_BUILD_WIN64) && defined(__GNUC__) && !defined(__cplusplus)

#ifndef LIBPORPOISE_HOST_PE_IMAGE_BASE
#define LIBPORPOISE_HOST_PE_IMAGE_BASE ((uintptr_t)0x10000000u)
#endif

#define PORPOISE_HOST_STATIC_U32_TAG ((uint32_t)0x80000000u)

extern char __ImageBase;

#define PORPOISE_HOST_STATIC_U32_CONSTANT_BITS(first, second)                      \
    ((const void*)(uintptr_t)(                                                     \
        ((uint64_t)(uint32_t)(uintptr_t)(second) << 32) |                         \
        (uint64_t)(uint32_t)(first)))

#define PORPOISE_HOST_STATIC_U32_BITS(first, second)                                      \
    __builtin_choose_expr(                                                               \
        __builtin_constant_p((uintptr_t)(second)),                                        \
        PORPOISE_HOST_STATIC_U32_CONSTANT_BITS((first), (second)),                        \
        __builtin_choose_expr(                                                            \
            __builtin_classify_type(second) == 5,                                         \
            (const void*)((const char*)(uintptr_t)(second) +                              \
                          ((uintptr_t)(uint32_t)((uint32_t)(first) +                       \
                                                 PORPOISE_HOST_STATIC_U32_TAG)             \
                           << 32)),                                                       \
            (const void*)((const char*)&__ImageBase - LIBPORPOISE_HOST_PE_IMAGE_BASE +    \
                          (uintptr_t)(uint32_t)(uintptr_t)(second) +                       \
                          ((uintptr_t)(uint32_t)((uint32_t)(first) +                       \
                                                 PORPOISE_HOST_STATIC_U32_TAG)             \
                           << 32))))

#endif

#endif
