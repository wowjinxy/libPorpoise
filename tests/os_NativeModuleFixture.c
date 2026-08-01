#include <porpoise/native_module.h>

#include <stdio.h>
#include <stdlib.h>

static void AppendMarker(char marker)
{
	const char* path = getenv("PORPOISE_NATIVE_MODULE_MARKER");
	FILE* file;

	if (path == NULL) {
		return;
	}

	file = fopen(path, "ab");
	if (file != NULL) {
		fputc(marker, file);
		fclose(file);
	}
}

static void FixtureProlog(void)
{
	AppendMarker('P');
}

static void FixtureEpilog(void)
{
	AppendMarker('E');
}

static void FixtureUnresolved(void)
{
	AppendMarker('U');
}

PORPOISE_DEFINE_NATIVE_MODULE(
	7, "porpoise-native-module-fixture", FixtureProlog, FixtureEpilog, FixtureUnresolved)
