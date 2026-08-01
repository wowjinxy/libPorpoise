#include <dolphin/base/PPCArch.h>
#include <dolphin/base/PPCPm.h>

#include <type_traits>

using PPCMsrOperation = u32 (*)(u32);

static_assert(std::is_same_v<decltype(&PPCOrMsr), PPCMsrOperation>);
static_assert(std::is_same_v<decltype(&PPCAndMsr), PPCMsrOperation>);
static_assert(std::is_same_v<decltype(&PPCAndCMsr), PPCMsrOperation>);

int main()
{
	const u32 msr = PPCMfmsr();
	const u32 mask = 0xA5A55A5AU;

	if (PPCOrMsr(mask) != (msr | mask)) {
		return 1;
	}
	if (PPCAndMsr(mask) != (msr & mask)) {
		return 2;
	}
	if (PPCAndCMsr(mask) != (msr & ~mask)) {
		return 3;
	}

	PMBegin();
	if (PMCycles() != 0 || PML1FetchMisses() != 0 ||
	    PML1MissCycles() != 0 || PMInstructions() != 0) {
		return 4;
	}
	PMEnd();

	return 0;
}
