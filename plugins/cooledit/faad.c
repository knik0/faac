#include <windows.h>
#include <stdio.h>  // FILE *
#include "filters.h" //CoolEdit
#include "faad.h"
#include "aacinfo.h"


#define MAX_CHANNELS 2


typedef struct input_tag // any special vars associated with input file
{
 FILE  *fFile;
 DWORD lSize;    
 DWORD len_ms;
 WORD  wChannels;
 DWORD dwSamprate;
 WORD  wBitsPerSample;
 char  szName[256];

 faacDecHandle hDecoder;
 faadAACInfo   file_info;
 unsigned char *buffer;
 DWORD full_size; // size of decoded file needed to set the length of progress bar
 DWORD tagsize;
 DWORD bytes_read; // from file
 DWORD bytes_consumed; // by faadDecDecode
 DWORD bytes_into_buffer;
 DWORD bytes_Enc;
//	   bytesDec; // Not used because I prefer decode until possible and not until file_info.lenght
} MYINPUT;


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

 if(hInput)  
 {
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);
  full_size=mi->full_size;

  GlobalUnlock(hInput);
 }

 return full_size;
}

__declspec(dllexport) DWORD FAR PASCAL FilterOptions(HANDLE hInput)
{
 return 0L;
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
  case 0:
       lstrcat(szString,"RAW\n");
       return 0;
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
   fclose(mi->fFile);
  
  if(mi->buffer)
   free(mi->buffer);

  if(mi->hDecoder)
   faacDecClose(mi->hDecoder);

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
faacDecHandle hDecoder;
DWORD  k,tmp;
//int    shift;
FILE   *infile;
DWORD  samplerate, channels;
DWORD  pos; // into the file. Needed to obtain length of file
DWORD  read;
int    *seek_table;
//faadAACInfo file_info;
unsigned char *buffer;
long tagsize;

 if(!(infile=fopen(lpstrFilename,"rb")))
  return 0;

 hInput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYINPUT));
 if(!hInput)
 {
  fclose(infile);
  return 0;
 }
 else
 {   
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);

  mi->fFile=infile;
  pos=ftell(infile);
  fseek(infile, 0, SEEK_END);
  mi->lSize=ftell(infile);
  fseek(infile, pos, SEEK_SET);
  if(!(buffer=(unsigned char*)malloc(768*MAX_CHANNELS)))
  {
   MessageBox(0, "Memory allocation error: buffer", "FAAD interface", MB_OK);
   fclose(infile);
   GlobalUnlock(hInput);
   return 0;
  }
  mi->buffer=buffer;
  memset(buffer, 0, 768*MAX_CHANNELS);

  if(mi->lSize<768*MAX_CHANNELS)
   tmp=mi->lSize;
  else
   tmp=768*MAX_CHANNELS;
  read=fread(buffer, 1, tmp, infile);
  if(read==tmp)
  {
   mi->bytes_read=read;
   mi->bytes_into_buffer=read;
  }
  else
  {
   MessageBox(0, "fread", "FAAD interface", MB_OK);
   fclose(mi->fFile);
   free(mi->buffer);
   GlobalUnlock(hInput);
   return 0;
  }

  tagsize=id3v2_tag(buffer);
  if(tagsize)
  {
   for (k=0; k<(768*MAX_CHANNELS - tagsize); k++)
   buffer[k]=buffer[k + tagsize];

   if(mi->bytes_read+tagsize<mi->lSize)
	tmp=tagsize;
   else
    tmp=mi->lSize-mi->bytes_read;
   read=fread(buffer+mi->bytes_into_buffer, 1, tmp, mi->fFile);
   if(read==tmp)
   {
    mi->bytes_read+=read;
    mi->bytes_into_buffer+=read;
   }
   else
   {
    MessageBox(0, "fread", "FAAD interface", MB_OK);
    fclose(mi->fFile);
    free(mi->buffer);
    GlobalUnlock(hInput);
    return 0;
   }
  }
  mi->tagsize=tagsize;
  mi->bytes_Enc=tagsize;
//  mi->bytesDec=0;

  hDecoder = faacDecOpen();
  if(!hDecoder)
  {
   MessageBox(0, "Can't init library", "FAAD interface", MB_OK);
   fclose(mi->fFile);
   free(mi->buffer);
   GlobalUnlock(hInput);
   return 0;
  }
  mi->hDecoder=hDecoder;

  if((mi->bytes_consumed=faacDecInit(hDecoder, buffer, &samplerate, &channels)) < 0)
  {
   MessageBox(hWnd, "Error retrieving information form input file", "FAAD interface", MB_OK);
   fclose(mi->fFile);
   free(mi->buffer);
   faacDecClose(mi->hDecoder);
   GlobalUnlock(hInput);
   return 0;
  }
  mi->bytes_into_buffer-=mi->bytes_consumed;
// if(mi->bytes_consumed>0) 
// faacDecInit reports there is an header to skip
// this operation will be done in ReadFilterInput

  *lSamprate=samplerate;
  *wBitsPerSample=16;
  *wChannels=channels;
  *lChunkSize=sizeof(short)*1024*channels;

  mi->wChannels=channels;
  mi->dwSamprate=samplerate;
  mi->wBitsPerSample=*wBitsPerSample;
  strcpy(mi->szName,lpstrFilename);

  if(seek_table=(int*)LocalAlloc(LPTR, 10800*sizeof(int)))
  {
   get_AAC_format(mi->szName, &(mi->file_info), seek_table);
   LocalFree(seek_table);
  }
  else
   if(!mi->file_info.version)
   {
    fclose(mi->fFile);
    free(mi->buffer);
    faacDecClose(hDecoder);
    GlobalUnlock(hInput);
    return 0;
   }

  if(mi->file_info.object_type==2) // Unupported type SSR profile
   {
    fclose(mi->fFile);
    free(mi->buffer);
    faacDecClose(hDecoder);
    GlobalUnlock(hInput);
    return 0;
   }

  mi->len_ms=1000*(mi->lSize*8)/mi->file_info.bitrate;
  if(mi->len_ms)
   mi->full_size=(DWORD)(mi->len_ms*((float)mi->dwSamprate/1000)*mi->wChannels*(16/8));
  else
   mi->full_size=mi->lSize; // corrupted stream?
/*
  if(mi->file_info.length)
   mi->full_size=(DWORD)(mi->file_info.length*((float)samplerate/1000)*channels*(16/8));
  else
   mi->full_size=mi->lSize; // corrupted stream?
*/
/*  {
   fclose(mi->fFile);
   free(mi->buffer);
   faacDecClose(hDecoder);
   GlobalUnlock(hInput);
   return 0;
  }*/

  GlobalUnlock(hInput);
 }

 return hInput;
}

__declspec(dllexport) DWORD FAR PASCAL ReadFilterInput(HANDLE hInput, unsigned char far *bufout, long lBytes)
{
DWORD read,
      tmp,
	  bytesDec,
	  k;
int   result;
unsigned char *buffer;

 if(hInput)
 {   
 MYINPUT *mi;
  mi=(MYINPUT *)GlobalLock(hInput);

  buffer=mi->buffer;

  mi->bytes_Enc+=mi->bytes_consumed;
  if(mi->bytes_Enc>=mi->lSize)
  {
//   MessageBox(0, "ReadFilterInput: mi->bytesEnc>mi->lSize", "FAAD interface", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }

  if(mi->bytes_consumed)
  {
/*   mi->bytesDec+=sizeof(short)*1024*mi->wChannels;
   if(mi->bytesDec>mi->full_size+sizeof(short)*1024*mi->wChannels)
   {
//    MessageBox(0, "ReadFilterInput: mi->bytesDec>mi->full_size", "FAAD interface", MB_OK);
    GlobalUnlock(hInput);
    return 0;
   }*/

   for(k=0; k<mi->bytes_into_buffer; k++)
    buffer[k]=buffer[k + mi->bytes_consumed];

   if(mi->bytes_read<mi->lSize)
   {
    if(mi->bytes_read+mi->bytes_consumed<mi->lSize)
	 tmp=mi->bytes_consumed;
	else
	 tmp=mi->lSize-mi->bytes_read;
    read=fread(buffer+mi->bytes_into_buffer, 1, tmp, mi->fFile);
    if(read==tmp)
	{
	 mi->bytes_read+=read;
     mi->bytes_into_buffer+=read;
	}
   }
   else
    memset(buffer+mi->bytes_into_buffer, 0, mi->bytes_consumed);
  }

  if(!mi->bytes_into_buffer)
  {
//   MessageBox(0, "ReadFilterInput: buffer empty", "FAAD interface", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }

  result=faacDecDecode(mi->hDecoder, buffer, &(mi->bytes_consumed), (short*)bufout);
  if(mi->bytes_into_buffer>mi->bytes_consumed)
   mi->bytes_into_buffer-=mi->bytes_consumed;
  else
   mi->bytes_into_buffer=0;
//  if(result>FAAD_OK_CHUPDATE)
  if(result==FAAD_FATAL_ERROR)
  {
//   MessageBox(0, "ReadFilterInput: FAAD_FATAL_ERROR or FAAD_ERROR", "FAAD interface", MB_OK);
//   MessageBox(0, "ReadFilterInput: FAAD_FATAL_ERROR", "FAAD interface", MB_OK);
   GlobalUnlock(hInput);
   return 0;
  }

  bytesDec=sizeof(short)*1024*mi->wChannels;

  GlobalUnlock(hInput);
 }

 return bytesDec;
}
