#ifndef _DOLPHIN_OS_OSLINK_H
#define _DOLPHIN_OS_OSLINK_H

#include <dolphin/os/OSModule.h>

BEGIN_SCOPE_EXTERN_C

/*
 * Host builds map a REL asset name to a native dynamic module beside the
 * executable. GameCube builds continue to use the SDK OSLink entry points.
 */
BOOL OSLinkByName(const char* moduleName, OSModuleInfo* newModule, void* bss);
u32 OSGetModuleBssSize(const OSModuleInfo* module);
BOOL OSRunModuleProlog(OSModuleInfo* module);
BOOL OSRunModuleEpilog(OSModuleInfo* module);
void OSRunModuleUnresolved(OSModuleInfo* module);
const char* OSGetModuleLastError(void);

END_SCOPE_EXTERN_C

#endif /* _DOLPHIN_OS_OSLINK_H */
