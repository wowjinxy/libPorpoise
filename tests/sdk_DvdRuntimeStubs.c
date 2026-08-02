#include <simulator/sim_host_Benchmark.h>

/*
 * DVD host tests do not create a renderer. VI still references these hooks
 * through the shared OS initialization graph, so provide inert test-runtime
 * implementations.
 */
void SIM_VIInit(void)
{
}

void SIM_Render(void)
{
}

void __GXHostServiceFifoBreakpoint(void)
{
}

int SIM_HostBenchmarkEnabled(void)
{
    return 0;
}

int SIM_HostBenchmarkNoPacing(void)
{
    return 0;
}

int SIM_HostBenchmarkNeutralInput(void)
{
    return 0;
}

void SIM_HostBenchmarkOnRetraceEnd(
    uint32_t retraceId,
    uint64_t paceTicks,
    uint64_t performanceFrequency)
{
    (void)retraceId;
    (void)paceTicks;
    (void)performanceFrequency;
}
