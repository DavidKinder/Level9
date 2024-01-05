// dark.h

#ifndef WM_UAHDRAWMENU
#define WM_UAHDRAWMENU 0x0091
#endif

#ifndef WM_UAHDRAWMENUITEM
#define WM_UAHDRAWMENUITEM 0x0092
#endif

extern bool g_DarkMode;

bool SetDarkMode(bool init);
void SetDarkTitle(HWND hWnd);
void SetDarkTheme(HWND hWnd);

BOOL DarkCtlColour(TMSG& Msg);
BOOL DarkDrawMenuBar(TMSG& Msg, HWND hWnd);
DWORD GetSysOrDarkColour(int nIndex);
DWORD UpdateColour(int nIndex, DWORD colour, bool wasDark);
