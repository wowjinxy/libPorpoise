#include <dolphin.h>
#include <dolphin/hw_regs.h>
#include <stddef.h>
#include <macros.h>

#include "__dsp.h"

#ifdef LIBPORPOISE_PORT
#include <simulator/sim_dsp.h>
#endif

#define BUILD_DATE "Dec 17 2001"
#define BUILD_TIME "18:25:00"

u32 DSPCheckMailToDSP(void) { return (__DSPRegs[0] & (1 << 15)) >> 15; }

u32 DSPCheckMailFromDSP(void) { 
	#ifdef LIBPORPOISE_PORT
	return (SIM_DSPReadMailFromDSP() != 0);
	#else
	return (__DSPRegs[2] & (1 << 15)) >> 15; 
	#endif
}

u32 DSPReadMailFromDSP(void) { 
	#ifdef LIBPORPOISE_PORT
	return SIM_DSPReadMailFromDSP();
	#else
	return (__DSPRegs[2] << 16) | __DSPRegs[3]; 
	#endif
}

void DSPSendMailToDSP(u32 mail)
{
#ifdef LIBPORPOISE_PORT
	SIM_DSPSendMailToDSP(mail);
#else
	__DSPRegs[0] = mail >> 16;
	__DSPRegs[1] = mail & 0xFFFF;
#endif
}


