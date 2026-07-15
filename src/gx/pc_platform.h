/* pc_platform.h - Minimal platform shim for ACPC GX backend in Porpoise */
#ifndef PC_PLATFORM_H
#define PC_PLATFORM_H

#include <dolphin/types.h>
#include <dolphin/vi.h>
#include <SDL2/SDL.h>
#include "glad/gl.h"

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ACPC GX constants used by pc_gx.c */
#define PC_GC_WIDTH 640
#define PC_GC_HEIGHT 480
#define PC_PIf 3.14159265358979323846f

/* Globals expected by the ACPC GX backend */
extern int g_pc_window_w;
extern int g_pc_window_h;
extern int g_pc_widescreen_stretch;
extern int g_pc_model_viewer_no_cull;
extern int pc_gx_draw_call_count;

#ifdef __cplusplus
}
#endif

#endif /* PC_PLATFORM_H */
