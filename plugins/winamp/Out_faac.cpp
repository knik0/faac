/*
FAAC - encoder plugin for Winamp 2
Copyright (C) 2002 Antonio Foranna

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation.
	
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
		
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
			
The author can be contacted at:
kreel@interfree.it
*/

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>  // FILE *
#include "resource.h"
#include "out.h"
#include <mp4.h>
#include <faac.h>
#include "CRegistry.h"
#include "defines.h"

#define RAW  0
#define ADTS 1

#define FREE(ptr) \
{ \
	if(ptr) \
		free(ptr); \
	ptr=0; \
}



void Config(HWND);
void About(HWND);
void Init();
void Quit();
int Open(int, int, int, int, int);
void Close();
int Write(char*, int);
int CanWrite();
int IsPlaying();
int Pause(int);
void SetVolume(int);
void SetPan(int);
void Flush(int);
int GetOutputTime();
int GetWrittenTime();



typedef struct output_tag  // any special vars associated with output file
{
FILE  *aacFile;         
DWORD lSize;
long  lSamprate;
WORD  wBitsPerSample;
WORD  wChannels;
// DWORD dwDataOffset;
//BOOL  bWrittenHeader;
char  szNAME[256];

faacEncHandle hEncoder;
int32_t			*buffer;
unsigned char	*bitbuf;
DWORD maxBytesOutput;
long  samplesInput;
BYTE  bStopEnc;

unsigned char *bufIn;
DWORD full_size; // size of decoded file needed to set the length of progress bar
DWORD tagsize;
DWORD bytes_read;		// from file
DWORD bytes_consumed;	// by faadDecDecode
DWORD bytes_into_buffer;
DWORD bytes_Enc;
} MYOUTPUT;



typedef struct mc
{
bool					AutoCfg;
bool					UseQuality;
faacEncConfiguration	EncCfg;
char					OutDir[MAX_PATH];
} MY_ENC_CFG;



static MYOUTPUT mo0,
				*mo=&mo0; // this is done to copy'n'paste code from CoolEdit plugin
static			HBITMAP hBmBrowse=NULL;
char			config_AACoutdir[MAX_PATH]="";
static int		srate, numchan, bps;
volatile int	writtentime, w_offset;
static int		last_pause=0;


Out_Module out = {
	OUT_VER,
	APP_NAME " " APP_VER,
	NULL,
    NULL, // hmainwindow
    NULL, // hdllinstance
    Config,
    About,
    Init,
    Quit,
    Open,
    Close,
    Write,
    CanWrite,
    IsPlaying,
    Pause,
    SetVolume,
    SetPan,
    Flush,
    GetWrittenTime,
    GetWrittenTime
};



// *********************************************************************************************



Out_Module *winampGetOutModule()
{
	return &out;
}
// *********************************************************************************************

BOOL WINAPI DllMain (HANDLE hInst, DWORD ulReason, LPVOID lpReserved)
{
   switch(ulReason)
   {
      case DLL_PROCESS_ATTACH:
           DisableThreadLibraryCalls((struct HINSTANCE__ *)hInst);
           if(!hBmBrowse)
            hBmBrowse=LoadBitmap((struct HINSTANCE__ *)hInst, MAKEINTRESOURCE(IDB_BROWSE));
         /* Code from LibMain inserted here.  Return TRUE to keep the
            DLL loaded or return FALSE to fail loading the DLL.
 
            You may have to modify the code in your original LibMain to
            account for the fact that it may be called more than once.
            You will get one DLL_PROCESS_ATTACH for each process that
            loads the DLL. This is different from LibMain which gets
            called only once when the DLL is loaded. The only time this
            is critical is when you are using shared data sections.
            If you are using shared data sections for statically
            allocated data, you will need to be careful to initialize it
            only once. Check your code carefully.
 
            Certain one-time initializations may now need to be done for
            each process that attaches. You may also not need code from
            your original LibMain because the operating system may now
            be doing it for you.
         */
         break;
 
      case DLL_THREAD_ATTACH:
         /* Called each time a thread is created in a process that has
            already loaded (attached to) this DLL. Does not get called
            for each thread that exists in the process before it loaded
            the DLL.
 
            Do thread-specific initialization here.
         */
         break;
 
      case DLL_THREAD_DETACH:
         /* Same as above, but called when a thread in the process
            exits.
 
            Do thread-specific cleanup here.
         */
         break;
 
      case DLL_PROCESS_DETACH:
           if(hBmBrowse)
           {
            DeleteObject(hBmBrowse);
            hBmBrowse=NULL;
           }
         /* Code from _WEP inserted here.  This code may (like the
            LibMain) not be necessary.  Check to make certain that the
            operating system is not doing it for you.
         */
         break;
   }
 
   /* The return value is only used for DLL_PROCESS_ATTACH; all other
      conditions are ignored.  */
   return TRUE;   // successful DLL_PROCESS_ATTACH
}
// *********************************************************************************************

#define SWAP32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) \
	| ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24))
#define SWAP16(x) (((x & 0xff) << 8) | ((x & 0xff00) >> 8))

inline void To32bit(int32_t *buf, BYTE *bufi, int size, BYTE samplebytes, BYTE bigendian)
{
int i;

	switch(samplebytes)
	{
	case 1:
		// this is endian clean
		for (i = 0; i < size; i++)
			buf[i] = (bufi[i] - 128) * 65536;
		break;
		
	case 2:
#ifdef WORDS_BIGENDIAN
		if (!bigendian)
#else
			if (bigendian)
#endif
			{
				// swap bytes
				for (i = 0; i < size; i++)
				{
					int16_t s = ((int16_t *)bufi)[i];
					
					s = SWAP16(s);
					
					buf[i] = ((u_int32_t)s) << 8;
				}
			}
			else
			{
				// no swap
				for (i = 0; i < size; i++)
				{
					int s = ((int16_t *)bufi)[i];
					
					buf[i] = s << 8;
				}
			}
			break;
			
	case 3:
		if (!bigendian)
		{
			for (i = 0; i < size; i++)
			{
				int s = bufi[3 * i] | (bufi[3 * i + 1] << 8) | (bufi[3 * i + 2] << 16);
				
				// fix sign
				if (s & 0x800000)
					s |= 0xff000000;
				
				buf[i] = s;
			}
		}
		else // big endian input
		{
			for (i = 0; i < size; i++)
			{
				int s = (bufi[3 * i] << 16) | (bufi[3 * i + 1] << 8) | bufi[3 * i + 2];
				
				// fix sign
				if (s & 0x800000)
					s |= 0xff000000;
				
				buf[i] = s;
			}
		}
		break;
		
	case 4:		
#ifdef WORDS_BIGENDIAN
		if (!bigendian)
#else
			if (bigendian)
#endif
			{
				// swap bytes
				for (i = 0; i < size; i++)
				{
					int s = bufi[i];
					
					buf[i] = SWAP32(s);
				}
			}
			else
				memcpy(buf,bufi,size*sizeof(u_int32_t));
		/*
		int exponent, mantissa;
		float *bufo=(float *)buf;
			
			for (i = 0; i < size; i++)
			{
				exponent=bufi[(i<<2)+3]<<1;
				if(bufi[i*4+2] & 0x80)
					exponent|=0x01;
				exponent-=126;
				mantissa=(DWORD)bufi[(i<<2)+2]<<16;
				mantissa|=(DWORD)bufi[(i<<2)+1]<<8;
				mantissa|=bufi[(i<<2)];
				bufo[i]=(float)ldexp(mantissa,exponent);
			}*/
			break;
	}
}
// *********************************************************************************************

static int CALLBACK WINAPI BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SetWindowText(hwnd,"Select Directory");
		SendMessage(hwnd,BFFM_SETSELECTION,(WPARAM)1,(LPARAM)config_AACoutdir);
	}
	return 0;
}

void ReadCfgEnc(MY_ENC_CFG *cfg) 
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME))
	{
		cfg->AutoCfg=reg.getSetRegDword("Auto",true) ? true : false; 
		cfg->EncCfg.mpegVersion=reg.getSetRegDword("MPEG version",MPEG2); 
		cfg->EncCfg.aacObjectType=reg.getSetRegDword("Profile",LOW); 
		cfg->EncCfg.allowMidside=reg.getSetRegDword("MidSide",true); 
		cfg->EncCfg.useTns=reg.getSetRegDword("TNS",true); 
		cfg->EncCfg.useLfe=reg.getSetRegDword("LFE",false);
		cfg->UseQuality=reg.getSetRegDword("Use quality",false) ? true : false;
		cfg->EncCfg.quantqual=reg.getSetRegDword("Quality",100); 
		cfg->EncCfg.bitRate=reg.getSetRegDword("BitRate",128000); 
		cfg->EncCfg.bandWidth=reg.getSetRegDword("BandWidth",0); 
		cfg->EncCfg.outputFormat=reg.getSetRegDword("Header",1); 
		reg.getSetRegStr("OutDir","",cfg->OutDir,MAX_PATH); 
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

void WriteCfgEnc(MY_ENC_CFG *cfg) 
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME))
	{
		reg.setRegDword("Auto",cfg->AutoCfg); 
		reg.setRegDword("MPEG version",cfg->EncCfg.mpegVersion); 
		reg.setRegDword("Profile",cfg->EncCfg.aacObjectType); 
		reg.setRegDword("MidSide",cfg->EncCfg.allowMidside); 
		reg.setRegDword("TNS",cfg->EncCfg.useTns); 
		reg.setRegDword("LFE",cfg->EncCfg.useLfe); 
		reg.setRegDword("Use quality",cfg->UseQuality); 
		reg.setRegDword("Quality",cfg->EncCfg.quantqual); 
		reg.setRegDword("BitRate",cfg->EncCfg.bitRate); 
		reg.setRegDword("BandWidth",cfg->EncCfg.bandWidth); 
		reg.setRegDword("Header",cfg->EncCfg.outputFormat); 
		reg.setRegStr("OutDir",cfg->OutDir); 
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}

#define INIT_CB(hWnd,nID,list,IdSelected) \
{ \
	for(int i=0; list[i]; i++) \
		SendMessage(GetDlgItem(hWnd, nID), CB_ADDSTRING, 0, (LPARAM)list[i]); \
	SendMessage(GetDlgItem(hWnd, nID), CB_SETCURSEL, IdSelected, 0); \
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

static BOOL CALLBACK DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
	case WM_INITDIALOG:
		{
		char buf[50];
		char *Quality[]={"Default","10","20","30","40","50","60","70","80","90","100","110","120","130","140","150","200","300","400","500",0};
		char *BitRate[]={"Auto","8","18","20","24","32","40","48","56","64","96","112","128","160","192","224","256","320","384",0};
		char *BandWidth[]={"Auto","Full","4000","8000","11025","16000","22050","24000","32000","44100","48000",0};
		MY_ENC_CFG cfg;
			
			ReadCfgEnc(&cfg);
			
			INIT_CB(hWndDlg,IDC_CB_QUALITY,Quality,0);
			INIT_CB(hWndDlg,IDC_CB_BITRATE,BitRate,0);
			INIT_CB(hWndDlg,IDC_CB_BANDWIDTH,BandWidth,0);
			
			SendMessage(GetDlgItem(hWndDlg, IDC_BTN_BROWSE), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM) hBmBrowse);
			if(!*cfg.OutDir)
				GetCurrentDirectory(MAX_PATH,cfg.OutDir);
			SetDlgItemText(hWndDlg, IDC_E_BROWSE, cfg.OutDir);
			
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
			case RAW:
				CheckDlgButton(hWndDlg,IDC_RADIO_RAW,TRUE);
				break;
			case ADTS:
				CheckDlgButton(hWndDlg,IDC_RADIO_ADTS,TRUE);
				break;
			}
			
			CheckDlgButton(hWndDlg, IDC_CHK_ALLOWMIDSIDE, cfg.EncCfg.allowMidside);
			CheckDlgButton(hWndDlg, IDC_CHK_USETNS, cfg.EncCfg.useTns);
			CheckDlgButton(hWndDlg, IDC_CHK_USELFE, cfg.EncCfg.useLfe);

			if(cfg.UseQuality)
				CheckDlgButton(hWndDlg,IDC_RADIO_QUALITY,TRUE);
			else
				CheckDlgButton(hWndDlg,IDC_RADIO_BITRATE,TRUE);

			switch(cfg.EncCfg.quantqual)
			{
			case 100:
				SendMessage(GetDlgItem(hWndDlg, IDC_CB_QUALITY), CB_SETCURSEL, 0, 0);
				break;
			default:
				if(cfg.EncCfg.quantqual<10)
					cfg.EncCfg.quantqual=10;
				if(cfg.EncCfg.quantqual>500)
					cfg.EncCfg.quantqual=500;
				sprintf(buf,"%lu",cfg.EncCfg.quantqual);
				SetDlgItemText(hWndDlg, IDC_CB_QUALITY, buf);
				break;
			}
			switch(cfg.EncCfg.bitRate)
			{
			case 0:
				SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_SETCURSEL, 0, 0);
				break;
			default:
				sprintf(buf,"%lu",cfg.EncCfg.bitRate);
				SetDlgItemText(hWndDlg, IDC_CB_BITRATE, buf);
				break;
			}
			switch(cfg.EncCfg.bandWidth)
			{
			case 0:
				SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, 0, 0);
				break;
			case 0xffffffff:
				SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, 1, 0);
				break;
			default:
				sprintf(buf,"%lu",cfg.EncCfg.bandWidth);
				SetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, buf);
				break;
			}
			
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
			char buf[50];
			MY_ENC_CFG cfg;
				
				cfg.AutoCfg=IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG) ? TRUE : FALSE;
				cfg.EncCfg.mpegVersion=IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4) ? MPEG4 : MPEG2;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MAIN))
					cfg.EncCfg.aacObjectType=MAIN;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
					cfg.EncCfg.aacObjectType=LOW;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_SSR))
					cfg.EncCfg.aacObjectType=SSR;
				if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LTP))
					cfg.EncCfg.aacObjectType=LTP;
				cfg.EncCfg.allowMidside=IsDlgButtonChecked(hWndDlg, IDC_CHK_ALLOWMIDSIDE);
				cfg.EncCfg.useTns=IsDlgButtonChecked(hWndDlg, IDC_CHK_USETNS);
				cfg.EncCfg.useLfe=IsDlgButtonChecked(hWndDlg, IDC_CHK_USELFE);
				
				GetDlgItemText(hWndDlg, IDC_CB_BITRATE, buf, 50);
				switch(*buf)
				{
				case 'A': // Auto
					cfg.EncCfg.bitRate=0;
					break;
				default:
					cfg.EncCfg.bitRate=1000*GetDlgItemInt(hWndDlg, IDC_CB_BITRATE, 0, FALSE);
				}
				GetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, buf, 50);
				switch(*buf)
				{
				case 'A': // Auto
					cfg.EncCfg.bandWidth=0;
					break;
				case 'F': // Full
					cfg.EncCfg.bandWidth=0xffffffff;
					break;
				default:
					cfg.EncCfg.bandWidth=GetDlgItemInt(hWndDlg, IDC_CB_BANDWIDTH, 0, FALSE);
				}
				cfg.UseQuality=IsDlgButtonChecked(hWndDlg,IDC_RADIO_QUALITY) ? TRUE : FALSE;
				GetDlgItemText(hWndDlg, IDC_CB_QUALITY, buf, 50);
				switch(*buf)
				{
				case 'D': // Default
					cfg.EncCfg.quantqual=100;
					break;
				default:
					cfg.EncCfg.quantqual=GetDlgItemInt(hWndDlg, IDC_CB_QUALITY, 0, FALSE);
				}
				cfg.EncCfg.outputFormat=IsDlgButtonChecked(hWndDlg,IDC_RADIO_RAW) ? RAW : ADTS;
				GetDlgItemText(hWndDlg, IDC_E_BROWSE, cfg.OutDir, MAX_PATH);
				
				WriteCfgEnc(&cfg);
				EndDialog(hWndDlg, (DWORD)hCfg);
			}
			break;
			
        case IDCANCEL:
			// Ignore data values entered into the controls        
			// and dismiss the dialog window returning FALSE
			EndDialog(hWndDlg, FALSE);
			break;
			
		case IDC_BTN_BROWSE:
			{
			char name[MAX_PATH];
			BROWSEINFO bi;
			ITEMIDLIST *idlist;
				bi.hwndOwner = hWndDlg;
				bi.pidlRoot = 0;
				bi.pszDisplayName = name;
				bi.lpszTitle = "Select a directory for AAC-MPEG4 file output:";
				bi.ulFlags = BIF_RETURNONLYFSDIRS;
				bi.lpfn = BrowseCallbackProc;
				bi.lParam = 0;
				
				GetDlgItemText(hWndDlg, IDC_E_BROWSE, config_AACoutdir, MAX_PATH);
				idlist = SHBrowseForFolder( &bi );
				if(idlist)
				{
					SHGetPathFromIDList( idlist, config_AACoutdir);
					SetDlgItemText(hWndDlg, IDC_E_BROWSE, config_AACoutdir);
				}
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

void Config(HWND hWnd)
{
	DialogBox(out.hDllInstance, MAKEINTRESOURCE(IDD_ENCODER), hWnd, DIALOGMsgProc);
//	dwOptions=DialogBoxParam((HINSTANCE)out.hDllInstance,(LPCSTR)MAKEINTRESOURCE(IDD_ENCODER), (HWND)hWnd, (DLGPROC)DIALOGMsgProc, dwOptions);
}
// *********************************************************************************************

void About(HWND hwnd)
{
char buf[256];
#ifndef FAACENC_VERSION
	sprintf(buf,
			APP_NAME " %s by Antonio Foranna\n\n"
			"This plugin uses FAAC encoder engine\n\n"
			"Compiled on %s\n",
			 APP_VER,
			 __DATE__
			 );
#elif
	sprintf(buf,
			APP_NAME " %s by Antonio Foranna\n\n"
			"This plugin uses FAAC encoder engine v%g\n\n"
			"Compiled on %s\n",
			 APP_VER,
			 FAACENC_VERSION,
			 __DATE__
			 );
#endif
	MessageBox(hwnd, buf, "About", MB_OK);
}
// *********************************************************************************************

void Init()
{
}
// *********************************************************************************************

void Quit()
{
}
// *********************************************************************************************

static char *scanstr_back(char *str, char *toscan, char *defval)
{
char *s=str+strlen(str)-1;

	if (strlen(str) < 1) return defval;
	if (strlen(toscan) < 1) return defval;
	while (1)
	{
		char *t=toscan;
		while (*t)
			if (*t++ == *s) return s;
		t=CharPrev(str,s);
		if (t==s) return defval;
		s=t;
	}
}

void GetNewFileName(char *lpstrFilename)
{
char temp2[MAX_PATH];
char *t,*p;

	GetWindowText(out.hMainWindow,temp2,sizeof(temp2));
	t=temp2;
	
	t=scanstr_back(temp2,"-",NULL);
	if (t) t[-1]=0;
	
	if (temp2[0] && temp2[1] == '.')
	{
		char *p1,*p2;
		p1=lpstrFilename;
		p2=temp2;
		while (*p2) *p1++=*p2++;
		*p1=0;
		p1 = temp2+1;
		p2 = lpstrFilename;
		while (*p2)
		{
			*p1++ = *p2++;
		}
		*p1=0;
		temp2[0] = '0';
	}
	p=temp2;
	while (*p != '.' && *p) p++;
	if (*p == '.') 
	{
		*p = '-';
		p=CharNext(p);
	}
	while (*p)
	{
		if (*p == '.' || *p == '/' || *p == '\\' || *p == '*' || 
			*p == '?' || *p == ':' || *p == '+' || *p == '\"' || 
			*p == '\'' || *p == '|' || *p == '<' || *p == '>') *p = '_';
		p=CharNext(p);
	}
	
	p=config_AACoutdir;
	if (p[0]) while (p[1]) p++;
	
	if (!config_AACoutdir[0] || config_AACoutdir[0] == ' ')
		Config(out.hMainWindow);
	if (!config_AACoutdir[0])
		wsprintf(lpstrFilename,"%s.aac",temp2);
	else if (p[0]=='\\')
		wsprintf(lpstrFilename,"%s%s.aac",config_AACoutdir,temp2);
	else
		wsprintf(lpstrFilename,"%s\\%s.aac",config_AACoutdir,temp2);
}

#define ERROR_O(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, "FAAC plugin", MB_OK); \
	Close(); \
	return -1; \
}

int Open(int lSamprate, int wChannels, int wBitsPerSample, int bufferlenms, int prebufferms)
{
MY_ENC_CFG			cfg;
DWORD			maxBytesOutput;
unsigned long	samplesInput;
int				bytesEncoded;
int				tmp;
char			lpstrFilename[MAX_PATH];

	w_offset = writtentime = 0;
	numchan = wChannels;
	srate = lSamprate;
	bps = wBitsPerSample;

	ReadCfgEnc(&cfg);

	strcpy(config_AACoutdir,cfg.OutDir);
	GetNewFileName(lpstrFilename);

	memset(mo,0,sizeof(MYOUTPUT));
	
	// open the aac output file
	if(!(mo->aacFile=fopen(lpstrFilename, "wb")))
		ERROR_O("Can't create file");
	
	// use bufferized stream
	setvbuf(mo->aacFile,NULL,_IOFBF,32767);

	// open the encoder library
	if(!(mo->hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
		ERROR_O("Can't init library");
	
	if(!(mo->bitbuf=(unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char))))
		ERROR_O("Memory allocation error: output buffer");
	
	if(!(mo->bufIn=(unsigned char*)malloc(samplesInput*(wBitsPerSample>>3))))
		ERROR_O("Memory allocation error: input buffer");

	if(!(mo->buffer=(int32_t *)malloc(samplesInput*sizeof(int32_t))))
		ERROR_O("Memory allocation error: input buffer");
	
//	ReadCfgEnc(&cfg);
	if(!cfg.AutoCfg)
	{
	faacEncConfigurationPtr myFormat=&cfg.EncCfg;
	faacEncConfigurationPtr CurFormat=faacEncGetCurrentConfiguration(mo->hEncoder);
		
		if(cfg.UseQuality)
		{
			myFormat->quantqual=cfg.EncCfg.quantqual;
			myFormat->bitRate=CurFormat->bitRate;
		}
		else
			if(!myFormat->bitRate)
				myFormat->bitRate=CurFormat->bitRate;
		
		switch(myFormat->bandWidth)
		{
		case 0:
			myFormat->bandWidth=CurFormat->bandWidth;
			break;
		case 0xffffffff:
			myFormat->bandWidth=lSamprate/2;
			break;
		default: break;
		}
		
		if(!faacEncSetConfiguration(mo->hEncoder, myFormat))
			ERROR_O("Unsupported parameters");
	}
	
	mo->lSamprate=lSamprate;
	mo->wBitsPerSample=wBitsPerSample;
	mo->wChannels=wChannels;
	strcpy(mo->szNAME,lpstrFilename);
	
	mo->maxBytesOutput=maxBytesOutput;
	mo->samplesInput=samplesInput;
	mo->bStopEnc=0;
	
	// init flushing process
    bytesEncoded=faacEncEncode(mo->hEncoder, 0, 0, mo->bitbuf, maxBytesOutput); // initializes the flushing process
    if(bytesEncoded>0)
	{
		tmp=fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile);
		if(tmp!=bytesEncoded)
			ERROR_O("fwrite()");
	}
	
	return 0;
}
// *********************************************************************************************

void Close()
{
	if(mo->bytes_into_buffer)
	{
	int bytesEncoded;
	int32_t *buf=mo->buffer;

		To32bit(buf,mo->bufIn,mo->samplesInput,mo->wBitsPerSample>>3,false);

		// call the actual encoding routine
		bytesEncoded=faacEncEncode(mo->hEncoder, (int32_t *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
		if(bytesEncoded>0)
			fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile);
	}
	
	if(mo->aacFile)
	{
		fclose(mo->aacFile);
		mo->aacFile=0;
	}
	
	if(mo->hEncoder)
		faacEncClose(mo->hEncoder);
	
	FREE(mo->bitbuf)
	FREE(mo->bufIn)
	FREE(mo->buffer)
	
//	CloseHandle(outfile);
}
// *********************************************************************************************

#define ERROR_W(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, "FAAC plugin", MB_OK); \
    mo->bStopEnc=1; \
	return -1; \
}

int Write(char *wabuf, int len)
{
int32_t *buf=mo->buffer;
BYTE	InputSize=mo->wBitsPerSample>>3;
int bytesWritten;
int bytesEncoded;
int shift=0;

	writtentime += len;

	if(!mo->bStopEnc)
		do
		{
			if(mo->bytes_into_buffer+len<mo->samplesInput*InputSize)
			{
				memcpy(mo->bufIn+mo->bytes_into_buffer, wabuf, len);
				mo->bytes_into_buffer+=len;
				len=0;
			}
			else
			{
				shift=mo->samplesInput*InputSize-mo->bytes_into_buffer;
				memcpy(mo->bufIn+mo->bytes_into_buffer, wabuf, shift);
				mo->bytes_into_buffer+=shift;
				wabuf+=shift;
				len-=shift;
				
				To32bit(buf,mo->bufIn,mo->samplesInput,InputSize,false);

				// call the actual encoding routine
				bytesEncoded=faacEncEncode(mo->hEncoder, (int32_t *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);

				mo->bytes_into_buffer=0;
				if(bytesEncoded>0)
				{
					// write bitstream to aac file 
					bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile);
					if(bytesWritten!=bytesEncoded)
						ERROR_W("bytesWritten and bytesEncoded are different");
				}
				else
					if(bytesEncoded<0)
						ERROR_W("faacEncEncode()");
			}
		}while(len);

	Sleep(10);
	return 0;
}
// *********************************************************************************************

int CanWrite()
{
	return last_pause ? 0 : 16*1024*1024;
//	return last_pause ? 0 : mo->samplesInput*(mo->wBitsPerSample>>3);
}
// *********************************************************************************************

int IsPlaying()
{
	return 0;
}
// *********************************************************************************************

int Pause(int pause)
{
	int t=last_pause;
	last_pause=pause;
	return t;
}
// *********************************************************************************************

void SetVolume(int volume)
{
}
// *********************************************************************************************

void SetPan(int pan)
{
}
// *********************************************************************************************

void Flush(int t)
{
int a;

	  w_offset=0;
	  a = t - GetWrittenTime();
	  w_offset=a;
}
// *********************************************************************************************
	
int GetWrittenTime()
{
int t=srate*numchan,l;
int ms=writtentime;

	l=ms%t;
	ms /= t;
	ms *= 1000;
	ms += (l*1000)/t;
	if (bps == 16) ms/=2;

	return ms + w_offset;
}
