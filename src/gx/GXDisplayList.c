#include <dolphin/gx.h>
#include <string.h>

#ifdef LIBPORPOISE_PORT
#include <simulator/sim_gx_CommandProcessor.h>
#else
static struct __GXFifoObj DisplayListFifo;
static volatile struct __GXFifoObj* OldCPUFifo;
#endif
static struct __GXData_struct __savedGXdata;

/**
 * @TODO: Documentation
 */
void GXBeginDisplayList(void* list, u32 size)
{
#ifndef LIBPORPOISE_PORT
	struct __GXFifoObj* CPUFifo = (struct __GXFifoObj*)GXGetCPUFifo();
#endif

	CHECK_GXBEGIN(0x7C, "GXBeginDisplayList");
	OSAssertMsgLine(0x7D, !gx->inDispList, "GXBeginDisplayList: display list already in progress");
	OSAssertMsgLine(0x7E, (size & 0x1F) == 0, "GXBeginDisplayList: size is not 32 byte aligned");
	OSAssertMsgLine(0x7F, ((uintptr_t)list & 0x1F) == 0, "GXBeginDisplayList: list is not 32 byte aligned");
	if (gx->dirtyState != 0) {
		__GXSetDirtyState();
	}
	if (gx->dlSaveContext != 0) {
		memcpy(&__savedGXdata, gx, sizeof(__savedGXdata));
	}
#ifdef LIBPORPOISE_PORT
	if (!SIM_GX_CommandProcessor_BeginDisplayList(list, size)) {
		return;
	}
	gx->inDispList = 1;
#else
	DisplayListFifo.base  = (u8*)list;
	DisplayListFifo.top   = (u8*)list + size - 4;
	DisplayListFifo.size  = size;
	DisplayListFifo.count = 0;
	DisplayListFifo.rdPtr = list;
	DisplayListFifo.wrPtr = list;
	gx->inDispList        = 1;
	GXSaveCPUFifo((GXFifoObj*)CPUFifo);
	OldCPUFifo = CPUFifo;
	GXSetCPUFifo((GXFifoObj*)&DisplayListFifo);
#endif
}

/**
 * @TODO: Documentation
 */
u32 GXEndDisplayList(void)
{
#ifndef LIBPORPOISE_PORT
	u32 ov;
	u32 reg;
#endif
	u32 listSize;
	BOOL enabled;
	u32 cpenable;
#if !DEBUG
	u8 unused[4]; // needed to match
#endif

	CHECK_GXBEGIN(0xB5, "GXEndDisplayList");
	OSAssertMsgLine(0xB6, gx->inDispList == TRUE, "GXEndDisplayList: no display list in progress");
	if (gx->dirtyState != 0) {
		__GXSetDirtyState();
	}
#ifdef LIBPORPOISE_PORT
	listSize = SIM_GX_CommandProcessor_EndDisplayList();
#else
	reg = __piReg[5];
	ov  = (reg >> 26) & 1;
	__GXSaveCPUFifoAux(&DisplayListFifo);
	OSAssertMsgLine(0xC3, !ov, "GXEndDisplayList: display list commands overflowed buffer");
	GXSetCPUFifo((GXFifoObj*)OldCPUFifo);
	listSize = ov ? 0 : (u32)DisplayListFifo.count;
#endif
	if (gx->dlSaveContext != 0) {
		enabled  = OSDisableInterrupts();
		cpenable = gx->cpEnable;
		memcpy(gx, &__savedGXdata, sizeof(*gx));
		gx->cpEnable = cpenable;
		OSRestoreInterrupts(enabled);
	}
	gx->inDispList = 0;
	return listSize;
}

/**
 * @TODO: Documentation
 */
void GXCallDisplayList(const void* list, u32 nbytes)
{
	CHECK_GXBEGIN(0xEC, "GXCallDisplayList");
	OSAssertMsgLine(0xED, !gx->inDispList, "GXCallDisplayList: display list already in progress");
	OSAssertMsgLine(0xEE, (nbytes & 0x1F) == 0, "GXCallDisplayList: nbytes is not 32 byte aligned");
	OSAssertMsgLine(0xEF, ((uintptr_t)list & 0x1F) == 0, "GXCallDisplayList: list is not 32 byte aligned");

	if (gx->dirtyState != 0) {
		__GXSetDirtyState();
	}
#if DEBUG
	__GXShadowDispList(list, nbytes);
#endif

	if (!GX_CHECK_FLUSH(gx)) {
		__GXSendFlushPrim();
	}
#ifdef LIBPORPOISE_PORT
	/*
	 * The host command processor has direct access to the full-width pointer.
	 * Replay the recorded stream here instead of encoding the GameCube's
	 * 32-bit physical-address call packet.
	 */
	SIM_GX_CommandProcessor_CallDisplayList(list, nbytes);
#else
	GX_WRITE_U8(0x40);
	GX_WRITE_U32(list);
	GX_WRITE_U32(nbytes);
#endif
}

void GXCallDisplayListLE(const void* list, u32 nbytes)
{
	GXCallDisplayList(list, nbytes);
}
