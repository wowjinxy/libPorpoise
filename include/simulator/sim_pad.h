#ifndef LIBPORPOISE_SIM_PAD_H
#define LIBPORPOISE_SIM_PAD_H

#include <dolphin/types.h>

#include <dolphin/pad.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_PAD_Read(PADStatus* status);

#ifdef __cplusplus
}
#endif

#endif
