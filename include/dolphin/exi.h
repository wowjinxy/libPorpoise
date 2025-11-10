/**
 * @file exi.h
 * @brief EXI (External Interface) API for libPorpoise
 * 
 * On GameCube/Wii: Manages external devices (memory cards, serial ports, broadband adapter)
 * On PC: Stub implementation - devices simulated elsewhere (CARD module will handle save data)
 */

#ifndef DOLPHIN_EXI_H
#define DOLPHIN_EXI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

// Forward declarations
typedef struct OSContext OSContext;

/*---------------------------------------------------------------------------*
    Constants
 *---------------------------------------------------------------------------*/

// EXI channels
#define EXI_CHANNEL_0   0
#define EXI_CHANNEL_1   1
#define EXI_CHANNEL_2   2
#define EXI_MAX_CHAN    3

// EXI devices per channel
#define EXI_DEVICE_0    0
#define EXI_DEVICE_1    1
#define EXI_DEVICE_2    2
#define EXI_MAX_DEV     3

// EXI frequencies
#define EXI_FREQ_1MHZ   0
#define EXI_FREQ_2MHZ   1
#define EXI_FREQ_4MHZ   2
#define EXI_FREQ_8MHZ   3
#define EXI_FREQ_16MHZ  4
#define EXI_FREQ_32MHZ  5

// EXI transfer modes
#define EXI_READ        0
#define EXI_WRITE       1
#define EXI_READ_WRITE  2

/*---------------------------------------------------------------------------*
    Types
 *---------------------------------------------------------------------------*/

/**
 * @brief EXI callback function type
 */
typedef void (*EXICallback)(s32 chan, OSContext* context);

/*---------------------------------------------------------------------------*
    Functions
 *---------------------------------------------------------------------------*/

/**
 * @brief Attach to EXI device
 */
BOOL EXIAttach(s32 chan, EXICallback detachCallback);

/**
 * @brief Detach from EXI device
 */
BOOL EXIDetach(s32 chan);

/**
 * @brief Lock EXI channel for exclusive access
 */
BOOL EXILock(s32 chan, u32 dev, EXICallback unlockCallback);

/**
 * @brief Unlock EXI channel
 */
BOOL EXIUnlock(s32 chan);

/**
 * @brief Select EXI device
 */
BOOL EXISelect(s32 chan, u32 dev, u32 freq);

/**
 * @brief Deselect EXI device
 */
BOOL EXIDeselect(s32 chan);

/**
 * @brief Immediate data transfer (small amounts)
 */
BOOL EXIImm(s32 chan, void* data, u32 len, u32 mode, EXICallback callback);

/**
 * @brief Immediate data transfer (extended)
 */
BOOL EXIImmEx(s32 chan, void* data, u32 len, u32 mode);

/**
 * @brief DMA data transfer (large amounts)
 */
BOOL EXIDma(s32 chan, void* buf, u32 len, u32 mode, EXICallback callback);

/**
 * @brief Wait for transfer to complete
 */
BOOL EXISync(s32 chan);

/**
 * @brief Probe for device presence
 */
BOOL EXIProbe(s32 chan);

/**
 * @brief Extended probe
 */
s32 EXIProbeEx(s32 chan);

/**
 * @brief Get device ID
 */
BOOL EXIGetID(s32 chan, u32 dev, u32* id);

/**
 * @brief Get transfer state
 */
u32 EXIGetState(s32 chan);

/**
 * @brief Initialize EXI subsystem
 */
void EXIInit(void);

/**
 * @brief Clear interrupt flags
 */
u32 EXIClearInterrupts(s32 chan, BOOL exi, BOOL tc, BOOL ext);

/**
 * @brief Reset probe state
 */
void EXIProbeReset(void);

/**
 * @brief Get device type
 */
s32 EXIGetType(s32 chan, u32 dev, u32* type);

/**
 * @brief Select SD device
 */
BOOL EXISelectSD(s32 chan, u32 dev, u32 freq);

/**
 * @brief Get device ID (extended)
 */
s32 EXIGetIDEx(s32 chan, u32 dev, u32* id);

/**
 * @brief Get console type
 */
u32 EXIGetConsoleType(void);

/**
 * @brief Wait for EXI operation
 */
void EXIWait(void);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_EXI_H

