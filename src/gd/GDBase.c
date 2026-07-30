/*---------------------------------------------------------------------------*
  Project:  Dolphin/Revolution GD library
  File:     GD.c

  $Log: GDBase.c,v $
  Revision 1.3  2006/02/20 04:24:39  mitu
  Changed include path from dolphin/ to revolution/.

  Revision 1.2  2006/02/03 08:54:52  hirose
  Avoided use of EPPC and use WIN32.

  Revision 1.1.1.1  2005/05/12 02:15:49  yasuh-to
  Ported from dolphin source tree.

    
    2     2001/09/12 3:56p Carl
    Renamed some functions to make it clear which act on "current".
    
    1     2001/09/12 1:52p Carl
    Initial revision of GD: Graphics Display List Library.

  $NoKeywords: $
 *---------------------------------------------------------------------------*/

#include <revolution/gd.h>
#include <revolution/os.h>

/*---------------------------------------------------------------------------*/

// This object is global, but the user should not access it directly:
GDLObj *__GDCurrentDL = NULL;

// Overflow callback pointer
static GDOverflowCallback overflowcb = NULL;

/*---------------------------------------------------------------------------*/

// Initialize parameters
void GDInitGDLObj(GDLObj *dl, void *start, u32 length)
{
    ASSERTMSG( ((u32) start & 31) == 0, "start must be aligned to 32 bytes");
    ASSERTMSG( (length & 31) == 0, "length must be aligned to 32 bytes");

    dl->start  = start;
    dl->ptr    = (u8*) start;
    dl->top    = (u8*) start + length;
    dl->length = length;
}
    
// Makes sure current DL is flushed out of cache (on HW)
void GDFlushCurrToMem()
{
#ifndef WIN32
    DCFlushRange(__GDCurrentDL->start, __GDCurrentDL->length);
#endif
}

// Pads current DL out to 32B
void GDPadCurr32()
{
    u32 n = ((u32) __GDCurrentDL->ptr & 31);
    if (n) 
    {
        for(; n<32; n++) 
        {
            __GDWrite(0);
        }
    }
}

// Overflow-related functions
void GDOverflowed()
{
    if (overflowcb)
    {
        (*overflowcb)();
    } else {
        ASSERTMSG(0, "GDWrite: display list overflowed");
    }
}

void GDSetOverflowCallback(GDOverflowCallback callback)
{
    overflowcb = callback;
}

GDOverflowCallback GDGetOverflowCallback( void )
{
    return overflowcb;
}

