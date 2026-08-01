#ifndef LIBPORPOISE_SIM_HOST_ALLOCATOR_HPP
#define LIBPORPOISE_SIM_HOST_ALLOCATOR_HPP

extern "C" bool SIM_HostAllocationActive();

namespace SIM {

void EnterHostAllocationScope() noexcept;
void LeaveHostAllocationScope() noexcept;

class HostAllocationScope {
public:
    HostAllocationScope() noexcept { EnterHostAllocationScope(); }
    ~HostAllocationScope() noexcept { LeaveHostAllocationScope(); }

    HostAllocationScope(const HostAllocationScope&) = delete;
    HostAllocationScope& operator=(const HostAllocationScope&) = delete;
    HostAllocationScope(HostAllocationScope&&) = delete;
    HostAllocationScope& operator=(HostAllocationScope&&) = delete;
};

}

#endif
