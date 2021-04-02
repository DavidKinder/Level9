#include <mywin.h>
#pragma hdrstop

static HMODULE GetUser32(void)
{
  static HMODULE user = 0;

  if (user == 0)
    user = LoadLibrary("user32.dll");
  return user;
}

UINT call_GetDpiForSystem(void)
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

UINT call_GetDpiForWindow(HWND hWnd)
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

VOID* call_SetThreadDpiAwarenessContext(VOID* dpiContext)
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
  DpiContextUnaware dpiNone;
  return GetWindowPlacement(hWnd,lpwndPlace);
}

BOOL SetWindowPlacementDpiNeutral(HWND hWnd, const WINDOWPLACEMENT* lpwndPlace)
{
  DpiContextUnaware dpiNone;
  return SetWindowPlacement(hWnd,lpwndPlace);
}

BOOL GetWindowRectDpiNeutral(HWND hWnd, LPRECT lpRect)
{
  DpiContextUnaware dpiNone;
  return GetWindowRect(hWnd,lpRect);
}

BOOL SetWindowPosDpiNeutral(HWND hWnd, HWND hWndAfter, int x, int y, int cx, int cy, UINT uFlag)
{
  DpiContextUnaware dpiNone;
  return SetWindowPos(hWnd,hWndAfter,x,y,cx,cy,uFlag);
}

DpiContextUnaware::DpiContextUnaware()
{
  // DPI_AWARENESS_CONTEXT_UNAWARE
  context = call_SetThreadDpiAwarenessContext((VOID*)-1); 
}

DpiContextUnaware::~DpiContextUnaware()
{
  call_SetThreadDpiAwarenessContext(context);
}

DpiContextSystem::DpiContextSystem()
{
  // DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
  context = call_SetThreadDpiAwarenessContext((VOID*)-2); 
}

DpiContextSystem::~DpiContextSystem()
{
  call_SetThreadDpiAwarenessContext(context);
}
