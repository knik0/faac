#include <windows.h>
#include <stdlib.h>  		// for atol conversion  
#include <stdio.h>  // for FILE *
#include "filters.h" //CoolEdit
#include "resource.h"

#include <faad.h>
#include "aacinfo.h"


#define MAX_CHANNELS 2


faacDecHandle hDecoder;

typedef struct input_tag // any special vars associated with input file
{
 FILE  *fFile;
 long  lSize;    
 DWORD dwFormat;  
 WORD  wChannels;
 DWORD dwSamprate;
 WORD  wBitsPerSample;
 DWORD dwDataOffset;		   
 char  szName[256];

 faacDecHandle hDecoder;
 faadAACInfo file_info;
 unsigned char *buffer;
 FILE  *infile;
 long  file_len, tagsize, buffercount, fileread, bytecount;
 char  supported;
 DWORD full_size;
 DWORD bytes_left;
} MYINPUT;

/*
static FILE   *infile;
static char Filename[1024];
static faadAACInfo file_info;
static long file_len;
static char supported;
DWORD   samplerate, channels;
DWORD full_size;
*/
static long tagsize, buffercount, fileread, bytecount, bytesFull;
//BYTE buffer[768*MAX_CHANNELS];

int id3v2_tag(unsigned char *buffer)
{
 if(StringComp(buffer, "ID3", 3) == 0) 
 {
unsigned long tagsize;

/* high bit is not used */
  tagsize=(buffer[6] << 21) | (buffer[7] << 14) |
          (buffer[8] <<  7) | (buffer[9] <<  0);
  tagsize += 10;
  return tagsize;
 }
 else 
  return 0;
}

__declspec(dllexport) BOOL FAR PASCAL FilterUnderstandsFormat(LPSTR filename)
{
WORD len;
 if((len=lstrlen(filename))>4 && 
	(!strcmpi(filename+len-4,".aac") ||
	 !strcmpi(filename+len-4,".mp4")))
  return TRUE;
 return FALSE;
}

__declspec(dllexport) long FAR PASCAL FilterGetFileSize(HANDLE hInput)
{
DWORD full_size;
//DWORD pos;

 if(hInput)  
 {
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);
  full_size=mi->full_size;
 
/*	pos=ftell(mi->fFile);
	fseek(mi->fFile, 0, SEEK_END);
	fileread=ftell(mi->fFile);
	fseek(mi->fFile, pos, SEEK_SET);*/
  GlobalUnlock(hInput);
 }

 return full_size; //file_len;
}

__declspec(dllexport) DWORD FAR PASCAL FilterOptions(HANDLE hInput)
{
	return 0L;
/*	MYINPUT far *mi;
	DWORD options;
	if (!hInput) return 0;
	mi=(MYINPUT far *)GlobalLock(hInput);
    
    options=(DWORD)mi->dwFormat;
    if (options==3)
    	options=2;
    
    GlobalUnlock(hInput);
     
    return options;*/
}	    

__declspec(dllexport) DWORD FAR PASCAL FilterOptionsString(HANDLE hInput, LPSTR szString)
{
char buf[20];

 if(hInput)
 {
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);
 
  lstrcpy(szString,"");

  if(mi->file_info.version == 2)
   lstrcat(szString,"MPEG2 - ");
  else
   lstrcat(szString,"MPEG4 - ");
 
  sprintf(buf,"%lu bps\n",mi->file_info.bitrate);
  lstrcat(szString,buf);
 
  switch(mi->file_info.headertype)
  {
  case 0: // Headerless 
     lstrcat(szString,"RAW\n");
     return 0;
     break;
  case 1:
     lstrcat(szString,"ADIF\n");
     break;
  case 2:
     lstrcat(szString,"ADTS\n");
     break;
  }

  switch(mi->file_info.object_type)
  {
  case 0:
     lstrcat(szString,"Main");
     break;
  case 1:
     lstrcat(szString,"Low Complexity");
     break;
  case 2:
     lstrcat(szString,"SSR (unsupported)");
     break;
  case 3:
     lstrcat(szString,"Main LTP");
     break;
  }

  GlobalUnlock(hInput);
 }
 return 1;
}

__declspec(dllexport) DWORD FAR PASCAL FilterGetFirstSpecialData(HANDLE hInput, 
	SPECIALDATA * psp)
{
return 0L;
}
    
__declspec(dllexport) DWORD FAR PASCAL FilterGetNextSpecialData(HANDLE hInput, SPECIALDATA * psp)
{	return 0; // only has 1 special data!  Otherwise we would use psp->hSpecialData
			  // as either a counter to know which item to retrieve next, or as a
			  // structure with other state information in it.
}

__declspec(dllexport) void FAR PASCAL CloseFilterInput(HANDLE hInput)
{
 if(hInput)
 {
 MYINPUT far *mi;
  mi=(MYINPUT far *)GlobalLock(hInput);

  if(mi->fFile)
  {
   fclose(mi->fFile);
   mi->fFile=0;
  }
  
  GlobalUnlock(hInput);
  GlobalFree(hInput);
 }
}

// return handle that will be passed in to close, and write routines
__declspec(dllexport) HANDLE FAR PASCAL OpenFilterInput( LPSTR lpstrFilename,
											long far *lSamprate,
											WORD far *wBitsPerSample,
											WORD far *wChannels,
											HWND hWnd,
											long far *lChunkSize)
{
HANDLE hInput;
DWORD  k;
int    shift;
FILE   *infile;
DWORD  samplerate, channels;
DWORD  pos;
int    *seek_table;
faadAACInfo file_info;
unsigned char *buffer;

 hInput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYINPUT));
 if(hInput)
 {   
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);

  if(!(infile=fopen(lpstrFilename,"rb")))
  {
   GlobalUnlock(hInput);
   return 0;
  }

  mi->fFile=infile;

  pos=ftell(infile);
  fseek(infile, 0, SEEK_END);
  mi->lSize=ftell(infile);
  fseek(infile, pos, SEEK_SET);

  if(!(buffer=(unsigned char*)malloc(768*MAX_CHANNELS)))
  {
   MessageBox(0, "buffer", "Memory allocation error", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }
  mi->buffer=buffer;
  memset(buffer, 0, 768*MAX_CHANNELS);

  buffercount=bytecount=0;
  fread(buffer, 1, 768*MAX_CHANNELS, infile);

  tagsize=id3v2_tag(buffer);
  mi->bytes_left=mi->lSize-tagsize;
  if(tagsize)
  {
   bytecount = tagsize;
   buffercount = 0;
   for (k=0; k<(768*MAX_CHANNELS - tagsize); k++)
   buffer[k]=buffer[k + tagsize];
   shift=(768*MAX_CHANNELS)-tagsize;
   fread(buffer+shift, 1, shift, infile);
  }

  hDecoder = faacDecOpen();
  if(!hDecoder)
  {
   MessageBox(0, "Can't init library", "Error", MB_OK);
   return 0;
  }
  if((buffercount=faacDecInit(hDecoder, buffer, &samplerate, &channels)) < 0)
  {
   MessageBox(hWnd, "Error retrieving information form input file", "FAAD Error", MB_OK);
   return 0;
  }
  // bytes will be shifted in ReadFilterInput

  *lSamprate=samplerate;
  *wBitsPerSample=16;
  *wChannels=channels;
  *lChunkSize=sizeof(short)*1024*channels; //16384;

  if(buffercount>0) // faacDecInit reports there is an header to skip
  {
//     file_len-=buffercount;
   bytecount+=buffercount;
   for (k=0; k<(768*MAX_CHANNELS - buffercount); k++)
    buffer[k]=buffer[k + buffercount];
   shift=(768*MAX_CHANNELS)-buffercount;
   fread(buffer+shift, 1, buffercount, infile);
   buffercount=0;
  }

  mi->dwFormat=0;
  mi->wChannels=channels;
  mi->dwSamprate=samplerate;
  mi->wBitsPerSample=*wBitsPerSample;
  mi->dwDataOffset=0;
  strcpy(mi->szName,lpstrFilename);

  mi->hDecoder=hDecoder;
  mi->tagsize=tagsize;

  file_info.version=0;
  if(seek_table=(int*)LocalAlloc(LPTR, 10800*sizeof(int)))
  {
   get_AAC_format(mi->szName, &file_info, seek_table);
   LocalFree(seek_table);
  }
  else
   file_info.version='?';
  memcpy(&(mi->file_info),&file_info,sizeof(file_info));
  if(file_info.length)
   mi->full_size=(DWORD)(file_info.length*((float)samplerate/1000)*channels*(16/8));
  else
  {
   GlobalUnlock(hInput);
   return 0;
  }

  if(file_info.object_type==2)
   mi->supported=0;
  else
   mi->supported=1;

  GlobalUnlock(hInput);
 }

 bytesFull=0;

 return hInput;
}

__declspec(dllexport) DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes)
{
DWORD read=0,
      bytesconsumed,
      bytestot=0,
	  k;
int   result;
unsigned char *buffer;

// MessageBox(0, "ReadFilterInput", "FAAD Error", MB_OK);


 if(hInput)
 {   
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);

  buffer=mi->buffer;

  if(!mi->supported)
  {
   MessageBox(0, "ReadFilterInput: format not supported", "FAAD Error", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }

  if(mi->bytes_left<buffercount)
   mi->bytes_left=0;
  else
   mi->bytes_left-=buffercount;

  if(bytesFull>mi->full_size)
  {
//   MessageBox(0, "ReadFilterInput: bytesFull>mi->lSize", "FAAD Error", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }

  if(!mi->bytes_left)
  {
   GlobalUnlock(hInput);
   return 0;
  }

  if(buffercount>0)
  {
   for(k=0; k<(768*MAX_CHANNELS - buffercount); k++)
    buffer[k]=buffer[k + buffercount];

   read=fread(buffer+(768*MAX_CHANNELS)-buffercount, buffercount, 1, mi->fFile);
   if(!read)
    memset(buffer+(768*MAX_CHANNELS)-buffercount, 0, buffercount);
   buffercount=0;
  }

  result=faacDecDecode(hDecoder, buffer, &bytesconsumed, (short*)bufout);

  if(result>FAAD_OK_CHUPDATE) // FAAD_FATAL_ERROR or FAAD_ERROR
   mi->bytes_left=0;

  bytestot=sizeof(short)*1024*mi->wChannels;
  
  GlobalUnlock(hInput);
 }
/*
 switch(result) 
 {
 case FAAD_FATAL_ERROR:
    MessageBox(0, "Fatal error decoding input file\n", "FAAD error", MB_OK);
    return 0;
  case FAAD_ERROR:
    return bytestot;
 }
*/
 buffercount=bytesconsumed;
 bytecount+=bytesconsumed;
 bytesFull+=bytestot;

 return bytestot;// read;
}
