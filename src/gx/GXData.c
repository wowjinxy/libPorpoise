#include <dolphin/gx.h>

static struct __GXData_struct gxData;
struct __GXData_struct* gx = &gxData;
vu16* __memReg;
vu16* __peReg;
vu16* __cpReg;
vu32* __piReg;
#if DEBUG
GXBool __GXinBegin;
#endif
