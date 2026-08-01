#include <dolphin/dsp.h>

namespace {

bool sOverrideCalled;

}  // namespace

extern "C" void DSPInit(void)
{
	sOverrideCalled = true;
}

int main()
{
	/* Force dsp.c into the link, then verify its default DSPInit stays
	 * overridable by a consumer-provided implementation. */
	DSPReset();
	DSPInit();
	return sOverrideCalled ? 0 : 1;
}
