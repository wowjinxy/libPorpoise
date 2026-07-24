#ifndef DOLPHIN_H
#define DOLPHIN_H

#if defined(LIBPORPOISE_PORT) && !defined(LIBPORPOISE_MAIN_HANDLED)
#define main DolphinMain
#endif

#include <dolphin/types.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/db.h>
#include <dolphin/os.h>
#include <dolphin/dvd.h>
#include <dolphin/pad.h>
#include <dolphin/mtx.h>
#include <dolphin/gx.h>
#include <dolphin/vi.h>
#include <dolphin/ai.h>
#include <dolphin/ar.h>
#include <dolphin/dsp.h>
#include <dolphin/card.h>

#endif  // DOLPHIN_H
