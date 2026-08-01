#ifndef _DOLPHIN_VI_H
#define _DOLPHIN_VI_H

#include <dolphin/types.h>

#include <dolphin/gx/GXTypes.h>
#include <dolphin/vi/vitypes.h>

BEGIN_SCOPE_EXTERN_C

///// VIDEO INTERFACE FUNCTIONS ////
// Basic VI functions.
void __VIInit(VITVMode mode);
void VIInit(void);
void VIFlush(void);
void VIWaitForRetrace(void);

#ifdef LIBPORPOISE_PORT
/*
 * Explicitly transfer host retrace/render ownership to the calling thread.
 * The previous owner must cease issuing GX work after the transfer. When an
 * adopted emulated OSThread exits, its lease returns to the prior live owner;
 * GL is attached there at the next ownership boundary. Raw host threads must
 * still transfer before exit because their lifetime is outside OS tracking.
 * Ownership is intentionally not inferred from arbitrary thread use.
 */
void __VIHostInitRuntime(void);
BOOL __VIHostAdoptRenderThread(void);
BOOL __VIHostIsRenderThread(void);
#endif

// Configure functions.
void VIConfigure(const GXRenderModeObj* obj);

// Retrace callbacks.
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback);
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback);

// Getters and setters
void VISetNextFrameBuffer(void* fb);
void* VIGetCurrentFrameBuffer();

void __VIGetCurrentPosition(s16* x, s16* y);

void VISetBlack(BOOL isBlack);

u32 VIGetRetraceCount(void);
u32 VIGetNextField(void);
u32 VIGetCurrentLine(void);
u32 VIGetTvFormat(void);

u32 VIGetDTVStatus(void);

// Unused/stripped in P2.
void VIConfigurePan(u16 panPosX, u16 panPosY, u16 panSizeX, u16 panSizeY);
void* VIGetNextFrameBuffer();
void VISetNextRightFrameBuffer(void* fb);
void VISet3D(); // unsure on arguments

////////////////////////////////////

END_SCOPE_EXTERN_C

#endif
