/*---------------------------------------------------------------------------*
  Project:  libPorpoise
  File:     demo.h
  
  Main header for the demo library.
  Includes all demo components for convenience.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_H
#define DOLPHIN_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Core types - must be first for ATTRIBUTE_ALIGN macro */
#include <dolphin/types.h>

/* Core OS and system includes */
#include <dolphin/os.h>
#include <dolphin/pad.h>
#include <dolphin/vi.h>
#include <dolphin/mtx.h>
#include <dolphin/gx.h>
#include <dolphin/tpl.h>

/* Demo library components */
#include <dolphin/demo/DEMOInit.h>
#include <dolphin/demo/DEMOPad.h>
#include <dolphin/demo/DEMOPuts.h>
#include <dolphin/demo/DEMOWin.h>
#include <dolphin/demo/DEMOStats.h>

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_H */
