#include <windows.h>
#include <stdio.h>  // FILE *
#include "filters.h" //CoolEdit
#include "resource.h"
#include "faac.h"
#include "registry.h"



#define PI_VER "v2.0 beta2"
#define REGISTRY_PROGRAM_NAME "SOFTWARE\\4N\\CoolEdit\\AAC-MPEG4"



typedef struct output_tag  // any special vars associated with output file
{
FILE			*fFile;
DWORD			lSize;
long			lSamprate;
WORD			wBitsPerSample;
WORD			wChannels;
char			szNAME[256];

faacEncHandle	hEncoder;
unsigned char	*bitbuf;
DWORD			maxBytesOutput;
long			samplesInput;
BYTE			bStopEnc;
} MYOUTPUT;



typedef struct mc
{
bool					AutoCfg;
faacEncConfiguration	EncCfg;
} MYCFG;



// *********************************************************************************************



void RD_Cfg(MYCFG *cfg) 
{ 
registryClass *reg=new registryClass();

	reg->openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME);
	cfg->AutoCfg=reg->getSetRegDword("Auto",true) ? true : false; 
	cfg->EncCfg.mpegVersion=reg->getSetRegDword("MPEG version",MPEG2); 
	cfg->EncCfg.aacObjectType=reg->getSetRegDword("Profile",LOW); 
	cfg->EncCfg.allowMidside=reg->getSetRegDword("MidSide",true); 
	cfg->EncCfg.useTns=reg->getSetRegDword("TNS",true); 
	cfg->EncCfg.useLfe=reg->getSetRegDword("LFE",false);
	cfg->EncCfg.bitRate=reg->getSetRegDword("BitRate",128000); 
	cfg->EncCfg.bandWidth=reg->getSetRegDword("BandWidth",0); 
	cfg->EncCfg.outputFormat=reg->getSetRegDword("Header",1); 
	delete reg;
}

void WR_Cfg(MYCFG *cfg) 
{ 
registryClass *reg=new registryClass();

	reg->openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME);
	reg->setRegDword("Auto",cfg->AutoCfg); 
	reg->setRegDword("MPEG version",cfg->EncCfg.mpegVersion); 
	reg->setRegDword("Profile",cfg->EncCfg.aacObjectType); 
	reg->setRegDword("MidSide",cfg->EncCfg.allowMidside); 
	reg->setRegDword("TNS",cfg->EncCfg.useTns); 
	reg->setRegDword("LFE",cfg->EncCfg.useLfe); 
	reg->setRegDword("BitRate",cfg->EncCfg.bitRate); 
	reg->setRegDword("BandWidth",cfg->EncCfg.bandWidth); 
	reg->setRegDword("Header",cfg->EncCfg.outputFormat); 
	delete reg;
}

#define DISABLE_LTP \
{ \
	if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG2) && \
	   IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP)) \
	{ \
		CheckDlgButton(hWndDlg,IDC_RADIO_LTP,FALSE); \
		CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE); \
	} \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), FALSE); \
}

//        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
//        EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
#define DISABLE_CTRL(Enabled) \
{ \
		CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, !Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG4), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG2), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_RAW), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_ADTS), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_ALLOWMIDSIDE), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_USETNS), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled); \
        EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled); \
		if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4)) \
			EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled); \
		else \
			DISABLE_LTP \
}

__declspec(dllexport) BOOL FAR PASCAL DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
//DWORD dwOptions=(DWORD)lParam;


	switch(Message)
	{
	case WM_INITDIALOG:
		{
		char buf[10];
		MYCFG cfg;

			RD_Cfg(&cfg);

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
//       SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, 0, 0);

			if(cfg.EncCfg.mpegVersion==MPEG4)
				CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
			else
				CheckDlgButton(hWndDlg,IDC_RADIO_MPEG2,TRUE);

			switch(cfg.EncCfg.aacObjectType)
			{
			case MAIN:
				CheckDlgButton(hWndDlg,IDC_RADIO_MAIN,TRUE);
				break;
			case LOW:
				CheckDlgButton(hWndDlg,IDC_RADIO_LOW,TRUE);
				break;
			case SSR:
				CheckDlgButton(hWndDlg,IDC_RADIO_SSR,TRUE);
				break;
			case LTP:
				CheckDlgButton(hWndDlg,IDC_RADIO_LTP,TRUE);
				DISABLE_LTP
				break;
			}

			switch(cfg.EncCfg.outputFormat)
			{
			case 0:
              CheckDlgButton(hWndDlg,IDC_RADIO_RAW,TRUE);
		      break;
			case 1:
              CheckDlgButton(hWndDlg,IDC_RADIO_ADTS,TRUE);
		      break;
			}

			CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, cfg.EncCfg.allowMidside);
			CheckDlgButton(hWndDlg, IDC_USETNS, cfg.EncCfg.useTns);
			CheckDlgButton(hWndDlg, IDC_USELFE, cfg.EncCfg.useLfe);
			sprintf(buf,"%lu",cfg.EncCfg.bitRate);
			SetDlgItemText(hWndDlg, IDC_CB_BITRATE, buf);
			sprintf(buf,"%lu",cfg.EncCfg.bandWidth);
			SetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, buf);

			CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, cfg.AutoCfg);

			DISABLE_CTRL(!cfg.AutoCfg);
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
				DISABLE_CTRL(Enabled);
			}
			break;

		case IDOK:
			{
			HANDLE hCfg=(HANDLE)lParam;
			MYCFG far *cfg=0;

				if(!(hCfg=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE,sizeof(MYCFG))))
					return TRUE;
				cfg=(MYCFG *)GlobalLock(hCfg);

				cfg->AutoCfg=IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG) ? TRUE : FALSE;
				cfg->EncCfg.mpegVersion=IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4) ? MPEG4 : MPEG2;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MAIN))
					cfg->EncCfg.aacObjectType=MAIN;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
					cfg->EncCfg.aacObjectType=LOW;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_SSR))
					cfg->EncCfg.aacObjectType=SSR;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP))
					cfg->EncCfg.aacObjectType=LTP;
				cfg->EncCfg.allowMidside=IsDlgButtonChecked(hWndDlg, IDC_ALLOWMIDSIDE);
				cfg->EncCfg.useTns=IsDlgButtonChecked(hWndDlg, IDC_USETNS);
				cfg->EncCfg.useLfe=IsDlgButtonChecked(hWndDlg, IDC_USELFE);
				cfg->EncCfg.bitRate=1000*GetDlgItemInt(hWndDlg, IDC_CB_BITRATE, 0, FALSE);
				cfg->EncCfg.bandWidth=GetDlgItemInt(hWndDlg, IDC_CB_BANDWIDTH, 0, FALSE);
				cfg->EncCfg.outputFormat=IsDlgButtonChecked(hWndDlg,IDC_RADIO_RAW) ? 0 : 1;
				WR_Cfg(cfg);

				GlobalUnlock(hCfg);
//				dwOptions=(DWORD)hCfg;

				EndDialog(hWndDlg, (DWORD)hCfg);
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
 			  sprintf(buf,	"AAC-MPEG4 plugin %s by 4N\n"
							"This plugin uses FAAC encoder engine v%g and FAAD2 decoder engine\n\n"
							"Compiled on %s\n",
							PI_VER,
							FAACENC_VERSION,
							__DATE__
						  );
		      MessageBox(hWndDlg, buf, "About", MB_OK);
             }
             break;

		case IDC_RADIO_MPEG4:
	         EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), !IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG));
			 break;

		case IDC_RADIO_MPEG2:
	         EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), FALSE);
			 DISABLE_LTP
			 break;
		}
		break; // End of WM_COMMAND
	default: 
		return FALSE;
	}
 
	return TRUE;
} // End of DIALOGSMsgProc                                      
// *********************************************************************************************

__declspec(dllexport) DWORD FAR PASCAL FilterGetOptions(HWND hWnd, HINSTANCE hInst, long lSamprate, WORD wChannels, WORD wBitsPerSample, DWORD dwOptions) // return 0 if no options box
{
long nDialogReturn=0;
FARPROC lpfnDIALOGMsgProc;
	
 lpfnDIALOGMsgProc=GetProcAddress(hInst,(LPCSTR)MAKELONG(20,0));			
 nDialogReturn=(long)DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)lpfnDIALOGMsgProc, dwOptions);

 return nDialogReturn;
}
// *********************************************************************************************

__declspec(dllexport) DWORD FAR PASCAL FilterWriteFirstSpecialData(HANDLE hInput, 
	SPECIALDATA * psp)
{
 return 0;
}
// *********************************************************************************************

__declspec(dllexport) DWORD FAR PASCAL FilterWriteNextSpecialData(HANDLE hInput, SPECIALDATA * psp)
{	
 return 0;
// only has 1 special data!  Otherwise we would use psp->hSpecialData
// as either a counter to know which item to retrieve next, or as a
// structure with other state information in it.
}
// *********************************************************************************************

__declspec(dllexport) DWORD FAR PASCAL FilterWriteSpecialData(HANDLE hOutput,
	LPCSTR szListType, LPCSTR szType, char * pData,DWORD dwSize)
{
 return 0;
}
// *********************************************************************************************

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
// *********************************************************************************************

#define ERROR_OFO(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, "FAAC plugin", MB_OK); \
	if(hOutput) \
	{ \
		GlobalUnlock(hOutput); \
		CloseFilterOutput(hOutput); \
	} \
	return 0; \
}

__declspec(dllexport) HANDLE FAR PASCAL OpenFilterOutput(LPSTR lpstrFilename,long lSamprate,WORD wBitsPerSample,WORD wChannels,long lSize, long far *lpChunkSize, DWORD dwOptions)
{
HANDLE			hOutput;
MYOUTPUT		*mo;
MYCFG			cfg;
DWORD			maxBytesOutput;
DWORD			samplesInput;
int				bytesEncoded;
int				tmp;

    hOutput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE,sizeof(MYOUTPUT));
    if(!hOutput)
		ERROR_OFO("Memory allocation error: hOutput");
	mo=(MYOUTPUT *)GlobalLock(hOutput);
	memset(mo,0,sizeof(MYOUTPUT));

// open the aac output file 
	if(!(mo->fFile=fopen(lpstrFilename, "wb")))
		ERROR_OFO("Can't create file");

// open the encoder library
	if(!(mo->hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
		ERROR_OFO("Can't init library");

	if(!(mo->bitbuf=(unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char))))
		ERROR_OFO("Memory allocation error: output buffer");

	*lpChunkSize=samplesInput*2;


	RD_Cfg(&cfg);
	if(!cfg.AutoCfg)
	{
    faacEncConfigurationPtr myFormat=&cfg.EncCfg;
//     myFormat=faacEncGetCurrentConfiguration(mo->hEncoder);

		if(!myFormat->bandWidth)
			myFormat->bandWidth=lSamprate/2;

		if(!faacEncSetConfiguration(mo->hEncoder, myFormat))
			ERROR_OFO("Unsupported parameters");
	 }

	mo->fFile=mo->fFile;
	mo->lSize=lSize;
	mo->lSamprate=lSamprate;
	mo->wBitsPerSample=wBitsPerSample;
	mo->wChannels=wChannels;
	strcpy(mo->szNAME,lpstrFilename);

	mo->hEncoder=mo->hEncoder;
    mo->bitbuf=mo->bitbuf;
	mo->maxBytesOutput=maxBytesOutput;
	mo->samplesInput=samplesInput;
	mo->bStopEnc=0;

// init flushing process
    bytesEncoded=faacEncEncode(mo->hEncoder, 0, 0, mo->bitbuf, maxBytesOutput); // initializes the flushing process
    if(bytesEncoded>0)
	{
		tmp=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
		if(tmp!=bytesEncoded)
			ERROR_OFO("fwrite");
	}

	GlobalUnlock(hOutput);

    return hOutput;
}
// *********************************************************************************************

#define ERROR_WFO(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, "FAAC plugin", MB_OK); \
	if(hOutput) \
	{ \
		mo->bStopEnc=1; \
		GlobalUnlock(hOutput); \
	} \
	return 0; \
}

__declspec(dllexport) DWORD FAR PASCAL WriteFilterOutput(HANDLE hOutput, unsigned char far *buf, long lBytes)
{
int bytesWritten;
int bytesEncoded;

 if(hOutput)
 { 
 MYOUTPUT far *mo;
  mo=(MYOUTPUT far *)GlobalLock(hOutput);

  if(!mo->bStopEnc) // Is this case possible?
  {
// call the actual encoding routine
   bytesEncoded=faacEncEncode(mo->hEncoder, (short *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
   if(bytesEncoded<1) // end of flushing process
   {
    if(bytesEncoded<0)
		ERROR_WFO("faacEncEncode");
	bytesWritten=lBytes ? 1 : 0; // bytesWritten==0 stops CoolEdit...
    GlobalUnlock(hOutput);
    return bytesWritten;
   }
// write bitstream to aac file 
   bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
   if(bytesWritten!=bytesEncoded)
		ERROR_WFO("fwrite");

   GlobalUnlock(hOutput);
  }
 }

 return bytesWritten;
}
