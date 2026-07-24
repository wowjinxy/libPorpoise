#ifndef LIBPORPOISE_SIM_GX_COMMAND_PROCESSOR_H
#define LIBPORPOISE_SIM_GX_COMMAND_PROCESSOR_H

#include <dolphin/types.h>


#include <dolphin/gx/GXAttr.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_GX_CommandProcessor_Init();

void SIM_GX_CommandProcessor_SendU8(u8 data);
void SIM_GX_CommandProcessor_SendU16(u16 data);
void SIM_GX_CommandProcessor_SendS16(s16 data);
void SIM_GX_CommandProcessor_SendU32(u32 data);
void SIM_GX_CommandProcessor_SendF32(f32 data);
void SIM_GX_CommandProcessor_SendU64(u64 data);

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride);

#ifdef __cplusplus
}
#endif

#endif