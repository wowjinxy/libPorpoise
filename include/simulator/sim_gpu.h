#ifndef LIBPORPOISE_SIM_GPU_H
#define LIBPORPOISE_SIM_GPU_H

#include <dolphin/types.h>


#include <dolphin/gx/GXAttr.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_GPU_Init();

void SIM_GPU_FifoSendU8(u8 data);
void SIM_GPU_FifoSendU16(u16 data);
void SIM_GPU_FifoSendS16(s16 data);
void SIM_GPU_FifoSendU32(u32 data);
void SIM_GPU_FifoSendF32(f32 data);
void SIM_GPU_FifoSendU64(u64 data);

void SIM_GPU_SetVertexArray(GXAttr attr, void * ptr, int stride);

#ifdef __cplusplus
}
#endif

#endif