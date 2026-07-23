/*---------------------------------------------------------------------------*
  Project:  libPorpoise Demo Library
  File:     DEMOWin.h
  
  Window/menu system for demo framework.
  
  Based on Nintendo's Revolution SDK demo library.
 *---------------------------------------------------------------------------*/

#ifndef DOLPHIN_DEMO_WIN_H
#define DOLPHIN_DEMO_WIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <dolphin/types.h>
#include <dolphin/pad.h>
#include <dolphin/gx/GXStruct.h>

/*---------------------------------------------------------------------------*
 * Definitions
 *---------------------------------------------------------------------------*/

// font-size calibrations
#define DEMOWIN_MAX_STRING_SIZE     128
#define FONT_CHAR_WIDTH               8
#define FONT_CHAR_HEIGHT              8
#define DEMOWIN_X_CHAR_PAD            7
#define DEMOWIN_Y_CHAR_PAD            2

// calculating minimum window dimensions
// takes number of characters as parameters; returns size in pixels
#define DEMOWIN_CALC_MENU_WIDTH_PIXELS(w)   (((w+DEMOWIN_X_CHAR_PAD)*FONT_CHAR_WIDTH)+4) 
#define DEMOWIN_CALC_MENU_HEIGHT_PIXELS(h)  (((DEMOWIN_Y_CHAR_PAD+h)*FONT_CHAR_HEIGHT)+4)

// window flags
#define DEMOWIN_FLAG_ATTACHED   0x00000001
#define DEMOWIN_FLAG_VISIBLE    0x00000002

// window priority (Z value) - note that DEMO lib looks down the positive Z axis
#define DEMOWIN_PRIORITY_FORE   0x0000
#define DEMOWIN_PRIORITY_BACK   0x0001

// window color attributes
#define DEMOWIN_COLOR_CAPTION   0
#define DEMOWIN_COLOR_BKGND     1
#define DEMOWIN_COLOR_BORDER    2
#define DEMOWIN_COLOR_RESET     3

// for window scrolling function
#define DEMOWIN_SCROLL_HOME     0 // set curr_view_line to curr_output_line
#define DEMOWIN_SCROLL_UP       1 // text moves down
#define DEMOWIN_SCROLL_DOWN     2 // text moves up

#define DEMOWIN_LIST_HOME       0
#define DEMOWIN_LIST_UP         1
#define DEMOWIN_LIST_DOWN       2

// ******************************************
// DEMOWIN PAD structure
// ******************************************

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

// *****************************************
// For PopUp menu system
// *****************************************

// item flags
#define DEMOWIN_ITM_NONE       0x00000000 //
#define DEMOWIN_ITM_DISABLED   0x00000001 //
#define DEMOWIN_ITM_CHECK      0x00000002 //
#define DEMOWIN_ITM_CHK_STATE  0x00000004 //
#define DEMOWIN_ITM_SEPARATOR  0x00000008 //
#define DEMOWIN_ITM_EOF        0x00000010 // menu should exit after invoking this function
#define DEMOWIN_ITM_TERMINATOR 0x80000000

// window flag
#define DEMOWIN_MNU_NONE       0x00000000 //
#define DEMOWIN_MNU_EOM        0x00000001 // parent should exit after invoking this menu

// menu navigation
#define DEMOWIN_MNU_UP         0x0001
#define DEMOWIN_MNU_DOWN       0x0002
#define DEMOWIN_MNU_LEFT       0x0003
#define DEMOWIN_MNU_RIGHT      0x0004
#define DEMOWIN_MNU_SELECT     0x0005 
#define DEMOWIN_MNU_CANCEL     0x0006

// *****************************************
// For PAD system
// *****************************************
#define DEMOWIN_PAD_REPEAT_THRESH_DEF  15   // in video frames
#define DEMOWIN_PAD_REPEAT_RATE_DEF     2   // in video frames

#define DEMOWIN_STICK_U           0x00010000      // analog value exceed specified threshold
#define DEMOWIN_STICK_D           0x00020000
#define DEMOWIN_STICK_R           0x00040000
#define DEMOWIN_STICK_L           0x00080000
#define DEMOWIN_SUBSTICK_U        0x00100000
#define DEMOWIN_SUBSTICK_D        0x00200000
#define DEMOWIN_SUBSTICK_R        0x00400000
#define DEMOWIN_SUBSTICK_L        0x00800000
#define DEMOWIN_TRIGGER_R         0x01000000
#define DEMOWIN_TRIGGER_L         0x02000000

/*---------------------------------------------------------------------------*
 * Types
 *---------------------------------------------------------------------------*/

typedef struct STRUCT_DEMOWIN
{
    // user defined window properties
    s32      x1;
    s32      y1; 
             
    s32      x2;
    s32      y2;
             
    u32      priority;
             
    u32      flags;                  
             
    u16      x_cal;             // corrective offset, for centering window contents horizontally
    u16      y_cal;             // corrective offset, for centering window contents vertically
             
    u16      pixel_width;       // width of window, in pixels
    u16      pixel_height;      // height of window, in pixels
             
    u16      char_width;        // width of window, in characters
    u16      char_height;       // height of window, in characters
             
    u16      num_scroll_lines;  // number of scrollback lines
    u16      total_lines;       // total number of buffer lines (including visible)
             
    u16      curr_output_line;  // current row in buffer at which "log printf" will output text  
    u16      curr_output_col;   // current col in a line at which "log printf" will output text
    u16      curr_view_line;    // for scrolling through the buffer
    s16      cursor_line;       // if <0, no highlight; otherwise, draw highlight
                   
    char    *caption;
    u8      *buffer;            // private; dynamically allocated

    GXColor  bkgnd;
    GXColor  cap;
    GXColor  border;

    void     (*refresh)(struct STRUCT_DEMOWIN *handle);
    struct   STRUCT_DEMOWIN   *next;
    struct   STRUCT_DEMOWIN   *prev;
    void     *parent;           // pointer to a 'superstructure' which includes this window handle

    DEMOWinPadInfo  pad;        // pad data for given window

} DEMOWinInfo;

/*---------------------------------------------------------------------
 * "Modal" menu system
 *---------------------------------------------------------------------*/

// ******************************************
// Menu item
// ******************************************

struct STRUCT_MENU;
typedef struct STRUCT_MENU_ITEM
{
    char *name;                                     // NULL terminated string; name of menu item
    u32  flags;                                     // menu item attributes

    void (*function)(struct STRUCT_MENU *menu, u32 item, u32 *result);       
    struct STRUCT_MENU *link;                       // child menu, if any

} DEMOWinMenuItem;

// ******************************************
// Menu information
// ******************************************
typedef struct STRUCT_MENU
{
    char            *title;                         // menu title
    DEMOWinInfo     *handle;                        // window associated with this menu
    DEMOWinMenuItem *items;                         // list of menu items
    s32              max_display_items;             // max number of items to display at once
    u32              flags;                         // menu attributes

    void (*cb_open)  (struct STRUCT_MENU *menu, u32 item);
    void (*cb_move)  (struct STRUCT_MENU *menu, u32 item);
    void (*cb_select)(struct STRUCT_MENU *menu, u32 item);
    void (*cb_cancel)(struct STRUCT_MENU *menu, u32 item);

    // private menu state information
    s32 num_display_items;      // actual number of items to display
    s32 num_items;              // actual number of items in list
    u32 max_str_len;            // max item/caption string length
    s32 curr_pos;               // curr position in list
    s32 display_pos;            // curr 'home' row displayed in list

} DEMOWinMenuInfo;

/*---------------------------------------------------------------------
 * List box stuff
 *---------------------------------------------------------------------*/

// ******************************************
// Listbox item
// ******************************************
typedef struct STRUCT_LISTBOX_ITEM
{
    char *name;     // name of item
    u32   flags;    // item attributes, if any
} DEMOWinListItem;

// ******************************************
// Listbox information
// ******************************************
typedef struct STRUCT_LISTBOX
{
    char            *title;      // listbox title/caption
    DEMOWinInfo     *handle;     // Window handle
    DEMOWinListItem *items;      // pointer to array of items

    s32  max_display_items;      // maximum number of items to display
    u32  flags;                  // listbox attributes, if any

    // private menu state information
    s32  num_display_items;      // actual number of items to display
    s32  num_items;              // actual number of items in list
    u32  max_str_len;            // max item/caption string length
    s32  curr_pos;               // curr position in list
    s32  display_pos;            // curr 'home' row displayed in list
    BOOL cursor_state;           // ON or OFF

} DEMOWinListInfo;

/*---------------------------------------------------------------------------*
 * Function Prototypes
 *---------------------------------------------------------------------------*/

// initialization
void             DEMOWinInit             (void);

// instantiation
DEMOWinInfo     *DEMOWinCreateWindow     (s32 x1, s32 y1, s32 x2, s32 y2, char *caption, u16 scroll, void *func);
void             DEMOWinOpenWindow       (DEMOWinInfo *handle);
void             DEMOWinCloseWindow      (DEMOWinInfo *handle);
void             DEMOWinDestroyWindow    (DEMOWinInfo *handle);

// attribute control
void             DEMOWinScrollWindow     (DEMOWinInfo *handle, u32 dir);
void             DEMOWinSetWindowColor   (DEMOWinInfo *handle, u32 item, u8 r, u8 g, u8 b, u8 a);
void             DEMOWinBringToFront     (DEMOWinInfo *handle);
void             DEMOWinSendToBack       (DEMOWinInfo *handle);

void             DEMOWinSetCursorLine    (DEMOWinInfo *handle, s16 n);
s32              DEMOWinGetCursorLine    (DEMOWinInfo *handle);

// I/O 
void             DEMOWinLogPrintf        (DEMOWinInfo *handle, char *fmt, ...);
void             DEMOWinPrintfXY         (DEMOWinInfo *handle, u16 col, u16 row, char *fmt, ...);
void             DEMOWinClearRow         (DEMOWinInfo *handle, u16 row);
void             DEMOWinClearWindow      (DEMOWinInfo *handle);
void             DEMOWinClearBuffer      (DEMOWinInfo *handle);

void             DEMOWinPadInit          (DEMOWinPadInfo *p);
void             DEMOWinPadRead          (DEMOWinPadInfo *p);
void             DEMOWinResetRepeat      (void);
void             DEMOWinSetRepeat        (u32 threshold, u32 rate);

// infrastructure
void             DEMOWinRefresh          (void);

// menu system
DEMOWinMenuInfo *DEMOWinCreateMenuWindow (DEMOWinMenuInfo *menu, u16 x, u16 y);
u32              DEMOWinMenuChild        (DEMOWinMenuInfo *items, BOOL child_flag);
void             DEMOWinDestroyMenuWindow(DEMOWinMenuInfo *menu);

// List box
DEMOWinListInfo *DEMOWinCreateListWindow (DEMOWinListInfo *list, u16 x, u16 y);
void             DEMOWinDestroyListWindow(DEMOWinListInfo *list);
void             DEMOWinListSetCursor    (DEMOWinListInfo *list, BOOL x);
s32              DEMOWinListScrollList   (DEMOWinListInfo *list, u32 dir); 
s32              DEMOWinListMoveCursor   (DEMOWinListInfo *list, u32 dir);

// aliased functions
#define          DEMOWinGetNumRows(h) (h->char_width)
#define          DEMOWinGetNumCols(h) (h->char_height)
#define          DEMOWinMenu(ptr) DEMOWinMenuChild(ptr,FALSE)

#define          DEMOWinListGetCurPos(l) (l->curr_pos)
#define          DEMOWinListSetCurPos(l,x) (l->curr_pos = (s32)(x))

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* DOLPHIN_DEMO_WIN_H */

