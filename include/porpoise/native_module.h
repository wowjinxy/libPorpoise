#ifndef PORPOISE_NATIVE_MODULE_H
#define PORPOISE_NATIVE_MODULE_H

#include <dolphin/types.h>

BEGIN_SCOPE_EXTERN_C

/*
 * Native modules are the host counterpart of a PowerPC REL. The descriptor
 * deliberately contains only fixed-width values and function pointers so a
 * module can validate its loader before any entry point is called.
 */
#define PORPOISE_NATIVE_MODULE_ABI_VERSION 1u
#define PORPOISE_NATIVE_MODULE_QUERY_SYMBOL "PorpoiseGetNativeModule"

#if defined(LIBPORPOISE_BUILD_WIN64)
#define PORPOISE_NATIVE_MODULE_EXPORT __declspec(dllexport)
#elif defined(LIBPORPOISE_BUILD_LINUX)
#define PORPOISE_NATIVE_MODULE_EXPORT __attribute__((visibility("default")))
#else
#define PORPOISE_NATIVE_MODULE_EXPORT
#endif

typedef void (*PorpoiseNativeModuleEntry)(void);

typedef struct PorpoiseNativeModuleDescriptor {
	u32 abiVersion;
	u32 structSize;
	u32 moduleId;
	u32 flags;
	const char* logicalName;
	PorpoiseNativeModuleEntry prolog;
	PorpoiseNativeModuleEntry epilog;
	PorpoiseNativeModuleEntry unresolved;
} PorpoiseNativeModuleDescriptor;

typedef const PorpoiseNativeModuleDescriptor* (*PorpoiseGetNativeModuleProc)(void);

/*
 * Embedded native modules remain owned by the registering executable. Their
 * descriptors and logicalName strings must remain valid until they are
 * unregistered. Unregistration fails while a descriptor is linked, which
 * makes descriptor lookup safe without copying title-owned callback state.
 */
BOOL PorpoiseRegisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor);
BOOL PorpoiseUnregisterNativeModule(const PorpoiseNativeModuleDescriptor* descriptor);

#define PORPOISE_NATIVE_MODULE_DESCRIPTOR(                                              \
	moduleIdValue, logicalNameValue, prologValue, epilogValue, unresolvedValue)          \
	{                                                                                    \
		PORPOISE_NATIVE_MODULE_ABI_VERSION,                                               \
		sizeof(PorpoiseNativeModuleDescriptor),                                           \
		(moduleIdValue),                                                                  \
		0,                                                                                \
		(logicalNameValue),                                                               \
		(prologValue),                                                                    \
		(epilogValue),                                                                    \
		(unresolvedValue),                                                                \
	}

#define PORPOISE_DEFINE_NATIVE_MODULE(                                                   \
	moduleIdValue, logicalNameValue, prologValue, epilogValue, unresolvedValue)          \
	static const PorpoiseNativeModuleDescriptor PorpoiseNativeModule =                   \
		PORPOISE_NATIVE_MODULE_DESCRIPTOR(                                                 \
			moduleIdValue,                                                                 \
			logicalNameValue,                                                              \
			prologValue,                                                                   \
			epilogValue,                                                                   \
			unresolvedValue);                                                              \
	PORPOISE_NATIVE_MODULE_EXPORT const PorpoiseNativeModuleDescriptor*                  \
	PorpoiseGetNativeModule(void)                                                        \
	{                                                                                    \
		return &PorpoiseNativeModule;                                                     \
	}

END_SCOPE_EXTERN_C

#endif
