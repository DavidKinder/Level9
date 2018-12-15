#include <mywin.h>
#pragma hdrstop

static HMODULE GetUser32(void)
{
  static HMODULE user = 0;

  if (user == 0)
    user = LoadLibrary("user32.dll");
  return user;
}

UINT GetDpiForSystem(void)
{
  typedef UINT(__stdcall *PFNGETDPIFORSYSTEM)(VOID);

  HMODULE user = GetUser32();
  PFNGETDPIFORSYSTEM getDpiForSystem = (PFNGETDPIFORSYSTEM)
    ::GetProcAddress(user,"GetDpiForSystem");
  if (getDpiForSystem != NULL)
    return (*getDpiForSystem)();

  HDC dc = GetDC(0);
  UINT dpi = GetDeviceCaps(dc,LOGPIXELSY);
  ReleaseDC(0,dc);
  return dpi;
}

UINT GetDpiForWindow(HWND hWnd)
{
  typedef UINT(__stdcall *PFNGETDPIFORWINDOW)(HWND);

  HMODULE user = GetUser32();
  PFNGETDPIFORWINDOW getDpiForWindow = (PFNGETDPIFORWINDOW)
    ::GetProcAddress(user,"GetDpiForWindow");
  if (getDpiForWindow != NULL)
    return (*getDpiForWindow)(hWnd);

  HDC dc = GetDC(hWnd);
  UINT dpi = GetDeviceCaps(dc,LOGPIXELSY);
  ReleaseDC(hWnd,dc);
  return dpi;
}

VOID* SetThreadDpiAwarenessContext(VOID* dpiContext)
{
  typedef VOID*(__stdcall *PFNSETTHREADDPIAWARENESSCONTEXT)(VOID*);

  HMODULE user = GetUser32();
  PFNSETTHREADDPIAWARENESSCONTEXT setThreadDpiAwarenessContext = (PFNSETTHREADDPIAWARENESSCONTEXT)
    ::GetProcAddress(user,"SetThreadDpiAwarenessContext");
  if (setThreadDpiAwarenessContext != NULL)
    return (*setThreadDpiAwarenessContext)(dpiContext);

  return 0;
}

BOOL GetWindowPlacementDpiNeutral(HWND hWnd, WINDOWPLACEMENT* lpwndPlace)
{
  DpiContextSystem dpiSys;
  return GetWindowPlacement(hWnd,lpwndPlace);
}

BOOL SetWindowPlacementDpiNeutral(HWND hWnd, const WINDOWPLACEMENT* lpwndPlace)
{
  DpiContextSystem dpiSys;
  return SetWindowPlacement(hWnd,lpwndPlace);
}

BOOL GetWindowRectDpiNeutral(HWND hWnd, LPRECT lpRect)
{
  DpiContextSystem dpiSys;
  return GetWindowRect(hWnd,lpRect);
}

BOOL SetWindowPosDpiNeutral(HWND hWnd, HWND hWndAfter, int x, int y, int cx, int cy, UINT uFlag)
{
  DpiContextSystem dpiSys;
  return SetWindowPos(hWnd,hWndAfter,x,y,cx,cy,uFlag);
}

DpiContextSystem::DpiContextSystem()
{
  // DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
  context = SetThreadDpiAwarenessContext((VOID*)-2); 
}

DpiContextSystem::~DpiContextSystem()
{
   SetThreadDpiAwarenessContext(context);
}
