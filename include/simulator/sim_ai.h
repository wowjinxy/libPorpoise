#ifndef SIM_AI_H
#define SIM_AI_H

#include <dolphin/types.h>
#include <dolphin/ai.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SIM_AISetRegValue(u32 reg, u32 newVal);
void SIM_AIInitDma(u32 startMemHndl, u32 length);
void SIM_AIStartDma();


#ifdef __cplusplus
}
#endif

#endif