#ifndef __DEMOWIN_H__
#define __DEMOWIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>

#define DEMOWIN_MAX_STRING_SIZE     128
#define FONT_CHAR_WIDTH               8
#define FONT_CHAR_HEIGHT              8
#define DEMOWIN_X_CHAR_PAD            7
#define DEMOWIN_Y_CHAR_PAD            2

#define DEMOWIN_CALC_MENU_WIDTH_PIXELS(w)   (((w+DEMOWIN_X_CHAR_PAD)*FONT_CHAR_WIDTH)+4) 
#define DEMOWIN_CALC_MENU_HEIGHT_PIXELS(h)  (((DEMOWIN_Y_CHAR_PAD+h)*FONT_CHAR_HEIGHT)+4)

#define DEMOWIN_FLAG_ATTACHED   0x00000001
#define DEMOWIN_FLAG_VISIBLE    0x00000002

#define DEMOWIN_PRIORITY_FORE   0x0000
#define DEMOWIN_PRIORITY_BACK   0x0001

#define DEMOWIN_COLOR_CAPTION   0
#define DEMOWIN_COLOR_BKGND     1
#define DEMOWIN_COLOR_BORDER    2
#define DEMOWIN_COLOR_RESET     3

#define DEMOWIN_SCROLL_HOME     0
#define DEMOWIN_SCROLL_UP       1
#define DEMOWIN_SCROLL_DOWN     2

#define DEMOWIN_LIST_HOME       0
#define DEMOWIN_LIST_UP         1
#define DEMOWIN_LIST_DOWN       2


typedef struct 
{
    PADStatus pads           [PAD_MAX_CONTROLLERS];
    u32       button         [PAD_MAX_CONTROLLERS];
    u32       old_button     [PAD_MAX_CONTROLLERS];
    u32       changed_button [PAD_MAX_CONTROLLERS];
    u32       repeat_button  [PAD_MAX_CONTROLLERS];
    u32       repeat_ctr     [PAD_MAX_CONTROLLERS];
} DEMOWinPadInfo;

#define PAD_THRESHOLD        64
#define TRIGGER_THRESHOLD   128

#define DEMOWIN_ITM_NONE       0x00000000
#define DEMOWIN_ITM_DISABLED   0x00000001
#define DEMOWIN_ITM_CHECK      0x00000002
#define DEMOWIN_ITM_CHK_STATE  0x00000004
#define DEMOWIN_ITM_SEPARATOR  0x00000008
#define DEMOWIN_ITM_EOF        0x00000010
#define DEMOWIN_ITM_TERMINATOR 0x80000000

#define DEMOWIN_MNU_NONE       0x00000000
#define DEMOWIN_MNU_EOM        0x00000001

#define DEMOWIN_MNU_UP         0x0001
#define DEMOWIN_MNU_DOWN       0x0002
#define DEMOWIN_MNU_LEFT       0x0003
#define DEMOWIN_MNU_RIGHT      0x0004
#define DEMOWIN_MNU_SELECT     0x0005 
#define DEMOWIN_MNU_CANCEL     0x0006

#define DEMOWIN_PAD_REPEAT_THRESH_DEF  15
#define DEMOWIN_PAD_REPEAT_RATE_DEF     2

#define DEMOWIN_STICK_U           0x00010000
#define DEMOWIN_STICK_D           0x00020000
#define DEMOWIN_STICK_R           0x00040000
#define DEMOWIN_STICK_L           0x00080000
#define DEMOWIN_SUBSTICK_U        0x00100000
#define DEMOWIN_SUBSTICK_D        0x00200000
#define DEMOWIN_SUBSTICK_R        0x00400000
#define DEMOWIN_SUBSTICK_L        0x00800000
#define DEMOWIN_TRIGGER_R         0x01000000
#define DEMOWIN_TRIGGER_L         0x02000000

typedef struct STRUCT_DEMOWIN
{
    s32 x1;
    s32 y1; 

    s32 x2;
    s32 y2;

    u32 priority;
        
    u32 flags;                  

    u16 x_cal;
    u16 y_cal;

    u16 pixel_width;
    u16 pixel_height;

    u16 char_width;
    u16 char_height;

    u16 num_scroll_lines;
    u16 total_lines;

    u16 curr_output_line;
    u16 curr_output_col;
    u16 curr_view_line;
    s16 cursor_line;

    char *caption;
    u8 *buffer;

    GXColor bkgnd;
    GXColor cap;
    GXColor border;

    void (*refresh)(struct STRUCT_DEMOWIN *handle);
    struct STRUCT_DEMOWIN *next;
    struct STRUCT_DEMOWIN *prev;
    void *parent;

    DEMOWinPadInfo  pad;
} DEMOWinInfo;

struct STRUCT_MENU;
typedef struct STRUCT_MENU_ITEM
{
    char *name;
    u32 flags;

    void (*function)(struct STRUCT_MENU *menu, u32 item, u32 *result);       
    struct STRUCT_MENU *link;
} DEMOWinMenuItem;

typedef struct STRUCT_MENU
{
    char *title;
    DEMOWinInfo *handle;
    DEMOWinMenuItem *items;
    s32 max_display_items;
    u32 flags;

    void (*cb_open)(struct STRUCT_MENU *menu, u32 item);
    void (*cb_move)(struct STRUCT_MENU *menu, u32 item);
    void (*cb_select)(struct STRUCT_MENU *menu, u32 item);
    void (*cb_cancel)(struct STRUCT_MENU *menu, u32 item);

    s32 num_display_items;
    s32 num_items;
    u32 max_str_len;
    s32 curr_pos;
    s32 display_pos;
} DEMOWinMenuInfo;



typedef struct STRUCT_LISTBOX_ITEM
{
    char *name;
    u32   flags;
} DEMOWinListItem;


typedef struct STRUCT_LISTBOX
{
    char *title;
    DEMOWinInfo *handle;
    DEMOWinListItem *items;

    s32 max_display_items;
    u32 flags;

    s32 num_display_items;
    s32 num_items;
    u32 max_str_len;
    s32 curr_pos;
    s32 display_pos;
    BOOL cursor_state;
} DEMOWinListInfo;

void DEMOWinInit(void);
DEMOWinInfo *DEMOWinCreateWindow(s32 x1, s32 y1, s32 x2, s32 y2, char *caption, u16 scroll, void *func);
void DEMOWinOpenWindow(DEMOWinInfo *handle);
void DEMOWinCloseWindow(DEMOWinInfo *handle);
void DEMOWinDestroyWindow(DEMOWinInfo *handle);

void DEMOWinScrollWindow(DEMOWinInfo *handle, u32 dir);
void DEMOWinSetWindowColor(DEMOWinInfo *handle, u32 item, u8 r, u8 g, u8 b, u8 a);
void DEMOWinBringToFront(DEMOWinInfo *handle);
void DEMOWinSendToBack(DEMOWinInfo *handle);

void DEMOWinSetCursorLine(DEMOWinInfo *handle, s16 n);
s32 DEMOWinGetCursorLine(DEMOWinInfo *handle);

void DEMOWinLogPrintf(DEMOWinInfo *handle, char *fmt, ...);
void DEMOWinPrintfXY(DEMOWinInfo *handle, u16 col, u16 row, char *fmt, ...);
void DEMOWinClearRow(DEMOWinInfo *handle, u16 row);
void DEMOWinClearWindow(DEMOWinInfo *handle);
void DEMOWinClearBuffer(DEMOWinInfo *handle);

void DEMOWinPadInit(DEMOWinPadInfo *p);
void DEMOWinPadRead(DEMOWinPadInfo *p);
void DEMOWinResetRepeat(void);
void DEMOWinSetRepeat(u32 threshold, u32 rate);

void DEMOWinRefresh(void);

DEMOWinMenuInfo *DEMOWinCreateMenuWindow(DEMOWinMenuInfo *menu, u16 x, u16 y);
u32 DEMOWinMenuChild(DEMOWinMenuInfo *items, BOOL child_flag);
void DEMOWinDestroyMenuWindow(DEMOWinMenuInfo *menu);

DEMOWinListInfo *DEMOWinCreateListWindow(DEMOWinListInfo *list, u16 x, u16 y);
void DEMOWinDestroyListWindow(DEMOWinListInfo *list);
void DEMOWinListSetCursor(DEMOWinListInfo *list, BOOL x);
s32 DEMOWinListScrollList(DEMOWinListInfo *list, u32 dir); 
s32 DEMOWinListMoveCursor(DEMOWinListInfo *list, u32 dir);

#define DEMOWinGetNumRows(h) (h->char_width)
#define DEMOWinGetNumCols(h) (h->char_height)
#define DEMOWinMenu(ptr) DEMOWinMenuChild(ptr,FALSE)

#define DEMOWinListGetCurPos(l) (l->curr_pos)
#define DEMOWinListSetCurPos(l,x) (l->curr_pos = (s32)(x))


#ifdef __cplusplus
}
#endif

#endif
