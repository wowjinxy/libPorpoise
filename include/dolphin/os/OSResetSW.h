#ifndef DOLPHIN_OSRESETSW_H
#define DOLPHIN_OSRESETSW_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OSResetCallback)(void);
typedef void (*OSPowerCallback)(void);

BOOL            OSGetResetButtonState(void);
OSResetCallback OSSetResetCallback   (OSResetCallback callback);
OSPowerCallback OSSetPowerCallback   (OSPowerCallback callback);
BOOL            OSGetResetSwitchState(void);

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_OSRESETSW_H */

