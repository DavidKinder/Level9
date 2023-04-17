/***********************************************************************\
*
* Level 9 interpreter
* Version 5.2
* Copyright (c) 1996-2023 Glen Summers and contributors.
* Contributions from David Kinder, Alan Staniforth, Simon Baldwin,
* Dieter Baron and Andreas Scherrer.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
* 
\***********************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "level9.h"

#define TEXTBUFFER_SIZE 10240
char TextBuffer[TEXTBUFFER_SIZE+1];
int TextBufferPtr = 0;

int Column = 0;
#define SCREENWIDTH 76

char TestScript[MAX_PATH];
int PlayScript = 1;

void os_printchar(char c)
{
	if (c == '\r')
	{
		os_flush();
		putchar('\n');
		Column = 0;
	}
	else if (isprint(c) != 0)
	{
		if (TextBufferPtr >= TEXTBUFFER_SIZE)
			os_flush();
		*(TextBuffer + (TextBufferPtr++)) = c;
	}
}

L9BOOL os_input(char *ibuff, int size)
{
	os_flush();
	if (PlayScript)
	{
		strcpy(ibuff,"#play");
		PlayScript = 0;
	}
	else
		strcpy(ibuff,"#quit");
	return TRUE;
}

char os_readchar(int millis)
{
	if (strncasecmp("press space",TextBuffer,11) == 0)
		return ' ';
	return 0;
}

L9BOOL os_stoplist(void)
{
	return FALSE;
}

void os_flush(void)
{
int ptr, space, lastspace, searching;

	if (TextBufferPtr < 1)
		return;
	*(TextBuffer+TextBufferPtr) = ' ';
	ptr = 0;
	while (TextBufferPtr + Column > SCREENWIDTH)
	{
		space = ptr;
		lastspace = space;
		searching = 1;
		while (searching)
		{
			while (TextBuffer[space] != ' ') space++;
			if (space - ptr + Column > SCREENWIDTH)
			{
				space = lastspace;
				printf("%.*s\n",space - ptr,TextBuffer + ptr);
				Column = 0;
				space++;
				if (TextBuffer[space] == ' ')
					space++;
				TextBufferPtr -= (space - ptr);
				ptr = space;
				searching = 0;
			}
			else
				lastspace = space;
			space++;
		}
	}
	if (TextBufferPtr > 0)
	{
		printf("%.*s",TextBufferPtr,TextBuffer + ptr);
		Column += TextBufferPtr;
	}
	TextBufferPtr = 0;
}

L9BOOL os_save_file(L9BYTE * Ptr, int Bytes)
{
	return FALSE;
}

L9BOOL os_load_file(L9BYTE *Ptr,int *Bytes,int Max)
{
	return FALSE;
}

L9BOOL os_get_game_file(char *NewName,int Size)
{
	return FALSE;
}

void os_set_filenumber(char *NewName,int Size,int n)
{
char *p;
int i;

#if defined(_Windows) || defined(__MSDOS__) || defined (_WIN32) || defined (__WIN32__)
	p = strrchr(NewName,'\\');
#else
	p = strrchr(NewName,'/');
#endif
	if (p == NULL)
		p = NewName;
	for (i = strlen(p)-1; i >= 0; i--)
	{
		if (isdigit(p[i]))
		{
			p[i] = '0'+n;
			return;
		}
	}
}

void os_graphics(int mode)
{
}

void os_cleargraphics(void)
{
}

void os_setcolour(int colour, int index)
{
}

void os_drawline(int x1, int y1, int x2, int y2, int colour1, int colour2)
{
}

void os_fill(int x, int y, int colour1, int colour2)
{
}

void os_show_bitmap(int pic, int x, int y)
{
}

FILE *os_open_script_file()
{
	return fopen(TestScript,"r");
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return 0;
	if (!LoadGame(argv[1],NULL))
		return 0;
	strncpy(TestScript,argv[2],MAX_PATH-1);
	while (RunGame());
	StopGame();
	FreeMemory();
	return 0;
}

