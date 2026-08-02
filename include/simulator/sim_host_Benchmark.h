#ifndef SIMULATOR_SIM_HOST_BENCHMARK_H
#define SIMULATOR_SIM_HOST_BENCHMARK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opt-in host benchmark support.  The normal simulator path remains disabled
 * unless LIBPORPOISE_BENCHMARK_OUTPUT is present in the environment.
 */
int SIM_HostBenchmarkConfigureFromEnvironment(void);
int SIM_HostBenchmarkInitializeGl(void);
int SIM_HostBenchmarkEnabled(void);
int SIM_HostBenchmarkNoPacing(void);
int SIM_HostBenchmarkNeutralInput(void);

void SIM_HostBenchmarkBeforeSwap(
    uint32_t retraceId,
    int drawableWidth,
    int drawableHeight);
void SIM_HostBenchmarkAfterSwap(
    uint32_t retraceId,
    uint64_t swapTicks,
    uint64_t performanceFrequency);
void SIM_HostBenchmarkOnRetraceEnd(
    uint32_t retraceId,
    uint64_t paceTicks,
    uint64_t performanceFrequency);

#ifdef __cplusplus
}
#endif

#endif
