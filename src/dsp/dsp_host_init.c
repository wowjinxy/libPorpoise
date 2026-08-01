#include <dolphin/dsp.h>

void __DSPHostInitDefault(void);

/* Keep the host default in its own archive member. A consumer that provides
 * DSPInit resolves the public symbol without extracting this object, while a
 * consumer that needs the stub gets the default implementation normally. */
void DSPInit(void)
{
	__DSPHostInitDefault();
}
