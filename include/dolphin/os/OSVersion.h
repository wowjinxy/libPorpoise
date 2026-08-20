#ifndef _DOLPHIN_OS_OSVERSION_H
#define _DOLPHIN_OS_OSVERSION_H

#include <dolphin/types.h>

// This is a fabricated header with fabricated macros in order to support multiple revisions of the SDK.
// `OS_BUILD_REVISION` is not granular enough because there are differences between the Oct. and Nov.
// versions of Dolphin SDK Revision 47, so prefer `OS_BUILD_VERSION` for conditionally compiled code.
// TODO: There are revisional differences in vi.c that cannot be explained by the build date and time.

/////////////// OS BUILD INFO /////////////////////////////////////////////////////////////////////

#define OS_BUILD_REVISION 49
#define OS_BUILD_DATE     "Dec 17 2001"
#define OS_BUILD_TIME     "18:46:45"
#define OS_BUILD_VERSION  20011217L

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif
