#include <simulator/sim_host_Allocator.hpp>

#include <atomic>
#include <thread>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<SIM::HostAllocationScope>);
static_assert(!std::is_copy_assignable_v<SIM::HostAllocationScope>);
static_assert(!std::is_move_constructible_v<SIM::HostAllocationScope>);
static_assert(!std::is_move_assignable_v<SIM::HostAllocationScope>);

int main()
{
    if (SIM_HostAllocationActive()) {
        return 1;
    }

    SIM::LeaveHostAllocationScope();
    if (SIM_HostAllocationActive()) {
        return 2;
    }

    SIM::EnterHostAllocationScope();
    if (!SIM_HostAllocationActive()) {
        return 3;
    }
    SIM::LeaveHostAllocationScope();
    if (SIM_HostAllocationActive()) {
        return 4;
    }

    {
        SIM::HostAllocationScope outer;
        if (!SIM_HostAllocationActive()) {
            return 5;
        }

        {
            SIM::HostAllocationScope inner;
            if (!SIM_HostAllocationActive()) {
                return 6;
            }
        }

        if (!SIM_HostAllocationActive()) {
            return 7;
        }

        std::atomic<bool> workerWasInactive = false;
        std::atomic<bool> workerBecameActive = false;
        std::thread worker([&]() {
            workerWasInactive = !SIM_HostAllocationActive();
            SIM::HostAllocationScope workerScope;
            workerBecameActive = SIM_HostAllocationActive();
        });
        worker.join();

        if (!workerWasInactive || !workerBecameActive) {
            return 8;
        }
        if (!SIM_HostAllocationActive()) {
            return 9;
        }
    }

    return SIM_HostAllocationActive() ? 10 : 0;
}
