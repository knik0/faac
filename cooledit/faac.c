#include <windows.h>
#include <stdio.h>  // FILE *
#include "filters.h" //CoolEdit
#include "resource.h"
#include "faac.h"


#define PI_VER "v1.0beta"


extern void config_init();
extern void config_read(DWORD *dwOptions);
extern void config_write(DWORD dwOptions);


typedef struct output_tag  // any special vars associated with output file
{
 FILE  *fFile;         
 DWORD lSize;
 long  lSamprate;
 WORD  wBitsPerSample;
 WORD  wChannels;
// DWORD dwDataOffset;
// BOOL  bWrittenHeader;
 char  szNAME[256];

 faacEncHandle hEncoder;
 unsigned char *bitbuf;
 DWORD maxBytesOutput;
 long  samplesInput;
 BOOL  bStopEnc;
} MYOUTPUT;



__declspec(dllexport) BOOL FAR PASCAL DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
DWORD dwOptions=(DWORD)lParam;

 switch(Message)
 {
  case WM_INITDIALOG:
	  {
      char buf[10];

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
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_SETCURSEL, 8, 0);

       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"0");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"4000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"8000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"16000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"22050");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)"24000");
       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, 0, 0);

//       if(!((dwOptions>>23)&1))
	   {
        config_init();
        config_read(&dwOptions);
	   }

	   if(dwOptions)
	   {
       char Enabled=!(dwOptions&1);
		CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, dwOptions&1);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG4), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG2), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_ALLOWMIDSIDE), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_USETNS), Enabled);
//        EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled);
        EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled);

	    if(((dwOptions>>29)&7)==MPEG4)
	     CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
		else
		 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG2,TRUE);

		switch((dwOptions>>27)&3)
		{
		 case 0:
              CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
		      break;
		 case 1:
              CheckDlgButton(hWndDlg,IDC_RADIO_LOW,TRUE);
		      break;
		 case 2:
              CheckDlgButton(hWndDlg,IDC_RADIO_SSR,TRUE);
		      break;
		  case 3:
               CheckDlgButton(hWndDlg,IDC_RADIO_LTP,TRUE);
		       break;
		}

		CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, (dwOptions>>26)&1);
        CheckDlgButton(hWndDlg, IDC_USETNS, (dwOptions>>25)&1);
        CheckDlgButton(hWndDlg, IDC_USELFE, (dwOptions>>24)&1);

        SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_SETCURSEL, (dwOptions>>19)&31, 0);
//        SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, (dwOptions>>1)&0x0000ffff, 0);
		sprintf(buf,"%lu",(dwOptions>>1)&0x0000ffff);
        SetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, buf);
		break;
	   } // End dwOptions

       CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, TRUE);
       CheckDlgButton(hWndDlg, IDC_USETNS, TRUE);
       CheckDlgButton(hWndDlg, IDC_USELFE, FALSE);

	   switch((long)lParam)
	   {
	    case IDC_RADIO_MPEG4:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
			 break;
		case IDC_RADIO_MPEG2:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG2,TRUE);
			 break;
		case IDC_RADIO_MAIN:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
			 break;
		case IDC_RADIO_LOW:
			 CheckDlgButton(hWndDlg,IDC_RADIO_LOW,TRUE);
			 break;
		case IDC_RADIO_SSR:
			 CheckDlgButton(hWndDlg,IDC_RADIO_SSR,TRUE);
			 break;
		case IDC_RADIO_LTP:
			 CheckDlgButton(hWndDlg,IDC_RADIO_LTP,TRUE);
			 break;
		case IDC_CHK_AUTOCFG:
			 CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, !IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG));
			 break;
		default:
			 CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
			 CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
			 break;
	   }         
	  }
       break; // End of WM_INITDIALOG                                 

  case WM_CLOSE:
       // Closing the Dialog behaves the same as Cancel               
       PostMessage(hWndDlg, WM_COMMAND, IDCANCEL, 0L);
       break; // End of WM_CLOSE                                      

  case WM_COMMAND:
	   switch(LOWORD(wParam))
       {
	   case IDC_CHK_AUTOCFG:
		   {
           char Enabled=!IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG4), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG2), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_ALLOWMIDSIDE), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_USETNS), Enabled);
//      		EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled);
      		EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled);
		   }
           break;

	   case IDOK:
		   {
		   DWORD retVal=0;
           faacEncConfiguration faacEncCfg;
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4))
			  {
               faacEncCfg.mpegVersion=MPEG4;
			   retVal|=MPEG4<<29;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG2))
			  {
               faacEncCfg.mpegVersion=MPEG2;
			   retVal|=MPEG2<<29;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MAIN))
			  {
               faacEncCfg.aacObjectType=MAIN;
			   retVal|=MAIN<<27;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
			  {
               faacEncCfg.aacObjectType=LOW;
			   retVal|=LOW<<27;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_SSR))
			  {
               faacEncCfg.aacObjectType=SSR;
			   retVal|=SSR<<27;
			  }
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP))
			  {
               faacEncCfg.aacObjectType=LTP;
			   retVal|=LTP<<27;
			  }

              faacEncCfg.allowMidside=IsDlgButtonChecked(hWndDlg, IDC_ALLOWMIDSIDE) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.allowMidside<<26;
              faacEncCfg.useTns=IsDlgButtonChecked(hWndDlg, IDC_USETNS) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.useTns<<25;
              faacEncCfg.useLfe=IsDlgButtonChecked(hWndDlg, IDC_USELFE) == BST_CHECKED ? 1 : 0;
              retVal|=faacEncCfg.useLfe<<24;
			  
			  faacEncCfg.bitRate=GetDlgItemInt(hWndDlg, IDC_CB_BITRATE, 0, FALSE);
			  retVal|=(SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_GETCURSEL, 0, 0)&31)<<19;
              //retVal|=faacEncCfg.bitRate;
			  faacEncCfg.bandWidth=GetDlgItemInt(hWndDlg, IDC_CB_BANDWIDTH, 0, FALSE);
//			  retVal|=(SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_GETCURSEL, 0, 0)&0x0000ffff)<<1;
			  retVal|=(faacEncCfg.bandWidth&0x0000ffff)<<1;
             
			  if(IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG))
			   retVal|=1;

              config_write(retVal);
			  
//			  retVal|=1<<23; // CFG has been written

              EndDialog(hWndDlg, retVal);
             }
             break;

        case IDCANCEL:
             // Ignore data values entered into the controls        
             // and dismiss the dialog window returning FALSE       
             EndDialog(hWndDlg, FALSE);
             break;

		case IDC_BTN_ABOUT:
           	 {
		     char buf[256];
 			  sprintf(buf,"AAC-MPEG4 encoder plug-in %s\nThis plugin uses FAAC encoder engine v%g\n\nCompiled on %s\n",
				          PI_VER,
		 	              FAACENC_VERSION,
						  __DATE__
						  );
		      MessageBox(hWndDlg, buf, "About", MB_OK);
             }
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
 nDialogReturn=(long)DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)lpfnDIALOGMsgProc, dwOptions);

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

//    if(!((dwOptions>>23)&1))
	{
     config_init();
     config_read(&dwOptions);
	}

// open the aac output file 
	if(!(outfile=fopen(lpstrFilename, "wb")))
	{
     MessageBox(0, "Can't create file", "FAAC interface", MB_OK);
	 return 0;
	}

// open the encoder library
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
//	 mo->dwDataOffset=0; // ???
//	 mo->bWrittenHeader=0;
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

	if(dwOptions && !(dwOptions&1))
	{
    faacEncConfigurationPtr myFormat;
     myFormat=faacEncGetCurrentConfiguration(hEncoder);

     myFormat->mpegVersion=(dwOptions>>29)&7;
	 myFormat->aacObjectType=(dwOptions>>27)&3;
	 myFormat->allowMidside=(dwOptions>>26)&1;
	 myFormat->useTns=(dwOptions>>25)&1;
	 myFormat->useLfe=(dwOptions>>24)&1;
	 switch((dwOptions>>1)&31)
	 {
	  case 0:
           myFormat->bitRate=8000;
		   break;
	  case 1:
           myFormat->bitRate=18000;
		   break;
	  case 2:
           myFormat->bitRate=20000;
		   break;
	  case 3:
           myFormat->bitRate=24000;
		   break;
	  case 4:
           myFormat->bitRate=32000;
		   break;
	  case 5:
           myFormat->bitRate=40000;
		   break;
	  case 6:
           myFormat->bitRate=48000;
		   break;
	  case 7:
           myFormat->bitRate=56000;
		   break;
	  case 8:
           myFormat->bitRate=64000;
		   break;
	  case 9:
           myFormat->bitRate=96000;
		   break;
	  case 10:
           myFormat->bitRate=112000;
		   break;
	  case 11:
           myFormat->bitRate=128000;
		   break;
	  case 12:
           myFormat->bitRate=160000;
		   break;
	  case 13:
           myFormat->bitRate=192000;
		   break;
	  case 14:
           myFormat->bitRate=256000;
		   break;
	 }
     myFormat->bandWidth=(dwOptions>>1)&0x0000ffff;
     if(!myFormat->bandWidth)
	  myFormat->bandWidth=lSamprate/2;

	 if(!faacEncSetConfiguration(hEncoder, myFormat))
	 {
      MessageBox(0, "Unsupported parameters", "FAAC interface", MB_OK);
      faacEncClose(hEncoder);
	  fclose(outfile);
	  free(bitbuf);
      GlobalFree(hOutput);
	  return 0;
	 }
	}

// init flushing process
    bytesEncoded=faacEncEncode(hEncoder, 0, 0, bitbuf, maxBytesOutput); // initializes the flushing process
    if(bytesEncoded>0)
	 fwrite(bitbuf, 1, bytesEncoded, outfile);

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
