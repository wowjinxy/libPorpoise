#ifndef _DOLPHIN_GXFIFO_H
#define _DOLPHIN_GXFIFO_H

#include <dolphin/types.h>

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXTypes.h>

#ifdef LIBPORPOISE_PORT
#include <simulator/sim_gx_CommandProcessor.h>
#endif

BEGIN_SCOPE_EXTERN_C

/////////////// FIFO STRUCTS ///////////////
#define GX_FIFO_MINSIZE  (64 * 1024)
#define GX_FIFO_OBJ_SIZE (128)

#define GXFIFO_ADDR 0xCC008000

// Generic struct for FIFO access (size 0x80).
typedef struct _GXFifoObj {
	u8 padding[GX_FIFO_OBJ_SIZE]; // _00
} GXFifoObj;

typedef struct __GXFifoObj {
	u8* base;
	u8* top;
	u32 size;
	u32 hiWatermark;
	u32 loWatermark;
	void* rdPtr;
	void* wrPtr;
	s32 count;
	GXBool wrap;
	u8 bind_cpu;
	u8 bind_gp;
} __GXFifoObj;

// Internal struct for FIFO access.
typedef struct _GXFifoObjPriv {
	void* base;        // _00
	void* end;         // _04
	u32 size;          // _08
	u32 highWatermark; // _0C
	u32 lowWatermark;  // _10
	void* readPtr;     // _14
	void* writePtr;    // _18
	s32 rwDistance;    // _1C
	u8 _20[0x60];      // _20
} GXFifoObjPriv;

typedef void (*GXBreakPtCallback)(void);

// PPC Write Gather Pipe
typedef union {
	u8 u8;
	u16 u16;
	u32 u32;
	u64 u64;
	s8 s8;
	s16 s16;
	s32 s32;
	s64 s64;
	f32 f32;
	f64 f64;
} PPCWGPipe;

extern volatile PPCWGPipe GXWGFifo AT_ADDRESS(GXFIFO_ADDR);

////////////////////////////////////////////

//////////// FIFO MACROS/INLINES ///////////
#ifdef LIBPORPOISE_PORT
#define GX_WRITE_U8(val) SIM_GX_CommandProcessor_SendU8(val)
#define GX_WRITE_U16(val) SIM_GX_CommandProcessor_SendU16(val)
#define GX_WRITE_U32(val) SIM_GX_CommandProcessor_SendU32(val)
#define GX_WRITE_S16(val) SIM_GX_CommandProcessor_SendS16(val)
#define GX_WRITE_F32(val) SIM_GX_CommandProcessor_SendF32(val)
#else
#define GX_WRITE_U8(val)  (GXWGFifo.u8 = val)
#define GX_WRITE_U16(val) (GXWGFifo.u16 = val)
#define GX_WRITE_U32(val) (GXWGFifo.u32 = (u32)val)
#define GX_WRITE_S16(val) (GXWGFifo.s16 = val)
#define GX_WRITE_F32(val) (GXWGFifo.f32 = (f32)val)
#endif


static inline void GXPosition1x8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXPosition1x16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXPosition2f32(const f32 x, const f32 y)
{
	GX_WRITE_F32(x);
	GX_WRITE_F32(y);
}

static inline void GXPosition3s16(const s16 x, const s16 y, const s16 z)
{
	GX_WRITE_S16(x);
	GX_WRITE_S16(y);
	GX_WRITE_S16(z);
}

static inline void GXPosition3s8(const s8 x, const s8 y, const s8 z)
{
	GX_WRITE_U8((u8)x);
	GX_WRITE_U8((u8)y);
	GX_WRITE_U8((u8)z);
}

static inline void GXPosition3u8(const u8 x, const u8 y, const u8 z)
{
	GX_WRITE_U8(x);
	GX_WRITE_U8(y);
	GX_WRITE_U8(z);
}

static inline void GXPosition3u16(const u16 x, const u16 y, const u16 z)
{
	GX_WRITE_U16(x);
	GX_WRITE_U16(y);
	GX_WRITE_U16(z);
}

static inline void GXPosition3f32(f32 x, f32 y, f32 z)
{
	GX_WRITE_F32(x);
	GX_WRITE_F32(y);
	GX_WRITE_F32(z);
}

static inline void GXNormal3f32(const f32 x, const f32 y, const f32 z)
{
	GX_WRITE_F32(x);
	GX_WRITE_F32(y);
	GX_WRITE_F32(z);
}

static inline void GXNormal1x8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXNormal1x16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXNormal3s8(const s8 x, const s8 y, const s8 z)
{
	GX_WRITE_U8((u8)x);
	GX_WRITE_U8((u8)y);
	GX_WRITE_U8((u8)z);
}

static inline void GXNormal3s16(const s16 x, const s16 y, const s16 z)
{
	GX_WRITE_S16(x);
	GX_WRITE_S16(y);
	GX_WRITE_S16(z);
}

static inline void GXColor1x8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXColor1x16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXColor1u16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXColor1u32(u32 c)
{
#ifdef LIBPORPOISE_PORT
	/* A PowerPC scalar write places R, G, B, A on the FIFO in that order.
	 * Splitting the packed value keeps the same contract on little-endian
	 * hosts without changing numeric U32 command writes. */
	GX_WRITE_U8((u8)(c >> 24));
	GX_WRITE_U8((u8)(c >> 16));
	GX_WRITE_U8((u8)(c >> 8));
	GX_WRITE_U8((u8)c);
#else
	GX_WRITE_U32(c);
#endif
}

static inline void GXColor4u8(const u8 r, const u8 g, const u8 b, const u8 a)
{
	GX_WRITE_U8(r);
	GX_WRITE_U8(g);
	GX_WRITE_U8(b);
	GX_WRITE_U8(a);
}

static inline void GXColor3u8(const u8 r, const u8 g, const u8 b)
{
	GX_WRITE_U8(r);
	GX_WRITE_U8(g);
	GX_WRITE_U8(b);
}

static inline void GXTexCoord1x8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXTexCoord1x16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXTexCoord2s8(const s8 u, const s8 v)
{
	GX_WRITE_U8((u8)u);
	GX_WRITE_U8((u8)v);
}

static inline void GXTexCoord2u8(u8 s, u8 t)
{
	GX_WRITE_U8(s);
	GX_WRITE_U8(t);
}

static inline void GXPosition2u16(u16 x, u16 y)
{
	GX_WRITE_U16(x);
	GX_WRITE_U16(y);
}

static inline void GXPosition2s16(s16 x, s16 y)
{
	GX_WRITE_S16(x);
	GX_WRITE_S16(y);
}

static inline void GXTexCoord2s16(const s16 u, const s16 v)
{
	GX_WRITE_S16(u);
	GX_WRITE_S16(v);
}

static inline void GXTexCoord2u16(const u16 u, const u16 v)
{
	GX_WRITE_U16(u);
	GX_WRITE_U16(v);
}

static inline void GXTexCoord2f32(const f32 u, const f32 v)
{
	GX_WRITE_F32(u);
	GX_WRITE_F32(v);
}

static inline void GXMatrixIndex1x8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXMatrixIndex1u8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXCmd1u8(const u8 x)
{
	GX_WRITE_U8(x);
}

static inline void GXParam1u16(const u16 x)
{
	GX_WRITE_U16(x);
}

static inline void GXParam1u32(const u32 x)
{
	GX_WRITE_U32(x);
}

static inline void GXEnd(void)
{
}

////////////////////////////////////////////

//////////// FIFO INIT/SET/SAVE ////////////
// Init.
extern void __GXFifoInit();
extern void GXInitFifoBase(GXFifoObj* obj, void* base, u32 size);
extern void GXInitFifoPtrs(GXFifoObj* obj, void* readPtr, void* writePtr);
extern void GXInitFifoLimits(GXFifoObj* obj, u32 hiWaterMark, u32 loWaterMark);

// Set.
extern void GXSetCPUFifo(GXFifoObj* obj);
extern void GXSetGPFifo(GXFifoObj* obj);
extern void GXSaveCPUFifo(GXFifoObj* obj);

////////////////////////////////////////////

/////////////// FIFO GETTERS ///////////////
extern void GXGetGPStatus(GXBool* isOverHi, GXBool* isUnderLo, GXBool* isReadIdle, GXBool* isCmdIdle, GXBool* isHitBrkPt);
extern GXFifoObj* GXGetCPUFifo();
extern GXFifoObj* GXGetGPFifo();
extern GXBool GXIsCPUGPFifoLinked(void);

u32 GXGetOverflowCount();
u32 GXResetOverflowCount();

////////////////////////////////////////////

//////////// DISPLAY LIST FUNCS ////////////
extern void GXBeginDisplayList(void* list, u32 size);
extern u32 GXEndDisplayList();
extern void GXCallDisplayList(const void* list, u32 numBytes);

////////////////////////////////////////////

///////////// BREAKPOINT FUNCS /////////////
extern GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback callback);

////////////////////////////////////////////

/////////////// OTHER FUNCS ////////////////
void __GXSaveCPUFifoAux(__GXFifoObj* obj);
void __GXFifoReadEnable();
void __GXFifoReadDisable();
void __GXFifoLink(u8);
void __GXWriteFifoIntEnable(u8, u8);
void __GXWriteFifoIntReset(u8, u8);
void __GXCleanGPFifo(void);

// Unused/inlined in P2.
extern void GXSaveGPFifo(GXFifoObj* obj);

extern void GXGetFifoStatus(GXFifoObj* obj, GXBool* isOverHi, GXBool* isUnderLo, u32* fifoCount, GXBool* isCpuWrite, GXBool* isGPRead,
                            GXBool* isFifoWrap);
extern void GXGetFifoPtrs(GXFifoObj* obj, void** readPtr, void** writePtr);
extern void* GXGetFifoBase(const GXFifoObj* obj);
extern u32 GXGetFifoSize(const GXFifoObj* obj);
extern void GXGetFifoLimits(const GXFifoObj* obj, u32* hi, u32* lo);
extern u32 GXGetFifoCount(const GXFifoObj* obj);
extern GXBool GXGetFifoWrap(const GXFifoObj* obj);

extern void GXEnableBreakPt(void* breakPtr);
extern void GXDisableBreakPt();

////////////////////////////////////////////

END_SCOPE_EXTERN_C

#endif
