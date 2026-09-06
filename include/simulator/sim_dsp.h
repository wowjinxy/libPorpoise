#ifndef SIM_DSP_H
#define SIM_DSP_H

#include <dolphin/dsp.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_DSPSendMailToDSP(u32 mail);
u32 SIM_DSPReadMailFromDSP();

#ifdef __cplusplus
}
#endif

#endif