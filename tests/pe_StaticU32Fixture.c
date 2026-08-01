#include <porpoise/host_static_u32.h>

#include <stdint.h>
#include <stdio.h>

typedef struct StaticU32Words {
    uint32_t first;
    uint32_t second;
} StaticU32Words;

typedef union StaticU32Pair {
    StaticU32Words words;
    const void* host_static_pointer;
    uint64_t alignment;
} StaticU32Pair;

#define STATIC_U32_PAIR(first, second) \
    { .host_static_pointer = PORPOISE_HOST_STATIC_U32_BITS((first), (second)) }

static unsigned char PointerTarget[16];

static StaticU32Pair PointerPair =
    STATIC_U32_PAIR(UINT32_C(0x01008010), PointerTarget);
static StaticU32Pair PointerOffsetPair =
    STATIC_U32_PAIR(UINT32_C(0x06000000), PointerTarget + 7);
static StaticU32Pair TypedWirePair =
    STATIC_U32_PAIR(
        UINT32_C(0xDA380003),
        ((const uint32_t*)(uintptr_t)UINT32_C(0x0D000000)) + 2
    );
static StaticU32Pair ZeroPair =
    STATIC_U32_PAIR(UINT32_C(0xDF000000), 0);
static StaticU32Pair SignedPair =
    STATIC_U32_PAIR(UINT32_C(0xB8000000), -1);
static StaticU32Pair HighBitPair =
    STATIC_U32_PAIR(UINT32_C(0xFA001234), UINT32_C(0xF6789ABC));

typedef char StaticU32PairMustRemainEightBytes[
    sizeof(StaticU32Pair) == 8 ? 1 : -1
];

static int CheckPair(
    const char* name,
    const StaticU32Pair* pair,
    uint32_t expected_first,
    uint32_t expected_second
) {
    if (pair->words.first == expected_first &&
        pair->words.second == expected_second) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected %08x %08x, got %08x %08x\n",
        name,
        expected_first,
        expected_second,
        pair->words.first,
        pair->words.second
    );
    return 1;
}

int main(void) {
    uintptr_t pointer_target = (uintptr_t)PointerTarget;
    int failed = 0;

    if (pointer_target > UINT32_MAX) {
        fprintf(stderr, "fixture target is outside the fixed low image\n");
        return 1;
    }

    failed |= CheckPair(
        "pointer",
        &PointerPair,
        UINT32_C(0x01008010),
        (uint32_t)pointer_target
    );
    failed |= CheckPair(
        "pointer offset",
        &PointerOffsetPair,
        UINT32_C(0x06000000),
        (uint32_t)(pointer_target + 7)
    );
    failed |= CheckPair(
        "typed numeric wire pointer",
        &TypedWirePair,
        UINT32_C(0xDA380003),
        UINT32_C(0x0D000008)
    );
    failed |= CheckPair(
        "zero",
        &ZeroPair,
        UINT32_C(0xDF000000),
        UINT32_C(0x00000000)
    );
    failed |= CheckPair(
        "signed",
        &SignedPair,
        UINT32_C(0xB8000000),
        UINT32_C(0xFFFFFFFF)
    );
    failed |= CheckPair(
        "high bit",
        &HighBitPair,
        UINT32_C(0xFA001234),
        UINT32_C(0xF6789ABC)
    );
    return failed;
}
