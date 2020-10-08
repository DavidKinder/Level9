#include <mywin.h>
#pragma hdrstop

#include <ctype.h>

#include "..\level9.h"

// define application name, main window title
#define AppName "Level9"
#define MainWinTitle "Level9"
 
// help file name and ini file are set from AppName
char HelpFileName[] = AppName".hlp";

#ifdef WIN16
char Ini[] = AppName".ini";
#else
char Ini[] = "Software\\CrapolaSoftware\\Level9\\1.00";
#endif

#include "level9.rh"

String Output="";
int Line=0;
int LineOffset=0;
int LineStart=0;
int LastWordEnd=0;
HWND hWndMain;
int FontHeight,LineSpacing;
LOGFONT lf;
HFONT Font;
COLORREF FontColour;
int PageWidth,PageHeight;
int Margin;
SimpleList<int> InputChars;
int iPos,Input;
String Hash(20);

void DisplayLine(int Line,char *Str,int Len)
{
	HDC dc=GetDC(hWndMain);
	HFONT OldFont=SelectObject(dc,Font);
	COLORREF OldCol=SetTextColor(dc,FontColour);
	COLORREF OldBk=SetBkColor(dc,GetSysColor(COLOR_WINDOW));
	TextOut(dc,Margin,Line*LineSpacing-LineOffset,Str,Len);
	SelectObject(dc,OldFont);
	SetTextColor(dc,OldCol);
	SetBkColor(dc,OldBk);
	ReleaseDC(hWndMain,dc);
}

void DisplayLineJust(int Line,char *Str,int Len)
{
	HDC dc=GetDC(hWndMain);
	HFONT OldFont=SelectObject(dc,Font);
	COLORREF OldCol=SetTextColor(dc,FontColour);
  COLORREF OldBk=SetBkColor(dc,GetSysColor(COLOR_WINDOW));

	SIZE Size;
#ifdef WIN32
	GetTextExtentPoint32(dc,Str,Len,&Size);
#else
	GetTextExtentPoint(dc,Str,Len,&Size);
#endif

  // count spaces
  int nBreaks=0;
  char *Ptr=Str;
	for (int i=0;i<Len;i++) if (*Ptr++==' ') nBreaks++;
	if (nBreaks) SetTextJustification(dc,PageWidth-Size.cx-2*Margin,nBreaks);

	TextOut(dc,Margin,Line*LineSpacing-LineOffset,Str,Len);

	SelectObject(dc,OldFont);
  SetTextColor(dc,OldCol);
   SetBkColor(dc,OldBk);
	ReleaseDC(hWndMain,dc);
}

int LineLength(char*str,int n)
{
	SIZE Size;
   HDC dc=GetDC(hWndMain);
	HFONT OldFont=SelectObject(dc,Font);
#ifdef WIN32
	GetTextExtentPoint32(dc,str,n,&Size);
#else
	GetTextExtentPoint(dc,str,n,&Size);
#endif
	SelectObject(dc,OldFont);
	ReleaseDC(hWndMain,dc);
	return Size.cx;
}

BOOL Caret=FALSE;
int Cursorx,Cursory;

void MakeCaret()
{
	if (GetFocus()==hWndMain && Caret)
	{
		CreateCaret(hWndMain, NULL, 2, FontHeight);
		SetCaretPos(Cursorx,Cursory);
		ShowCaret(hWndMain);
	}
}

void KillCaret()
{
	if (Caret) DestroyCaret();
}

void SetCaret(int x,int y)
{
	Cursorx=x;
	Cursory=y;
	SetCaretPos(x,y);
}

char os_readchar()
{
	SimpleNode<int> *C;
	App::PeekLoop();
	if ((C=InputChars.GetFirst())==NULL) return 0;
	char InputChar=C->Item;
	delete C;
	return InputChar;
}

SimpleList<String> History;
int HistoryLines=0;
#define NUMHIST 20

void Erase(int Start,int End)
{
	RECT rc;
	rc.left=Margin+Start;
	rc.right=Margin+End;
	rc.top=Line*LineSpacing-LineOffset;
	rc.bottom=Line*LineSpacing+FontHeight-LineOffset;
	HDC dc=GetDC(hWndMain);
#ifdef WIN32
	FillRect(dc,&rc,(HBRUSH) GetClassLong(hWndMain,GCL_HBRBACKGROUND));
#else
	FillRect(dc,&rc,(HBRUSH) GetClassLong(hWndMain,GCW_HBRBACKGROUND));
#endif
	ReleaseDC(hWndMain,dc);
}

void CancelInput()
{
	if (Caret) InputChars.AddTail(-1);
}

void HashCommand(char *h)
{
	if (Caret)
	{
		Hash=h;
		InputChars.AddTail(-2);
	}
}

BOOL os_input(char*ibuff,int size)
{
	slIterator<String> HistPtr(History);
	if (HistPtr()) HistPtr--; // force out

	Input=Output.Len();
	iPos=0;
	Caret=TRUE;
	MakeCaret();
	SetCaret(Margin+LineLength(Output+LineStart,Input-LineStart),Line*LineSpacing-LineOffset);

	while (TRUE)
	{
		SimpleNode<int> *C;
		while ((C=InputChars.GetFirst())==NULL) App::PeekLoop();
		int InputChar=C->Item;
		delete C;
    if (InputChar<0)
    {
			strcpy(ibuff,Hash);
			KillCaret();
			Caret=FALSE;
     	return InputChar<-1;
		}
		else if (InputChar=='\r')
		{
			strcpy(ibuff,Output+Input); // >500 chrs??
			KillCaret();
			Caret=FALSE;
			HistPtr.Last();
			if (*ibuff && (!HistPtr() || (String) HistPtr!=ibuff))
			{
				History.AddTailRef(String(ibuff));
				if (HistoryLines==NUMHIST)
				delete History.GetFirst();
				else HistoryLines++;
			}
			return TRUE;
		}
		else switch (InputChar)
		{
			case 256+VK_DELETE:
				if (iPos>=Output.Len()-Input) break;
				iPos++;
			case 8:
				if (iPos>0)
				{
					int OldLen=LineLength(Output+LineStart,Output.Len()-LineStart);
					Output.Remove(Input+--iPos,1);
					int Len=LineLength(Output+LineStart,Output.Len()-LineStart);
					Erase(Len,OldLen);
				}
				break;
			case 256+VK_UP:
				if (!HistPtr()) HistPtr.Last();
				else
				{
					--HistPtr;
					if (!HistPtr()) HistPtr.First();
				}
				if (HistPtr())
				{
					int OldLen=LineLength(Output+LineStart,Output.Len()-LineStart);
					Output.Len(Input);
					Output.Insert((String)HistPtr,Input);
					iPos=Output.Len()-Input;
					int Len=LineLength(Output+LineStart,Output.Len()-LineStart);
					Erase(Len,OldLen);
				}
				break;
			case 256+VK_DOWN:
				if (HistPtr())
				{
					++HistPtr;
					if (!HistPtr()) HistPtr.Last();
					else
					{
						int OldLen=LineLength(Output+LineStart,Output.Len()-LineStart);
						Output.Len(Input);
						Output.Insert((String)HistPtr,Input);
						iPos=Output.Len()-Input;
						int Len=LineLength(Output+LineStart,Output.Len()-LineStart);
						Erase(Len,OldLen);
					}
				}
				break;
			case 256+VK_LEFT:
				if (iPos>0) iPos--;
				break;
			case 256+VK_RIGHT:
				if (iPos<Output.Len()-Input) iPos++;
				break;
			case 256+VK_END:
				iPos=Output.Len()-Input;
				break;
			case 256+VK_HOME:
				iPos=0;
				break;
         case 26: // escape (clear?)
         	break;

			default:
				// insert char at Pos;
				if (InputChar<256) Output.Insert(InputChar,Input+iPos++);
				break;
		}

		HideCaret(hWndMain);
		DisplayLine(Line,Output+LineStart,Output.Len()-LineStart);
		SetCaret(Margin+LineLength(Output+LineStart,Input-LineStart+iPos),Line*LineSpacing-LineOffset);
		ShowCaret(hWndMain);
	}
}

int FindLineLength(int LineStart)
{
	int LastWordEnd=0;
	int Pos=LineStart;
	char c;
	while (TRUE)
	{
		c= Output[Pos++];
		if (c=='\r' || c==' ' || c==0)
		{
			if (LineLength((char*)Output+LineStart,Pos-LineStart-1)>PageWidth-2*Margin) return LastWordEnd+1;
			else if (c==0) return Pos-1;
			else if (c=='\r') return Pos;
			LastWordEnd=Pos-LineStart-1;
		}
	}
}

#define SCROLLBACK 2000

void NewLine()
{
	while (Output.Len()>SCROLLBACK)
	{
		int Len=FindLineLength(0);
		Output.Remove(0,Len);
		LineStart-=Len;
		LineOffset-=LineSpacing;
		Line--;
	}
	Line++;

	if (Line*LineSpacing+FontHeight-LineOffset>PageHeight)
	{
		int Shift=Line*LineSpacing+FontHeight-LineOffset-PageHeight;
		ScrollWindow(hWndMain,0,-Shift,NULL,NULL);
		LineOffset+=Shift;
		UpdateWindow(hWndMain);
	}
	//((Window*)App::MainWindow)->SetVirtualExtent(PageWidth,Line*LineSpacing+FontHeight,1,1);
	// scroll
}

void os_flush()
{
	if (LineLength((char*)Output+LineStart,Output.Len()-LineStart)>PageWidth-2*Margin)
	{
		DisplayLineJust(Line,(char*) Output+LineStart,LastWordEnd);
		LineStart+=LastWordEnd+1;
		NewLine();
	}
	DisplayLine(Line,(char*) Output+LineStart,Output.Len()-LineStart);
	LastWordEnd=Output.Len()-LineStart;
}

void os_printchar(char c)
{
	Output << c;
	if (c=='\r' || c==' ')
	{
		if (LineLength((char*)Output+LineStart,Output.Len()-LineStart-1)>PageWidth-2*Margin)
		{
			DisplayLineJust(Line,(char*) Output+LineStart,LastWordEnd);
			LineStart+=LastWordEnd+1;
			NewLine();
		}
		if (c=='\r')
		{
			DisplayLine(Line,(char*) Output+LineStart,Output.Len()-LineStart-1);
			LineStart=Output.Len();
			NewLine();
		}
		LastWordEnd=Output.Len()-LineStart-1;
	}
}

void Redraw()
{
	int l=0;
	int LineStart=0,LastWordEnd=0;
	int Pos=0;
	char c;
	do
	{
		c= Output[Pos++];
		if (c=='\r' || c==' ' || c==0)
		{
			if (LineLength((char*)Output+LineStart,Pos-LineStart-1)>PageWidth-2*Margin)
			{
				DisplayLineJust(l,(char*) Output+LineStart,LastWordEnd);
				LineStart+=LastWordEnd+1;
				l++;
			}
			if (c=='\r' || c==0)
			{
				DisplayLine(l,(char*) Output+LineStart,Pos-LineStart-1);
				if (c=='\r')
				{
					LineStart=Pos;
					l++;
				}
			}
			LastWordEnd=Pos-LineStart-1;
		}
	} while (c);
}

void Paginate()
{
	int l=0;
	int LineStart=0,LastWordEnd=0;
	int Pos=0;
	char c;
	do
	{
		c= Output[Pos++];
		if (c=='\r' || c==' ' || c==0)
		{
			if (LineLength((char*)Output+LineStart,Pos-LineStart-1)>PageWidth-2*Margin)
			{
				LineStart+=LastWordEnd+1;
				l++;
			}
			if (c=='\r')
			{
				LineStart=Pos;
				l++;
			}
			LastWordEnd=Pos-LineStart-1;
		}
	} while (c);
	Line=l;
	LineOffset=max(0,Line*LineSpacing+FontHeight-PageHeight);
	//((Window*)App::MainWindow)->SetVirtualExtent(PageWidth,Line*LineSpacing+FontHeight,1,1);

	if (Caret) SetCaret(Margin+LineLength(Output+LineStart,Input-LineStart+iPos),Line*LineSpacing-LineOffset);
}

const char Filters[]="Level 9 Game Files (*.dat)\0*.dat\0Spectrum Snapshots (*.sna)\0*.sna\0All Files (*.*)\0*.*\0\0";
int FiltIndex;
const char GameFilters[]="Saved game file (*.sav)\0*.sav\0All Files (*.*)\0*.*\0\0";
FName LastGameFile;
int GameFiltIndex;

void os_set_filenumber(char *NewName,int Size,int n)
{
	FName fn(NewName);
	String S;
	fn.GetBaseName(S);
	while (isdigit(S.Last())) S.Remove(S.Len()-1,1);
	fn.NewBaseName(S << n);
	strcpy(NewName,fn);
}

BOOL os_get_game_file(char* Name,int Size)
{
	return CustFileDlg(App::MainWindow,-1,Name,Size,"Open Game File",Filters,&FiltIndex).Execute();
}

BOOL os_save_file(BYTE *Ptr,int Bytes)
{
	CancelInput();
	if (CustFileDlg(App::MainWindow,-1,LastGameFile,LastGameFile.Size(),"Save File",GameFilters,&GameFiltIndex,OFN_OVERWRITEPROMPT | OFN_EXPLORER).Execute())
	{
		LastGameFile.Update();
   	if (!LastGameFile.GetExt()) LastGameFile.NewExt("sav");

		FILE *f=fopen(LastGameFile,"wb");
		if (f)
		{
			fwrite(Ptr,1,Bytes,f);
			fclose(f);
         return TRUE;
      }
	}
   return FALSE;
}

BOOL os_load_file(BYTE *Ptr,int *Bytes,int Max)
{
	CancelInput();
	if (CustFileDlg(App::MainWindow,-1,LastGameFile,LastGameFile.Size(),"Load File",GameFilters,&GameFiltIndex,OFN_FILEMUSTEXIST | OFN_EXPLORER).Execute())
  {
		LastGameFile.Update();
   	FILE *f=fopen(LastGameFile,"rb");
    if (f)
    {
     	*Bytes=filelength(f);
			if (*Bytes>Max)
         	MessageBox(App::MainWindow->hWnd,"Not a valid saved game file","Load Error",MB_OK | MB_ICONEXCLAMATION);
			else
      {
				fread(Ptr,1,*Bytes,f);
   	    fclose(f);
        return TRUE;
      }
    }
	}
  return FALSE;
}

// About Dialog ******************************************

class AboutDialog : public Dialog
{
public:
	AboutDialog(Object *Parent) : Dialog(Parent,IDD_ABOUT,"AboutDialog") {}
};

// MainWindow *****************************************

class MainWindow : public HashWindow
{
public:
	MainWindow(Object *Parent,char *Title);
	~MainWindow();
	static FName LastFile;

	BOOL HelpState;
	BOOL Playing;

	void Destroy();
	BOOL SetupWindow();
	void OpenFile(char *name);

	void CmHelpContents();
	void CmHelpUsing();
	void CmAbout();
	void CmExit();
	void CmOpen();
	void CmSelectFont();
	void CmRestore() { HashCommand("#restore"); }
	void CmSave() { HashCommand("save"); }
	void CmDictionary() { HashCommand("#dictionary"); }

	void SetFont();
	void DelFonts();

// message response functions

	BOOL LButtonDown(TMSG &);
	BOOL RButtonDown(TMSG &);
	BOOL LButtonUp(TMSG &);
	BOOL WMMouseMove(TMSG &);
	BOOL WMSize(TMSG &);

	BOOL WMKeyDown(TMSG &);
	BOOL WMChar(TMSG &);
	BOOL WMSetFocus(TMSG&);
	BOOL WMKillFocus(TMSG&);

// window paint request
	void Paint(HDC, BOOL, RECT&);

// enable message response
	HASH_EV_ENABLE(MainWindow)
} ;

// define response functions
EV_START(MainWindow)
// command messages
	EV_COMMAND(CM_ABOUT, CmAbout)
	EV_COMMAND(CM_HELPCONTENTS, CmHelpContents)
	EV_COMMAND(CM_HELPUSING, CmHelpUsing)
	EV_COMMAND(CM_EXIT, CmExit)
	EV_COMMAND(CM_OPEN, CmOpen)
	EV_COMMAND(CM_FONT, CmSelectFont)
	EV_COMMAND(CM_FILELOAD, CmRestore)
	EV_COMMAND(CM_FILESAVE, CmSave)
	EV_COMMAND(CM_DICTIONARY, CmDictionary)

// windows messages
	EV_MESSAGE(WM_LBUTTONDOWN, LButtonDown)
	EV_MESSAGE(WM_RBUTTONDOWN, RButtonDown)
	EV_MESSAGE(WM_MOUSEMOVE, WMMouseMove)
	EV_MESSAGE(WM_LBUTTONUP, LButtonUp)
	EV_MESSAGE(WM_KEYDOWN, WMKeyDown)
	EV_MESSAGE(WM_CHAR, WMChar)
	EV_MESSAGE(WM_SIZE, WMSize)
	EV_MESSAGE(WM_SETFOCUS,WMSetFocus)
	EV_MESSAGE(WM_KILLFOCUS,WMKillFocus)

EV_END

// main window constructor
MainWindow::MainWindow(Object *Parent,char *Title) : HashWindow(Parent,Title,"MainWindow")
{
	// set window style
	Style=WS_OVERLAPPEDWINDOW;

	// give it a menu and icon
	AssignMenu(IDM_MENU);
	SetIcon(IDI_ICON);

	// set help flag
	HelpState = FALSE;
	Flags|=W_OVERSCROLL;
}

// this is called to setup the window
BOOL MainWindow::SetupWindow()
{
#ifdef WIN32
	SetWindowLong(hWnd,GWL_EXSTYLE,WS_EX_OVERLAPPEDWINDOW);
#endif

	// load window pos from ini file (will also be automatically saved on exit)
	GetWindowState();
	SetMru(hWnd,CM_EXIT);
  hWndMain=hWnd;
	SetFont();
	Playing=FALSE;

	return TRUE;
}

void MainWindow::CmSelectFont()
{
	CHOOSEFONT cf;

	cf.lStructSize=sizeof(cf);
	cf.hwndOwner=hWnd;
	cf.lpLogFont=&lf;
	cf.rgbColors=FontColour;

	cf.Flags=CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;

	if (ChooseFont(&cf))
	{
		DelFonts();
		SetFont();
		FontColour=cf.rgbColors;
		KillCaret();
		Paginate();
		InvalidateRect(hWnd,NULL,TRUE);
		MakeCaret();
	}
}

void MainWindow::SetFont()
{
	Font=CreateFontIndirect(&lf);
	HDC dc=GetDC(hWnd);
	HFONT OldFont=SelectObject(dc,Font);
	TEXTMETRIC tm;
	GetTextMetrics(dc,&tm);
	FontHeight=tm.tmHeight;
	Margin=tm.tmAveCharWidth;
	LineSpacing=(int) 1.1*FontHeight;
	SelectObject(dc,OldFont);
	ReleaseDC(dc,hWnd);
}

void MainWindow::DelFonts()
{
	DeleteObject(Font);
}

// this is called when the window is destroyed
void MainWindow::Destroy()
{
	// close help if open
	if (HelpState) WinHelp(hWnd,HelpFileName, HELP_QUIT, 0L);
	DelFonts();
	StopGame();
	Playing=FALSE;
	FreeMemory();
	CancelInput();
}

MainWindow::~MainWindow()
{
}

FName MainWindow::LastFile;

void MainWindow::OpenFile(char *name)
{
// in input routine?, cause fall through
	CancelInput();
  // clear buffers ect..., enen if fails as invalidaes game memory
  Output="";
  LineStart=LastWordEnd=0;
  Paginate();
  InvalidateRect(hWnd,NULL,TRUE);

	if (!LoadGame(name)) MessageBox(hWnd,"Unable to load game file","Load Error",MB_OK | MB_ICONEXCLAMATION);
	else
	{
		LastFile=name;
		AddToMru(LastFile);

		if (Playing) return;
		Playing=TRUE;
		while (Playing && RunGame()) App::PeekLoop();
		Playing=FALSE;
	}
}

void MainWindow::CmOpen()
{
	FName fn=LastFile;
	if (os_get_game_file(fn,fn.Size()))
		OpenFile(fn);
}

void MainWindow::CmExit()
{
	DestroyWindow();
}

void MainWindow::CmHelpContents()
{
	FName HelpFile;
	GetModuleFileName(App::hInstance,HelpFile,HelpFile.Size());
	HelpFile.Update();
	HelpFile.NewName(HelpFileName);
	HelpState = WinHelp(hWnd, HelpFile, HELP_CONTENTS, 0L);
}

void MainWindow::CmHelpUsing()
{
	FName HelpFile;
	GetModuleFileName(App::hInstance,HelpFile,HelpFile.Size());
	HelpFile.Update();
	HelpFile.NewName(HelpFileName);
	HelpState = WinHelp(hWnd, HelpFile, HELP_HELPONHELP, 0L);
}

void MainWindow::Paint(HDC, BOOL, RECT&)
{
//	LineOffset=yPos;
	Redraw();
}

void MainWindow::CmAbout()
{
	AboutDialog(this).Execute();
}

BOOL MainWindow::WMKeyDown(TMSG &Msg)
{
	switch ((int) Msg.wParam)     // the virtual key code
	{
		case VK_F1:
			CmHelpContents();
			break;
		case VK_F2: CmOpen(); break;
		case VK_F3: CmRestore(); break;
		case VK_F4: CmSave(); break;
		case VK_F5: CmDictionary(); break;
			
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
    case VK_DELETE:
    case VK_END:
    case VK_HOME:
			InputChars.AddTail(256+Msg.wParam);
			break;
		}
	return TRUE; // message handled
}

BOOL MainWindow::WMChar(TMSG &Msg)
{
	InputChars.AddTail(Msg.wParam);
	return TRUE;
}

BOOL MainWindow::LButtonDown(TMSG &)
{
	// int xPos = LOWORD(Msg.lParam);  /* horizontal position of cursor */
	// int yPos = HIWORD(Msg.lParam);  /* vertical position of cursor   */
	// could instigate drag here
	// SetCapture(hWnd);
	return TRUE;
}

BOOL MainWindow::WMMouseMove(TMSG &)
{
	// int xPos = LOWORD(Msg.lParam);  /* horizontal position of cursor */
	// int yPos = HIWORD(Msg.lParam);  /* vertical position of cursor   */
	return TRUE;
}

BOOL MainWindow::LButtonUp(TMSG &)
{
	// ReleaseCapture();
	return TRUE;
}

BOOL MainWindow::RButtonDown(TMSG &)
{
	// int xPos = LOWORD(Msg.lParam);  /* horizontal position of cursor */
	// int yPos = HIWORD(Msg.lParam);  /* vertical position of cursor   */
	return TRUE;
}

BOOL MainWindow::WMSize(TMSG &Msg)
{
	PageWidth = LOWORD(Msg.lParam);
	PageHeight = HIWORD(Msg.lParam);
	Paginate();
	InvalidateRect(hWnd,NULL,TRUE);
	return TRUE;
}

BOOL MainWindow::WMSetFocus(TMSG&)
{
	MakeCaret();
	return TRUE;
}

BOOL MainWindow::WMKillFocus(TMSG&)
{
	KillCaret();
	return TRUE;
}

// App *****************************************

class MyApp : public App
{
public:
	MyApp(char *Name);
	~MyApp();
	
private:
	void InitMainWindow();
	void SetDefs();
	void ReadIni();
	void WriteIni();
	void FirstIn();
};

void MyApp::SetDefs()
{
	// Set application default settings here
	MainWindow::LastFile="";
  FiltIndex=0;
	LastGameFile="";
  GameFiltIndex=0;

	memset(&lf,0,sizeof(lf));
	lf.lfHeight=-16;
	lf.lfWeight=FW_NORMAL;
	lf.lfCharSet=ANSI_CHARSET;
	lf.lfOutPrecision=OUT_TT_PRECIS;
	lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
	lf.lfQuality=PROOF_QUALITY;
	lf.lfPitchAndFamily=4; //FIXED_PITCH | 4 | FF_MODERN;
	strcpy(lf.lfFaceName,"Times New Roman");

  FontColour=RGB(0,0,0);
}

void MyApp::ReadIni()
{
	// read information from ini file
	SetDefs();
	ReadIniString("General","LastFile",(String&)MainWindow::LastFile);
	ReadIniInt("General","FiltIndex",FiltIndex);
	ReadIniString("General","LastGameFile",(String&)LastGameFile);
	ReadIniInt("General","GameFiltIndex",GameFiltIndex);

	ReadIniInt("Font","Size",lf.lfHeight);
	String S(LF_FACESIZE);
	ReadIniString("Font","Name",S);
   if (*S) strcpy(lf.lfFaceName,S);
   ReadIniInt("Font","Colour",(long&) FontColour);
}

void MyApp::WriteIni()
{
	// write information to ini file
	WriteIniString("General","LastFile",MainWindow::LastFile);
	WriteIniInt("General","FiltIndex",FiltIndex);
	WriteIniString("General","LastGameFile",LastGameFile);
	WriteIniInt("General","GameFiltIndex",GameFiltIndex);

	long FontHeight=lf.lfHeight;
	WriteIniInt("Font","Size",FontHeight);
	WriteIniString("Font","Name",lf.lfFaceName);
  WriteIniInt("Font","Colour",(long) FontColour);
}

MyApp::MyApp(char *Name) : App(Name,Ini)
{
	// enable 3d dialogue boxes
#ifdef WIN16
	EnableCtl3d();
#else
	OSVERSIONINFO ovi;
	ovi.dwOSVersionInfoSize=sizeof(ovi);
	GetVersionEx(&ovi);
	if (ovi.dwMajorVersion == 3) EnableCtl3d();
#endif

	// read from ini file
	ReadMru(4);
	ReadIni();
};

void MyApp::InitMainWindow()
{
	MainWindow=new ::MainWindow(0,MainWinTitle);
}

MyApp::~MyApp()
{
	// write information to ini file
	WriteMru();
	WriteIni();
}


void MyApp::FirstIn()
{
	((::MainWindow*) MainWindow)->CmOpen();
}

// main function, called from WinMain()
int Main()
{
	// create and run application
	return MyApp(AppName).Run();
}

