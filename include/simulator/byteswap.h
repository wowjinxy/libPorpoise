#ifndef LIBPORPOISE_SIMULATOR_BYTESWAP_H
#define LIBPORPOISE_SIMULATOR_BYTESWAP_H

#include <dolphin/types.h>
#if defined(LIBPORPOISE_BUILD_LINUX)
#include <byteswap.h>
#elif defined(LIBPORPOISE_BUILD_WIN)
#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#endif

#endif
