/* MS-DOS interface for Level 9 */
/* Interface by David Kinder */

#include <dos.h>
#include <conio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "level9.h"

unsigned _stklen = 16384;

#define TEXTBUFFER_SIZE 10240
char TextBuffer[TEXTBUFFER_SIZE+1];
int TextBufferPtr = 0;

#define HISTORY_LINES 20
unsigned char *History[HISTORY_LINES];
int HistoryPosition = HISTORY_LINES;

struct text_info TextInfo;
int Hotkey = 1;
int MoreCount = 0;

#define KEY_BRK   3
#define KEY_HOME  327
#define KEY_CRSRU 328
#define KEY_CRSRL 331
#define KEY_CRSRR 333
#define KEY_END   335
#define KEY_CRSRD 336
#define KEY_DEL   339
#define KEY_F12   390

L9UINT32 filelength(FILE *f);

int character(void);
void into_buffer(char *buffer,char *newb,int *x,int *i);
void store_hist(char *line);

void os_printchar(char c)
{
	if (c == '\r')
	{
		os_flush();
		cprintf("\r\n");

		if (++MoreCount >= TextInfo.screenheight)
		{
			MoreCount = 0;
			cprintf("[More]");
			character();
			gotoxy(1,wherey());
			cprintf("      ");
			gotoxy(1,wherey());
		}
		return;
	}

	if (isprint(c) == 0) return;
	if (TextBufferPtr >= TEXTBUFFER_SIZE) os_flush();
	*(TextBuffer + (TextBufferPtr++)) = c;
}

L9BOOL os_input(char *ibuff, int size)
{
int x,y,c,i = 0;

	x = wherex();
	y = wherey();
	*ibuff = '\0';

	while (1)
	{
		switch (c = character())
		{
			case '\r':
				if (strlen(ibuff) > 0 || Hotkey == 0)
				{
					os_printchar('\r');
					MoreCount = 0;
					store_hist(ibuff);
					return TRUE;
				}
			case '\b':
				if (i > 0)
				{
					memmove(ibuff+i-1,ibuff+i,strlen(ibuff+i)+1);
					i--;
					gotoxy(x,y);
					cprintf("%s ",ibuff);
					gotoxy(x+i,y);
				}
				break;
			case KEY_DEL:
				if (i < strlen(ibuff))
				{
					memmove(ibuff+i,ibuff+i+1,strlen(ibuff+i+1)+1);
					gotoxy(x,y);
					cprintf("%s ",ibuff);
					gotoxy(x+i,y);
				}
				break;
			case KEY_CRSRL:
				if (i > 0) i--;
				gotoxy(x+i,y);
				break;
			case KEY_CRSRR:
				if (i < strlen(ibuff)) i++;
				gotoxy(x+i,y);
				break;
			case KEY_HOME:
				i = 0;
				gotoxy(x,y);
				break;
			case KEY_END:
				i = strlen(ibuff);
				gotoxy(x+i,y);
				break;
			case KEY_CRSRU:
				if ((HistoryPosition > 0) && (*(History+HistoryPosition-1) != NULL))
				{
					HistoryPosition--;
					into_buffer(ibuff,*(History+HistoryPosition),&x,&i);
				}
				break;
			case KEY_CRSRD:
				if ((HistoryPosition < HISTORY_LINES-1) && (*(History+HistoryPosition+1) != NULL))
				{
					HistoryPosition++;
					into_buffer(ibuff,HistoryPosition == HISTORY_LINES ? "" : *(History+HistoryPosition),&x,&i);
				}
				else
				{
					HistoryPosition = HISTORY_LINES;
					into_buffer(ibuff,"",&x,&i);
				}
				break;
			default:
				if (c < 256 && i < size && isprint(c) && wherex()+strlen(ibuff+i) < TextInfo.screenwidth)
				{
					memmove(ibuff+i+1,ibuff+i,strlen(ibuff+i)+1);
					*(ibuff+i++) = c;
					gotoxy(x,y);
					cprintf("%s",ibuff);
					gotoxy(x+i,y);
				}
				break;
		}
	}
}

char os_readchar(void)
{
	os_flush();
	if (kbhit() != 0)
	{
		MoreCount = 0;
		return character();
	}
	else return 0;
}

void os_flush(void)
{
static int semaphore = 0;
int ptr, space, lastspace, searching;

	if (TextBufferPtr < 1) return;
	if (semaphore) return;
	semaphore = 1;

	*(TextBuffer+TextBufferPtr) = ' ';
	ptr = 0;
	while (TextBufferPtr+wherex()-1 > TextInfo.screenwidth)
	{
		space = ptr;
		lastspace = space;
		searching = 1;
		while (searching)
		{
			while (TextBuffer[space] != ' ') space++;
			if (space-ptr+wherex()-1 > TextInfo.screenwidth)
			{
				space = lastspace;
				cprintf("%.*s%s",space-ptr,TextBuffer+ptr,wherex()+space-ptr > TextInfo.screenwidth ? "" : "\r\n");
				if (TextBuffer[space] == ' ') space++;
				TextBufferPtr -= (space-ptr);
				ptr = space;
				searching = 0;
			}
			else lastspace = space;
			space++;
		}
	}
	cprintf("%.*s", TextBufferPtr, TextBuffer+ptr);
	TextBufferPtr = 0;

	semaphore = 0;
}

L9BOOL os_save_file(L9BYTE * Ptr, int Bytes)
{
char name[256];
FILE *f;

	os_flush();
	cprintf("Save file: ");
	Hotkey = 0;
	os_input(name,256);
	Hotkey = 1;

	f = fopen(name, "wb");
	if (!f) return FALSE;
	fwrite(Ptr, 1, Bytes, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_load_file(L9BYTE *Ptr,int *Bytes,int Max)
{
char name[256];
FILE *f;

	os_flush();
	cprintf("Load file: ");
	Hotkey = 0;
	os_input(name,256);
	Hotkey = 1;

	f = fopen(name, "rb");
	if (!f) return FALSE;

	*Bytes = filelength(f);
	if (*Bytes > Max)
	{
		fclose(f);
		return FALSE;
	}
	fread(Ptr, 1, *Bytes, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_get_game_file(char *NewName,int Size)
{
	os_flush();
	cprintf("Load next game: ");
	Hotkey = 0;
	os_input(NewName,Size);
	Hotkey = 1;
	return TRUE;
}

void os_set_filenumber(char *NewName,int Size,int n)
{
char *p;
int i;

	p = strrchr(NewName,FILE_DELIM);
	if (p == NULL) p = NewName;
	for (i = strlen(p)-1; i >= 0; i--)
	{
		if (isdigit(p[i]))
		{
			p[i] = '0'+n;
			return;
		}
	}
}

int main(int argc, char **argv)
{
int i;

	for (i = 0; i < HISTORY_LINES; i++) *(History+i) = NULL;
	gettextinfo(&TextInfo);

	clrscr();
	gotoxy(1,TextInfo.screenheight);
	cprintf("Level 9 Interpreter v2.0");
	os_printchar('\r');
	os_printchar('\r');

	if (argc != 2)
	{
		cprintf("Syntax: %s <gamefile>\r\n",argv[0]);
		return 0;
	}
	if (!LoadGame(argv[1]))
	{
		cprintf("Error: Unable to open game file\r\n");
		return 0;
	}
	while (RunGame());
	StopGame();
	FreeMemory();
	return 0;
}

int character(void)
{
int c;

	c = getch();
	if (c == 0) c = getch()+256;
	if (c == KEY_F12 || c == KEY_BRK)
	{
		StopGame();
		FreeMemory();
		clrscr();
		exit(0);
	}
	return c;
}

void into_buffer(char *buffer,char *newb,int *x,int *i)
{
	gotoxy(*x,wherey());
	strcpy(buffer,newb);
	cprintf("%s",buffer);
	clreol();
	*i = strlen(buffer);
	gotoxy(*x+*i,wherey());
}

int cmp(const char *a,const char *b)
{
	if (a == 0 || b == 0) return 1;
	return strcmp(a,b);
}

void store_hist(char *line)
{
int i;

	if (*line != NULL && cmp(*(History+HISTORY_LINES-1),line) != 0)
	{
		if (*History) free(*History);
		for (i = 0; i < HISTORY_LINES-1; i++) *(History+i) = *(History+i+1);
		*(History+HISTORY_LINES-1) = malloc(strlen(line)+1);
		if (*(History+HISTORY_LINES-1)) strcpy(*(History+HISTORY_LINES-1),line);
	}
	HistoryPosition = HISTORY_LINES;
}

