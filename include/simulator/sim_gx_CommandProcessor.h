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

GXBool SIM_GX_CommandProcessor_BeginDisplayList(void* list, u32 size);
u32 SIM_GX_CommandProcessor_EndDisplayList(void);
void SIM_GX_CommandProcessor_CallDisplayList(const void* list, u32 size);

void SIM_GX_CommandProcessor_SetVertexArray(GXAttr attr, void * ptr, int stride);
void SIM_GX_CommandProcessor_SetVertexArrayU32(
    GXAttr attr, void * ptr, int stride);
void SIM_GX_CommandProcessor_LoadTlut(
    u32 id, const void* data, u32 format, u16 entries);
void SIM_GX_CommandProcessor_LoadTexture(
    u32 id, const void* data, u16 width, u16 height, u32 format,
    u32 wrap_s, u32 wrap_t, u32 min_filter, u32 mag_filter,
    u32 tlut_name);

#ifdef __cplusplus
}
#endif

#endif
