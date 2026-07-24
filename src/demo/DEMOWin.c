#include <demo.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dolphin.h>

#include <demo/DEMOWin.h>

DEMOWinInfo *__first_node;
DEMOWinInfo *__last_node;
DEMOWinInfo *__curr_node;

GXRenderModeObj *__rmp;

#define __DEF_BKGND_R 0x1a
#define __DEF_BKGND_G 0x1f
#define __DEF_BKGND_B 0x21
#define __DEF_BKGND_A 0xff

#define __DEF_CAP_R 0x55
#define __DEF_CAP_G 0x1f
#define __DEF_CAP_B 0x1f
#define __DEF_CAP_A 0xff

#define __DEF_BORDER_R 0x45
#define __DEF_BORDER_G 0x25
#define __DEF_BORDER_B 0x25
#define __DEF_BORDER_A 0xff

s32 fontShift = 0;

static u32 __DEMOWIN_PAD_repeat_threshold = DEMOWIN_PAD_REPEAT_THRESH_DEF;
static u32 __DEMOWIN_PAD_repeat_rate = DEMOWIN_PAD_REPEAT_RATE_DEF;
static void __DEMOWin_add_node(DEMOWinInfo *handle);
static void __DEMOWin_delete_node(DEMOWinInfo *handle);
static void __DEMOWin_puts_n(s16 x, s16 y, s16 z, u16 n, char *string);

static u16 __DEMOWinMenu_get_user_input(DEMOWinPadInfo *p);
static void __DEMOWinMenu_refesh_menu(DEMOWinInfo *w);
static void __DEMOWinList_refresh_list(DEMOWinInfo *w);

void DEMOWinInit(void) {
  __first_node = NULL;
  __last_node = NULL;
  __curr_node = NULL;

  __rmp = DEMOGetRenderModeObj();

  GXSetCopyClear((GXColor){0x00, 0x00, 0x00, 0x00}, GX_MAX_Z24);
}

DEMOWinInfo *DEMOWinCreateWindow(s32 x1, s32 y1, s32 x2, s32 y2, char *caption,
                                 u16 scroll, void *func) {
  DEMOWinInfo *handle;

  ASSERTMSG((x1 < x2), "DEMOWIN: Illegal X coords for window\n");
  ASSERTMSG((y1 < y2), "DEMOWIN: Illegal y coords for window\n");

  handle = (DEMOWinInfo *)OSAlloc(sizeof(DEMOWinInfo));
  ASSERTMSG(handle, "DEMOWIN: FAILED TO ALLOCATE WINDOW!\n");

  handle->x1 = x1;
  handle->y1 = y1;
  handle->x2 = x2;
  handle->y2 = y2;

  handle->pixel_width = (u16)(x2 - x1 + 1);
  handle->pixel_height = (u16)(y2 - y1 + 1);

  handle->caption = caption;

  handle->char_width = (u16)(((handle->pixel_width) / FONT_CHAR_WIDTH) - 1);
  handle->char_height = (u16)(((handle->pixel_height) / FONT_CHAR_HEIGHT) - 2);

  handle->x_cal =
      (u16)((handle->pixel_width - (handle->char_width * FONT_CHAR_WIDTH) + 1) /
            2);
  handle->y_cal = (u16)(((handle->pixel_height - FONT_CHAR_HEIGHT) -
                         (handle->char_height * FONT_CHAR_HEIGHT) + 1) /
                        2);
#ifdef DEMOWIN_DEBUG
  OSReport("\n");
  OSReport("======================================\n");
  OSReport("WINDOW: '%s'\n", caption);
  OSReport("======================================\n");
  OSReport("x1: %4d\n", handle->x1);
  OSReport("y1: %4d\n", handle->y1);
  OSReport("x2: %4d\n", handle->x2);
  OSReport("y2: %4d\n", handle->y2);
  OSReport("pixel_width : %4d\n", handle->pixel_width);
  OSReport("pixel_height: %4d\n", handle->pixel_height);
  OSReport("char_width  : %4d\n", handle->char_width);
  OSReport("char_height : %4d\n", handle->char_height);
  OSReport("x_cal       : %4d\n", handle->x_cal);
  OSReport("y_cal       : %4d\n", handle->y_cal);
#endif

  handle->num_scroll_lines = scroll;
  handle->total_lines = (u16)(handle->char_height + handle->num_scroll_lines);
  handle->curr_output_line = 0;
  handle->curr_output_col = 0;
  handle->curr_view_line = 0;

  handle->refresh = func;

  handle->flags = 0;

  handle->priority = DEMOWIN_PRIORITY_FORE;

  handle->buffer =
      (u8 *)OSAlloc(sizeof(u8) * handle->total_lines * handle->char_width);
  ASSERTMSG(handle->buffer,
            "DEMOWinCreateWindow(): Unable to allocation buffer!\n");

  memset((void *)(handle->buffer), 0x20,
         sizeof(u8) * handle->total_lines * handle->char_width);

  DEMOWinSetWindowColor(handle, DEMOWIN_COLOR_RESET, 0, 0, 0, 0);

  handle->cursor_line = -1;

  handle->parent = NULL;

  __DEMOWin_add_node(handle);

  return (handle);
}

void DEMOWinDestroyWindow(DEMOWinInfo *handle) {
  BOOL old;

  ASSERTMSG(handle, "DEMOWinDestroyWindow(): NULL handle!\n");

  old = OSDisableInterrupts();

  __DEMOWin_delete_node(handle);

  OSFree(handle->buffer);

  OSFree(handle);

  OSRestoreInterrupts(old);
}

void DEMOWinOpenWindow(DEMOWinInfo *handle) {

  ASSERTMSG(handle, "DEMOWinOpenWindow(): NULL handle!\n");
  handle->flags |= DEMOWIN_FLAG_VISIBLE;
}

void DEMOWinCloseWindow(DEMOWinInfo *handle) {
  ASSERTMSG(handle, "DEMOWinCloseWindow(): NULL handle!\n");
  handle->flags &= ~DEMOWIN_FLAG_VISIBLE;
}

void DEMOWinSetWindowColor(DEMOWinInfo *handle, u32 item, u8 r, u8 g, u8 b,
                           u8 a) {

  ASSERTMSG(handle, "DEMOWinSetWinColor(): NULL window handle\n");

  switch (item) {
  case DEMOWIN_COLOR_CAPTION:
    handle->cap.r = r;
    handle->cap.g = g;
    handle->cap.b = b;
    handle->cap.a = a;
    break;

  case DEMOWIN_COLOR_BORDER:
    handle->border.r = r;
    handle->border.g = g;
    handle->border.b = b;
    handle->border.a = a;
    break;

  case DEMOWIN_COLOR_BKGND:
    handle->bkgnd.r = r;
    handle->bkgnd.g = g;
    handle->bkgnd.b = b;
    handle->bkgnd.a = a;
    break;

  case DEMOWIN_COLOR_RESET:
    handle->bkgnd.r = __DEF_BKGND_R;
    handle->bkgnd.g = __DEF_BKGND_G;
    handle->bkgnd.b = __DEF_BKGND_B;
    handle->bkgnd.a = __DEF_BKGND_A;

    handle->cap.r = __DEF_CAP_R;
    handle->cap.g = __DEF_CAP_G;
    handle->cap.b = __DEF_CAP_B;
    handle->cap.a = __DEF_CAP_A;

    handle->border.r = __DEF_BORDER_R;
    handle->border.g = __DEF_BORDER_G;
    handle->border.b = __DEF_BORDER_B;
    handle->border.a = __DEF_BORDER_A;
    break;

  default:
    ASSERTMSG(0, "DEMOWinSetWinColor(): Unknown item\n");
    break;
  }
}

void DEMOWinLogPrintf(DEMOWinInfo *handle, char *fmt, ...) {
  va_list vlist;
  char buffer[DEMOWIN_MAX_STRING_SIZE];
  u16 len;
  u16 i;
  BOOL old;

  u16 index = 0;

  va_start(vlist, fmt);
  vsprintf(buffer, fmt, vlist);
  va_end(vlist);

  old = OSDisableInterrupts();

  len = (u16)strlen(buffer);

  for (i = 0; i < len; i++) {

    if (buffer[i] == '\n') {

      handle->curr_output_line =
          (u16)((handle->curr_output_line + 1) % handle->total_lines);
      handle->curr_view_line =
          (u16)((handle->curr_view_line + 1) % handle->total_lines);
      handle->curr_output_col = 0;

      index = (u16)(handle->curr_output_line * handle->char_width +
                    handle->curr_output_col);
      memset((void *)(&handle->buffer[index]), 0x20,
             sizeof(u8) * handle->char_width);

    } else {
      index = (u16)(handle->curr_output_line * handle->char_width +
                    handle->curr_output_col);
      handle->buffer[index] = (u8)buffer[i];
      handle->curr_output_col++;
    }

    if (handle->curr_output_col >= handle->char_width) {
      handle->curr_output_col = 0;
      handle->curr_output_line =
          (u16)((handle->curr_output_line + 1) % handle->total_lines);
      handle->curr_view_line =
          (u16)((handle->curr_view_line + 1) % handle->total_lines);

      index = (u16)(handle->curr_output_line * handle->char_width +
                    handle->curr_output_col);
      memset((void *)(&handle->buffer[index]), 0x20,
             sizeof(u8) * handle->char_width);
    }
  }

  OSRestoreInterrupts(old);
}

void DEMOWinPrintfXY(DEMOWinInfo *handle, u16 col, u16 row, char *fmt, ...) {

  BOOL old;

  va_list vlist;
  char string[DEMOWIN_MAX_STRING_SIZE];

  u16 buffer_row;
  u16 i;
  u16 index;

  if ((row >= handle->char_height) || (col >= handle->char_width)) {
    return;
  }

  old = OSDisableInterrupts();

  va_start(vlist, fmt);
  vsprintf(string, fmt, vlist);
  va_end(vlist);

  buffer_row = (u16)(((handle->curr_view_line + handle->total_lines) -
                      (handle->char_height - 1)) %
                     handle->total_lines);

  buffer_row = (u16)((buffer_row + row) % handle->total_lines);

  string[handle->char_width - col] = 0;

  index = (u16)(buffer_row * handle->char_width + col);

  for (i = 0; i < strlen(string); i++) {
    handle->buffer[index + i] = (u8)string[i];
  }

  OSRestoreInterrupts(old);
}

void DEMOWinScrollWindow(DEMOWinInfo *handle, u32 dir) {

  BOOL old;

  u16 n;
  u16 v_start;

  ASSERTMSG(handle, "DEMOWinScrollWindow(): NULL handle!\n");
  ASSERTMSG(handle->num_scroll_lines,
            "DEMOWinScrollWindow(): No scrollback buffer!\n");

  switch (dir) {
  case DEMOWIN_SCROLL_UP:
    old = OSDisableInterrupts();

    n = (u16)(((handle->curr_view_line + handle->total_lines) - 1) %
              handle->total_lines);

    v_start = (u16)(((n + handle->total_lines) - handle->char_height + 1) %
                    handle->total_lines);

    if (v_start != handle->curr_output_line) {
      handle->curr_view_line = n;
    }
    OSRestoreInterrupts(old);
    break;

  case DEMOWIN_SCROLL_DOWN:
    old = OSDisableInterrupts();
    if (handle->curr_view_line != handle->curr_output_line) {
      handle->curr_view_line =
          (u16)((handle->curr_view_line + 1) % handle->total_lines);
    }
    OSRestoreInterrupts(old);
    break;

  case DEMOWIN_SCROLL_HOME:
    old = OSDisableInterrupts();
    handle->curr_view_line = handle->curr_output_line;
    OSRestoreInterrupts(old);
    break;

  default:
    ASSERTMSG(0, "DEMOWinScrollWindow(): Unknown token\n");
    break;
  }
}

void DEMOWinBringToFront(DEMOWinInfo *handle) {
  DEMOWinInfo *ptr;

  ASSERTMSG(__first_node, "DEMOWinBringToFront(): Window list is empty!\n");
  ASSERTMSG(handle, "DEMOWinBringToFront(): NULL handle!\n");

  if (DEMOWIN_PRIORITY_FORE == handle->priority) {
    return;
  }

  ptr = __first_node;
  while (ptr) {
    ptr->priority = DEMOWIN_PRIORITY_BACK;
    ptr = ptr->next;
  }

  handle->priority = DEMOWIN_PRIORITY_FORE;
}

void DEMOWinSendToBack(DEMOWinInfo *handle) {

  ASSERTMSG(handle, "DEMOWinSendToBack(): NULL handle!\n");
  handle->priority = DEMOWIN_PRIORITY_BACK;
}

void DEMOWinClearRow(DEMOWinInfo *handle, u16 row) {

  u16 buffer_row;
  u16 index;
  u16 i;
  BOOL old;

  ASSERTMSG(handle, "DEMOWinClearRow(): NULL handle!\n");

  if (row >= handle->char_height) {
    return;
  }

  old = OSDisableInterrupts();

  buffer_row = (u16)(((handle->curr_view_line + handle->total_lines) -
                      (handle->char_height - 1)) %
                     handle->total_lines);

  buffer_row = (u16)((buffer_row + row) % handle->total_lines);

  index = (u16)(buffer_row * handle->char_width);

  for (i = 0; i < handle->char_width; i++) {
    handle->buffer[index + i] = 0x20;
  }

  OSRestoreInterrupts(old);
}

void DEMOWinClearWindow(DEMOWinInfo *handle) {

  u16 buffer_row;
  u16 index;
  u16 i;
  BOOL old;

  ASSERTMSG(handle, "DEMOWinClearWindow(): NULL handle!\n");

  old = OSDisableInterrupts();

  buffer_row = (u16)(((handle->curr_view_line + handle->total_lines) -
                      (handle->char_height - 1)) %
                     handle->total_lines);

  for (i = 0; i < handle->char_height; i++) {
    index = (u16)(buffer_row * handle->char_width);
    memset((void *)(&handle->buffer[index]), 0x20,
           sizeof(u8) * handle->char_width);
    buffer_row = (u16)((buffer_row + 1) % handle->total_lines);
  }

  OSRestoreInterrupts(old);
}

void DEMOWinClearBuffer(DEMOWinInfo *handle) {
  BOOL old;

  ASSERTMSG(handle, "DEMOWinClearBuffer(): NULL handle!\n");

  old = OSDisableInterrupts();

  memset(handle->buffer, 0x20,
         sizeof(u8) * handle->total_lines * handle->char_width);

  OSRestoreInterrupts(old);
}

void DEMOWinRefresh(void) {

  DEMOWinInfo *ptr;

  u16 i;
  u16 index;
  u16 n;

  u16 y;

  BOOL old;

  ASSERTMSG(__first_node, "DEMOWinRefresh(): Windowlist is empty!\n");

  ptr = __first_node;

  while (ptr) {

    if (ptr->flags & DEMOWIN_FLAG_VISIBLE) {

      GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
      GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

      GXClearVtxDesc();
      GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
      GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);

      GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
      GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

      GXSetNumChans(1);
      GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX,
                    GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);

      GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
                    GX_COLOR0A0);
      GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
      GXSetNumTexGens(0);
      GXSetNumTevStages(1);

      GXSetLineWidth(6, GX_TO_ZERO);

      GXBegin(GX_QUADS, GX_VTXFMT0, 4);
      GXPosition3f32(ptr->x1, ptr->y1, (f32)(ptr->priority));
      GXColor4u8(ptr->bkgnd.r, ptr->bkgnd.g, ptr->bkgnd.b, ptr->bkgnd.a);

      GXPosition3f32(ptr->x2, ptr->y1, (f32)(ptr->priority));
      GXColor4u8(ptr->bkgnd.r, ptr->bkgnd.g, ptr->bkgnd.b, ptr->bkgnd.a);

      GXPosition3f32(ptr->x2, ptr->y2, (f32)(ptr->priority));
      GXColor4u8(ptr->bkgnd.r, ptr->bkgnd.g, ptr->bkgnd.b, ptr->bkgnd.a);

      GXPosition3f32(ptr->x1, ptr->y2, (f32)(ptr->priority));
      GXColor4u8(ptr->bkgnd.r, ptr->bkgnd.g, ptr->bkgnd.b, ptr->bkgnd.a);
      GXEnd();

      GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
      GXBegin(GX_QUADS, GX_VTXFMT0, 4);
      GXPosition3f32(ptr->x1, ptr->y1, (f32)(ptr->priority));
      GXColor4u8(ptr->cap.r, ptr->cap.g, ptr->cap.b, 0xff);

      GXPosition3f32(ptr->x2, ptr->y1, (f32)(ptr->priority));
      GXColor4u8(0, 0, 0, 64);

      GXPosition3f32(ptr->x2, ptr->y1 + FONT_CHAR_HEIGHT + 2,
                     (f32)(ptr->priority));
      GXColor4u8(0, 0, 0, 64);

      GXPosition3f32(ptr->x1, ptr->y1 + FONT_CHAR_HEIGHT + 2,
                     (f32)(ptr->priority));
      GXColor4u8(ptr->cap.r, ptr->cap.g, ptr->cap.b, 0xff);
      GXEnd();

      GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
      {
        u8 r1, g1, b1;
        u8 r2, g2, b2;
        u8 a;

        r1 = (u8)((f32)(ptr->border.r) * 1.3);
        g1 = (u8)((f32)(ptr->border.g) * 1.3);
        b1 = (u8)((f32)(ptr->border.b) * 1.3);

        r2 = (u8)((f32)(ptr->border.r) * 0.40);
        g2 = (u8)((f32)(ptr->border.g) * 0.40);
        b2 = (u8)((f32)(ptr->border.b) * 0.40);

        a = 0x40;

        GXSetLineWidth(6, GX_TO_ZERO);
        GXBegin(GX_LINESTRIP, GX_VTXFMT0, 7);
        GXPosition3f32(ptr->x1, ptr->y1, (f32)(ptr->priority));
        GXColor4u8(r1, g1, b1, a);

        GXPosition3f32(ptr->x2, ptr->y1, (f32)(ptr->priority));
        GXColor4u8(r1, g1, b1, a);

        GXPosition3f32(ptr->x2, ptr->y1, (f32)(ptr->priority));
        GXColor4u8(r2, g2, b2, a);

        GXPosition3f32(ptr->x2, ptr->y2, (f32)(ptr->priority));
        GXColor4u8(r2, g2, b2, a);

        GXPosition3f32(ptr->x1, ptr->y2, (f32)(ptr->priority));
        GXColor4u8(r2, g2, b2, a);

        GXPosition3f32(ptr->x1, ptr->y2, (f32)(ptr->priority));
        GXColor4u8(r1, g1, b1, a);

        GXPosition3f32(ptr->x1, ptr->y1, (f32)(ptr->priority));
        GXColor4u8(r1, g1, b1, a);
        GXEnd();
      }

      if (ptr->refresh) {

        (*(ptr->refresh))(ptr);
      }

      DEMOInitCaption(DM_FT_XLU, __rmp->fbWidth, __rmp->efbHeight);
      GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);

      old = OSDisableInterrupts();

      n = ptr->curr_view_line;

      y = (u16)(ptr->y2 - FONT_CHAR_HEIGHT - ptr->y_cal);
      n = (u16)(ptr->curr_view_line);
      index = (u16)(n * ptr->char_width);

      for (i = 0; i < ptr->char_height; i++) {

        __DEMOWin_puts_n((s16)(ptr->x1 + ptr->x_cal), (s16)y,
                         (s16)ptr->priority, ptr->char_width,
                         (char *)(&ptr->buffer[index]));

        y = (u16)(y - FONT_CHAR_HEIGHT);
        n = (u16)((((n + ptr->total_lines) - 1) % ptr->total_lines));
        index = (u16)(n * ptr->char_width);
      }

      DEMOPrintf((s16)(ptr->x1 + 2), (s16)ptr->y1, (s16)ptr->priority, "%s",
                 ptr->caption);

      if (ptr->cursor_line >= 0) {

        GXSetLineWidth(6, GX_TO_ZERO);

        GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);

        GXClearVtxDesc();
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);

        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

        GXSetNumChans(1);
        GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX,
                      GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);

        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
                      GX_COLOR0A0);
        GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GXSetNumTexGens(0);
        GXSetNumTevStages(1);

        GXSetLineWidth(6, GX_TO_ZERO);
        {
          u8 r;
          u8 g;
          u8 b;
          u8 a;
          s32 curr_y;

          curr_y = ptr->y2 - FONT_CHAR_HEIGHT - ptr->y_cal;
          curr_y = curr_y - (((ptr->char_height - 1) * FONT_CHAR_HEIGHT) -
                             ((ptr->cursor_line) * FONT_CHAR_HEIGHT));

          r = (u8)((f32)(ptr->bkgnd.r) * 1.9);
          g = (u8)((f32)(ptr->bkgnd.g) * 1.9);
          b = (u8)((f32)(ptr->bkgnd.b) * 1.9);
          a = 0x64;

          GXBegin(GX_QUADS, GX_VTXFMT0, 4);
          GXPosition3f32((f32)(ptr->x1), (f32)(curr_y), (f32)(ptr->priority));
          GXColor4u8(r, g, b, a);

          GXPosition3f32((f32)(ptr->x2), (f32)(curr_y), (f32)(ptr->priority));
          GXColor4u8(r, g, b, a);

          GXPosition3f32((f32)(ptr->x2), (f32)(curr_y + FONT_CHAR_HEIGHT),
                         (f32)(ptr->priority));
          GXColor4u8(r, g, b, a);

          GXPosition3f32((f32)(ptr->x1), (f32)(curr_y + FONT_CHAR_HEIGHT),
                         (f32)(ptr->priority));
          GXColor4u8(r, g, b, a);
          GXEnd();
        }
      }

      OSRestoreInterrupts(old);
    }

    ptr = ptr->next;
  }
}

static void __DEMOWin_add_node(DEMOWinInfo *handle) {

  ASSERTMSG(handle, "__add_node(): you're adding a NULL node!\n");

  if (NULL == __last_node) {

    __first_node = __last_node = __curr_node = handle;

    handle->next = NULL;
    handle->prev = NULL;

    ASSERTMSG(__first_node, "  > __first_node: NULL HANDLE!\n");
  } else {

    __last_node->next = handle;
    handle->next = NULL;
    handle->prev = __last_node;
    __last_node = handle;
  }

  handle->flags |= DEMOWIN_FLAG_ATTACHED;
}

static void __DEMOWin_delete_node(DEMOWinInfo *handle) {

  ASSERTMSG(handle, "__delete_node(): you're deleting a NULL node!\n");

  if (__first_node == handle) {

    if (handle->next) {
      __first_node = (handle->next);
      (handle->next)->prev = NULL;
    } else {

      __first_node = __last_node = NULL;
    }

  } else if (__last_node == handle) {

    if (handle->prev) {

      __last_node = (handle->prev);
      (handle->prev)->next = NULL;

    } else {

      __first_node = __last_node = NULL;
    }

  } else {
    (handle->prev)->next = (handle->next);
    (handle->next)->prev = (handle->prev);
  }

  handle->flags = (handle->flags & (~DEMOWIN_FLAG_ATTACHED));
}

static void __DEMOWin_puts_n(s16 x, s16 y, s16 z, u16 n, char *string) {
  s32 s, t;
  s32 w, len, i;

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_S16, 1);

  len = n;

  if (len > 0) {

    GXBegin(GX_QUADS, GX_VTXFMT0, (u16)(len * 4));
    for (i = 0; i < len; i++) {

      w = string[i] - ' ';
      s = (w % 8) * 16 + fontShift;
      t = (w / 8) * 16 + fontShift;
      GXPosition3s16((s16)(i * 8 + x), (s16)(y), z);
      GXTexCoord2s16((s16)(s), (s16)(t));
      GXPosition3s16((s16)(i * 8 + x + 8), (s16)(y), z);
      GXTexCoord2s16((s16)(s + 16), (s16)(t));
      GXPosition3s16((s16)(i * 8 + x + 8), (s16)(y + 8), z);
      GXTexCoord2s16((s16)(s + 16), (s16)(t + 16));
      GXPosition3s16((s16)(i * 8 + x), (s16)(y + 8), z);
      GXTexCoord2s16((s16)(s), (s16)(t + 16));
    }
    GXEnd();
  }
}

DEMOWinMenuInfo *DEMOWinCreateMenuWindow(DEMOWinMenuInfo *menu, u16 x, u16 y) {

  DEMOWinMenuItem *ptr;

  ptr = menu->items;

  menu->max_str_len = strlen(menu->title);
  menu->num_items = 0;

  while (!(ptr->flags & DEMOWIN_ITM_TERMINATOR)) {
    if (strlen(ptr->name) > menu->max_str_len) {
      menu->max_str_len = strlen(ptr->name);
    }
    (menu->num_items)++;
    ptr++;
  }

  if (menu->num_items > menu->max_display_items) {
    menu->num_display_items = menu->max_display_items;
  } else {
    menu->num_display_items = menu->num_items;
  }

  menu->handle = DEMOWinCreateWindow(
      (s16)x, (s16)y,
      (s16)(x + DEMOWIN_CALC_MENU_WIDTH_PIXELS(menu->max_str_len)),
      (s16)(y + DEMOWIN_CALC_MENU_HEIGHT_PIXELS(menu->num_display_items)),
      menu->title, 0, __DEMOWinMenu_refesh_menu);

  (menu->handle)->parent = menu;

  if (menu->num_items) {
    return (menu);
  }

  return (NULL);
}

void DEMOWinDestroyMenuWindow(DEMOWinMenuInfo *menu) {

  if (menu->handle) {
    DEMOWinCloseWindow(menu->handle);
    DEMOWinDestroyWindow(menu->handle);

    menu->handle = NULL;
  }
}

u32 DEMOWinMenuChild(DEMOWinMenuInfo *menu, BOOL child_flag) {

  DEMOWinPadInfo *pad;
  DEMOWinInfo *handle;

  u16 user_input;
  BOOL exit_flag = FALSE;
  u32 result = 0;

  handle = menu->handle;
  pad = &(menu->handle)->pad;

  DEMOWinOpenWindow(handle);
  DEMOWinBringToFront(handle);

  menu->curr_pos = 0;
  menu->display_pos = 0;

  if (menu->cb_open) {
    (*menu->cb_open)(menu, (u32)menu->curr_pos);
  }

  DEMOWinPadInit(pad);

  DEMOBeforeRender();
  DEMOWinRefresh();
  DEMODoneRender();

  DEMOWinPadRead(pad);

  DEMOBeforeRender();
  DEMOWinRefresh();
  DEMODoneRender();

  while (!exit_flag) {

    user_input = __DEMOWinMenu_get_user_input(pad);

    switch (user_input) {
    case DEMOWIN_MNU_UP:

      menu->curr_pos =
          (u16)((menu->curr_pos - 1 + menu->num_items) % (menu->num_items));
      while ((menu->items[menu->curr_pos].flags &
              (DEMOWIN_ITM_DISABLED | DEMOWIN_ITM_SEPARATOR))) {
        menu->curr_pos =
            (u16)((menu->curr_pos - 1 + menu->num_items) % (menu->num_items));
      }
      if (menu->cb_move) {
        (*(menu->cb_move))(menu, (u32)menu->curr_pos);
      }

      break;

    case DEMOWIN_MNU_DOWN:
      menu->curr_pos = (u16)((menu->curr_pos + 1) % (menu->num_items));
      while ((menu->items[menu->curr_pos].flags &
              (DEMOWIN_ITM_DISABLED | DEMOWIN_ITM_SEPARATOR))) {
        menu->curr_pos = (u16)((menu->curr_pos + 1) % (menu->num_items));
      }
      if (menu->cb_move) {
        (*(menu->cb_move))(menu, (u32)menu->curr_pos);
      }

      break;

    case DEMOWIN_MNU_LEFT:
      if (TRUE == child_flag) {

        exit_flag = TRUE;
        if (menu->cb_cancel) {
          (*(menu->cb_cancel))(menu, (u32)menu->curr_pos);
        }
      }
      break;

    case DEMOWIN_MNU_RIGHT:

      if (menu->cb_move) {
        (*(menu->cb_move))(menu, (u32)menu->curr_pos);
      }

      if (menu->items[menu->curr_pos].link) {

        if ((menu->items[menu->curr_pos].link)->handle) {

          ((menu->items[menu->curr_pos].link)->handle)->x1 =
              (u16)(handle->x1 + 20);
          ((menu->items[menu->curr_pos].link)->handle)->y1 =
              (u16)(handle->y1 + 20);

          result = DEMOWinMenuChild(menu->items[menu->curr_pos].link, TRUE);
          if ((menu->items[menu->curr_pos].link)->flags & DEMOWIN_MNU_EOM) {
            exit_flag = TRUE;
          }

        } else {

          DEMOWinCreateMenuWindow(menu->items[menu->curr_pos].link,
                                  (u16)(handle->x1 + 20),
                                  (u16)(handle->y1 + 20));
          result = DEMOWinMenuChild(menu->items[menu->curr_pos].link, TRUE);
          if ((menu->items[menu->curr_pos].link)->flags & DEMOWIN_MNU_EOM) {
            exit_flag = TRUE;
          }
          DEMOWinDestroyMenuWindow(menu->items[menu->curr_pos].link);
        }
        VIWaitForRetrace();
        DEMOWinPadRead(pad);
      }

      break;

    case DEMOWIN_MNU_SELECT:
      if (menu->cb_select) {
        (*(menu->cb_select))(menu, (u32)menu->curr_pos);
      }

      if (menu->items[menu->curr_pos].link) {

        if ((menu->items[menu->curr_pos].link)->handle) {

          ((menu->items[menu->curr_pos].link)->handle)->x1 =
              (u16)(handle->x1 + 20);
          ((menu->items[menu->curr_pos].link)->handle)->y1 =
              (u16)(handle->y1 + 20);

          result = DEMOWinMenuChild(menu->items[menu->curr_pos].link, TRUE);
          if ((menu->items[menu->curr_pos].link)->flags & DEMOWIN_MNU_EOM) {
            exit_flag = TRUE;
          }

        } else {

          DEMOWinCreateMenuWindow(menu->items[menu->curr_pos].link,
                                  (u16)(handle->x1 + 20),
                                  (u16)(handle->y1 + 20));

          result = DEMOWinMenuChild(menu->items[menu->curr_pos].link, TRUE);

          if ((menu->items[menu->curr_pos].link)->flags & DEMOWIN_MNU_EOM) {
            exit_flag = TRUE;
          }
          DEMOWinDestroyMenuWindow(menu->items[menu->curr_pos].link);
        }
        VIWaitForRetrace();
        DEMOWinPadRead(pad);

      } else {
        if (menu->items[menu->curr_pos].function) {
          (menu->items[menu->curr_pos].function)(menu, (u32)menu->curr_pos,
                                                 &result);
          if (menu->items[menu->curr_pos].flags & DEMOWIN_ITM_EOF) {
            exit_flag = TRUE;
          }
          VIWaitForRetrace();
          DEMOWinPadRead(pad);
        }
      }
      break;

    case DEMOWIN_MNU_CANCEL:
      if (menu->cb_cancel) {
        (*(menu->cb_cancel))(menu, (u32)menu->curr_pos);
      }
      exit_flag = TRUE;
      break;
    }

    if (menu->curr_pos > (menu->display_pos + menu->num_display_items - 1)) {
      menu->display_pos = menu->curr_pos - menu->num_display_items + 1;

    } else if (menu->curr_pos < menu->display_pos) {
      menu->display_pos = menu->curr_pos;
    }

    if (menu->display_pos > menu->curr_pos) {
      handle->cursor_line = (s16)(menu->display_pos - menu->curr_pos);
    } else {
      handle->cursor_line = (s16)(menu->curr_pos - menu->display_pos);
    }

    DEMOBeforeRender();
    DEMOWinRefresh();
    DEMODoneRender();
  }

  DEMOWinCloseWindow(handle);

  DEMOBeforeRender();
  DEMOWinRefresh();
  DEMODoneRender();

  return (result);
}

static void __DEMOWinMenu_refesh_menu(DEMOWinInfo *w) {

  DEMOWinMenuInfo *m;

  s32 i;
  s32 j;
  char check;
  char para_start;
  char para_end;
  char link;

  DEMOWinClearWindow(w);

  m = (w->parent);

  j = m->display_pos;

  for (i = 0; i < m->num_display_items; i++) {
    if (m->items[j].flags & DEMOWIN_ITM_SEPARATOR) {

      if (strlen(m->items[(u16)j].name)) {
        DEMOWinPrintfXY(w, 0, (u16)i, "   %s     ", m->items[(u16)j].name);
      }

    } else {

      check = (char)((m->items[j].flags & DEMOWIN_ITM_CHK_STATE) ? 'X' : ' ');
      para_start =
          (char)((m->items[j].flags & DEMOWIN_ITM_DISABLED) ? '(' : ' ');
      para_end = (char)((m->items[j].flags & DEMOWIN_ITM_DISABLED) ? ')' : ' ');
      link = (char)((NULL != m->items[j].link) ? '>' : ' ');

      DEMOWinPrintfXY(w, 0, (u16)i, "%c %c%s%c %c", check, para_start,
                      m->items[(u16)j].name, para_end, link);
    }
    j++;
  }
}

void DEMOWinPadInit(DEMOWinPadInfo *p) {
  u16 i;

  for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
    p->old_button[i] = 0;
    p->changed_button[i] = 0;
    p->repeat_button[i] = 0;
    p->repeat_ctr[i] = 0;
  }
}

void DEMOWinPadRead(DEMOWinPadInfo *p) {

  PADStatus *pad;

  u16 index;

  u32 curr;
  u32 old;

  u32 repeat;

  PADRead(p->pads);

  for (index = 0; index < PAD_MAX_CONTROLLERS; index++) {

    old = p->old_button[index];
    pad = &(p->pads[index]);

    curr = ((pad->stickX > PAD_THRESHOLD) ? DEMOWIN_STICK_R : 0) |
           ((pad->stickX < -PAD_THRESHOLD) ? DEMOWIN_STICK_L : 0) |
           ((pad->stickY > PAD_THRESHOLD) ? DEMOWIN_STICK_U : 0) |
           ((pad->stickY < -PAD_THRESHOLD) ? DEMOWIN_STICK_D : 0) |
           ((pad->substickX > PAD_THRESHOLD) ? DEMOWIN_SUBSTICK_R : 0) |
           ((pad->substickX < -PAD_THRESHOLD) ? DEMOWIN_SUBSTICK_L : 0) |
           ((pad->substickY > PAD_THRESHOLD) ? DEMOWIN_SUBSTICK_U : 0) |
           ((pad->substickY < -PAD_THRESHOLD) ? DEMOWIN_SUBSTICK_D : 0) |
           ((pad->triggerLeft > TRIGGER_THRESHOLD) ? DEMOWIN_TRIGGER_L : 0) |
           ((pad->triggerRight > TRIGGER_THRESHOLD) ? DEMOWIN_TRIGGER_R : 0) |
           (u32)pad->button;

    p->changed_button[index] = (u32)((old ^ curr) & curr);

    if (curr) {
      if (old == curr) {
        p->repeat_ctr[index]++;
      } else {
        p->repeat_ctr[index] = 1;
      }
    } else {
      p->repeat_ctr[index] = 0;
    }

    repeat = p->repeat_ctr[index];

    if (repeat == 1) {
      p->repeat_button[index] = curr;
    } else if (repeat > __DEMOWIN_PAD_repeat_threshold) {
      if (((repeat - __DEMOWIN_PAD_repeat_threshold) %
           __DEMOWIN_PAD_repeat_rate) == 0) {
        p->repeat_button[index] = curr;
      } else {
        p->repeat_button[index] = 0;
      }
    } else {
      p->repeat_button[index] = 0;
    }
    p->old_button[index] = curr;
  }
}

static u16 __DEMOWinMenu_get_user_input(DEMOWinPadInfo *p) {

  u16 user_input;

  DEMOWinPadRead(p);

  if (p->repeat_button[0] & (PAD_BUTTON_UP | DEMOWIN_STICK_U)) {
    user_input = DEMOWIN_MNU_UP;
  } else if (p->repeat_button[0] & (PAD_BUTTON_DOWN | DEMOWIN_STICK_D)) {
    user_input = DEMOWIN_MNU_DOWN;
  } else if (p->repeat_button[0] & (PAD_BUTTON_LEFT | DEMOWIN_STICK_L)) {
    user_input = DEMOWIN_MNU_LEFT;
  } else if (p->repeat_button[0] & (PAD_BUTTON_RIGHT | DEMOWIN_STICK_R)) {
    user_input = DEMOWIN_MNU_RIGHT;
  } else if (p->changed_button[0] & PAD_BUTTON_A) {
    user_input = DEMOWIN_MNU_SELECT;
  } else if (p->changed_button[0] & PAD_BUTTON_B) {
    user_input = DEMOWIN_MNU_CANCEL;
  } else {
    user_input = 0;
  }

  return (user_input);
}

void DEMOWinSetRepeat(u32 threshold, u32 rate) {

  __DEMOWIN_PAD_repeat_rate = rate;
  __DEMOWIN_PAD_repeat_threshold = threshold;
}

void DEMOWinResetRepeat(void) {

  __DEMOWIN_PAD_repeat_threshold = DEMOWIN_PAD_REPEAT_THRESH_DEF;
  __DEMOWIN_PAD_repeat_rate = DEMOWIN_PAD_REPEAT_RATE_DEF;
}

DEMOWinListInfo *DEMOWinCreateListWindow(DEMOWinListInfo *list, u16 x, u16 y) {

  DEMOWinListItem *ptr;

  ASSERTMSG(list, "DEMOWinCreateListWindow(): List is NULL!\n");

  ptr = list->items;

  list->max_str_len = strlen(list->title);
  list->num_items = 0;

  while (!(ptr->flags & DEMOWIN_ITM_TERMINATOR)) {
    if (strlen(ptr->name) > list->max_str_len) {
      list->max_str_len = strlen(ptr->name);
    }
    (list->num_items)++;
    ptr++;
  }

  if (list->num_items > list->max_display_items) {
    list->num_display_items = list->max_display_items;
  } else {
    list->num_display_items = list->num_items;
  }

  list->handle = DEMOWinCreateWindow(
      (s16)x, (s16)y,
      (s16)(x + DEMOWIN_CALC_MENU_WIDTH_PIXELS(list->max_str_len)),
      (s16)(y + DEMOWIN_CALC_MENU_HEIGHT_PIXELS(list->num_display_items)),
      list->title, 0, __DEMOWinList_refresh_list);

  (list->handle)->parent = (void *)(list);

  if (list->num_items) {
    return (list);
  }

  return (NULL);
}

void DEMOWinDestroyListWindow(DEMOWinListInfo *list) {

  if (list->handle) {
    DEMOWinCloseWindow(list->handle);
    DEMOWinDestroyWindow(list->handle);

    list->handle = NULL;
  }
}

static void __DEMOWinList_refresh_list(DEMOWinInfo *w) {

  DEMOWinListInfo *l;

  s32 i;
  s32 j;

  l = (w->parent);

  l->curr_pos = l->curr_pos % l->num_items;

  if (l->curr_pos > (l->display_pos + l->num_display_items - 1)) {
    l->display_pos = l->curr_pos - l->num_display_items + 1;

  } else if (l->curr_pos < l->display_pos) {
    l->display_pos = l->curr_pos;
  }

  if (l->cursor_state) {

    if (l->display_pos > l->curr_pos) {
      w->cursor_line = (s16)(l->display_pos - l->curr_pos);
    } else {
      w->cursor_line = (s16)(l->curr_pos - l->display_pos);
    }

  } else {
    w->cursor_line = -1;
  }

  DEMOWinClearWindow(w);

  j = l->display_pos;

  for (i = 0; i < l->num_display_items; i++) {
    if (!(l->items[j].flags & DEMOWIN_ITM_SEPARATOR)) {
      DEMOWinPrintfXY(w, 0, (u16)i, " %s ", l->items[(u16)j].name);
    }
    j++;
  }
}

void DEMOWinListSetCursor(DEMOWinListInfo *list, BOOL x) {

  list->cursor_state = x;
}

s32 DEMOWinListScrollList(DEMOWinListInfo *list, u32 dir) {

  ASSERTMSG(list, "DEMOWinListScrollList(): NULL handle!\n");

  switch (dir) {
  case DEMOWIN_SCROLL_UP:
    if (list->display_pos) {

      list->display_pos =
          (u16)((list->display_pos - 1 + list->num_items) % (list->num_items));
    }
    break;

  case DEMOWIN_SCROLL_DOWN:
    if (list->display_pos < (list->num_items - list->num_display_items)) {

      list->display_pos = (u16)((list->display_pos + 1) % (list->num_items));
    }
    break;

  case DEMOWIN_SCROLL_HOME:
    list->display_pos = 0;
    break;

  default:
    ASSERTMSG(0, "DEMOWinListScrollList(): Invalid dimension!\n");
    break;
  }

  if (list->curr_pos > (list->display_pos + list->num_display_items - 1)) {
    list->curr_pos = list->display_pos + list->num_display_items - 1;
  } else if (list->curr_pos < list->display_pos) {
    list->curr_pos = list->display_pos;
  }

  return (list->display_pos);
}

s32 DEMOWinListMoveCursor(DEMOWinListInfo *list, u32 dir) {

  ASSERTMSG(list, "DEMOWinListScrollList(): NULL handle!\n");

  switch (dir) {
  case DEMOWIN_LIST_UP:
    list->curr_pos = (list->curr_pos + list->num_items - 1) % list->num_items;
    break;

  case DEMOWIN_LIST_DOWN:
    list->curr_pos = (list->curr_pos + 1) % list->num_items;
    break;

  default:
    ASSERTMSG(0, "DEMOWinListMoveCursor(): Invalid dimension!\n");
    break;
  }

  return (list->curr_pos);
}
