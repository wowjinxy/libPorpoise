/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOWin.c
  
  Window/menu system - stub implementation for compilation.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#include <dolphin/demo.h>
#include <dolphin/os.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global window list */
static DEMOWinInfo* s_windowList = NULL;

void DEMOWinInit(void) {
    // Stub - initialize window system
    s_windowList = NULL;
}

DEMOWinInfo* DEMOWinCreateWindow(s32 x1, s32 y1, s32 x2, s32 y2, char* caption, u16 scroll, void* func) {
    (void)x1; (void)y1; (void)x2; (void)y2; (void)caption; (void)scroll; (void)func;
    DEMOWinInfo* win = (DEMOWinInfo*)calloc(1, sizeof(DEMOWinInfo));
    if (win) {
        win->x1 = x1;
        win->y1 = y1;
        win->x2 = x2;
        win->y2 = y2;
        #ifdef LIBPORPOISE_BUILD_WIN64
        win->caption = caption ? _strdup(caption) : NULL;
        #elif LIBPORPOISE_BUILD_LINUX
        win->caption = caption ? strdup(caption) : NULL;
        #endif
        win->refresh = (void(*)(DEMOWinInfo*))func;
    }
    return win; // Stub
}

void DEMOWinOpenWindow(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinCloseWindow(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinDestroyWindow(DEMOWinInfo* handle) {
    if (handle) {
        if (handle->caption) free(handle->caption);
        if (handle->buffer) free(handle->buffer);
        free(handle);
    }
}

void DEMOWinScrollWindow(DEMOWinInfo* handle, u32 dir) {
    (void)handle; (void)dir;
    // Stub
}

void DEMOWinSetWindowColor(DEMOWinInfo* handle, u32 item, u8 r, u8 g, u8 b, u8 a) {
    (void)handle; (void)item; (void)r; (void)g; (void)b; (void)a;
    // Stub
}

void DEMOWinBringToFront(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinSendToBack(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinSetCursorLine(DEMOWinInfo* handle, s16 n) {
    if (handle) handle->cursor_line = n;
}

s32 DEMOWinGetCursorLine(DEMOWinInfo* handle) {
    return handle ? handle->cursor_line : -1;
}

void DEMOWinLogPrintf(DEMOWinInfo* handle, char* fmt, ...) {
    (void)handle;
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    OSReport("%s\n", buffer);
    va_end(args);
}

void DEMOWinPrintfXY(DEMOWinInfo* handle, u16 col, u16 row, char* fmt, ...) {
    (void)handle; (void)col; (void)row;
    va_list args;
    va_start(args, fmt);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    OSReport("%s\n", buffer);
    va_end(args);
}

void DEMOWinClearRow(DEMOWinInfo* handle, u16 row) {
    (void)handle; (void)row;
    // Stub
}

void DEMOWinClearWindow(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinClearBuffer(DEMOWinInfo* handle) {
    (void)handle;
    // Stub
}

void DEMOWinPadInit(DEMOWinPadInfo* p) {
    if (p) memset(p, 0, sizeof(DEMOWinPadInfo));
}

void DEMOWinPadRead(DEMOWinPadInfo* p) {
    (void)p;
    // Stub - forward to DEMOPadRead
    DEMOPadRead();
}

void DEMOWinResetRepeat(void) {
    // Stub
}

void DEMOWinSetRepeat(u32 threshold, u32 rate) {
    (void)threshold; (void)rate;
    // Stub
}

void DEMOWinRefresh(void) {
    // Stub
}

DEMOWinMenuInfo* DEMOWinCreateMenuWindow(DEMOWinMenuInfo* menu, u16 x, u16 y) {
    (void)menu; (void)x; (void)y;
    return NULL; // Stub
}

u32 DEMOWinMenuChild(DEMOWinMenuInfo* items, BOOL child_flag) {
    (void)items; (void)child_flag;
    return 0; // Stub
}

void DEMOWinDestroyMenuWindow(DEMOWinMenuInfo* menu) {
    (void)menu;
    // Stub
}

DEMOWinListInfo* DEMOWinCreateListWindow(DEMOWinListInfo* list, u16 x, u16 y) {
    (void)list; (void)x; (void)y;
    return NULL; // Stub
}

void DEMOWinDestroyListWindow(DEMOWinListInfo* list) {
    (void)list;
    // Stub
}

void DEMOWinListSetCursor(DEMOWinListInfo* list, BOOL x) {
    if (list) list->cursor_state = x;
}

s32 DEMOWinListScrollList(DEMOWinListInfo* list, u32 dir) {
    (void)list; (void)dir;
    return 0; // Stub
}

s32 DEMOWinListMoveCursor(DEMOWinListInfo* list, u32 dir) {
    (void)list; (void)dir;
    return 0; // Stub
}

#ifdef __cplusplus
} // extern "C"
#endif

