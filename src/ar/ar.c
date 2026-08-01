#include <dolphin/ar.h>
#include <dolphin/hw_regs.h>
#include <dolphin/os.h>
#include <stddef.h>
#include <string.h>
#ifdef LIBPORPOISE_PORT
#include <dolphin/os/OSHostAddress.h>
#include <dolphin/os/OSHostMemory.h>
#include <stdint.h>
#include <stdio.h>
#endif

static ARCallback __AR_Callback;
static u32 __AR_Size;
#if OS_BUILD_VERSION >= 20011217L
static u32 __AR_InternalSize;
static u32 __AR_ExpansionSize;
#endif
static u32 __AR_StackPointer;
static u32 __AR_FreeBlocks;
static u32* __AR_BlockLength;
#ifdef LIBPORPOISE_PORT
static u32* __AR_BlockLengthBase;
static u32 __AR_AllocatedBlocks;
#endif

static volatile BOOL __AR_init_flag = FALSE;

static void __ARHandler(__OSInterrupt interrupt, OSContext* context);
static void __ARChecksize(void);

#ifdef LIBPORPOISE_PORT
#define HOST_ARAM_SIZE 0x01000000U

static u8 __ARHostMemory[HOST_ARAM_SIZE];
static volatile BOOL __ARHostDMABusy = FALSE;
static volatile ARDMAResult __ARHostLastDMAResult =
	AR_DMA_RESULT_NOT_STARTED;

static ARDMAResult __ARHostValidateMainMemoryRange(
	u32 address,
	u32 length,
	void** pointer)
{
	const OSHostMemoryLayout* layout;
	uintptr_t decodedAddress;
	uintptr_t cachedBase;
	uintptr_t uncachedBase;
	u32 offset;

	*pointer = __OSHostDecodeAddress(address);
	if (*pointer == NULL) {
		return AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE;
	}
	if (((uintptr_t)*pointer & (ARQ_DMA_ALIGNMENT - 1U)) != 0) {
		return AR_DMA_RESULT_INVALID_ALIGNMENT;
	}

	/*
	 * A token proves that the pointer is live, but the legacy u32 API carries
	 * no allocation length. The caller remains responsible for the capacity
	 * of a token-backed buffer. Strict console-memory addresses can be
	 * bounds-checked completely.
	 */
	if (__OSHostIsAddressToken(address)) {
		return AR_DMA_RESULT_SUCCESS;
	}

	layout = __OSHostMemoryGetLayout();
	if (layout == NULL) {
		return AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE;
	}

	decodedAddress = (uintptr_t)*pointer;
	cachedBase = (uintptr_t)layout->cachedBase;
	uncachedBase = (uintptr_t)layout->uncachedBase;
	if (decodedAddress >= cachedBase &&
	    decodedAddress - cachedBase < layout->size) {
		offset = (u32)(decodedAddress - cachedBase);
	} else if (decodedAddress >= uncachedBase &&
	           decodedAddress - uncachedBase < layout->size) {
		offset = (u32)(decodedAddress - uncachedBase);
	} else {
		return AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE;
	}
	return length <= layout->size - offset
	           ? AR_DMA_RESULT_SUCCESS
	           : AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE;
}

static ARDMAResult __ARHostValidateDMA(
	u32 type,
	u32 mainmem_addr,
	u32 aram_addr,
	u32 length,
	void** mainMemory)
{
	if (type != ARAM_DIR_MRAM_TO_ARAM &&
	    type != ARAM_DIR_ARAM_TO_MRAM) {
		return AR_DMA_RESULT_INVALID_DIRECTION;
	}
	if ((aram_addr & (ARQ_DMA_ALIGNMENT - 1U)) != 0 ||
	    (length & (ARQ_DMA_ALIGNMENT - 1U)) != 0) {
		return AR_DMA_RESULT_INVALID_ALIGNMENT;
	}
	if (aram_addr > HOST_ARAM_SIZE ||
	    length > HOST_ARAM_SIZE - aram_addr) {
		return AR_DMA_RESULT_INVALID_ARAM_RANGE;
	}
	return __ARHostValidateMainMemoryRange(
	    mainmem_addr,
	    length,
	    mainMemory);
}

static const char* __ARHostDMAResultReason(ARDMAResult result)
{
	switch (result) {
	case AR_DMA_RESULT_BUSY:
		return "another transfer is already in progress";
	case AR_DMA_RESULT_INVALID_DIRECTION:
		return "invalid transfer direction";
	case AR_DMA_RESULT_INVALID_ALIGNMENT:
		return "main-memory pointer, ARAM address, or length is not 32-byte aligned";
	case AR_DMA_RESULT_INVALID_ARAM_RANGE:
		return "ARAM transfer is outside the 16 MiB backing store";
	case AR_DMA_RESULT_INVALID_MAIN_MEMORY_RANGE:
		return "main-memory address is invalid, stale, or out of bounds";
	default:
		return "unknown failure";
	}
}

static void __ARHostReportDMAFailure(
	ARDMAResult result,
	u32 type,
	u32 mainmem_addr,
	u32 aram_addr,
	u32 length)
{
	fprintf(
	    stderr,
	    "libPorpoise AR: rejected DMA "
	    "(type=%u, mram=0x%08x, aram=0x%08x, length=%u): %s.\n",
	    type,
	    mainmem_addr,
	    aram_addr,
	    length,
	    __ARHostDMAResultReason(result));
}
#endif

/**
 * @TODO: Documentation
 */
ARCallback ARRegisterDMACallback(ARCallback callback)
{
	ARCallback oldCb;
	BOOL enabled;
	oldCb         = __AR_Callback;
	enabled       = OSDisableInterrupts();
	__AR_Callback = callback;
	OSRestoreInterrupts(enabled);
	return oldCb;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00003C
 */
u32 ARGetDMAStatus(void)
{
#ifdef LIBPORPOISE_PORT
	return __ARHostDMABusy ? 1U : 0U;
#else
	return __DSPRegs[DSP_CONTROL_STATUS] & 0x0200U;
#endif
}

#ifdef LIBPORPOISE_PORT
ARDMAResult ARValidateDMA(
	u32 type,
	u32 mainmem_addr,
	u32 aram_addr,
	u32 length)
{
	void* mainMemory = NULL;

	return __ARHostValidateDMA(
	    type,
	    mainmem_addr,
	    aram_addr,
	    length,
	    &mainMemory);
}

ARDMAResult ARGetLastDMAResult(void)
{
	return __ARHostLastDMAResult;
}

ARDMAResult ARStartDMAEx(
	u32 type,
	u32 mainmem_addr,
	u32 aram_addr,
	u32 length)
{
	ARDMAResult result;
	BOOL enabled;
	void* mainMemory = NULL;

	OSCheckAlarmQueue();
	enabled = OSDisableInterrupts();

	if (__ARHostDMABusy) {
		result = AR_DMA_RESULT_BUSY;
		__ARHostLastDMAResult = result;
		__ARHostReportDMAFailure(
		    result,
		    type,
		    mainmem_addr,
		    aram_addr,
		    length);
		OSRestoreInterrupts(enabled);
		OSCheckAlarmQueue();
		return result;
	}

	result = __ARHostValidateDMA(
	    type,
	    mainmem_addr,
	    aram_addr,
	    length,
	    &mainMemory);
	if (result == AR_DMA_RESULT_SUCCESS) {
		__ARHostDMABusy = TRUE;
		__DSPRegs[DSP_CONTROL_STATUS] |= 0x0200U;

		// Set main mem address
		__DSPRegs[DSP_ARAM_DMA_MM_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_MM_HI] & ~0x3ff) | (u16)(mainmem_addr >> 16);
		__DSPRegs[DSP_ARAM_DMA_MM_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_MM_LO] & ~0xffe0) | (u16)(mainmem_addr & 0xffff);

		// Set ARAM address
		__DSPRegs[DSP_ARAM_DMA_ARAM_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_ARAM_HI] & ~0x3ff) | (u16)(aram_addr >> 16);
		__DSPRegs[DSP_ARAM_DMA_ARAM_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_ARAM_LO] & ~0xffe0) | (u16)(aram_addr & 0xffff);

		// Set DMA buffer size
		__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x8000) | (type << 15));
		__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x3ff) | (u16)(length >> 16);
		__DSPRegs[DSP_ARAM_DMA_SIZE_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_LO] & ~0xffe0) | (u16)(length & 0xffff);

		if (length != 0) {
			if (type == ARAM_DIR_MRAM_TO_ARAM) {
				memcpy(&__ARHostMemory[aram_addr], mainMemory, length);
			} else {
				memcpy(mainMemory, &__ARHostMemory[aram_addr], length);
			}
		}

		__ARHostDMABusy = FALSE;
		__DSPRegs[DSP_CONTROL_STATUS] &= (u16)~0x0200U;
	} else {
		__ARHostReportDMAFailure(
		    result,
		    type,
		    mainmem_addr,
		    aram_addr,
		    length);
	}

	/* Publish completion before invoking the synchronous host callback. */
	__ARHostLastDMAResult = result;
	if (__AR_Callback != NULL) {
		(*__AR_Callback)();
	}

	OSRestoreInterrupts(enabled);
	OSCheckAlarmQueue();
	return result;
}
#endif

/**
 * @TODO: Documentation
 */
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length)
{
#ifdef LIBPORPOISE_PORT
	(void)ARStartDMAEx(type, mainmem_addr, aram_addr, length);
#else
	BOOL enabled;
	enabled = OSDisableInterrupts();

	// Set main mem address
	__DSPRegs[DSP_ARAM_DMA_MM_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_MM_HI] & ~0x3ff) | (u16)(mainmem_addr >> 16);
	__DSPRegs[DSP_ARAM_DMA_MM_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_MM_LO] & ~0xffe0) | (u16)(mainmem_addr & 0xffff);

	// Set ARAM address
	__DSPRegs[DSP_ARAM_DMA_ARAM_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_ARAM_HI] & ~0x3ff) | (u16)(aram_addr >> 16);
	__DSPRegs[DSP_ARAM_DMA_ARAM_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_ARAM_LO] & ~0xffe0) | (u16)(aram_addr & 0xffff);

	// Set DMA buffer size
	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x8000) | (type << 15));
	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x3ff) | (u16)(length >> 16);
	__DSPRegs[DSP_ARAM_DMA_SIZE_LO] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_LO] & ~0xffe0) | (u16)(length & 0xffff);
	OSRestoreInterrupts(enabled);
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000068
 */
u32 ARAlloc(u32 length)
{
	u32 address;
	BOOL old;

	old = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	/*
	 * The SDK allocator is a stack whose index table is supplied to ARInit().
	 * Host builds reject invalid requests instead of relying on SDK assertions,
	 * which are normally compiled out in this configuration.
	 */
	if (!__AR_init_flag ||
	    (length & (ARQ_DMA_ALIGNMENT - 1U)) != 0 ||
	    __AR_FreeBlocks == 0 ||
	    __AR_BlockLength == NULL ||
	    __AR_BlockLengthBase == NULL ||
	    __AR_StackPointer < __AR_ARAM_USR_BASE_ADDR ||
	    __AR_StackPointer > __AR_Size ||
	    length > __AR_Size - __AR_StackPointer) {
		OSRestoreInterrupts(old);
		return 0;
	}
#else
	ASSERTMSG((length % ARQ_DMA_ALIGNMENT) == 0,
	          "ARAlloc(): length is not multiple of 32bytes!");
	ASSERTMSG(length <= __AR_Size - __AR_StackPointer,
	          "ARAlloc(): Out of ARAM!");
	ASSERTMSG(__AR_FreeBlocks, "ARAlloc(): No more free blocks!");
#endif

	address = __AR_StackPointer;
	__AR_StackPointer += length;
	*__AR_BlockLength = length;
	__AR_BlockLength++;
	__AR_FreeBlocks--;
#ifdef LIBPORPOISE_PORT
	__AR_AllocatedBlocks++;
#endif

	OSRestoreInterrupts(old);
	return address;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000074
 */
u32 ARFree(u32* length)
{
	u32 blockLength;
	BOOL old;

	old = OSDisableInterrupts();

#ifdef LIBPORPOISE_PORT
	if (!__AR_init_flag ||
	    __AR_AllocatedBlocks == 0 ||
	    __AR_BlockLength == NULL ||
	    __AR_BlockLengthBase == NULL ||
	    __AR_BlockLength <= __AR_BlockLengthBase) {
		if (length != NULL) {
			*length = 0;
		}
		OSRestoreInterrupts(old);
		return 0;
	}

	blockLength = __AR_BlockLength[-1];
	if ((blockLength & (ARQ_DMA_ALIGNMENT - 1U)) != 0 ||
	    __AR_StackPointer < __AR_ARAM_USR_BASE_ADDR ||
	    blockLength > __AR_StackPointer - __AR_ARAM_USR_BASE_ADDR) {
		if (length != NULL) {
			*length = 0;
		}
		OSRestoreInterrupts(old);
		return 0;
	}
#else
	__AR_BlockLength--;
	blockLength = *__AR_BlockLength;
#endif

#ifdef LIBPORPOISE_PORT
	__AR_BlockLength--;
#endif
	__AR_StackPointer -= blockLength;
	__AR_FreeBlocks++;
#ifdef LIBPORPOISE_PORT
	__AR_AllocatedBlocks--;
#endif

	if (length != NULL) {
		*length = blockLength;
	}

	OSRestoreInterrupts(old);
	return __AR_StackPointer;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008
 */
BOOL ARCheckInit(void)
{
#ifdef LIBPORPOISE_PORT
	return __AR_init_flag;
#else
	TRAP_UNIMPLEMENTED;
	return FALSE;
#endif
}

/**
 * @TODO: Documentation
 */
u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
	BOOL old;
#if OS_BUILD_VERSION >= 20011217L
#else
	u16 refresh;
#endif

	if (__AR_init_flag == TRUE) {
		return __AR_ARAM_USR_BASE_ADDR;
	}

	old = OSDisableInterrupts();

	__AR_Callback = NULL;

#ifdef LIBPORPOISE_PORT
	__AR_StackPointer = __AR_ARAM_USR_BASE_ADDR;
	__AR_FreeBlocks = num_entries;
	__AR_BlockLength = stack_index_addr;
	__AR_BlockLengthBase = stack_index_addr;
	__AR_AllocatedBlocks = 0;
	__AR_Size = HOST_ARAM_SIZE;
#if OS_BUILD_VERSION >= 20011217L
	__AR_InternalSize = HOST_ARAM_SIZE;
	__AR_ExpansionSize = 0;
#endif
	__ARHostDMABusy = FALSE;
	__ARHostLastDMAResult = AR_DMA_RESULT_NOT_STARTED;
	memset(__ARHostMemory, 0, sizeof(__ARHostMemory));
	__AR_init_flag = TRUE;
	OSRestoreInterrupts(old);
	return __AR_StackPointer;
#else
	__OSSetInterruptHandler(__OS_INTERRUPT_DSP_ARAM, __ARHandler);
	__OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_ARAM);

	__AR_StackPointer = __AR_ARAM_USR_BASE_ADDR;
	__AR_FreeBlocks   = num_entries;
	__AR_BlockLength  = stack_index_addr;

#if OS_BUILD_VERSION >= 20011217L
	// WHY?
	__DSPRegs[DSP_ARAM_REFRESH] = __DSPRegs[DSP_ARAM_REFRESH] & 0xff | __DSPRegs[DSP_ARAM_REFRESH] & ~0xff;
#else
	refresh = 196.0f * (OS_BUS_CLOCK / 202500000.0f);

	__DSPRegs[DSP_ARAM_REFRESH] = (u16)((__DSPRegs[DSP_ARAM_REFRESH] & ~0xFF) | (refresh & 0xFF));
#endif

	__ARChecksize();

	__AR_init_flag = TRUE;

	OSRestoreInterrupts(old);

	return __AR_StackPointer;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00000C
 */
void ARReset(void)
{
#ifdef LIBPORPOISE_PORT
	BOOL old = OSDisableInterrupts();

	__AR_init_flag = FALSE;
	__AR_Callback = NULL;
	__AR_StackPointer = __AR_ARAM_USR_BASE_ADDR;
	__AR_FreeBlocks = 0;
	__AR_BlockLength = NULL;
	__AR_BlockLengthBase = NULL;
	__AR_AllocatedBlocks = 0;
	__ARHostDMABusy = FALSE;
	__ARHostLastDMAResult = AR_DMA_RESULT_NOT_STARTED;
	memset(__ARHostMemory, 0, sizeof(__ARHostMemory));

	OSRestoreInterrupts(old);
#else
	__AR_init_flag = FALSE;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000004
 */
void ARSetSize(void)
{
	TRAP_UNIMPLEMENTED;
}

/**
 * @TODO: Documentation
 */
u32 ARGetBaseAddress()
{
	return __AR_ARAM_USR_BASE_ADDR;
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000008
 */
u32 ARGetSize(void)
{
#ifdef LIBPORPOISE_PORT
	return __AR_Size;
#else
	TRAP_UNIMPLEMENTED;
	return 0;
#endif
}

/**
 * @TODO: Documentation
 */
void __ARHandler(__OSInterrupt interrupt, OSContext* context)
{
	OSContext exceptionContext;
	u16 tmp;

	tmp                           = __DSPRegs[DSP_CONTROL_STATUS];
	tmp                           = (u16)((tmp & ~(0x80 | 0x8)) | 0x20);
	__DSPRegs[DSP_CONTROL_STATUS] = tmp;

	OSClearContext(&exceptionContext);
	OSSetCurrentContext(&exceptionContext);

	if (__AR_Callback) {
		(*__AR_Callback)();
	}

	OSClearContext(&exceptionContext);
	OSSetCurrentContext(context);
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 000018 (Matching by size)
 */
void __ARWaitForDMA(void)
{
	do {
	} while ((__DSPRegs[DSP_CONTROL_STATUS] & 0x200));
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00009C
 */
void __ARWriteDMA(u32 mmem_addr, u32 aram_addr, u32 length)
{
	// Main mem address
	__DSPRegs[DSP_ARAM_DMA_MM_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_MM_HI] & ~0x03ff) | (u16)(mmem_addr >> 16));
	__DSPRegs[DSP_ARAM_DMA_MM_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_MM_LO] & ~0xffe0) | (u16)(mmem_addr & 0xffff));

	// ARAM address
	__DSPRegs[DSP_ARAM_DMA_ARAM_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_ARAM_HI] & ~0x03ff) | (u16)(aram_addr >> 16));
	__DSPRegs[DSP_ARAM_DMA_ARAM_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_ARAM_LO] & ~0xffe0) | (u16)(aram_addr & 0xffff));

	// DMA buffer size
	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x8000);

	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x03ff) | (u16)(length >> 16));
	__DSPRegs[DSP_ARAM_DMA_SIZE_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_LO] & ~0xffe0) | (u16)(length & 0xffff));

	__ARWaitForDMA();

#if OS_BUILD_VERSION >= 20011217L
	__DSPRegs[DSP_CONTROL_STATUS] = __DSPRegs[DSP_CONTROL_STATUS] & ~0x88 | 0x20;
#endif
}

/**
 * @TODO: Documentation
 * @note UNUSED Size: 00009C
 */
void __ARReadDMA(u32 mmem_addr, u32 aram_addr, u32 length)
{
	// Main mem address
	__DSPRegs[DSP_ARAM_DMA_MM_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_MM_HI] & ~0x03ff) | (u16)(mmem_addr >> 16));
	__DSPRegs[DSP_ARAM_DMA_MM_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_MM_LO] & ~0xffe0) | (u16)(mmem_addr & 0xffff));

	// ARAM address
	__DSPRegs[DSP_ARAM_DMA_ARAM_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_ARAM_HI] & ~0x03ff) | (u16)(aram_addr >> 16));
	__DSPRegs[DSP_ARAM_DMA_ARAM_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_ARAM_LO] & ~0xffe0) | (u16)(aram_addr & 0xffff));

	// DMA buffer size
	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)(__DSPRegs[DSP_ARAM_DMA_SIZE_HI] | 0x8000);

	__DSPRegs[DSP_ARAM_DMA_SIZE_HI] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_HI] & ~0x03ff) | (u16)(length >> 16));
	__DSPRegs[DSP_ARAM_DMA_SIZE_LO] = (u16)((__DSPRegs[DSP_ARAM_DMA_SIZE_LO] & ~0xffe0) | (u16)(length & 0xffff));

	__ARWaitForDMA();

#if OS_BUILD_VERSION >= 20011217L
	__DSPRegs[DSP_CONTROL_STATUS] = __DSPRegs[DSP_CONTROL_STATUS] & ~0x88 | 0x20;
#endif
}

// Really repetitive changes to the following function that are better represented by macros.
#if OS_BUILD_VERSION >= 20011217L
#define __ARWaitForDMAToFinish(buffer, size) PPCSync()
#define __ARSetExpansionSize(value)          (__AR_ExpansionSize = (value))
#else
#define __ARWaitForDMAToFinish(buffer, size) DCInvalidateRange(buffer, size)
#define __ARSetExpansionSize(value)          ((void)0)
#endif

/**
 * @TODO: Documentation
 */
void __ARChecksize(void)
{
	u8 test_data_pad[63];
	u8 dummy_data_pad[63];
	u8 buffer_pad[63];
	u32* test_data;
	u32* dummy_data;
	u32* buffer;
	u16 ARAM_mode;
	u32 ARAM_size;
	u32 i;

#if OS_BUILD_VERSION >= 20011217L
	do {
	} while (!(__DSPRegs[DSP_ARAM_MODE] & 1));

	ARAM_mode = 3;
	ARAM_size = __AR_InternalSize = 0x1000000;

	__DSPRegs[DSP_ARAM_SIZE] = ((__DSPRegs[DSP_ARAM_SIZE] & 0xFFFFFFC0) | ARAM_mode) | 0x20;
#else
	ARAM_mode = 0;
	ARAM_size = 0;
#endif

	test_data  = (u32*)(OSRoundUp32B((u32)(test_data_pad)));
	dummy_data = (u32*)(OSRoundUp32B((u32)(dummy_data_pad)));
	buffer     = (u32*)(OSRoundUp32B((u32)(buffer_pad)));

	for (i = 0; i < 8; i++) {
		test_data[i]  = 0xDEADBEEF;
		dummy_data[i] = 0xBAD0BAD0;
	}

	DCFlushRange(test_data, 0x20);
	DCFlushRange(dummy_data, 0x20);

#if OS_BUILD_VERSION >= 20011217L
#else
	do {
	} while (!(__DSPRegs[DSP_ARAM_MODE] & 1));

	__DSPRegs[DSP_ARAM_SIZE] = ((__DSPRegs[DSP_ARAM_SIZE] & 0xFFFFFFC0) | 4) | 0x20;

	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x0, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x200000, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x200, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x1000000, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x400000, 0x20U);

	memset(buffer, 0, 0x20);
	DCFlushRange(buffer, 0x20);
	__ARWriteDMA((u32)test_data, 0U, 0x20U);
	__ARReadDMA((u32)buffer, 0U, 0x20U);
	DCInvalidateRange(buffer, 0x20);

	if (*buffer == *test_data) {
		memset(buffer, 0, 0x20);
		DCFlushRange(buffer, 0x20);
		__ARReadDMA((u32)buffer, 0x200000U, 0x20U);
		DCInvalidateRange(buffer, 0x20);
		if (*buffer == *test_data) {
			ARAM_mode = 0;
			ARAM_size = 0x200000;
		} else {
			memset(buffer, 0, 0x20);
			DCFlushRange(buffer, 0x20);
			__ARReadDMA((u32)buffer, 0x01000000U, 0x20U);
			DCInvalidateRange(buffer, 0x20);

			if (*buffer == *test_data) {
				ARAM_mode = 1;
				ARAM_size = 0x400000;

			} else {
				memset(buffer, 0, 0x20);
				DCFlushRange(buffer, 0x20);
				__ARReadDMA((u32)buffer, 0x200U, 0x20U);
				DCInvalidateRange(buffer, 0x20);

				if (*buffer == *test_data) {
					ARAM_mode = 2;
					ARAM_size = 0x800000;

				} else {
					memset(buffer, 0, 0x20);
					DCFlushRange(buffer, 0x20);
					__ARReadDMA((u32)buffer, 0x400000U, 0x20U);
					DCInvalidateRange(buffer, 0x20);

					if (*buffer == *test_data) {
						ARAM_mode = 3;
						ARAM_size = 0x01000000;

					} else {
						ARAM_mode = 4;
						ARAM_size = 0x02000000;
					}
				}
			}
		}
	}

	__DSPRegs[DSP_ARAM_SIZE] = (u16)((__DSPRegs[DSP_ARAM_SIZE] & 0xFFFFFFC0) | 0x20) | ARAM_mode;
#endif

	__ARSetExpansionSize(0);

	__ARWriteDMA((u32)dummy_data, ARAM_size, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x200000, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x01000000, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x200, 0x20U);
	__ARWriteDMA((u32)dummy_data, ARAM_size + 0x400000, 0x20U);

	memset(buffer, 0, 0x20);
	DCFlushRange(buffer, 0x20);
	__ARWriteDMA((u32)test_data, ARAM_size, 0x20U);
#if OS_BUILD_VERSION >= 20011217L
	DCInvalidateRange(buffer, 0x20U); // Probably related to the revisional difference in `__ARWaitForDMAToFinish`.
#endif
	__ARReadDMA((u32)buffer, ARAM_size, 0x20U);
	__ARWaitForDMAToFinish(buffer, 0x20);

	if (*buffer == *test_data) {
		memset(buffer, 0, 0x20);
		DCFlushRange(buffer, 0x20);
		__ARReadDMA((u32)buffer, ARAM_size + 0x200000, 0x20U);
		__ARWaitForDMAToFinish(buffer, 0x20);

		if (*buffer == *test_data) {
			ARAM_size += 0x200000;
			__ARSetExpansionSize(0x200000);
		} else {
			memset(buffer, 0, 0x20);
			DCFlushRange(buffer, 0x20);
			__ARReadDMA((u32)buffer, ARAM_size + 0x01000000, 0x20U);
			__ARWaitForDMAToFinish(buffer, 0x20);

			if (*buffer == *test_data) {
				ARAM_mode |= 8;
				ARAM_size += 0x400000;
				__ARSetExpansionSize(0x400000);
			} else {
				memset(buffer, 0, 0x20);
				DCFlushRange(buffer, 0x20);
				__ARReadDMA((u32)buffer, ARAM_size + 0x200, 0x20U);
				__ARWaitForDMAToFinish(buffer, 0x20);

				if (*buffer == *test_data) {
					ARAM_mode |= 0x10;
					ARAM_size += 0x800000;
					__ARSetExpansionSize(0x800000);
				} else {
					memset(buffer, 0, 0x20);
					DCFlushRange(buffer, 0x20);
					__ARReadDMA((u32)buffer, ARAM_size + 0x400000, 0x20U);
					__ARWaitForDMAToFinish(buffer, 0x20);

					if (*buffer == *test_data) {
						ARAM_mode |= 0x18;
						ARAM_size += 0x01000000;
						__ARSetExpansionSize(0x1000000);
					} else {
						ARAM_mode |= 0x20;
						ARAM_size += 0x02000000;
						__ARSetExpansionSize(0x2000000);
					}
				}
			}
		}
		__DSPRegs[DSP_ARAM_SIZE] = ((u16)(__DSPRegs[DSP_ARAM_SIZE] & 0xFFFFFFC0) | ARAM_mode);
	}
	*(u32*)OSPhysicalToUncached(0xD0) = ARAM_size;
	__AR_Size                         = ARAM_size;
}

#undef __ARWaitForDMAToFinish
#undef __ARSetExpansionSize
