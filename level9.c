#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "level9.h"

/* #define _DEBUG */
/* #define CODEFOLLOW */
/* #define FULLSCAN */

/* The input routine will repond to the following 'hash' commands
#restore	restores position file directly (bypasses any protection code)
#quit			terminates current game, RunGame() will return FALSE
#cheat		tries to bypass restore protection on v3,4 games (can be slow)
#dictionary	lists game dictionary, (press a key to interrupt)
*/

/* "L901" */
#define L9_ID 0x4c393031

#define LISTAREASIZE 0x800
#define STACKSIZE 1024
#define IBUFFSIZE 500

#define V1FILESIZE 0x600

L9BYTE* startfile=NULL,*pictureaddress=NULL;
L9BYTE* startdata;
L9UINT32 FileSize,picturesize;

L9BYTE *L9Pointers[12];
L9BYTE *absdatablock,*list2ptr,*list3ptr,*list9startptr,*acodeptr;
L9BYTE *startmd,*endmd,*endwdp5,*wordtable,*dictdata,*defdict;
L9UINT16 dictdatalen;
L9BYTE *startmdV2;

int wordcase;
int unpackcount;
char unpackbuf[8];
L9BYTE* dictptr;
char threechars[34];
enum L9GameTypes {L9_V1,L9_V2,L9_V3,L9_V4};
int L9GameType;
enum V2MsgTypes {V2M_NORMAL,V2M_ERIK};
int V2MsgType;
char LastGame[MAX_PATH];

#define RAMSAVESLOTS 10
typedef struct
{
	L9UINT16 vartable[256];
	L9BYTE listarea[LISTAREASIZE];
} SaveStruct;
typedef struct
{
	L9UINT32 Id;
	L9UINT16 codeptr,stackptr,listsize,stacksize,filenamesize,checksum;
	L9UINT16 vartable[256];
	L9BYTE listarea[LISTAREASIZE];
	L9UINT16 stack[STACKSIZE];	
	char filename[MAX_PATH];
} GameState;
GameState workspace;

#if defined(AMIGA) && defined(_DCC)
	__far SaveStruct ramsavearea[RAMSAVESLOTS];
#else
	SaveStruct ramsavearea[RAMSAVESLOTS];
#endif

L9UINT16 randomseed;
L9BOOL Running;

char ibuff[IBUFFSIZE];
char obuff[34];

L9BOOL Cheating=FALSE;
int CheatWord;
GameState CheatWorkspace;

L9BOOL LoadGame2(char *filename);

#ifdef CODEFOLLOW
#define CODEFOLLOWFILE "c:\\temp\\output"
FILE *f;
L9UINT16 *cfvar,*cfvar2;
char *codes[]=
{
"Goto",
"intgosub",
"intreturn",
"printnumber",
"messagev",
"messagec",
"function",
"input",
"varcon",
"varvar",
"_add",
"_sub",
"ilins",
"ilins",
"jump",
"Exit",
"ifeqvt",
"ifnevt",
"ifltvt",
"ifgtvt",
"screen",
"cleartg",
"picture",
"getnextobject",
"ifeqct",
"ifnect",
"ifltct",
"ifgtct",
"printinput",
"ilins",
"ilins",
"ilins",
};
char *functions[]=
{
	"calldriver",
	"Random",
	"save",
	"restore",
	"clearworkspace",
	"clearstack"
};
char *drivercalls[]=
{
"init",
"drivercalcchecksum",
"driveroswrch",
"driverosrdch",
"driverinputline",
"driversavefile",
"driverloadfile",
"settext",
"resettask",
"returntogem",
"10 *",
"loadgamedatafile",
"randomnumber",
"13 *",
"v4 driver14",
"15 *",
"driverclg",
"line",
"fill",
"driverchgcol",
"20 *",
"21 *",
"ramsave",
"ramload",
"24 *",
"lensdisplay",
"26 *",
"27 *",
"28 *",
"29 *",
"allocspace",
"31 *",
"v4 driver 32",
"33 *",
"v4 driver34"
};
#endif

void initdict(L9BYTE *ptr)
{
	dictptr=ptr;
	unpackcount=8;
}

char getdictionarycode(void)
{
	if (unpackcount!=8) return unpackbuf[unpackcount++];
	else
	{
		/* unpackbytes */
		L9BYTE d1=*dictptr++,d2;
		unpackbuf[0]=d1>>3;
		d2=*dictptr++;
		unpackbuf[1]=((d2>>6) + (d1<<2)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[2]=(d2>>1) & 0x1f;
		unpackbuf[3]=((d1>>4) + (d2<<4)) & 0x1f;
		d2=*dictptr++;
		unpackbuf[4]=((d1<<1) + (d2>>7)) & 0x1f;
		d1=*dictptr++;
		unpackbuf[5]=(d2>>2)  & 0x1f;
		unpackbuf[6]=((d2<<3) + (d1>>5)) & 0x1f;
		unpackbuf[7]=d1 & 0x1f;
		unpackcount=1;
		return unpackbuf[0];
	}
}

int getlongcode(void);

int getdictionary(int d0)
{
	if (d0>=0x1a) return getlongcode();
	else return d0+0x61;
}

int getlongcode(void)
{
	int d0,d1;
	d0=getdictionarycode();
	if (d0==0x10)
	{
		wordcase=1;
		d0=getdictionarycode();
		return getdictionary(d0);/* reentrant?? */
	}
	d1=getdictionarycode();
	return 0x80 | ((d0<<5) & 0xe0) | (d1 & 0x1f);
}

char lastchar='.';
char lastactualchar=0;

void printchar(char c)
{
	if (Cheating) return;

	if (c&128) lastchar=(c&=0x7f);
	else if (c!=0x20 && c!=0x0d && c!=0x7e && (c<'\"' || c>='.'))
	{
		if (lastchar=='!' || lastchar=='?' || lastchar=='.') c=toupper(c);
		lastchar=c;
	}
/* eat multiple CRs */
	if (c!=0x0d || lastactualchar!=0x0d) os_printchar(c);
	lastactualchar=c;
}

void printstring(char*buf)
{
	int i;
	for (i=0;i< (int) strlen(buf);i++) printchar(buf[i]);
}

void printdecimald0(int d0)
{
	char temp[12];
	sprintf(temp,"%d",d0);
	printstring(temp);
}

void error(char *fmt,...)
{
	char buf[256];
	va_list ap;
	va_start(ap,fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	printstring(buf);
}

int d5;

void printautocase(int d0)
{
	if (d0 & 128) printchar((char) d0);
	else
	{
		if (wordcase) printchar((char) toupper(d0));
		else if (d5<6) printchar((char) d0);
		else
		{
			wordcase=0;
			printchar((char) toupper(d0));
		}
	}
}

void displaywordref(L9UINT16 Off)
{
	static int mdtmode=0;

	wordcase=0;
	d5=(Off>>12)&7;
	Off&=0xfff;
	if (Off<0xf80)
	{
	/* dwr01 */
		L9BYTE *a0,*oPtr,*a3;
		int d0,d2,i;

		if (mdtmode==1) printchar(0x20);
		mdtmode=1;

		/* setindex */
		a0=dictdata;
		d2=dictdatalen;

	/* dwr02 */
		oPtr=a0;
		while (d2 && Off >= L9WORD(a0+2))
		{
			a0+=4;
			d2--;
		}
	/* dwr04 */
		if (a0==oPtr)
		{
			a0=defdict;
		}
		else
		{
			a0-=4;
			Off-=L9WORD(a0+2);
			a0=startdata+L9WORD(a0);
		}
	/* dwr04b */
		Off++;
		initdict(a0);
		a3=(L9BYTE*) threechars; /* a3 not set in  original, prevent possible spam */

		/* dwr05 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0<0x1c)
			{
				/* dwr06 */
				if (d0>=0x1a) d0=getlongcode();
				else d0+=0x61;
				*a3++=d0;
			}
			else
			{
				d0&=3;
				a3=(L9BYTE*) threechars+d0;
				if (--Off==0) break;
			}
		}
		for (i=0;i<d0;i++) printautocase(threechars[i]);

		/* dwr10 */
		while (TRUE)
		{
			d0=getdictionarycode();
			if (d0>=0x1b) return;
			printautocase(getdictionary(d0));
		}
	}

	else
	{
		if (d5&2) printchar(0x20); /* prespace */
		mdtmode=2;
		Off&=0x7f;
		if (L9GameType!=L9_V4 || Off!=0x7e) printchar((char)Off);
		if (d5&1) printchar(0x20); /* postspace */
	}
}

int getmdlength(L9BYTE **Ptr)
{
	int tot=0,len;
	do
	{
		len=(*(*Ptr)++ -1) & 0x3f;
		tot+=len;
	} while (len==0x3f);
	return tot;
}

void printmessage(int Msg)
{
	L9BYTE* Msgptr=startmd;
	L9BYTE Data;

	int len;
	L9UINT16 Off;

	while (Msg>0 && Msgptr-endmd<=0)
	{
		Data=*Msgptr;
		if (Data&128)
		{
			Msgptr++;
			Msg-=Data&0x7f;
		}
		else Msgptr+=getmdlength(&Msgptr);
		Msg--;
	}
	if (Msg<0 || *Msgptr & 128) return;

	len=getmdlength(&Msgptr);
	if (len==0) return;

	while (len)
	{
		Data=*Msgptr++;
		len--;
		if (Data&128)
		{
		/* long form (reverse word) */
			Off=(Data<<8) + *Msgptr++;
			len--;
		}
		else
		{
			Off=(wordtable[Data*2]<<8) + wordtable[Data*2+1];
		}
		if (Off==0x8f80) break;
		displaywordref(Off);
	}
}

/* v2 message stuff */

int msglenV2(L9BYTE **ptr)
{
	int i=0;
	L9BYTE a;
	
	/* catch berzerking code */
	if (*ptr >= startdata+FileSize) return 0;

	while ((a=**ptr)==0)
	{
	 (*ptr)++;
	 
	 if (*ptr >= startdata+FileSize) return 0;

	 i+=255;
	}
	i+=a;
	return i;
}

void printcharV2(char c)
{
	if (c==0x25) c=0xd;
	else if (c==0x5f) c=0x20;
	printautocase(c);
}

void displaywordV2(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	if (msg==0) return;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	n=msglenV2(&ptr);
	
	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return;
	
		if (a>=0x5e) displaywordV2(startmdV2-1,a-0x5d);
		else printcharV2((char)(a+0x1d));
	}
}

int msglenV25(L9BYTE **ptr)
{
	L9BYTE *ptr2=*ptr;
	while (ptr2<startdata+FileSize && *ptr2++!=1) ;
	return ptr2-*ptr;
}

void displaywordV25(L9BYTE *ptr,int msg)
{
	int n;
	L9BYTE a;
	while (msg--)
	{
		ptr+=msglenV25(&ptr);
	}
	n=msglenV25(&ptr);
	
	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return;
	
		if (a>=0x5e) displaywordV25(startmdV2,a-0x5e);
		else printcharV2((char)(a+0x1d));
	}
}

L9BOOL amessageV2(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;
	if (msg==0) return FALSE;
	while (--msg)
	{
		ptr+=msglenV2(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV2(&ptr);
	
	while (--n>0)
	{
		a=*++ptr;
		if (a<3) return TRUE;
	
		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV2(startmdV2-1,a-0x5d,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
}

L9BOOL amessageV25(L9BYTE *ptr,int msg,long *w,long *c)
{
	int n;
	L9BYTE a;
	static int depth=0;
	
	while (msg--)
	{
		ptr+=msglenV25(&ptr);
	}
	if (ptr >= startdata+FileSize) return FALSE;
	n=msglenV25(&ptr);
	
	while (--n>0)
	{
		a=*ptr++;
		if (a<3) return TRUE;
	
		if (a>=0x5e)
		{
			if (++depth>10 || !amessageV25(startmdV2,a-0x5e,w,c))
			{
				depth--;
				return FALSE;
			}
			depth--;
		}
		else
		{
			char ch=a+0x1d;
			if (ch==0x5f || ch==' ') (*w)++;
			else (*c)++;
		}
	}
	return TRUE;
}

L9BOOL analyseV2(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=1;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV2(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}
	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

L9BOOL analyseV25(double *wl)
{
	long words=0,chars=0;
	int i;
	for (i=0;i<256;i++)
	{
		long w=0,c=0;
		if (amessageV25(startmd,i,&w,&c))
		{
			words+=w;
			chars+=c;
		}
		else return FALSE;
	}
	
	*wl=words ? (double) chars/words : 0.0;
	return TRUE;
}

void printmessageV2(int Msg)
{
	if (V2MsgType==V2M_NORMAL) displaywordV2(startmd,Msg);
	else displaywordV25(startmd,Msg);
}

L9UINT32 filelength(FILE *f)
{
	L9UINT32 pos,FileSize;

	pos=ftell(f);
	fseek(f,0,SEEK_END);
	FileSize=ftell(f);
	fseek(f,pos,SEEK_SET);
	return FileSize;
}

void Allocate(L9BYTE **ptr,L9UINT32 Size)
{
	if (*ptr) free(*ptr);
	*ptr=malloc(Size);
}

void FreeMemory(void)
{
	if (startfile)
   {
   	free(startfile);
      startfile=NULL;
   }
	if (pictureaddress)
   {
   	free(pictureaddress);
      pictureaddress=NULL;
   }
}

L9BOOL load(char *filename)
{
	FILE *f=fopen(filename,"rb");
	if (!f) return FALSE;
	FileSize=filelength(f);

	Allocate(&startfile,FileSize);

	if (fread(startfile,1,FileSize,f)!=FileSize)
	{
		fclose(f);
		return FALSE;
	}
 	fclose(f);
	return TRUE;
}

L9UINT16 scanmovewa5d0(L9BYTE* Base,L9UINT32 *Pos)
{
	L9UINT16 ret=L9WORD(Base+*Pos);
	(*Pos)+=2;
	return ret;
}

L9UINT32 scangetaddr(int Code,L9BYTE *Base,L9UINT32 *Pos,L9UINT32 acode,int *Mask)
{
	(*Mask)|=0x20;
	if (Code&0x20)
	{
/*    getaddrshort */
		signed char diff=Base[(*Pos)++];
		return (*Pos)+diff-1;
	}
	else
	{
		return acode+scanmovewa5d0(Base,Pos);
	}
}

void scangetcon(int Code,L9UINT32 *Pos,int *Mask)
{
	(*Pos)++;
	if (!(Code & 64)) (*Pos)++;
	(*Mask)|=0x40;
}

L9BOOL ValidateSequence(L9BYTE* Base,L9BYTE* Image,L9UINT32 iPos,L9UINT32 acode,L9UINT32 *Size,L9UINT32 FileSize,L9UINT32 *Min,L9UINT32 *Max,L9BOOL Rts,L9BOOL *JumpKill)
{
	L9UINT32 Pos;
	L9BOOL Finished=FALSE,Valid;
	L9UINT32 Strange=0;
	int ScanCodeMask;
	int Code;
	*JumpKill=FALSE;

	if (iPos>=FileSize)
   	return FALSE;
	Pos=iPos;
	if (Pos<*Min) *Min=Pos;

	if (Image[Pos]) return TRUE; /* hit valid code */

	do
	{
		Code=Base[Pos];
		Valid=TRUE;
		if (Image[Pos]) break; /* converged to found code */
		Image[Pos++]=2;
		if (Pos>*Max) *Max=Pos;

		ScanCodeMask=0x9f;
		if (Code&0x80)
		{
			ScanCodeMask=0xff;
			if ((Code&0x1f)>0xa)
				Valid=FALSE;
			Pos+=2;
		}
		else switch (Code & 0x1f)
		{
			case 0: /* goto */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE /* Rts*/,JumpKill);
				Finished=TRUE;
				break;
			}
			case 1: /* intgosub */
			{
				L9UINT32 Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,TRUE,JumpKill);
				break;
			}
			case 2:/*  intreturn */
				Valid=Rts;
				Finished=TRUE;
				break;
			case 3:/* printnumber */
				Pos++;
				break;
			case 4:/* messagev */
				Pos++;
				break;
			case 5:/*  messagec */
				scangetcon(Code,&Pos,&ScanCodeMask);
				break;
			case 6:/*	function */
				switch (Base[Pos++])
				{
					case 2:/* random */
						Pos++;
               case 1:/* calldriver */
					case 3:/* save */
					case 4:/* restore */
					case 5:/* clearworkspace */
					case 6:/* clear stack */
						break;
					case 250: /* printstr */
						while (Base[Pos++]);
						break;

					default:
#ifdef _DEBUG
						/* printf("scan: illegal function call: %d",Base[Pos-1]); */
#endif
						Valid=FALSE;
						break;
				}
				break;
			case 7:/*	input */
				Pos+=4;
				break;
			case 8:/*  varcon */
				scangetcon(Code,&Pos,&ScanCodeMask);
				Pos++;
				break;
			case 9:/*  varvar */
				Pos+=2;
				break;
			case 10:/*	_add */
				Pos+=2;
				break;
			case 11:/* _sub */
				Pos+=2;
				break;
			case 14:/*	jump */
#ifdef _DEBUG
				/* printf("jmp at codestart: %ld",acode); */
#endif
				*JumpKill=TRUE;
				Finished=TRUE;
				break;
			case 15:/*	exit */
				Pos+=4;
				break;
			case 16:/*	ifeqvt */
			case 17:/*	ifnevt */
			case 18:/*	ifltvt */
			case 19:/*	ifgtvt */
			{
				L9UINT32 Val;
				Pos+=2;
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill);
				break;
			}
			case 20:/*	screen */
				if (Base[Pos++]) Pos++;
				break;
			case 21:/*	cleartg */
				Pos++;
				break;
			case 22:/*	picture */
				Pos++;
				break;
			case 23:/*	getnextobject */
				Pos+=6;
				break;
			case 24:/*	ifeqct */
			case 25:/*	ifnect */
			case 26:/*	ifltct */
			case 27:/*	ifgtct */
			{
				L9UINT32 Val;
				Pos++;
				scangetcon(Code,&Pos,&ScanCodeMask);
				Val=scangetaddr(Code,Base,&Pos,acode,&ScanCodeMask);
				Valid=ValidateSequence(Base,Image,Val,acode,Size,FileSize,Min,Max,Rts,JumpKill);
				break;
			}
			case 28:/*	printinput */
				break;
			case 12:/*	ilins */
			case 13:/*	ilins */
			case 29:/*	ilins */
			case 30:/*	ilins */
			case 31:/*	ilins */
#ifdef _DEBUG 
				/* printf("scan: illegal instruction"); */
#endif
				Valid=FALSE;
				break;
		}
	if (Valid && (Code & ~ScanCodeMask))
		Strange++;
	} while (Valid && !Finished && Pos<FileSize); /* && Strange==0); */
	(*Size)+=Pos-iPos;
	return Valid; /* && Strange==0; */
}

L9BYTE calcchecksum(L9BYTE* ptr,L9UINT32 num)
{
	L9BYTE d1=0;
	while (num--!=0) d1+=*ptr++;
	return d1;
}

/*
L9BOOL Check(L9BYTE* StartFile,L9UINT32 FileSize,L9UINT32 Offset)
{
	L9UINT16 d0,num;
	int i;
	L9BYTE* Image;
	L9UINT32 Size=0,Min,Max;
	L9BOOL ret,JumpKill;

	for (i=0;i<12;i++)
	{
		d0=L9WORD (StartFile+Offset+0x12 + i*2);
		if (d0>=0x8000+LISTAREASIZE) return FALSE;
	}

	num=L9WORD(StartFile+Offset)+1;
	if (Offset+num>FileSize) return FALSE;
	if (calcchecksum(StartFile+Offset,num)) return FALSE; 

	Image=calloc(FileSize,1);

	Min=Max=Offset+d0;
	ret=ValidateSequence(StartFile,Image,Offset+d0,Offset+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill);
	free(Image);
	return ret;
}
*/

long Scan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk=malloc(FileSize+1);
	L9BYTE *Image=calloc(FileSize,1);
  L9UINT32 i,num,Size,MaxSize=0;
  int j;
  L9UINT16 d0,l9,md,ml,dd,dl;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill;

  Chk[0]=0;
  for (i=1;i<=FileSize;i++)
  Chk[i]=Chk[i-1]+StartFile[i-1];

  for (i=0;i<FileSize-33;i++)
  {
   	num=L9WORD(StartFile+i)+1;
/*
      Chk[i] = 0 +...+ i-1
      Chk[i+n] = 0 +...+ i+n-1
      Chk[i+n] - Chk[i] = i + ... + i+n
*/
      if (num>0x2000 && i+num<=FileSize && Chk[i+num]==Chk[i])
      {

      	md=L9WORD(StartFile+i+0x2);
         ml=L9WORD(StartFile+i+0x4);
      	dd=L9WORD(StartFile+i+0xa);
         dl=L9WORD(StartFile+i+0xc);

         if (ml>0 && md>0 && i+md+ml<=FileSize && dd>0 && dl>0 && i+dd+dl*4<=FileSize)
         {
            /* v4 files may have acodeptr in 8000-9000, need to fix */
            for (j=0;j<12;j++)
            {
               d0=L9WORD (StartFile+i+0x12 + j*2);
               if (j!=11 && d0>=0x8000 && d0<0x9000)
               {
               	if (d0>=0x8000+LISTAREASIZE) break;
               }
               else if (i+d0>FileSize) break;
            }
            /* list9 ptr must be in listarea, acode ptr in data */
            if (j<12 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

            l9=L9WORD(StartFile+i+0x12 + 10*2);
            if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

            Size=0;
            Min=Max=i+d0;
            if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill))
            {
#ifdef _DEBUG 
							printf("Found valid header at %ld, code size %ld",i,Size);
#endif
							if (Size>MaxSize)
							{
								Offset=i;
								MaxSize=Size;
							}
              /* break; */
            }
         }
      }
   }
   free(Chk);
   free(Image);
   return Offset;
}

long ScanV2(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Chk=malloc(FileSize+1);
	L9BYTE *Image=calloc(FileSize,1);
  L9UINT32 i,Size,MaxSize=0,num;
  int j;
  L9UINT16 d0,l9;
	L9UINT32 Min,Max;
	long Offset=-1;
	L9BOOL JumpKill;

  Chk[0]=0;
  for (i=1;i<=FileSize;i++)
  Chk[i]=Chk[i-1]+StartFile[i-1];

  for (i=0;i<FileSize-28;i++)
  {
		num=L9WORD(StartFile+i+28)+1;
		if (i+num<=FileSize && ((Chk[i+num]-Chk[i+32])&0xff)==StartFile[i+0x1e])
		{
			for (j=0;j<14;j++)
			{
				 d0=L9WORD (StartFile+i+ j*2);
				 if (j!=13 && d0>=0x8000 && d0<0x9000)
				 {
					if (d0>=0x8000+LISTAREASIZE) break;
				 }
				 else if (i+d0>FileSize) break;
			}
			/* list9 ptr must be in listarea, acode ptr in data */
			if (j<14 /*|| (d0>=0x8000 && d0<0x9000)*/) continue;

			l9=L9WORD(StartFile+i+6 + 9*2);
			if (l9<0x8000 || l9>=0x8000+LISTAREASIZE) continue;

			Size=0;
			Min=Max=i+d0;
			if (ValidateSequence(StartFile,Image,i+d0,i+d0,&Size,FileSize,&Min,&Max,FALSE,&JumpKill))
			{
#ifdef _DEBUG 
				printf("Found valid V2 header at %ld, code size %ld",i,Size);
#endif
				if (Size>MaxSize)
				{
					Offset=i;
					MaxSize=Size;
				}
			}
    }
  }
	free(Chk);
	free(Image);
  return Offset;
}


long ScanV1(L9BYTE* StartFile,L9UINT32 FileSize)
{
	return -1;

/*
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	long MaxPos=-1;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	L9BOOL JumpKill,MaxJK;

	L9BYTE c;
	int maxdict,maxdictlen;
	L9BYTE *ptr,*start;

	for (i=0;i<FileSize;i++)
	{
		Size=0;
		Min=Max=i;
		Replace=0;
		if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill))
		{
			if (Size>MaxCount)
			{
				MaxCount=Size;
				MaxMin=Min;
				MaxMax=Max;

				MaxPos=i;
				MaxJK=JumpKill;
			}
			Replace=0;
		}
		for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++) if (*ImagePtr==2) *ImagePtr=Replace;
	}
#ifdef _DEBUG
	printf("V1scan found code at %ld size %ld",MaxPos,MaxCount);
#endif

	// find dictionary 
	ptr=StartFile;
	maxdictlen=0;
	do
	{
		start=ptr;
		do
		{
			do
			{
				c=*ptr++;
			} while (((c>='A' && c<='Z') || c=='-') && ptr<StartFile+FileSize);
			if (c<0x7f || (((c&0x7f)<'A' || (c&0x7f)>'Z') && (c&0x7f)!='/')) break;
			ptr++;
		} while (TRUE);
		if (ptr-start-1>maxdictlen)
		{
			maxdict=start-StartFile;
			maxdictlen=ptr-start-1;
		}
	} while (ptr<StartFile+FileSize);
#ifdef _DEBUG
	if (maxdictlen>0) printf("V1scan found dictionary at %ld size %ld",maxdict,maxdictlen);
#endif

	MaxPos=-1;

  free(Image);
	return MaxPos;
*/
}

#ifdef FULLSCAN
void FullScan(L9BYTE* StartFile,L9UINT32 FileSize)
{
	L9BYTE *Image=calloc(FileSize,1);
	L9UINT32 i,Size;
	int Replace;
	L9BYTE* ImagePtr;
	L9UINT32 MaxPos=0;
	L9UINT32 MaxCount=0;
	L9UINT32 Min,Max,MaxMin,MaxMax;
	int Offset;
	L9BOOL JumpKill,MaxJK;
	for (i=0;i<FileSize;i++)
	{
		Size=0;
		Min=Max=i;
		Replace=0;
		if (ValidateSequence(StartFile,Image,i,i,&Size,FileSize,&Min,&Max,FALSE,&JumpKill))
		{
			if (Size>MaxCount)
			{
				MaxCount=Size;
				MaxMin=Min;
				MaxMax=Max;

				MaxPos=i;
				MaxJK=JumpKill;
			}
			Replace=0;
		}
		for (ImagePtr=Image+Min;ImagePtr<=Image+Max;ImagePtr++) if (*ImagePtr==2) *ImagePtr=Replace;
	}
	printf("%ld %ld %ld %ld %s",MaxPos,MaxCount,MaxMin,MaxMax,MaxJK ? "jmp killed" : "");
	/* search for reference to MaxPos */
	Offset=0x12 + 11*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
		{
			printf("possible v3,4 Code reference at : %ld",i);
			/* startdata=StartFile+i; */
		}
	}
	Offset=13*2;
	for (i=0;i<FileSize-Offset-1;i++)
	{
		if ((L9WORD(StartFile+i+Offset)) +i==MaxPos)
			printf("possible v2 Code reference at : %ld",i);
	}
  free(Image);
}
#endif

L9BOOL intinitialise(char*filename)
{
/* init */
/* driverclg */
/*	char PicFile[MAX_PATH];
	FILE *f;*/
			
	int i;
	int hdoffset;
  long Offset;

	if (!load(filename))
  {
   	error("\rUnable to load: %s\r",filename);
   	return FALSE;
  }

/* picture code not implemented yet */

/* try to load graphics, path\picture.dat */
/*	strcpy(PicFile,filename);
   char *delim=strrchr(PicFile,FILE_DELIM);
   if (delim==NULL) delim=PicFile;
	strcpy(delim"picture.dat");
	f=fopen(PicFile,"rb");
	if (f)
	{
		picturesize=filelength(f);
		Allocate(&pictureaddress,picturesize);
		if (fread(pictureaddress,1,picturesize,f)!=picturesize)
		{
			free(pictureaddress);
			pictureaddress=NULL;
		}

		fclose(f);
	} */
#ifdef FULLSCAN
	FullScan(startfile,FileSize);
#endif

	Offset=Scan(startfile,FileSize);
	L9GameType=L9_V3;
	if (Offset<0)
  {
		Offset=ScanV2(startfile,FileSize);
		L9GameType=L9_V2;
		if (Offset<0)
		{
			Offset=ScanV1(startfile,FileSize);
			L9GameType=L9_V1;
			if (Offset<0)
			{
		   	error("\rUnable to locate valid header in file: %s\r",filename);
			 	return FALSE;
			}
		}
  }
	
	startdata=startfile+Offset;
	FileSize-=Offset;

/* setup pointers */
	if (L9GameType!=L9_V1)
	{
		/* V2,V3,V4 */

		hdoffset=L9GameType==L9_V2 ? 4 :  0x12;

		for (i=0;i<12;i++)
		{
			L9UINT16 d0=L9WORD(startdata+hdoffset+i*2);
			L9Pointers[i]= (i!=11 && d0>=0x8000 && d0<=0x9000) ? workspace.listarea+d0-0x8000 : startdata+d0;
		}
		absdatablock=L9Pointers[0];
		list2ptr=L9Pointers[3];
		list3ptr=L9Pointers[4];
		/*list9startptr */
	/*	if ((((L9UINT32) L9Pointers[10])&1)==0) L9Pointers[10]++; amiga word access hack */
		list9startptr=L9Pointers[10];
		acodeptr=L9Pointers[11];
	}
	

	switch (L9GameType)
	{
		case L9_V1:
			break;
		case L9_V2:
		{
			double a2,a25;
			startmd=startdata + L9WORD(startdata+0x0);
			startmdV2=startdata + L9WORD(startdata+0x2);
			
			/* determine message type */
			if (analyseV2(&a2) && a2>2 && a2<10)
			{
				V2MsgType=V2M_NORMAL;
				#ifdef _DEBUG
				printf("V2 msg table: normal, wordlen=%.2lf",a2);
				#endif
			}
			else if (analyseV25(&a25) && a25>2 && a25<10)
			{
				V2MsgType=V2M_ERIK;
				#ifdef _DEBUG
				printf("V2 msg table: Erik, wordlen=%.2lf",a25);
				#endif
			}
			else
			{
				error("\rUnable to identify V2 message table in file: %s\r",filename);
				return FALSE;
			}
			break;
		}
		case L9_V3:
		case L9_V4:
			startmd=startdata + L9WORD(startdata+0x2);
			endmd=startmd + L9WORD(startdata+0x4);
			defdict=startdata+L9WORD(startdata+6);
			endwdp5=defdict + 5 + L9WORD(startdata+0x8);
			dictdata=startdata+L9WORD(startdata+0x0a);
			dictdatalen=L9WORD(startdata+0x0c);
			wordtable=startdata + L9WORD(startdata+0xe);	
			break;
	}		
	return TRUE;
}

L9BOOL checksumgamedata(void)
{
	return calcchecksum(startdata,L9WORD(startdata)+1)==0;
}

/* instruction codes */

L9BYTE* codeptr;
L9BYTE code;

L9UINT16 movewa5d0(void)
{
	L9UINT16 ret=L9WORD(codeptr);
	codeptr+=2;
	return ret;
}

L9UINT16 getcon(void)
{
	if (code & 64)
	{
	/*   getconsmall */
		return *codeptr++;
	}
	else return movewa5d0();
}

L9BYTE* getaddr(void)
{
	if (code&0x20)
	{
/*    getaddrshort */
		signed char diff=*codeptr++;
		return codeptr+ diff-1;
	}
	else
	{
		return acodeptr+movewa5d0();
	}
}

L9UINT16 *getvar(void)
{
#ifndef CODEFOLLOW
	return workspace.vartable + *codeptr++;
#else
	cfvar2=cfvar;
	return cfvar=workspace.vartable + *codeptr++;
#endif
}

void Goto(void)
{
	codeptr=getaddr();
}

void intgosub(void)
{
	L9BYTE* newcodeptr=getaddr();
	if (workspace.stackptr==STACKSIZE)
	{
		error("\rStack overflow error\r");
		Running=FALSE;
		return;
	}
	workspace.stack[workspace.stackptr++]=(L9UINT16) (codeptr-acodeptr);
	codeptr=newcodeptr;
}

void intreturn(void)
{
	if (workspace.stackptr==0)
	{
		error("\rStack underflow error\r");
		Running=FALSE;
		return;
	}
	codeptr=acodeptr+workspace.stack[--workspace.stackptr];
}

void printnumber(void)
{
	printdecimald0(*getvar());
}

void messagec(void)
{
	if (L9GameType==L9_V2)
		printmessageV2(getcon());
	else
		printmessage(getcon());
}

void messagev(void)
{
	if (L9GameType==L9_V2)
		printmessageV2(*getvar());
	else
		printmessage(*getvar());
}

void init(L9BYTE* a6)
{
#ifdef _DEBUG
	printf("driver - init");
#endif
/*	Running=FALSE; */
}
void randomnumber(L9BYTE* a6)
{
#ifdef _DEBUG
	printf("driver - randomnumber");
#endif   
/*	Running=FALSE; */
	L9SETWORD(a6,rand());
}
void driverclg(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - driverclg");
#endif   
/*	Running=FALSE; */
}
void line(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - line");
#endif   
/*	Running=FALSE; */
}
void fill(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - fill");
#endif   
/*	Running=FALSE; */
}
void driverchgcol(L9BYTE* a6)
{
#ifdef _DEBUG
	printf("driver - driverchgcol %d %d", a6[1], a6[0]);
#endif   
}
void drivercalcchecksum(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - calcchecksum");
#endif   
/*	Running=FALSE; */
}
void driveroswrch(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - driveroswrch");
#endif   
/*	Running=FALSE; */
}
void driverosrdch(L9BYTE* a6)
{
#ifdef _DEBUG
	printf("driver - driverosrdch");
#endif
	os_flush();
	*a6=os_readchar();
}
void driversavefile(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - driversavefile");
#endif
/*	Running=FALSE; */
}
void driverloadfile(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - driverloadfile");
#endif
/*	Running=FALSE; */
}
void settext(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - settext");
#endif
/*	Running=FALSE; */
}
void resettask(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - resettask");
#endif   
/*	Running=FALSE; */
}
void driverinputline(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - driverinputline");
#endif   
/*	Running=FALSE; */
}
void returntogem(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - returntogem");
#endif   
/*	Running=FALSE; */
}
void lensdisplay(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - lensdisplay");
#endif
/*   Running=FALSE; */
}
void allocspace(L9BYTE*a6)
{
#ifdef _DEBUG
	printf("driver - allocspace");
#endif
/*   Running=FALSE; */
}

/* v4 */
void driver14(L9BYTE *a6)
{
#ifdef _DEBUG
	printf("driver - v4 call 14");
#endif
	L9GameType=L9_V4;
	*a6=0;
}
void driver32(L9BYTE *a6)
{
#ifdef _DEBUG
	printf("driver - v4 call 32");
#endif
	L9GameType=L9_V4;
}
void driver34(L9BYTE *a6)
{
#ifdef _DEBUG
	printf("driver - v4 call 34");
#endif
	L9GameType=L9_V4;
	*a6=0;
}

void driver(int d0,L9BYTE* a6)
{
	switch (d0)
	{
		case 0: init(a6); break;
		case 0x0c: randomnumber(a6); break;
		case 0x10: driverclg(a6); break;
		case 0x11: line(a6); break;
		case 0x12: fill(a6); break;
		case 0x13: driverchgcol(a6); break;
		case 0x01: drivercalcchecksum(a6); break;
		case 0x02: driveroswrch(a6); break;
		case 0x03: driverosrdch(a6); break;
		case 0x05: driversavefile(a6); break;
		case 0x06: driverloadfile(a6); break;
		case 0x07: settext(a6); break;
		case 0x08: resettask(a6); break;
		case 0x04: driverinputline(a6); break;
		case 0x09: returntogem(a6); break;
		/* case 0x16: ramsave(a6); break; */
		/* case 0x17: ramload(a6); break; */
		case 0x19: lensdisplay(a6); break;
		case 0x1e: allocspace(a6); break;

/* v4 */
  		case 0x0e: driver14(a6); break;
  		case 0x20: driver32(a6); break;
  		case 0x22: driver34(a6); break;

   }
}

void ramsave(int i)
{
#ifdef _DEBUG
	printf("driver - ramsave %d",i);
#endif   
	memmove(ramsavearea+i,workspace.vartable,sizeof(SaveStruct));
}

void ramload(int i)
{
#ifdef _DEBUG
	printf("driver - ramload %d",i);
#endif   
	memmove(workspace.vartable,ramsavearea+i,sizeof(SaveStruct));
}

void calldriver(void)
{
	L9BYTE* a6=list9startptr;
	int d0=*a6++;
#ifdef CODEFOLLOW
	fprintf(f," %s",drivercalls[d0]);
#endif

	if (d0==0x16 || d0==0x17)
	{
		int d1=*a6;
		if (d1>0xfa) *a6=1;
		else if (d1+1>=RAMSAVESLOTS) *a6=0xff;
		else
		{
			*a6=0;
			if (d0==0x16) ramsave(d1+1); else ramload(d1+1);
		}
		*list9startptr=*a6;
	}
  else if (d0==0x0b)
  {
		char NewName[MAX_PATH];
		strcpy(NewName,LastGame);
		if (*a6==0)
		{
			printstring("\rSearching for next sub-game file.\r");
			if (!os_get_game_file(NewName,MAX_PATH))
			{
				printstring("\rFailed to load game.\r");
				return;
			}
		}
		else
		{
			os_set_filenumber(NewName,MAX_PATH,*a6);
		}
    LoadGame2(NewName);
  }
	else driver(d0,a6);
}

void Random(void)
{
	randomseed=(((randomseed<<8) + 0x0a - randomseed) <<2) + randomseed + 1;
	*getvar()=randomseed & 0xff;
}

void save(void)
{
	L9UINT16 checksum;
	int i;
#ifdef _DEBUG
	printf("function - save");
#endif
/* does a full save, workpace, stack, codeptr, stackptr, game name, checksum */

	workspace.Id=L9_ID;
	workspace.codeptr=codeptr-acodeptr;
	workspace.listsize=LISTAREASIZE;
	workspace.stacksize=STACKSIZE;
	workspace.filenamesize=MAX_PATH;
	workspace.checksum=0;
	strcpy(workspace.filename,LastGame);
	
	checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum+=((L9BYTE*) &workspace)[i];
	workspace.checksum=checksum;

	if (os_save_file((L9BYTE*) &workspace,sizeof(workspace))) printstring("\rGame saved.\r");
	else printstring("\rUnable to save game.\r");
}

L9BOOL CheckFile(GameState *gs)
{
	L9UINT16 checksum;
	int i;
	if (gs->Id!=L9_ID) return FALSE;
	checksum=gs->checksum;
	gs->checksum=0;
	for (i=0;i<sizeof(GameState);i++) checksum-=*((L9BYTE*) gs+i);
	if (checksum) return FALSE;
	if (stricmp(gs->filename,LastGame))
	{
		printstring("\rWarning: game filename does not match, you may have loaded this position file into the wrong story file.\r");
	}
	return TRUE;
}

void NormalRestore(void)
{
	GameState temp;
	int Bytes;
#ifdef _DEBUG
	printf("function - restore");
#endif
	if (Cheating)
	{
		/* not really an error */
		Cheating=FALSE;
		error("\rWord is: %s\r",ibuff);
	}

	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memmove(workspace.vartable,temp.vartable,sizeof(SaveStruct));
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void restore(void)
{
	int Bytes;
	GameState temp;
	if (os_load_file((L9BYTE*) &temp,&Bytes,sizeof(GameState)))
	{
		if (Bytes==V1FILESIZE)
		{
			printstring("\rGame restored.\r");
			/* only copy in workspace */
			memset(workspace.listarea,0,LISTAREASIZE);
			memmove(workspace.vartable,&temp,V1FILESIZE);
		}
		else if (CheckFile(&temp))
		{
			printstring("\rGame restored.\r");
			/* full restore */
			memmove(&workspace,&temp,sizeof(GameState));
			codeptr=acodeptr+workspace.codeptr;
		}
		else
		{
			printstring("\rSorry, unrecognised format. Unable to restore\r");
		}
	}
	else printstring("\rUnable to restore game.\r");
}

void clearworkspace(void)
{
	memset(workspace.vartable,0,sizeof(workspace.vartable));
}

void ilins(int d0)
{
	error("\rIllegal instruction: %d\r",d0);
	Running=FALSE;
}

void function(void)
{
	int d0=*codeptr++;
#ifdef CODEFOLLOW
	fprintf(f," %s",d0==250 ? "printstr" : functions[d0-1]);
#endif

	switch (d0)
	{
		case 1: calldriver(); break;
		case 2: Random(); break;
		case 3: save(); break;
		case 4: NormalRestore(); break;
		case 5: clearworkspace(); break;
		case 6: workspace.stackptr=0; break;
		case 250:
			printstring((char*) codeptr);
			while (*codeptr++);
			break;

		default: ilins(d0);
	}
}

L9BYTE* list9ptr;

void findmsgequiv(int d7)
{
	int d4=-1,d0;
	L9BYTE* a2=startmd;

	do
	{
		d4++;
		if (a2>endmd) return;
		d0=*a2;
		if (d0&0x80)
		{
			a2++;
			d4+=d0&0x7f;
		}
		else if (d0&0x40)
		{
			int d6=getmdlength(&a2);
			do
			{
				int d1;
				if (d6==0) break;

				d1=*a2++;
				d6--;
				if (d1 & 0x80)
				{
					if (d1<0x90)
					{
						a2++;
						d6--;
					}
					else
					{
						d0=(d1<<8) + *a2++;
						d6--;
						if (d7==(d0 & 0xfff))
						{
							d0=((d0<<1)&0xe000) | d4;
							list9ptr[1]=d0;
							list9ptr[0]=d0>>8;
							list9ptr+=2;
							if (list9ptr>=list9startptr+0x20) return;
						}
					}
				}
			} while (TRUE);
		}
		else a2+=getmdlength(&a2);
	} while (TRUE);
}

int unpackd3;

L9BOOL unpackword(void)
{
	L9BYTE *a3;

	if (unpackd3==0x1b) return TRUE;

	a3=(L9BYTE*) threechars + (unpackd3&3);

/*uw01 */
	while (TRUE)
	{
		L9BYTE d0=getdictionarycode();
		if (dictptr>=endwdp5) return TRUE;
		if (d0>=0x1b)
		{
			*a3=0;
			unpackd3=d0;
			return FALSE;
		}
		*a3++=getdictionary(d0);
	}
}

L9BOOL initunpack(L9BYTE* ptr)
{
	initdict(ptr);
	unpackd3=0x1c;
	return unpackword();
}

int partword(char c)
{
	c=tolower(c);

	if (c==0x27 || c==0x2d) return 0;
	if (c<0x30) return 1;
	if (c<0x3a) return 0;
	if (c<0x61) return 1;
	if (c<0x7b) return 0;
	return 1;
}

L9UINT32 readdecimal(char *buff)
{
	return atol(buff);
}

L9BYTE* ibuffptr;

void checknumber(void)
{
	if (*obuff>=0x30 && *obuff<0x3a)
	{
		if (L9GameType==L9_V4)
		{
			*list9ptr=1;
			L9SETWORD(list9ptr+1,readdecimal(obuff));
			L9SETWORD(list9ptr+3,0);
		}
		else
		{
			L9SETDWORD(list9ptr,readdecimal(obuff));
			L9SETWORD(list9ptr+4,0);
		}
	}
	else
	{
		L9SETWORD(list9ptr,0x8000);
		L9SETWORD(list9ptr+2,0);
	}
}

L9BOOL GetWordV2(char *buff,int Word);
L9BOOL GetWordV3(char *buff,int Word);

void NextCheat(void)
{
	/* restore game status */
	memmove(&workspace,&CheatWorkspace,sizeof(GameState));
	codeptr=acodeptr+workspace.codeptr;
	
	if (!((L9GameType==L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++)))
	{
		Cheating=FALSE;
		printstring("\rCheat failed.\r");		
		*ibuff=0;
	}
}
	
void StartCheat(void)
{
	Cheating=TRUE;
	CheatWord=0;
	/* save current game status */
	
	memmove(&CheatWorkspace,&workspace,sizeof(GameState));
	CheatWorkspace.codeptr=codeptr-acodeptr;

	NextCheat();
}
	
/* v3,4 input routine */

L9BOOL GetWordV3(char *buff,int Word)
{
	int i;
	int subdict=0;
	/* 26*4-1=103 */

	initunpack(startdata+L9WORD(dictdata));
	unpackword();

	while (Word--)
	{
		if (unpackword())
		{
			if (++subdict==dictdatalen) return FALSE;
			initunpack(startdata+L9WORD(dictdata+(subdict<<2)));
			Word++; /* force unpack again */
		}
	}
	strcpy(buff,threechars);
	for (i=0;i<(int)strlen(buff);i++) buff[i]&=0x7f;
	return TRUE;
}

L9BOOL CheckHash(void)
{
	if (stricmp(ibuff,"#cheat")==0) StartCheat();
	else if (stricmp(ibuff,"#restore")==0)
	{
		restore();
		return TRUE;
	}
	else if (stricmp(ibuff,"#quit")==0)
	{
		StopGame();
		printstring("\rGame Terminated\r");
		return TRUE;
	}
	else if (stricmp(ibuff,"#dictionary")==0)
	{
		CheatWord=0;
		printstring("\r");
		while ((L9GameType==L9_V2) ? GetWordV2(ibuff,CheatWord++) : GetWordV3(ibuff,CheatWord++))
		{
			error("%s ",ibuff);
			if (os_readchar() || !Running) break;
		}
		printstring("\r");
		return TRUE;
	}
	return FALSE;
}

L9BOOL corruptinginput(void)
{
	L9BYTE *a0,*a2,*a6;
	int d0,d1,d2,keywordnumber,abrevword;

	list9ptr=list9startptr;

	if (ibuffptr==NULL)
	{
		if (Cheating) NextCheat();
		else
		{
			/* flush */
			os_flush();
			lastchar='.';
			/* get input */
			if (!os_input(ibuff,IBUFFSIZE)) return FALSE; /* fall through */
			if (CheckHash()) return FALSE;

			/* force CR but prevent others */
			os_printchar(lastactualchar='\r');
		}
		ibuffptr=(L9BYTE*) ibuff;
	}

	a2=(L9BYTE*) obuff;
	a6=ibuffptr;

/*ip05 */
	while (TRUE)
	{
		d0=*a6++;
		if (d0==0)
		{
			ibuffptr=NULL;
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
		if (partword((char)d0)==0) break;
		if (d0!=0x20)
		{
			ibuffptr=a6;
			L9SETWORD(list9ptr,d0);
			L9SETWORD(list9ptr+2,0);
			*a2=0x20;
			keywordnumber=-1;
			return TRUE;
		}
	}

	a6--;
/*ip06loop */
	do
	{
		d0=*a6++;
		if (partword((char)d0)==1) break;
		d0=tolower(d0);
		*a2++=d0;
	} while (a2<(L9BYTE*) obuff+0x1f);
/*ip06a */
	*a2=0x20;
	a6--;
	ibuffptr=a6;
	abrevword=-1;
	keywordnumber=-1;
	list9ptr=list9startptr;
/* setindex */
	a0=dictdata;
	d2=dictdatalen;
	d0=*obuff-0x61;
	if (d0<0)
	{
		a6=defdict;
		d1=0;
	}
	else
	{
	/*ip10 */
		d1=0x67;
		if (d0<0x1a)
		{
			d1=d0<<2;
			d0=obuff[1];
			if (d0!=0x20) d1+=((d0-0x61)>>3)&3;
		}
	/*ip13 */
		if (d1>=d2)
		{
			checknumber();
			return TRUE;
		}
		a0+=d1<<2;
		a6=startdata+L9WORD(a0);
		d1=L9WORD(a0+2);
	}
/*ip13gotwordnumber */

	initunpack(a6);
/*ip14 */
	d1--;
	do
	{
		d1++;
		if (unpackword())
		{/* ip21b */
			if (abrevword==-1) break; /* goto ip22 */
			else d0=abrevword; /* goto ip18b */
		}
		else
		{
			L9BYTE* a1=(L9BYTE*) threechars;
			int d6=-1;

			a0=(L9BYTE*) obuff;
		/*ip15 */
			do
			{
				d6++;
				d0=tolower(*a1++ & 0x7f);
				d2=*a0++;
			} while (d0==d2);

			if (d2!=0x20)
			{/* ip17 */
				if (abrevword==-1) continue;
				else d0=-1;
			}
			else if (d0==0) d0=d1;
			else if (abrevword!=-1) break;
			else if (d6>=4) d0=d1;
			else
			{
				abrevword=d1;
				continue;
			}
		}
		/*ip18b */
		findmsgequiv(d1);

		abrevword=-1;
		if (list9ptr!=list9startptr)
		{
			L9SETWORD(list9ptr,0);
			return TRUE;
		}
	} while (TRUE);
/* ip22 */
	checknumber();
	return TRUE;
}

/* version 2 stuff hacked from bbc v2 files */

L9BOOL GetWordV2(char *buff,int Word)
{
	L9BYTE *ptr=L9Pointers[1],x;
	
	while (Word--)
	{
		do
		{
			x=*ptr++;
		} while (x>0 && x<0x7f);
		if (x==0) return FALSE; /* no more words */
		ptr++;
	}
	do
	{
		x=*ptr++;
		*buff++=x&0x7f;
	} while (x>0 && x<0x7f);
	*buff=0;
	return TRUE;
}

L9BOOL inputV2(int *wordcount)
{
	L9BYTE a,x;
	L9BYTE *ibuffptr,*obuffptr,*ptr,*list0ptr;
	
	if (Cheating) NextCheat();
	else
	{
		os_flush();
		lastchar='.';
		/* get input */
		if (!os_input(ibuff,IBUFFSIZE)) return FALSE; /* fall through */
		if (CheckHash()) return FALSE;
		
		/* force CR but prevent others */
		os_printchar(lastactualchar='\r');
	}
	/* add space onto end */
	ibuffptr=(L9BYTE*) strchr(ibuff,0);
	*ibuffptr++=32;
	*ibuffptr=0;

	*wordcount=0;
	ibuffptr=(L9BYTE*) ibuff;
	obuffptr=(L9BYTE*) obuff;
	/* ibuffptr=76,77 */
	/* obuffptr=84,85 */
	/* list0ptr=7c,7d */
	list0ptr=L9Pointers[1];

	while (*ibuffptr==32) ++ibuffptr;

	ptr=ibuffptr;
	do
	{
		while (*ptr==32) ++ptr;
		if (*ptr==0) break;
		(*wordcount)++;
		do
		{
			a=*++ptr;
		} while (a!=32 && a!=0);
	} while (*ptr>0);
	
	while (TRUE)
	{
		ptr=ibuffptr; /* 7a,7b */
		while (*ibuffptr==32) ++ibuffptr;
		
		while (TRUE)
		{
			a=*ibuffptr;
			x=*list0ptr++;

			if (a==32) break;
			if (a==0)
			{
				*obuffptr++=0;
				return TRUE;
			}
			
			++ibuffptr;
			if (tolower(x&0x7f) != tolower(a))
			{
				while (x>0 && x<0x7f) x=*list0ptr++;
				if (x==0)
				{
					do
					{
						a=*ibuffptr++;
						if (a==0)
						{
							*obuffptr=0;
							return TRUE;
						}
					} while (a!=32);
					while (*ibuffptr==32) ++ibuffptr;
					list0ptr=L9Pointers[1];
					ptr=ibuffptr;
				}
				else
				{
					list0ptr++;
					ibuffptr=ptr;
				}
			}
			else if (x>=0x7f) break;
		}

		a=*ibuffptr;
		if (a!=32)
		{
			ibuffptr=ptr;
			list0ptr+=2;
			continue;
		}
		--list0ptr;
		while (*list0ptr++<0x7e);
		*obuffptr++=*list0ptr;
		while (*ibuffptr==32) ++ibuffptr;
		list0ptr=L9Pointers[1];
	}	
}

void input(void)
{
   /* if corruptinginput() returns false then, input will be called again
   	next time around instructionloop, this is used when save() and restore()
      are called out of line */
	
	codeptr--;
	if (L9GameType==L9_V2)
	{
		int wordcount;
		if (inputV2(&wordcount))
		{
			L9BYTE *obuffptr=(L9BYTE*) obuff;
			codeptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr++;
			*getvar()=*obuffptr;
			*getvar()=wordcount;
		}
	}
	else
		if (corruptinginput()) codeptr+=5;
}

void varcon(void)
{
	L9UINT16 d6=getcon();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=%d)",cfvar-workspace.vartable,*cfvar);
#endif

}

void varvar(void)
{
	L9UINT16 d6=*getvar();
	*getvar()=d6;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]=Var[%d] (=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d6);
#endif
}

void _add(void)
{
	L9UINT16 d0=*getvar();
  *getvar()+=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]+=Var[%d] (+=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif
}

void _sub(void)
{
	L9UINT16 d0=*getvar();
  *getvar()-=d0;

#ifdef CODEFOLLOW
	fprintf(f," Var[%d]-=Var[%d] (-=%d)",cfvar-workspace.vartable,cfvar2-workspace.vartable,d0);
#endif

}

void jump(void)
{
	L9UINT16 d0=L9WORD(codeptr);
	L9BYTE* a0;
	codeptr+=2;

	a0=acodeptr+((d0+((*getvar())<<1))&0xffff);
	codeptr=acodeptr+L9WORD(a0);
}

L9BYTE exitreversaltable[16]= {0x00,0x04,0x06,0x07,0x01,0x08,0x02,0x03,0x05,0x0a,0x09,0x0c,0x0b,0xff,0xff,0x0f};

/* bug */
void exit1(L9BYTE *d4,L9BYTE *d5,L9BYTE d6,L9BYTE d7)
{
	L9BYTE* a0=absdatablock;
	L9BYTE d1=d7,d0;
	if (--d1)
	{
		do
		{
			d0=*a0;
			a0+=2;
		}
		while ((d0&0x80)==0 || --d1);
	}

	do
	{
		*d4=*a0++;
		if (((*d4)&0xf)==d6)
		{
			*d5=*a0;
			return;
		}
		a0++;
	}
	while (((*d4)&0x80)==0);

	/* notfn4 */
	d6=exitreversaltable[d6];
	a0=absdatablock;
	*d5=1;

	do
	{
		*d4=*a0++;
		if (((*d4)&0x10)==0 || ((*d4)&0xf)!=d6) a0++;
		else if (*a0++==d7) return;
		/* exit6noinc */
		if ((*d4)&0x80) (*d5)++;
	} while (*d4);
	*d5=0;
}

void Exit(void)
{
	L9BYTE d4,d5;
	L9BYTE d7=(L9BYTE) *getvar();
	L9BYTE d6=(L9BYTE) *getvar();
	exit1(&d4,&d5,d6,d7);

	*getvar()=(d4&0x70)>>4;
	*getvar()=d5;
}

void ifeqvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0==d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnevt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
	L9BYTE* a0=getaddr();
	if (d0!=d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
  L9BYTE* a0=getaddr();
  if (d0<d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtvt(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=*getvar();
  L9BYTE* a0=getaddr();
  if (d0>d1) codeptr=a0;

#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>Var[%d] goto %d (%s)",cfvar2-workspace.vartable,cfvar-workspace.vartable,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

void screen(void)
{
   int textmode=*codeptr++;
   if (textmode==0)
   {
		/* stopgint */
      /* set textmode */
#ifdef _DEBUG
		printf("textmode");
#endif      
	}
   else
   {
	   codeptr++;
/*		stopgint */
/*		gintclearg */
#ifdef _DEBUG
		printf("graphmode");
#endif      
	}
}

void cleartg(void)
{
   int textmode=*codeptr++;
   if (textmode==0)
   {
#ifdef _DEBUG
		printf("clear text screen");
#endif
	}
   else
   {
/*		stopgint */
/*		gintclearg */
#ifdef _DEBUG
		printf("clear graph screen");
#endif
	}
}

L9BOOL findsub(int d0,L9BYTE** a5)
{
	int d1,d2,d3,d4;

	d1=d0 << 4;
	d2=d1 >> 8;
	*a5=pictureaddress;
/*findsubloop */
	while (TRUE)
	{
		d3=*(*a5)++;
		if (d3&0x80) return FALSE;
		if (d2==d3)
		{
			if (d1==(*(*a5) & 0xf0))
			{
				(*a5)+=2;
				return TRUE;
			}
		}
		d3=*(*a5)++ & 0xf0;
		d4=**a5;
		if ((d3|d4)==0) return 0xff;

		(*a5)+=(d3<<8) + d4 - 2;
	}
}

void getxy1(int d7,int *d5,int *d6)
{
	*d5=(char) (d7<<2)>>5;
	*d6=(char) (d7<<5)>>3;
/*   if (reflectflag & 2) *d5=-*d5; */
/*   if (reflectflag & 1) *d6=-*d6; */
}

void newxy(int x,int y)
{
/*	a2+=(x*scale)&~7; */
/*	a3+=(y*scale)&~7; */
}

void sdraw(int d7)
{
	int x,y;
	getxy1(d7,&x,&y);

	newxy(x,y);
/*   driver(0x11); */
}
void smove(void)
{
}
void sgosub(void)
{
}
void draw(void)
{
}
void _move(void)
{
}
void icolour(void)
{
}
void size(void)
{
}
void gintfill(void)
{
}
void gosub(void)
{
}
void reflect(void)
{
}

void notimp(void)
{
}
void gintchgcol(void)
{
}
void amove(void)
{
}
void opt(void)
{
}
void restorescale(void)
{
}

L9BOOL getinstruction(int d0)
{
	if ((d0&0xc0)!=0xc0)
   	switch ((d0>>6)&3)
      {
	      case 0: sdraw(d0); break;
      	case 1: smove(); break;
   	   case 2: sgosub(); break;
	   }
   else if ((d0&0x38)!=0x38)
   	switch ((d0>>3)&7)
      {
      	case 0: draw(); break;
      	case 1: _move(); break;
      	case 2: icolour(); break;
      	case 3: size(); break;
      	case 4: gintfill(); break;
      	case 5: gosub(); break;
      	case 6: reflect(); break;
      }
   else
   	switch (d0&7)
		{
      	case 0: notimp(); break;
      	case 1: gintchgcol(); break;
      	case 2: notimp(); break;
      	case 3: amove(); break;
      	case 4: opt(); break;
      	case 5: restorescale(); break;
      	case 6: notimp(); break;
      	case 7: return FALSE;
      }
   return TRUE;
}

void absrunsub(int d0)
{
	L9BYTE* a5;
	if (!findsub(d0,&a5)) return;
   while (getinstruction(*a5++));
}

void picture(void)
{
	int Pic=*getvar();
#ifdef _DEBUG
	printf("picture %d",Pic);
#endif   
/* stopgint */
/* resettask */
/*	driverclg */

/* gintinit */
/*   gintcolour=3; */
/*   option=0x80; */
/*   reflectflag=0; */
/*   sizereset(); */
/*   a4=gintstackbase; */

/*	absrunsub(0); */
/*	absrunsub(Pic); */
}

L9UINT16 gnostack[128];
L9BYTE gnoscratch[32];
int object,gnosp,numobjectfound,searchdepth,inithisearchpos;

void initgetobj(void)
{
	int i;
	numobjectfound=0;
	object=0;
	for (i=0;i<32;i++) gnoscratch[i]=0;
}

void getnextobject(void)
{
	int d2,d3,d4;
	L9UINT16 *hisearchposvar,*searchposvar;

#ifdef _DEBUG
	printf("getnextobject");
#endif

	d2=*getvar();
	hisearchposvar=getvar();
	searchposvar=getvar();
	d3=*hisearchposvar;
	d4=*searchposvar;

/* gnoabs */
	do
	{
		if ((d3 | d4)==0)
		{
			/* initgetobjsp */
			gnosp=128;
			searchdepth=0;
			initgetobj();
			break;
		}

		if (numobjectfound==0) inithisearchpos=d3;

	/* gnonext */
		do
		{
			if (d4==list2ptr[++object])
			{
				/* gnomaybefound */
				int d6=list3ptr[object]&0x1f;
				if (d6!=d3)
				{
					if (d6==0 || d3==0) continue;
					if (d3!=0x1f)
					{
						gnoscratch[d6]=d6;
						continue;
					}
					d3=d6;
				}
				/*gnofound */
				numobjectfound++;
				gnostack[--gnosp]=object;
				gnostack[--gnosp]=0x1f;

				*hisearchposvar=d3;
				*searchposvar=d4;
				*getvar()=object;
				*getvar()=numobjectfound;
				*getvar()=searchdepth;
				return;
			}
		} while (object<=d2);

		if (inithisearchpos==0x1f)
		{
			gnoscratch[d3]=0;
			d3=0;

		/* gnoloop */
			do
			{
				if (gnoscratch[d3])
				{
					gnostack[--gnosp]=d4;
					gnostack[--gnosp]=d3;
				}
			} while (++d3<0x1f);
		}
	/*gnonewlevel */
		if (gnosp!=128)
		{
			d3=gnostack[gnosp++];
			d4=gnostack[gnosp++];
		}
		else d3=d4=0;

		numobjectfound=0;
		if (d3==0x1f) searchdepth++;

		initgetobj();
	} while (d4);

/* gnofinish */
/* gnoreturnargs */
	*hisearchposvar=0;
	*searchposvar=0;
	*getvar()=object=0;
	*getvar()=numobjectfound;
	*getvar()=searchdepth;
}

void ifeqct(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
  if (d0==d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0==d1 ? "Yes":"No");
#endif
}

void ifnect(void)
{
	L9UINT16 d0=*getvar();
	L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
  if (d0!=d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]!=%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0!=d1 ? "Yes":"No");
#endif
}

void ifltct(void)
{
	L9UINT16 d0=*getvar();
  L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
  if (d0<d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]<%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0<d1 ? "Yes":"No");
#endif
}

void ifgtct(void)
{
	L9UINT16 d0=*getvar();
  L9UINT16 d1=getcon();
	L9BYTE* a0=getaddr();
  if (d0>d1) codeptr=a0;
#ifdef CODEFOLLOW
	fprintf(f," if Var[%d]>%d goto %d (%s)",cfvar-workspace.vartable,d1,(L9UINT32) (a0-acodeptr),d0>d1 ? "Yes":"No");
#endif
}

void printinput(void)
{

	L9BYTE* ptr=(L9BYTE*) obuff;
	char c;
	while ((c=*ptr++)!=' ') printchar(c);

#ifdef _DEBUG
	printf("printinput");
#endif   
}

void listhandler(void)
{
	L9BYTE *a4,*MinAccess,*MaxAccess;
	L9UINT16 val;
	L9UINT16 *var;
#ifdef CODEFOLLOW
	int offset; 
#endif

	if ((code&0x1f)>0xa)
	{
		error("\rillegal list access %d\r",code&0x1f);
		Running=FALSE;
		return;
	}
	a4=L9Pointers[1+code&0x1f];

	if (a4>=workspace.listarea && a4<workspace.listarea+LISTAREASIZE)
	{
		MinAccess=workspace.listarea;
		MaxAccess=workspace.listarea+LISTAREASIZE;
	}
	else
	{
		MinAccess=startdata;
		MaxAccess=startdata+FileSize;
	}

	if (code>=0xe0)
	{
		/* listvv */

#ifndef CODEFOLLOW
		a4+=*getvar();
		val=*getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef _DEBUG
		else printf("Out of range list access");
		#endif
	}
	else if (code>=0xc0)
	{
		/* listv1c */
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		var=getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		fprintf(f," Var[%d]= list %d [%d])",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef _DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else if (code>=0xa0)
	{
		/* listv1v */

#ifndef CODEFOLLOW
		a4+=*getvar();
		var=getvar();
#else
		offset=*getvar();
		a4+=offset;
		var=getvar();
		
		fprintf(f," Var[%d] =list %d [%d]",var-workspace.vartable,code&0x1f,offset);
		if (a4>=MinAccess && a4<MaxAccess) fprintf(f," (=%d)",*a4);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *var=*a4;
		else
		{
			*var=0;
			#ifdef _DEBUG
			printf("Out of range list access");
			#endif
		}
	}
	else
	{
#ifndef CODEFOLLOW
		a4+=*codeptr++;
		val=*getvar();
#else
		offset=*codeptr++;
		a4+=offset;
		var=getvar();
		val=*var;
		fprintf(f," list %d [%d]=Var[%d] (=%d)",code&0x1f,offset,var-workspace.vartable,val);
#endif

		if (a4>=MinAccess && a4<MaxAccess) *a4=(L9BYTE) val;
		#ifdef _DEBUG
		else printf("Out of range list access");
		#endif
	}
}

void executeinstruction(void)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"a");
	fprintf(f,"%ld (s:%d) %x",(L9UINT32) (codeptr-acodeptr)-1,workspace.stackptr,code);
	if (!(code&0x80))
		fprintf(f," = %s",codes[code&0x1f]);
#endif

	if (code & 0x80) listhandler();
	else switch (code & 0x1f)
	{
		case 0:	Goto();break;
		case 1:	intgosub();break;
		case 2:  intreturn();break;
		case 3:  printnumber();break;
		case 4:	messagev();break;
		case 5:  messagec();break;
		case 6:	function();break;
		case 7:	input();break;
		case 8:  varcon();break;
		case 9:  varvar();break;
		case 10:	_add();break;
		case 11: _sub();break;
		case 12: ilins(code & 0x1f);break;
		case 13:	ilins(code & 0x1f);break;
		case 14:	jump();break;
		case 15:	Exit();break;
		case 16:	ifeqvt();break;
		case 17:	ifnevt();break;
		case 18:	ifltvt();break;
		case 19:	ifgtvt();break;
		case 20:	screen();break;
		case 21:	cleartg();break;
		case 22:	picture();break;
		case 23:	getnextobject();break;
		case 24:	ifeqct();break;
		case 25:	ifnect();break;
		case 26:	ifltct();break;
		case 27:	ifgtct();break;
		case 28:	printinput();break;
		case 29:	ilins(code & 0x1f);break;
		case 30:	ilins(code & 0x1f);break;
		case 31:	ilins(code & 0x1f);break;
	}
#ifdef CODEFOLLOW
	fprintf(f,"\n");
	fclose(f);
#endif
}

L9BOOL LoadGame2(char *filename)
{
#ifdef CODEFOLLOW
	f=fopen(CODEFOLLOWFILE,"w");
	fprintf(f,"Spank file...\n");
	fclose(f);
#endif

	/* may be already running a game, maybe in input routine */
	Running=FALSE;
	ibuffptr=NULL;

/* intstart */
	if (!intinitialise(filename)) return FALSE;
/*	if (!checksumgamedata()) return FALSE; */

	codeptr=acodeptr;
	randomseed=rand();
  strcpy(LastGame,filename);
	return Running=TRUE;
}

L9BOOL LoadGame(char *filename)
{
	L9BOOL ret=LoadGame2(filename);
	clearworkspace();
	workspace.stackptr=0;
	/* need to clear listarea as well */
	memset((L9BYTE*) workspace.listarea,0,LISTAREASIZE);
  return ret;
}

/* can be called from input to cause fall through for exit */

void StopGame(void)
{
	Running=FALSE;
}

L9BOOL RunGame(void)
{
	code=*codeptr++;
/*	printf("%d",code); */
	executeinstruction();
	return Running;
}
