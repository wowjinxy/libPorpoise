/**
 * @file ar.h
 * @brief ARAM (Auxiliary RAM / Audio RAM) API for libPorpoise
 * 
 * ARAM is 16MB of auxiliary memory on GameCube used primarily for audio.
 * On PC, we simulate ARAM using regular heap memory.
 */

#ifndef DOLPHIN_AR_H
#define DOLPHIN_AR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// ARAM sizes (GameCube has 16MB internal ARAM)
#define AR_INTERNAL_SIZE    0x1000000   // 16MB
#define AR_EXPANSION_SIZE   0           // No expansion on retail units

// DMA types
#define AR_MRAM_TO_ARAM     0
#define AR_ARAM_TO_MRAM     1

// Reserved area (OS uses bottom 16KB)
#define AR_OS_RESERVED      0x4000      // 16KB

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief ARAM DMA callback function type
 */
typedef void (*ARCallback)(void);

/**
 * @brief ARQ request structure (for queued DMA operations)
 */
typedef struct ARQRequest {
    struct ARQRequest* next;        ///< Next request in queue
    u32   owner;                    ///< Owner ID
    u32   type;                     ///< DMA type (MRAM_TO_ARAM or ARAM_TO_MRAM)
    u32   priority;                 ///< Priority (0=high, 1=low)
    u32   source;                   ///< Source address
    u32   dest;                     ///< Destination address
    u32   length;                   ///< Transfer length
    void (*callback)(struct ARQRequest*);  ///< Completion callback
} ARQRequest;

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize ARAM subsystem
 * 
 * Allocates simulated ARAM and sets up DMA system.
 * 
 * @param stackIndexAddr  Pointer to stack index for allocation tracking
 * @param numEntries      Number of allocation entries
 * @return Base address of user ARAM (after OS reserved area)
 */
u32 ARInit(u32* stackIndexAddr, u32 numEntries);

/**
 * @brief Reset ARAM subsystem
 */
void ARReset(void);

/**
 * @brief Get total ARAM size
 * 
 * @return ARAM size in bytes (16MB on GameCube)
 */
u32 ARGetSize(void);

/**
 * @brief Get internal ARAM size
 * 
 * @return Internal ARAM size in bytes
 */
u32 ARGetInternalSize(void);

/**
 * @brief Get base address of user ARAM
 * 
 * @return Base address (after OS reserved area)
 */
u32 ARGetBaseAddress(void);

/**
 * @brief Allocate ARAM space
 * 
 * @param length  Number of bytes to allocate
 * @return ARAM address, or 0 if allocation failed
 */
u32 ARAlloc(u32 length);

/**
 * @brief Free ARAM space
 * 
 * @param length  Pointer to receive size freed
 * @return ARAM address of freed block
 */
u32 ARFree(u32* length);

/**
 * @brief Check if ARAM is initialized
 * 
 * @return TRUE if ARInit() has been called
 */
BOOL ARCheckInit(void);

/**
 * @brief Clear ARAM contents
 * 
 * @param flag  Clear flag (non-zero to clear)
 */
void ARClear(u32 flag);

/**
 * @brief Start DMA transfer between ARAM and main RAM
 * 
 * @param type          Transfer type (AR_MRAM_TO_ARAM or AR_ARAM_TO_MRAM)
 * @param mainmemAddr   Main memory address (must be 32-byte aligned)
 * @param aramAddr      ARAM address (must be 32-byte aligned)
 * @param length        Transfer length (must be multiple of 32 bytes)
 */
void ARStartDMA(u32 type, u32 mainmemAddr, u32 aramAddr, u32 length);

/**
 * @brief Get DMA status
 * 
 * @return 0 if DMA idle, non-zero if busy
 */
u32 ARGetDMAStatus(void);

/**
 * @brief Register DMA completion callback
 * 
 * @param callback  Callback function
 * @return Previous callback
 */
ARCallback ARRegisterDMACallback(ARCallback callback);

/**
 * @brief Clear ARAM interrupt
 */
void __ARClearInterrupt(void);

/**
 * @brief Get ARAM interrupt status
 */
u32 __ARGetInterruptStatus(void);

/*---------------------------------------------------------------------------*
    ARQ - ARAM Queue (for queued DMA operations)
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize ARQ (ARAM Queue) subsystem
 */
void ARQInit(void);

/**
 * @brief Reset ARQ subsystem
 */
void ARQReset(void);

/**
 * @brief Post a DMA request to the queue
 * 
 * @param request   ARQ request structure
 * @param owner     Owner ID
 * @param type      DMA type
 * @param priority  Priority (0=high, 1=low)
 * @param source    Source address
 * @param dest      Destination address
 * @param length    Transfer length
 * @param callback  Completion callback
 */
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority,
                    u32 source, u32 dest, u32 length,
                    void (*callback)(ARQRequest*));

/**
 * @brief Remove a request from the queue
 * 
 * @param request  Request to remove
 */
void ARQRemoveRequest(ARQRequest* request);

/**
 * @brief Remove all requests from specific owner
 * 
 * @param owner  Owner ID
 */
void ARQRemoveOwnerRequest(u32 owner);

/**
 * @brief Flush all pending requests
 */
void ARQFlushQueue(void);

/**
 * @brief Set chunk size for DMA transfers
 * 
 * @param size  Chunk size in bytes
 */
void ARQSetChunkSize(u32 size);

/**
 * @brief Get current chunk size
 * 
 * @return Chunk size in bytes
 */
u32 ARQGetChunkSize(void);

/**
 * @brief Check if ARQ is initialized
 * 
 * @return TRUE if ARQInit() has been called
 */
BOOL ARQCheckInit(void);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_AR_H

