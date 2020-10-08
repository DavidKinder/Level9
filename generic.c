#include <stdio.h>
#include <string.h>
#include "level9.h"

#define TEXTBUFFER_SIZE 10240
char TextBuffer[TEXTBUFFER_SIZE+1];
int TextBufferPtr = 0;

int Column = 0;
#define SCREENWIDTH 76

void os_printchar(char c)
{
	if (c == '\r')
	{
		os_flush();
		putchar('\n');
		Column = 0;
		return;
	}
	if (isprint(c) == 0) return;
	if (TextBufferPtr >= TEXTBUFFER_SIZE) os_flush();
	*(TextBuffer + (TextBufferPtr++)) = c;
}

L9BOOL os_input(char *ibuff, int size)
{
char *nl;

	os_flush();
	fgets(ibuff, size, stdin);
	nl = strchr(ibuff, '\n');
	if (nl) *nl = 0;
}

char os_readchar(void)
{
	os_flush();
	return getc(stdin); /* will require enter key as well */
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
				printf("%.*s\n", space - ptr, TextBuffer + ptr);
				Column = 0;
				space++;
				if (TextBuffer[space] == ' ') space++;
				TextBufferPtr -= (space - ptr);
				ptr = space;
				searching = 0;
			}
			else lastspace = space;
			space++;
		}
	}
	printf("%.*s", TextBufferPtr, TextBuffer + ptr);
	Column += TextBufferPtr;
	TextBufferPtr = 0;

	semaphore = 0;
}

L9BOOL os_save_file(L9BYTE * Ptr, int Bytes)
{
char name[256];
char *nl;
FILE *f;

	os_flush();
	printf("Save file: ");
	fgets(name, 256, stdin);
	nl = strchr(name, '\n');
	if (nl) *nl = 0;
	f = fopen(name, "wb");
	if (!f) return FALSE;
	fwrite(Ptr, 1, Bytes, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_load_file(L9BYTE *Ptr,int *Bytes,int Max)
{
char name[256];
char *nl;
FILE *f;

	os_flush();
	printf("Load file: ");
	fgets(name, 256, stdin);
	nl = strchr(name, '\n');
	if (nl) *nl = 0;
	f = fopen(name, "rb");
	if (!f) return FALSE;
	*Bytes = fread(Ptr, 1, Max, f);
	fclose(f);
	return TRUE;
}

L9BOOL os_get_game_file(char *NewName,int Size)
{
char *nl;

	os_flush();
	printf("Load next game: ");
	fgets(NewName, Size, stdin);
	nl = strchr(NewName, '\n');
	if (nl) *nl = 0;
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
	printf("Level 9 Interpreter\n\n");
	if (argc != 2)
	{
		printf("Syntax: %s <gamefile>\n",argv[0]);
		return 0;
	}
	if (!LoadGame(argv[1]))
	{
		printf("Error: Unable to open game file\n");
		return 0;
	}
	while (RunGame());
	StopGame();
	FreeMemory();
	return 0;
}

