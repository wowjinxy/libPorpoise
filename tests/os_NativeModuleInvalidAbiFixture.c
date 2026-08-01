#include <porpoise/native_module.h>

static const PorpoiseNativeModuleDescriptor InvalidAbiDescriptor = {
	PORPOISE_NATIVE_MODULE_ABI_VERSION + 1,
	sizeof(PorpoiseNativeModuleDescriptor),
	7,
	0,
	"porpoise-native-module-invalid-abi-fixture",
	NULL,
	NULL,
	NULL,
};

PORPOISE_NATIVE_MODULE_EXPORT const PorpoiseNativeModuleDescriptor* PorpoiseGetNativeModule(void)
{
	return &InvalidAbiDescriptor;
}
