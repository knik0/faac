#include <windows.h>
#include <stdio.h>  // for FILE *
#include "filters.h" //CoolEdit
#include "resource.h"
#include "faac.h"

static faacEncHandle hEncoder;
faacEncConfiguration faacEncCfg;

typedef struct output_tag  // any special vars associated with output file
{
 FILE *fFile;         
 long lSize;
 long lSamprate;
 WORD wBitsPerSample;
 WORD wChannels;
 DWORD dwDataOffset;
 BOOL bWrittenHeader;
 char szNAME[256];

 faacEncHandle hEncoder;
 unsigned char *bitbuf;
 DWORD maxBytesOutput;
 long samplesInput;
 BOOL bStopEnc;
} MYOUTPUT;

__declspec(dllexport) DWORD FAR PASCAL FilterGetOptions(HWND hWnd, HINSTANCE hInst, long lSamprate, WORD wChannels, WORD wBitsPerSample, DWORD dwOptions) // return 0 if no options box
{
long nDialogReturn=0L;
FARPROC lpfnDIALOGMsgProc;
	
	lpfnDIALOGMsgProc=GetProcAddress(hInst,(LPCSTR)MAKELONG(20,0));			
	nDialogReturn=(long)DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)lpfnDIALOGMsgProc,nDialogReturn);

	return nDialogReturn;
}

__declspec(dllexport) DWORD FAR PASCAL FilterWriteFirstSpecialData(HANDLE hInput, 
	SPECIALDATA * psp)
{
return 0L;
}

__declspec(dllexport) DWORD FAR PASCAL FilterWriteNextSpecialData(HANDLE hInput, SPECIALDATA * psp)
{	return 0; // only has 1 special data!  Otherwise we would use psp->hSpecialData
			  // as either a counter to know which item to retrieve next, or as a
			  // structure with other state information in it.
}

__declspec(dllexport) DWORD FAR PASCAL FilterWriteSpecialData(HANDLE hOutput,
	LPCSTR szListType, LPCSTR szType, char * pData,DWORD dwSize)
{
 return 0;
}

__declspec(dllexport) void FAR PASCAL CloseFilterOutput(HANDLE hOutput)
{
 if(hOutput)
 {
 MYOUTPUT *mo;
  mo=(MYOUTPUT *)GlobalLock(hOutput);

 if(mo->fFile)
 {
  fclose(mo->fFile);
  mo->fFile=0;
 }

 faacEncClose(mo->hEncoder);

 if(mo->bitbuf)
 {
  free(mo->bitbuf);
  mo->bitbuf=0;
 }

  GlobalUnlock(hOutput);
  GlobalFree(hOutput);
 }
}              

__declspec(dllexport) HANDLE FAR PASCAL OpenFilterOutput(LPSTR lpstrFilename,long lSamprate,WORD wBitsPerSample,WORD wChannels,long lSize, long far *lpChunkSize, DWORD dwOptions)
{
HANDLE hOutput;
long samplesInput;
int bytesEncoded;

//faacEncHandle hEncoder;
FILE *outfile=0;
short *pcmbuf=0;
unsigned char *bitbuf=0;
DWORD maxBytesOutput=0;

	/* open the aac output file */
	outfile=fopen(lpstrFilename, "wb");
	if(!outfile)
	{
     MessageBox(0, lpstrFilename, "Can't create file", MB_OK);
	 return 0;
	}

	/* open the encoder library */
	hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput);
	if(!hEncoder)
	{
	 MessageBox(0, "Can't init library", "Error", MB_OK);
	 return 0;
	}

	if(!(pcmbuf=(short*)malloc(samplesInput*sizeof(short))))
	{
	 MessageBox(0, "PCM buffer", "Memory allocation error", MB_OK);
	 return 0;
	}
	if(!(bitbuf=(unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char))))
	{
	 MessageBox(0, "Output buffer", "Memory allocation error", MB_OK);
	 return 0;
	}

    bytesEncoded=faacEncEncode(hEncoder, 0, 0, bitbuf, maxBytesOutput); // initializes the flushing process
    if(bytesEncoded>0)
	 fwrite(bitbuf, 1, bytesEncoded, outfile);

	*lpChunkSize=samplesInput*2;

    hOutput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE,sizeof(MYOUTPUT));
    if(hOutput)
    {
	MYOUTPUT *mo;
	 mo=(MYOUTPUT *)GlobalLock(hOutput);
	 mo->fFile=outfile;
	 mo->lSize=lSize;
	 mo->lSamprate=lSamprate;
	 mo->wBitsPerSample=wBitsPerSample;
	 mo->wChannels=wChannels;
	 mo->dwDataOffset=0; // ??
	 mo->bWrittenHeader=0;
	 strcpy(mo->szNAME,lpstrFilename);

	 mo->hEncoder=hEncoder;
     mo->bitbuf=bitbuf;
	 mo->maxBytesOutput=maxBytesOutput;
	 mo->samplesInput=samplesInput;
	 mo->bStopEnc=0;

	 GlobalUnlock(hOutput);
    }
	else
	 MessageBox(0, "hOutput=NULL", "FAAC Error", MB_OK);

	if(dwOptions==2)
	{
faacEncConfigurationPtr myFormat;
     myFormat=faacEncGetCurrentConfiguration(hEncoder);

	 myFormat->mpegVersion=faacEncCfg.mpegVersion;
	 myFormat->aacObjectType=faacEncCfg.aacObjectType;
	 myFormat->allowMidside=faacEncCfg.allowMidside;
	 myFormat->useLfe=faacEncCfg.useLfe;
	 myFormat->useTns=faacEncCfg.useTns;
	 myFormat->bandWidth=faacEncCfg.bandWidth;
	 myFormat->bitRate=faacEncCfg.bitRate;

	 if(!faacEncSetConfiguration(hEncoder, myFormat))
	 {
      MessageBox(0, "Unsupported parameters\n", "FAAC error", MB_OK);
	  return 0;
	 }
	}

    return hOutput;
}

__declspec(dllexport) DWORD FAR PASCAL WriteFilterOutput(HANDLE hOutput, unsigned char far *buf, long lBytes)
{
int bytesWritten;
int bytesEncoded;
unsigned char *bitbuf;

 if(hOutput)
 { 
 MYOUTPUT far *mo;

  mo=(MYOUTPUT far *)GlobalLock(hOutput);

  if(!mo->bStopEnc)
  {
   bitbuf=mo->bitbuf;

// call the actual encoding routine
   bytesEncoded=faacEncEncode(mo->hEncoder, (short *)buf, mo->samplesInput, bitbuf, mo->maxBytesOutput);
   if(!bytesEncoded) // end of flushing process
	mo->bStopEnc=1;
  }

  if(!bytesEncoded) // end of flushing process
  {
   GlobalUnlock(hOutput);
   return 0;
  }
  else
   if(bytesEncoded<0)
   {
    MessageBox(0, "faacEncEncode() failed", "Error", MB_OK);
    GlobalUnlock(hOutput);
    return 0;
   }

// write bitstream to aac file 
  bytesWritten=fwrite(bitbuf, 1, bytesEncoded, mo->fFile);

  GlobalUnlock(hOutput);
 }

 if(bytesWritten!=bytesEncoded)
 {
  MessageBox(0, "bytesWritten and bytesEncoded are different", "Output error", MB_OK);
  return 0;
 }

 return bytesWritten;
}
