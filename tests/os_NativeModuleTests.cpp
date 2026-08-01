#include <dolphin/os/OSHostEndian.h>
#include <dolphin/os/OSLink.h>
#include <porpoise/native_module.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
OSModuleQueue __OSModuleInfoList = {};
const void* __OSStringTable = nullptr;

void OSReport(const char*, ...)
{
}

static int RegisteredPrologCount;
static int RegisteredEpilogCount;
static int RegisteredUnresolvedCount;

static void RegisteredProlog()
{
	++RegisteredPrologCount;
}

static void RegisteredEpilog()
{
	++RegisteredEpilogCount;
}

static void RegisteredUnresolved()
{
	++RegisteredUnresolvedCount;
}

static const PorpoiseNativeModuleDescriptor RegisteredFixture =
	PORPOISE_NATIVE_MODULE_DESCRIPTOR(
		7, "registered-fixture", RegisteredProlog, RegisteredEpilog, RegisteredUnresolved);
static const PorpoiseNativeModuleDescriptor DuplicateNameFixture =
	PORPOISE_NATIVE_MODULE_DESCRIPTOR(8, "registered-fixture", nullptr, nullptr, nullptr);
static const PorpoiseNativeModuleDescriptor DuplicateIdFixture =
	PORPOISE_NATIVE_MODULE_DESCRIPTOR(7, "registered-fixture-other-name", nullptr, nullptr, nullptr);
}

static int Fail(const char* message)
{
	std::fprintf(stderr, "%s\nOSLink error: %s\n", message, OSGetModuleLastError());
	return 1;
}

int main()
{
	const char* markerPath = "porpoise-native-module-marker.txt";
	unsigned char relStorage[0x25] = {};
	unsigned char* relHeader = relStorage + 1;
	OSModuleInfo* module = reinterpret_cast<OSModuleInfo*>(relHeader);
	char markers[4] = {};
	FILE* markerFile;
	u32 section = 99;
	u32 offset = 99;
	int stringTableMarker = 0;
	unsigned char registeredStorageA[0x29] = {};
	unsigned char registeredStorageB[0x2a] = {};
	OSModuleInfo* registeredModuleA = reinterpret_cast<OSModuleInfo*>(registeredStorageA + 1);
	OSModuleInfo* registeredModuleB = reinterpret_cast<OSModuleInfo*>(registeredStorageB + 2);

#ifdef _WIN32
	_putenv_s("PORPOISE_NATIVE_MODULE_MARKER", markerPath);
#else
	setenv("PORPOISE_NATIVE_MODULE_MARKER", markerPath, 1);
#endif
	std::remove(markerPath);

	OSWriteBigEndian32(relHeader, 7);
	OSWriteBigEndian32(relHeader + 0x20, 0x12345678u);
	if (OSGetModuleBssSize(module) != 0x12345678u) {
		return Fail("OSGetModuleBssSize did not decode the unaligned big-endian REL header");
	}

	__OSModuleInit();
	OSSetStringTable(&stringTableMarker);
	if (__OSStringTable != &stringTableMarker) {
		return Fail("OSSetStringTable did not retain the provided table");
	}

	OSWriteBigEndian32(registeredStorageA + 1, 7);
	OSWriteBigEndian32(registeredStorageA + 1 + 0x20, 0x11111111u);
	OSWriteBigEndian32(registeredStorageB + 2, 7);
	OSWriteBigEndian32(registeredStorageB + 2 + 0x20, 0x22222222u);
	registeredStorageB[2 + 0x1f] = 0xa5;
	if (!PorpoiseRegisterNativeModule(&RegisteredFixture)) {
		return Fail("an embedded native module could not be registered");
	}
	if (PorpoiseRegisterNativeModule(&DuplicateNameFixture)) {
		return Fail("two embedded modules registered the same logical name");
	}
	if (std::strstr(OSGetModuleLastError(), "logical name") == nullptr) {
		return Fail("a duplicate embedded logical name produced no useful error");
	}
	if (PorpoiseRegisterNativeModule(&DuplicateIdFixture)) {
		return Fail("two embedded modules registered the same nonzero module ID");
	}
	if (std::strstr(OSGetModuleLastError(), "module ID") == nullptr) {
		return Fail("a duplicate embedded module ID produced no useful error");
	}
	OSWriteBigEndian32(registeredStorageB + 2, 8);
	if (OSLinkByName("registered-fixture.rel", registeredModuleB, nullptr)) {
		return Fail("an embedded module linked a REL header with the wrong big-endian ID");
	}
	OSWriteBigEndian32(registeredStorageB + 2, 7);
	if (OSGetModuleBssSize(registeredModuleA) != 0x11111111u ||
	    OSGetModuleBssSize(registeredModuleB) != 0x22222222u) {
		return Fail("registered modules did not retain independent big-endian REL headers");
	}
	if (!OSLinkByName("/registered-fixture.rel.szs", registeredModuleA, nullptr)) {
		return Fail("an embedded native module did not link");
	}
	if (OSLinkByName("registered-fixture.rel", registeredModuleB, nullptr)) {
		return Fail("different REL headers linked the same embedded module concurrently");
	}
	if (std::strstr(OSGetModuleLastError(), "already linked") == nullptr) {
		return Fail("a duplicate embedded link produced no useful error");
	}
	if (!OSRunModuleProlog(registeredModuleA)) {
		return Fail("the first embedded module prolog did not run");
	}
	OSRunModuleUnresolved(registeredModuleA);
	if (!OSRunModuleEpilog(registeredModuleA)) {
		return Fail("the first embedded module epilog did not run");
	}
	if (PorpoiseUnregisterNativeModule(&RegisteredFixture)) {
		return Fail("a linked embedded module was unregistered");
	}
	if (!OSUnlink(registeredModuleA)) {
		return Fail("the first embedded module instance did not unlink");
	}
	if (!OSLinkByName("registered-fixture.rel", registeredModuleB, nullptr)) {
		return Fail("an embedded module did not relink with a different raw REL header");
	}
	if (!OSRunModuleProlog(registeredModuleB)) {
		return Fail("the relinked embedded module prolog did not run");
	}
	OSRunModuleUnresolved(registeredModuleB);
	if (!OSRunModuleEpilog(registeredModuleB)) {
		return Fail("the relinked embedded module epilog did not run");
	}
	if (!OSUnlink(registeredModuleB) || !PorpoiseUnregisterNativeModule(&RegisteredFixture)) {
		return Fail("an inactive embedded module descriptor did not unregister");
	}
	if (RegisteredPrologCount != 2 || RegisteredUnresolvedCount != 2 ||
	    RegisteredEpilogCount != 2) {
		return Fail("embedded module lifecycle callbacks ran an unexpected number of times");
	}
	if (!OSLinkByName("/porpoise-native-module-fixture.rel.szs", module, nullptr)) {
		return Fail("OSLinkByName could not load the executable-relative fixture");
	}
	if (OSSearchModule(module, &section, &offset) != module || section != 0 || offset != 0) {
		return Fail("OSSearchModule did not find the linked native module");
	}
	if (OSLinkByName("porpoise-native-module-fixture.rel", module, nullptr)) {
		return Fail("the same REL header was linked twice");
	}
	if (!OSRunModuleProlog(module)) {
		return Fail("fixture prolog failed");
	}
	OSRunModuleUnresolved(module);
	if (!OSRunModuleEpilog(module)) {
		return Fail("fixture epilog failed");
	}
	if (!OSUnlink(module)) {
		return Fail("OSUnlink failed");
	}
	if (OSRunModuleProlog(module)) {
		return Fail("an unlinked module unexpectedly ran its prolog");
	}

	markerFile = std::fopen(markerPath, "rb");
	if (markerFile == nullptr) {
		return Fail("fixture callbacks did not create their marker");
	}
	if (std::fread(markers, 1, 3, markerFile) != 3) {
		std::fclose(markerFile);
		return Fail("fixture callbacks did not write every marker");
	}
	std::fclose(markerFile);
	std::remove(markerPath);
	if (std::strcmp(markers, "PUE") != 0) {
		return Fail("fixture callbacks ran in the wrong order");
	}

	if (OSLinkByName("porpoise-native-module-invalid-abi-fixture.rel", module, nullptr)) {
		return Fail("a native module with an incompatible ABI unexpectedly linked");
	}
	if (std::strstr(OSGetModuleLastError(), "ABI version") == nullptr) {
		return Fail("an incompatible ABI produced no useful error");
	}

	OSWriteBigEndian32(relHeader, 8);
	if (OSLinkByName("porpoise-native-module-fixture.rel", module, nullptr)) {
		return Fail("a native module with the wrong REL ID unexpectedly linked");
	}

	if (OSLinkByName("module-that-does-not-exist.rel", module, nullptr)) {
		return Fail("a missing native module unexpectedly linked");
	}
	if (OSGetModuleLastError()[0] == '\0') {
		return Fail("a missing native module produced no useful error");
	}

	return 0;
}
