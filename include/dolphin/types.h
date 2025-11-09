#ifndef DOLPHIN_TYPES_H
#define DOLPHIN_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types matching GameCube/Wii SDK conventions */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef float    f32;
typedef double   f64;

typedef volatile u8  vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

typedef volatile s8  vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef volatile f32 vf32;
typedef volatile f64 vf64;

/* Boolean type */
typedef int BOOL;

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Useful macros */
#define ROUND_UP(x, align)      (((x) + (align) - 1) & -(align))
#define ROUND_DOWN(x, align)    ((x) & -(align))
#define ALIGN(x, align)         ROUND_UP(x, align)

#define ARRAY_LENGTH(arr)       (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define CLAMP(x, min, max)      (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))

/* Attribute macros */
#ifdef _MSC_VER
#define ATTRIBUTE_ALIGN(x)      __declspec(align(x))
#define ATTRIBUTE_PACKED
#else
#define ATTRIBUTE_ALIGN(x)      __attribute__((aligned(x)))
#define ATTRIBUTE_PACKED        __attribute__((packed))
#endif

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_TYPES_H */

