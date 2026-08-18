#ifndef SIM_VI_H
#define SIM_VI_H

#include <dolphin/types.h>
#include <dolphin/vi/vitypes.h>

#ifdef __cplusplus
namespace SIM::VI {
void Init();
void HandlePreRetrace();
void HandlePostRetrace();
void WaitForRetrace();
u32 GetWaitForRetraceCount();

[[nodiscard]] u32 GetRetraceCount();

void SetPreRetraceCallback(VIRetraceCallback callback);
void SetPostRetraceCallback(VIRetraceCallback callback);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C APIs
void SIM_VIWaitForRetrace();
u32 SIM_VIGetRetraceCount();
void SIM_VISetPreRetraceCallback(VIRetraceCallback callback);
void SIM_VISetPostRetraceCallback(VIRetraceCallback callback);

#ifdef __cplusplus
}
#endif

#endif
