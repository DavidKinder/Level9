#include <mywin.h>
#pragma hdrstop

bool g_DarkMode = false;
const static COLORREF DarkBackColour = RGB(0x00,0x00,0x00);
const static COLORREF DarkTextColour = RGB(0xf0,0xf0,0xf0);
const static COLORREF DarkColour1 = RGB(0x2b,0x2b,0x2b);
const static COLORREF DarkColour2 = RGB(0x41,0x41,0x41);

static bool CheckWindowsVersion(DWORD major, DWORD minor, DWORD build)
{
  static DWORD os_major = 0, os_minor = 0, os_build = 0;

  if (os_major == 0)
  {
    typedef void(__stdcall *PFNRTLGETNTVERSIONNUMBERS)(LPDWORD,LPDWORD,LPDWORD);
    PFNRTLGETNTVERSIONNUMBERS getNtVersionNumbers = (PFNRTLGETNTVERSIONNUMBERS)
      GetProcAddress(GetModuleHandle("ntdll.dll"),"RtlGetNtVersionNumbers");
    if (getNtVersionNumbers)
    {
      (*getNtVersionNumbers)(&os_major,&os_minor,&os_build);
      os_build &= 0x0FFFFFFF;
    }
  }

  if (major != os_major)
    return (major < os_major);
  if (minor != os_minor)
    return (minor < os_minor);
  return (build <= os_build);
}

static HMODULE GetUxtheme(void)
{
  static HMODULE uxtheme = 0;

  if (uxtheme == 0)
    uxtheme = LoadLibrary("uxtheme.dll");
  return uxtheme;
}

static void call_SetAppDarkMode(int mode)
{
  if (CheckWindowsVersion(10,0,18362)) // Windows 10 build 1903 "19H1"
  {
    typedef int(__stdcall *PFNSETPREFERREDAPPMODE)(int);

    HMODULE uxtheme = GetUxtheme();
    PFNSETPREFERREDAPPMODE setPreferredAppMode = (PFNSETPREFERREDAPPMODE)
      GetProcAddress(uxtheme,MAKEINTRESOURCE(135));
    if (setPreferredAppMode != NULL)
      (*setPreferredAppMode)(mode);
  }
}

static HRESULT call_SetWindowTheme(HWND hWnd, LPCWSTR pszName, LPCWSTR pszList)
{
  typedef HRESULT(__stdcall *PFNSETWINDOWTHEME)(HWND,LPCWSTR,LPCWSTR);

  HMODULE uxtheme = GetUxtheme();
  PFNSETWINDOWTHEME setWindowTheme = (PFNSETWINDOWTHEME)
    GetProcAddress(uxtheme,"SetWindowTheme");
  if (setWindowTheme != NULL)
    return (*setWindowTheme)(hWnd,pszName,pszList);
  return S_FALSE;
}

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static HMODULE GetDwmapi(void)
{
  static HMODULE dwmapi = 0;

  if (dwmapi == 0)
    dwmapi = LoadLibrary("dwmapi.dll");
  return dwmapi;
}

static HRESULT call_DwmSetWindowAttribute(HWND hWnd, DWORD dwAttr, LPCVOID pvAttr, DWORD cbAttr)
{
  typedef HRESULT(__stdcall *PFNDWMSETWINDOWATTRIBUTE)(HWND,DWORD,LPCVOID,DWORD);

  HMODULE dwmapi = GetDwmapi();
  PFNDWMSETWINDOWATTRIBUTE setWindowAttribute = (PFNDWMSETWINDOWATTRIBUTE)
    GetProcAddress(dwmapi,"DwmSetWindowAttribute");
  if (setWindowAttribute != NULL)
    return (*setWindowAttribute)(hWnd,dwAttr,pvAttr,cbAttr);
  return S_FALSE;
}

static HBRUSH GetDarkBrush(void)
{
  static HBRUSH DarkBrush = 0;

  if (DarkBrush == 0)
    DarkBrush = CreateSolidBrush(DarkColour1);
  return DarkBrush;
}

static void SolidRect(HDC hdc, RECT* r, COLORREF c)
{
  SetBkColor(hdc,c);
  ExtTextOut(hdc,0,0,ETO_OPAQUE,r,NULL,0,NULL);
}

bool SetDarkMode(void)
{
  bool previous = g_DarkMode;
  g_DarkMode = false;

  // Has dark mode been disabled?
  DWORD disable = 0;
  DWORD len = sizeof(DWORD);
  if (RegQueryValueEx(App::User,"DarkMode:Disable",NULL,NULL,(LPBYTE)&disable,&len) == ERROR_SUCCESS)
  {
    if (disable != 0)
      return (g_DarkMode != previous);
  }
  else
  {
    // Ensure key exists so that the user can disable dark mode
    len = sizeof(DWORD);
    RegSetValueEx(App::User,"DarkMode:Disable",0,REG_DWORD,(BYTE*)&disable,len);
  }

  // No dark mode if high contrast is active
  HIGHCONTRAST contrast = { sizeof(contrast), 0 };
  if (SystemParametersInfo(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,FALSE))
  {
    if (contrast.dwFlags & HCF_HIGHCONTRASTON)
      return (g_DarkMode != previous);
  }

  // Check light or dark theme set in Windows
  DWORD light = 1;
  HKEY personalize;
  if (RegOpenKeyEx(HKEY_CURRENT_USER,"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_READ,&personalize) == ERROR_SUCCESS)
  {
    len = sizeof(DWORD);
    RegQueryValueEx(personalize,"AppsUseLightTheme",NULL,NULL,(LPBYTE)&light,&len);
    RegCloseKey(personalize);
  }
  if (light != 0)
    return (g_DarkMode != previous);

  // Turn on dark mode
  g_DarkMode = true;
  call_SetAppDarkMode(1);
  return (g_DarkMode != previous);
}

void SetDarkTitle(HWND hWnd)
{
  BOOL darkTitle = g_DarkMode ? TRUE : FALSE;
  call_DwmSetWindowAttribute(hWnd,DWMWA_USE_IMMERSIVE_DARK_MODE,&darkTitle,sizeof darkTitle);
  RedrawWindow(hWnd,NULL,NULL,RDW_ERASE|RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME);
}

void SetDarkTheme(HWND hWnd)
{
  LPCWSTR theme = g_DarkMode ? L"DarkMode_Explorer" : NULL;
  call_SetWindowTheme(hWnd,theme,NULL);
}

BOOL DarkCtlColor(TMSG& Msg)
{
  if (g_DarkMode)
  {
    HDC dc = (HDC)Msg.wParam;
    SetTextColor(dc,DarkTextColour);
    SetBkColor(dc,DarkColour1);
    Msg.RetVal = (LRESULT)GetDarkBrush();
    return TRUE;
  }
  return FALSE;
}

typedef struct _UAHDRAWMENU
{
  HMENU hmenu;
  HDC hdc;
  DWORD dwFlags;
}
UAHDRAWMENU;

typedef struct _UAHDRAWMENUITEM
{
  DRAWITEMSTRUCT dis;
  UAHDRAWMENU um;
  int iPos;
}
UAHDRAWMENUITEM;

BOOL DarkDrawMenuBar(TMSG& Msg, HWND hWnd)
{
  if (g_DarkMode)
  {
    switch (Msg.Msg)
    {
    case WM_UAHDRAWMENU:
      {
        UAHDRAWMENU* draw = (UAHDRAWMENU*)Msg.lParam;

        RECT rcw;
        GetWindowRect(hWnd,&rcw);
        MENUBARINFO mbi = { sizeof MENUBARINFO,0 };
        GetMenuBarInfo(hWnd,OBJID_MENU,0,&mbi);

        RECT rc = mbi.rcBar;
        OffsetRect(&rc,-rcw.left,-rcw.top);

        SolidRect(draw->hdc,&rc,DarkBackColour);
        Msg.RetVal = TRUE;
      }
      return TRUE;

    case WM_UAHDRAWMENUITEM:
      {
        UAHDRAWMENUITEM* draw = (UAHDRAWMENUITEM*)Msg.lParam;

        COLORREF back = DarkBackColour;
        if (draw->dis.itemState & ODS_SELECTED)
          back = DarkColour2;
        else if (draw->dis.itemState & ODS_HOTLIGHT)
          back = DarkColour1;

        char title[256] = { 0 };
        MENUITEMINFO mii = { sizeof MENUITEMINFO,MIIM_STRING,0 };
        mii.dwTypeData = title;
        mii.cch = sizeof(title);
        GetMenuItemInfo(draw->um.hmenu,draw->iPos,TRUE,&mii);

        DWORD format = DT_CENTER|DT_SINGLELINE|DT_VCENTER;
        if (draw->dis.itemState & ODS_NOACCEL)
          format |= DT_HIDEPREFIX;

        SolidRect(draw->um.hdc,&(draw->dis.rcItem),back);
        SetTextColor(draw->um.hdc,DarkTextColour);
        SetBkColor(draw->um.hdc,back);
        DrawText(draw->um.hdc,title,mii.cch,&(draw->dis.rcItem),format);
      }
      return TRUE;

    case WM_NCPAINT:
    case WM_NCACTIVATE:
      {
        Msg.RetVal = DefWindowProc(hWnd,Msg.Msg,Msg.wParam,Msg.lParam);

        RECT rcw;
        GetWindowRect(hWnd,&rcw);
        MENUBARINFO mbi = { sizeof MENUBARINFO,0 };
        GetMenuBarInfo(hWnd,OBJID_MENU,0,&mbi);

        RECT rc = mbi.rcBar;
        OffsetRect(&rc,-rcw.left,-rcw.top);
        rc.top = rc.bottom;
        rc.bottom += 2;

        HDC hdc = GetWindowDC(hWnd);
        SolidRect(hdc,&rc,DarkBackColour);
        ReleaseDC(hWnd,hdc);
      }
      return TRUE;
    }
  }
  return FALSE;
}

DWORD GetSysOrDarkColor(int nIndex)
{
  if (g_DarkMode)
  {
    switch (nIndex)
    {
    case COLOR_WINDOW:
      return DarkBackColour;
    case COLOR_WINDOWTEXT:
      return DarkTextColour;
    }
  }
  return GetSysColor(nIndex);
}
