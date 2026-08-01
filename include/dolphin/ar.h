#ifndef _DOLPHIN_AR_H
#define _DOLPHIN_AR_H

#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

///////////////// AR TYPES /////////////////
typedef struct ARQRequest ARQRequest;

// AR callback function type.
typedef void (*ARCallback)(void);

// ARQ callback function type.
typedef void (*ARQCallback)(u32 ptrToRequest);

#ifdef LIBPORPOISE_PORT
/*
 * Host DMA completion result. The SDK's ARStartDMA() remains source-compatible
 * and discards this value; host callers that need reliable failure reporting
 * can use ARStartDMAEx() or query ARGetLastDMAResult().
 */
typedef enum ARDMAResult {
	AR_DMA_RESULT_SUCCESS                   = 0,
	AR_DMA_RESULT_NOT_STARTED               = -1,
	AR_DMA_RESULT_BUSY                      = -2,
	AR_DMA_RESULT_INVALID_DIRECTION         = -3,
	AR_DMA_RESULT_INVALID_ALIGNMENT         = -4,
	AR_DMA_RESULT_INVALID_ARAM_RANGE        = -5,
	AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE = -6,
} ARDMAResult;
#endif

struct ARQRequest {
	ARQRequest* next;     // _00
	u32 owner;            // _04
	u32 type;             // _08
	u32 priority;         // _0C
	u32 source;           // _10
	u32 dest;             // _14
	u32 length;           // _18
	ARQCallback callback; // _1C
};

////////////////////////////////////////////

/////////////// AR FUNCTIONS ///////////////
// ARQ functions.
void __ARQServiceQueueLo();
void __ARQCallbackHack();
void __ARQInterruptServiceRoutine();
void ARQInit();
BOOL ARQCheckInit(void);
void ARQReset(void);
void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length, ARQCallback callback);
void ARQSetChunkSize(u32 size);
u32 ARQGetChunkSize(void);

// AR functions.
ARCallback ARRegisterDMACallback(ARCallback callback);
u32 ARGetDMAStatus(void);
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
#ifdef LIBPORPOISE_PORT
ARDMAResult ARValidateDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
ARDMAResult ARStartDMAEx(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
ARDMAResult ARGetLastDMAResult(void);
#endif
u32 ARInit(u32* stack_index_addr, u32 num_entries);
BOOL ARCheckInit(void);
u32 ARAlloc(u32 length);
u32 ARFree(u32* length);
void ARReset(void);
u32 ARGetBaseAddress();
u32 ARGetSize(void);

////////////////////////////////////////////
u16 __ARGetInterruptStatus();
//////////////// AR DEFINES ////////////////
// AR defines.
#define AR_STACK_INDEX_ENTRY_SIZE sizeof(u32)

#define ARAM_DIR_MRAM_TO_ARAM 0x00
#define ARAM_DIR_ARAM_TO_MRAM 0x01

#define AR_CLEAR_INTERNAL_ALL  0x00
#define AR_CLEAR_INTERNAL_USER 0x01
#define AR_CLEAR_EXPANSION     0x02

#define __AR_ARAM_OS_BASE_ADDR  0x0000 // OS area at bottom of ARAM
#define __AR_ARAM_USR_BASE_ADDR 0x4000 // USR area at 16KB (0x4000)

// AR macros.
#define ARStartDMARead(mmem, aram, len)  ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, mmem, aram, len)
#define ARStartDMAWrite(mmem, aram, len) ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, mmem, aram, len)

// ARQ defines.
#define ARQ_DMA_ALIGNMENT      32
#define ARQ_CHUNK_SIZE_DEFAULT 4096

#define ARQ_TYPE_MRAM_TO_ARAM ARAM_DIR_MRAM_TO_ARAM
#define ARQ_TYPE_ARAM_TO_MRAM ARAM_DIR_ARAM_TO_MRAM

#define ARQ_PRIORITY_LOW  0
#define ARQ_PRIORITY_HIGH 1

////////////////////////////////////////////

END_SCOPE_EXTERN_C

#endif
