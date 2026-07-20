#ifndef LIBPORPOISE_SIM_GPU_H
#define LIBPORPOISE_SIM_GPU_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_GPU_Init();

void SIM_GPU_FifoSendU8(u8 data);
void SIM_GPU_FifoSendU16(u16 data);
void SIM_GPU_FifoSendU32(u32 data);

#ifdef __cplusplus
}
#endif

#endif