/*---------------------------------------------------------------------------*
  OSFastCast.c - Fast Integer/Float Conversion Helpers

  On GC/Wii, these APIs use paired-single quantization load/store operations
  configured by OSInitFastCast via GQR registers.

  On PC, we provide API-compatible conversions with defined behavior:
  - float->int conversions are truncating (toward zero)
  - out-of-range values are clamped to destination range
  - NaN converts to 0
 *---------------------------------------------------------------------------*/

#include <dolphin/os/OSFastCast.h>

#include <limits.h>
#include <math.h>

static BOOL s_fastCastInitialized = FALSE;

static void EnsureFastCastInitialized(void) {
    if (!s_fastCastInitialized) {
        OSInitFastCast();
    }
}

static s16 FloatToS16(f32 value) {
    if (isnan(value)) {
        return 0;
    }
    if (value >= (f32)SHRT_MAX) {
        return SHRT_MAX;
    }
    if (value <= (f32)SHRT_MIN) {
        return SHRT_MIN;
    }
    return (s16)value;
}

static s8 FloatToS8(f32 value) {
    if (isnan(value)) {
        return 0;
    }
    if (value >= (f32)SCHAR_MAX) {
        return SCHAR_MAX;
    }
    if (value <= (f32)SCHAR_MIN) {
        return SCHAR_MIN;
    }
    return (s8)value;
}

static u16 FloatToU16(f32 value) {
    if (isnan(value) || value <= 0.0f) {
        return 0;
    }
    if (value >= (f32)USHRT_MAX) {
        return USHRT_MAX;
    }
    return (u16)value;
}

static u8 FloatToU8(f32 value) {
    if (isnan(value) || value <= 0.0f) {
        return 0;
    }
    if (value >= (f32)UCHAR_MAX) {
        return UCHAR_MAX;
    }
    return (u8)value;
}

void OSInitFastCast(void) {
    s_fastCastInitialized = TRUE;
}

void OSf32tos16(f32* in, s16* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = FloatToS16(*in);
}

void OSf32tos8(f32* in, s8* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = FloatToS8(*in);
}

void OSf32tou16(f32* in, u16* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = FloatToU16(*in);
}

void OSf32tou8(f32* in, u8* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = FloatToU8(*in);
}

void OSs16tof32(s16* in, f32* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = (f32)(*in);
}

void OSs8tof32(s8* in, f32* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = (f32)(*in);
}

void OSu16tof32(u16* in, f32* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = (f32)(*in);
}

void OSu8tof32(u8* in, f32* out) {
    if (!in || !out) {
        return;
    }
    EnsureFastCastInitialized();
    *out = (f32)(*in);
}
