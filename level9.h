
typedef unsigned char L9BYTE;
typedef unsigned short L9UINT16;
typedef unsigned long L9UINT32;
typedef int L9BOOL;

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#if defined(_Windows) || defined(__MSDOS__) || defined (_WIN32) || defined (__WIN32__)
	#define L9WORD(x) (*(L9UINT16*)(x))
	#define L9SETWORD(x,val) (*(L9UINT16*)(x)=(L9UINT16)val)
	#define L9SETDWORD(x,val) (*(L9UINT32*)(x)=val)
	#define FILE_DELIM '\\'
#else
	#define L9WORD(x) (*(x) + ((*(x+1))<<8))
	#define L9SETWORD(x,val) *(x)=(L9BYTE) val; *(x+1)=(L9BYTE)(val>>8);
	#define L9SETDWORD(x,val) *(x)=(L9BYTE)val; *(x+1)=(L9BYTE)(val>>8); *(x+2)=(L9BYTE)(val>>16); *(x+3)=(L9BYTE)(val>>24);
	#define FILE_DELIM '/'
#endif

#if defined(_Windows) && !defined(__WIN32__)
#include <alloc.h>
#define malloc farmalloc
#define calloc farcalloc
#define free farfree
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* routines provided by os dependent code */
void os_printchar(char c);
L9BOOL os_input(char*ibuff,int size);
char os_readchar(void);
void os_flush(void);
L9BOOL os_save_file(L9BYTE *Ptr,int Bytes);
L9BOOL os_load_file(L9BYTE *Ptr,int *Bytes,int Max);
L9BOOL os_get_game_file(char *NewName,int Size);
void os_set_filenumber(char *NewName,int Size,int n);

/* routines provided by level9 interpreter */
L9BOOL LoadGame(char *filename);
L9BOOL RunGame(void);
void StopGame(void);
void FreeMemory(void);

#ifdef __cplusplus
}
#endif

