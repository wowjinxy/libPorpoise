#ifndef SIM_GX_THREAD_H
#define SIM_GX_THREAD_H

#include <dolphin/types.h>
#include <dolphin/gx/GXEnum.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_GX_Fifo_SendU8(u8 data);

void SIM_GX_Fifo_SendU16(u16 data);

void SIM_GX_Fifo_SendS16(s16 data);

void SIM_GX_Fifo_SendU32(u32 data);

void SIM_GX_Fifo_SendF32(f32 data);

void SIM_GX_Fifo_SendU64(u64 data);

void SIM_GX_BeginDisplayList(u8 * ptr, u32 size);

u32 SIM_GX_EndDisplayList();

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride);

#ifdef __cplusplus
}
#endif

#endif