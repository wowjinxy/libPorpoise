#include <simulator/sim_host_Allocator.hpp>

#include <cstdlib>
#include <limits>

namespace {

thread_local unsigned HostAllocationDepth;

}

extern "C" bool SIM_HostAllocationActive()
{
    return HostAllocationDepth != 0;
}

void SIM::EnterHostAllocationScope() noexcept
{
    if (HostAllocationDepth == std::numeric_limits<unsigned>::max()) {
        std::abort();
    }
    ++HostAllocationDepth;
}

void SIM::LeaveHostAllocationScope() noexcept
{
    if (HostAllocationDepth != 0) {
        --HostAllocationDepth;
    }
}
