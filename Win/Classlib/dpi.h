// dpi.h

UINT GetDpiForWindow(HWND hWnd);
UINT GetDpiForSystem(void);
VOID* SetThreadDpiAwarenessContext(VOID* dpiContext);

BOOL GetWindowPlacementDpiNeutral(HWND hWnd, WINDOWPLACEMENT* lpwndPlace);
BOOL SetWindowPlacementDpiNeutral(HWND hWnd, const WINDOWPLACEMENT* lpwndPlace);
BOOL GetWindowRectDpiNeutral(HWND hWnd, LPRECT lpRect);
BOOL SetWindowPosDpiNeutral(HWND hWnd, HWND hWndAfter, int x, int y, int cx, int cy, UINT uFlag);

class DpiContextUnaware
{
public:
  DpiContextUnaware();
  ~DpiContextUnaware();

private:
  VOID* context;
};

class DpiContextSystem
{
public:
  DpiContextSystem();
  ~DpiContextSystem();

private:
  VOID* context;
};
