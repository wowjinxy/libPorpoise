#ifndef LIBPORPOISE_SIM_H
#define LIBPORPOISE_SIM_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_VIInit();

void SIM_Render();

/* Host GL-context handoff. These must run on the releasing/acquiring thread. */
BOOL SIM_HostReleaseRenderContext(void);
BOOL SIM_HostAcquireRenderContext(void);

void SIM_DebugBreak();

#ifdef __cplusplus
}
#endif

#endif
