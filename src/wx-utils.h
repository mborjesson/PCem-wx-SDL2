#include <wx/defs.h>

#ifndef LONG_PARAM
#define LONG_PARAM wxIntPtr
#endif

#ifndef INT_PARAM
#define INT_PARAM wxInt32
#endif

#ifdef __cplusplus
extern "C" {
#endif
int wx_messagebox(void* nothing, char* message, char* title, int style);
int wx_xrcid(const char* s);
int wx_filedialog(void* window, const char* title, const char* path, const char* extensions, int open, char* file);
void wx_checkmenuitem(void* window, int id, int checked);
void wx_enablemenuitem(void* menu, int id, int enable);
void* wx_getmenu(void* window);

void wx_setwindowtitle(void* window, char* s);
int wx_sendmessage(void* window, int type, INT_PARAM param1, LONG_PARAM param2);
void* wx_getdlgitem(void* window, int id);
void wx_setdlgitemtext(void* window, int id, char* str);
void wx_enablewindow(void* window, int enabled);
void wx_showwindow(void* window, int show);
void wx_togglewindow(void* window);

void wx_enddialog(void* window, int ret_code);

int wx_dialogbox(void* window, char* name, int(*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2));

void wx_exit(void* window, int value);

int (*wx_keydown_func)(void* window, int keycode, int modifiers);
int (*wx_keyup_func)(void* window, int keycode, int modifiers);
#ifdef __cplusplus
}
#endif

#define WX_ID wx_xrcid

#define WX_MB_CHECKED 1
#define WX_MB_UNCHECKED 0

#define WX_INITDIALOG 1
#define WX_COMMAND 2

#define WX_CB_ADDSTRING 1
#define WX_CB_SETCURSEL 2
#define WX_CB_GETCURSEL 3
#define WX_CB_RESETCONTENT 4
#define WX_CB_GETLBTEXT 5

#define WX_BM_SETCHECK 20
#define WX_BM_GETCHECK 21

#define WX_WM_SETTEXT 40
#define WX_WM_GETTEXT 41

#define WX_UDM_SETPOS 50
#define WX_UDM_GETPOS 51
#define WX_UDM_SETINCR 52
#define WX_UDM_SETRANGE 53

#define WX_MB_OK wxOK
#define WX_MB_OKCANCEL wxOK|wxCANCEL
#define WX_IDOK wxOK

