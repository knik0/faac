#include <windows.h>
#include <stdio.h>  // FILE *
#include "filters.h" //CoolEdit
#include "resource.h"
#include "faac.h"


typedef struct output_tag  // any special vars associated with output file
{
 FILE  *fFile;         
 DWORD lSize;
 long  lSamprate;
 WORD  wBitsPerSample;
 WORD  wChannels;
 DWORD dwDataOffset;
 BOOL  bWrittenHeader;
 char  szNAME[256];

 faacEncHandle hEncoder;
 unsigned char *bitbuf;
 DWORD maxBytesOutput;
 long  samplesInput;
 BOOL  bStopEnc;
} MYOUTPUT;


faacEncConfiguration faacEncCfg;

__declspec(dllexport) BOOL FAR PASCAL DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
 switch(Message)
 {
  case WM_INITDIALOG:
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"8");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"18");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"20");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"24");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"32");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"40");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"48");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"56");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"64");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"96");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"112");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"128");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"160");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"192");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"256");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_SETCURSEL, 11, 0);

       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"4000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"8000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"16000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"22050");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"24000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, 3, 0);

       CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, TRUE);
       CheckDlgButton(hWndDlg, IDC_USETNS, TRUE);
       CheckDlgButton(hWndDlg, IDC_USELFE, FALSE);

	   switch((long)lParam)
	   {
	    case 1:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
			 break;
		case 2:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG2,TRUE);
			 break;
		case 3:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
			 break;
		case 4:
			 CheckDlgButton(hWndDlg,IDC_RADIO_LOW,TRUE);
			 break;
		case 5:
			 CheckDlgButton(hWndDlg,IDC_RADIO_SSR,TRUE);
			 break;
		case 6:
			 CheckDlgButton(hWndDlg,IDC_RADIO_LTP,TRUE);
			 break;
		case 19:
			 CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, !IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG));
			 break;
		default:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
			 CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
			 break;
	   }         
       break; // End of WM_INITDIALOG                                 

  case WM_CLOSE:
       // Closing the Dialog behaves the same as Cancel               
       PostMessage(hWndDlg, WM_COMMAND, IDCANCEL, 0L);
       break; // End of WM_CLOSE                                      

  case WM_COMMAND:
	   switch(LOWORD(wParam))
       {
	    case IDOK:
           	 {
			 DWORD retVal=0;
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4))
			  {
               faacEncCfg.mpegVersion=4;
			   retVal|=4<<29;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG2))
			  {
               faacEncCfg.mpegVersion=2;
			   retVal|=2<<29;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MAIN))
               faacEncCfg.aacObjectType=0;
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
			  {
               faacEncCfg.aacObjectType=1;
			   retVal|=1<<27;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_SSR))
			  {
               faacEncCfg.aacObjectType=2;
			   retVal|=2<<27;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP))
			  {
               faacEncCfg.aacObjectType=3;
			   retVal|=3<<27;
			  }

              faacEncCfg.allowMidside=IsDlgButtonChecked(hWndDlg, IDC_ALLOWMIDSIDE) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.allowMidside<<26;
              faacEncCfg.useTns=IsDlgButtonChecked(hWndDlg, IDC_USETNS) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.useTns<<25;
              faacEncCfg.useLfe=IsDlgButtonChecked(hWndDlg, IDC_USELFE) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.useLfe<<24;
			  
			  faacEncCfg.bitRate=GetDlgItemInt(hWndDlg, IDC_CB_BITRATE, 0, FALSE);
              retVal|=faacEncCfg.bitRate;
			  faacEncCfg.bandWidth=GetDlgItemInt(hWndDlg, IDC_CB_BANDWIDTH, 0, FALSE);
			  retVal|=(faacEncCfg.bandWidth/1000)<<16;
             
			  if(IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG))
			   retVal=1;

			  EndDialog(hWndDlg, retVal);
             }
             break;

        case IDCANCEL:
             // Ignore data values entered into the controls        
             // and dismiss the dialog window returning FALSE       
             EndDialog(hWndDlg, FALSE);
             break;
       }
       break; // End of WM_COMMAND                                 
  default: return FALSE;
 }
 return TRUE;
} // End of DIALOGSMsgProc                                      

__declspec(dllexport) DWORD FAR PASCAL FilterGetOptions(HWND hWnd, HINSTANCE hInst, long lSamprate, WORD wChannels, WORD wBitsPerSample, DWORD dwOptions) // return 0 if no options box
{
long nDialogReturn=0;
FARPROC lpfnDIALOGMsgProc;
	
 lpfnDIALOGMsgProc=GetProcAddress(hInst,(LPCSTR)MAKELONG(20,0));			
 nDialogReturn=(long)DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)lpfnDIALOGMsgProc,nDialogReturn);

 return nDialogReturn;
}

__declspec(dllexport) DWORD FAR PASCAL FilterWriteFirstSpecialData(HANDLE hInput, 
	SPECIALDATA * psp)
{
 return 0;
}

__declspec(dllexport) DWORD FAR PASCAL FilterWriteNextSpecialData(HANDLE hInput, SPECIALDATA * psp)
{	
 return 0;
// only has 1 special data!  Otherwise we would use psp->hSpecialData
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

 if(mo->hEncoder)
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
HANDLE        hOutput;
faacEncHandle hEncoder;
FILE          *outfile;
unsigned char *bitbuf;
DWORD         maxBytesOutput;
long          samplesInput;
int           bytesEncoded;

	/* open the aac output file */
	if(!(outfile=fopen(lpstrFilename, "wb")))
	{
     MessageBox(0, "Can't create file", "FAAC interface", MB_OK);
	 return 0;
	}

	/* open the encoder library */
	if(!(hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
	{
	 MessageBox(0, "Can't init library", "FAAC interface", MB_OK);
	 fclose(outfile);
	 return 0;
	}

	if(!(bitbuf=(unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char))))
	{
	 MessageBox(0, "Memory allocation error: output buffer", "FAAC interface", MB_OK);
     faacEncClose(hEncoder);
	 fclose(outfile);
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
	 mo->dwDataOffset=0; // ???
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
	{
	 MessageBox(0, "hOutput=NULL", "FAAC interface", MB_OK);
     faacEncClose(hEncoder);
	 fclose(outfile);
	 free(bitbuf);
	 return 0;
	}

	if(dwOptions && dwOptions!=1)
	{
    faacEncConfigurationPtr myFormat;
     myFormat=faacEncGetCurrentConfiguration(hEncoder);

	 myFormat->mpegVersion=(dwOptions>>29)&7;
	 myFormat->aacObjectType=(dwOptions>>27)&3;
	 myFormat->allowMidside=(dwOptions>>26)&1;
	 myFormat->useTns=(dwOptions>>25)&1;
	 myFormat->useLfe=(dwOptions>>24)&1;
	 myFormat->bitRate=(dwOptions&0x00ff)*1000;
	 myFormat->bandWidth=((dwOptions>>16)&127)*1000;
     switch(myFormat->bandWidth)
	 {
     case 11000:
		  myFormat->bandWidth=11025;
		  break;
     case 22000:
		  myFormat->bandWidth=22050;
		  break;
     case 44000:
		  myFormat->bandWidth=44100;
		  break;
	 }

	 if(!faacEncSetConfiguration(hEncoder, myFormat))
	 {
      MessageBox(0, "Unsupported parameters", "FAAC interface", MB_OK);
      faacEncClose(hEncoder);
	  fclose(outfile);
	  free(bitbuf);
      GlobalFree(hOutput);
	  return 0;
	 }
/*	{
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
      MessageBox(0, "Unsupported parameters", "FAAC interface", MB_OK);
      faacEncClose(hEncoder);
	  fclose(outfile);
	  free(bitbuf);
      GlobalFree(hOutput);
	  return 0;
	 }*/
	}

    return hOutput;
}

__declspec(dllexport) DWORD FAR PASCAL WriteFilterOutput(HANDLE hOutput, unsigned char far *buf, long lBytes)
{
int bytesWritten;
int bytesEncoded;

 if(hOutput)
 { 
 MYOUTPUT far *mo;
  mo=(MYOUTPUT far *)GlobalLock(hOutput);

  if(!mo->bStopEnc)
  {
// call the actual encoding routine
   bytesEncoded=faacEncEncode(mo->hEncoder, (short *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
   if(bytesEncoded<1) // end of flushing process
   {
    if(bytesEncoded<0)
     MessageBox(0, "faacEncEncode() failed", "FAAC interface", MB_OK);
    mo->bStopEnc=1;
    GlobalUnlock(hOutput);
    return 0;
   }
// write bitstream to aac file 
   bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
   if(bytesWritten!=bytesEncoded)
   {
    MessageBox(0, "bytesWritten and bytesEncoded are different", "FAAC interface", MB_OK);
    mo->bStopEnc=1;
    GlobalUnlock(hOutput);
    return 0;
   }

   GlobalUnlock(hOutput);
  }
 }

 return bytesWritten;
}
