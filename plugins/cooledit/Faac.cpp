/*
FAAC - codec plugin for Cooledit
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
kreel@tiscali.it
*/

#include <windows.h>
#include <shellapi.h>	// ShellExecute
#include <stdio.h>		// FILE *
#include <stdlib.h>		// malloc, free
#include "resource.h"
#include "filters.h"	// CoolEdit
#include <mp4.h>		// int32_t, ...
#include <faac.h>
#include <faad.h>		// FAAD2 version
#include <win32_ver.h>	// mpeg4ip version
#include "CRegistry.h"
#include "Defines.h"	// my defines
#include "Structs.h"	// my structs

// *********************************************************************************************

extern HINSTANCE hInst;

// -----------------------------------------------------------------------------------------------

extern BOOL DialogMsgProcDecoder(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam);
extern void ReadCfgDec(MY_DEC_CFG *cfg);
extern void WriteCfgDec(MY_DEC_CFG *cfg);

// *********************************************************************************************

#ifdef	ADTS
#undef	ADTS
#define ADTS 1
#endif

// *********************************************************************************************

typedef struct output_tag  // any special vars associated with output file
{
FILE			*aacFile;
long			Samprate;
WORD			BitsPerSample;
WORD			Channels;
//DWORD			src_size;
//char			*dst_name;		// name of compressed file

faacEncHandle	hEncoder;
int32_t			*buffer;
unsigned char	*bitbuf;
DWORD			maxBytesOutput;
long			samplesInput;
} MYOUTPUT;



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
/*
DWORD PackCfg(MY_ENC_CFG *cfg)
{
DWORD dwOptions=0;

	if(cfg->AutoCfg)
		dwOptions=1<<31;
	dwOptions|=(DWORD)cfg->EncCfg.mpegVersion<<30;
	dwOptions|=(DWORD)cfg->EncCfg.aacObjectType<<28;
	dwOptions|=(DWORD)cfg->EncCfg.allowMidside<<27;
	dwOptions|=(DWORD)cfg->EncCfg.useTns<<26;
	dwOptions|=(DWORD)cfg->EncCfg.useLfe<<25;
	dwOptions|=(DWORD)cfg->EncCfg.outputFormat<<24;
	if(cfg->UseQuality)
		dwOptions|=(((DWORD)cfg->EncCfg.quantqual>>1)&0xff)<<16; // [2,512]
	else
		dwOptions|=(((DWORD)cfg->EncCfg.bitRate>>1)&0xff)<<16; // [2,512]
	if(cfg->UseQuality)
		dwOptions|=1<<15;
	dwOptions|=((DWORD)cfg->EncCfg.bandWidth>>1)&&0x7fff; // [0,65536]

	return dwOptions;
}
// -----------------------------------------------------------------------------------------------

void UnpackCfg(MY_ENC_CFG *cfg, DWORD dwOptions)
{
	cfg->AutoCfg=dwOptions>>31;
	cfg->EncCfg.mpegVersion=(dwOptions>>30)&1;
	cfg->EncCfg.aacObjectType=(dwOptions>>28)&3;
	cfg->EncCfg.allowMidside=(dwOptions>>27)&1;
	cfg->EncCfg.useTns=(dwOptions>>26)&1;
	cfg->EncCfg.useLfe=(dwOptions>>25)&1;
	cfg->EncCfg.outputFormat=(dwOptions>>24)&1;
	cfg->EncCfg.bitRate=((dwOptions>>16)&0xff)<<1;
	cfg->UseQuality=(dwOptions>>15)&1;
	cfg->EncCfg.bandWidth=(dwOptions&0x7fff)<<1;
}*/
// -----------------------------------------------------------------------------------------------

void ReadCfgEnc(MY_ENC_CFG *cfg) 
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME "\\FAAC"))
	{
		cfg->AutoCfg=reg.getSetRegBool("Auto",true);
		cfg->EncCfg.mpegVersion=reg.getSetRegDword("MPEG version",MPEG4); 
		cfg->EncCfg.aacObjectType=reg.getSetRegDword("Profile",LOW); 
		cfg->EncCfg.allowMidside=reg.getSetRegDword("MidSide",true); 
		cfg->EncCfg.useTns=reg.getSetRegDword("TNS",true); 
		cfg->EncCfg.useLfe=reg.getSetRegDword("LFE",false);
		cfg->UseQuality=reg.getSetRegBool("Use quality",false);
		cfg->EncCfg.quantqual=reg.getSetRegDword("Quality",100); 
		cfg->EncCfg.bitRate=reg.getSetRegDword("BitRate",0); 
		cfg->EncCfg.bandWidth=reg.getSetRegDword("BandWidth",0); 
		cfg->EncCfg.outputFormat=reg.getSetRegDword("Header",ADTS); 
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

void WriteCfgEnc(MY_ENC_CFG *cfg) 
{ 
CRegistry reg;

	if(reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME "\\FAAC"))
	{
		reg.setRegBool("Auto",cfg->AutoCfg); 
		reg.setRegDword("MPEG version",cfg->EncCfg.mpegVersion); 
		reg.setRegDword("Profile",cfg->EncCfg.aacObjectType); 
		reg.setRegDword("MidSide",cfg->EncCfg.allowMidside); 
		reg.setRegDword("TNS",cfg->EncCfg.useTns); 
		reg.setRegDword("LFE",cfg->EncCfg.useLfe); 
		reg.setRegBool("Use quality",cfg->UseQuality); 
		reg.setRegDword("Quality",cfg->EncCfg.quantqual); 
		reg.setRegDword("BitRate",cfg->EncCfg.bitRate); 
		reg.setRegDword("BandWidth",cfg->EncCfg.bandWidth); 
		reg.setRegDword("Header",cfg->EncCfg.outputFormat); 
	}
	else
		MessageBox(0,"Can't open registry!",0,MB_OK|MB_ICONSTOP);
}
// -----------------------------------------------------------------------------------------------

#define INIT_CB(hWnd,nID,list,IdSelected) \
{ \
	for(int i=0; list[i]; i++) \
		SendMessage(GetDlgItem(hWnd, nID), CB_ADDSTRING, 0, (LPARAM)list[i]); \
	SendMessage(GetDlgItem(hWnd, nID), CB_SETCURSEL, IdSelected, 0); \
}
// -----------------------------------------------------------------------------------------------

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
// -----------------------------------------------------------------------------------------------

//        EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
//        EnableWindow(GetDlgItem(hWndDlg, IDC_CHK_USELFE), Enabled);
#define DISABLE_CTRL(Enabled) \
{ \
	CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, !Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG4), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG2), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_RAW), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_ADTS), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_CHK_ALLOWMIDSIDE), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_CHK_USETNS), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled); \
	EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled); \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled); \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled); \
    EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled); \
	if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4)) \
		EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled); \
	else \
		DISABLE_LTP \
}
// -----------------------------------------------------------------------------------------------

BOOL DialogMsgProcAbout(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
	case WM_INITDIALOG:
		{
		  char buf[512];
		  unsigned long samplesInput, maxBytesOutput;

		  faacEncHandle hEncoder =
		    faacEncOpen(44100, 2, &samplesInput, &maxBytesOutput);
		  faacEncConfigurationPtr myFormat =
		    faacEncGetCurrentConfiguration(hEncoder);

			sprintf(buf,
					APP_NAME " plugin " APP_VER " by Antonio Foranna\n\n"
					"Engines used:\n"
					"\tlibfaac v%s\n"
					"\tFAAD2 v" FAAD2_VERSION "\n"
					"\t" PACKAGE " v" VERSION "\n\n"
					"This code is given with FAAC package and does not contain executables.\n"
					"This program is free software and can be distributed/modifyed under the terms of the GNU General Public License.\n"
					"This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY.\n\n"
					"Compiled on %s\n",
				(myFormat->version == FAAC_CFG_VERSION)
				? myFormat->name : " bad version",
					__DATE__
					);
			SetDlgItemText(hWndDlg, IDC_L_ABOUT, buf);
			faacEncClose(hEncoder);
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hWndDlg, 0);
			break;
		case IDC_AUDIOCODING:
			ShellExecute(hWndDlg, NULL, "http://www.audiocoding.com", NULL, NULL, SW_SHOW);
			break;
		case IDC_MPEG4IP:
			ShellExecute(hWndDlg, NULL, "http://www.mpeg4ip.net", NULL, NULL, SW_SHOW);
			break;
		case IDC_EMAIL:
			ShellExecute(hWndDlg, NULL, "mailto:kreel@tiscali.it", NULL, NULL, SW_SHOW);
			break;
		}
		break;
	default: 
		return FALSE;
	}

	return TRUE;
}

// -----------------------------------------------------------------------------------------------

BOOL DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
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
		PostMessage(hWndDlg, WM_COMMAND, IDCANCEL, 0);
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
					cfg.EncCfg.bitRate=GetDlgItemInt(hWndDlg, IDC_CB_BITRATE, 0, FALSE);
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

				WriteCfgEnc(&cfg);

				EndDialog(hWndDlg, 1);
			}
			break;

        case IDCANCEL:
			// Ignore data values entered into the controls        
			// and dismiss the dialog window returning FALSE
			EndDialog(hWndDlg, FALSE);
			break;

		case IDC_BTN_ABOUT:
			DialogBox((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_ABOUT), (HWND)hWndDlg, (DLGPROC)DialogMsgProcAbout);
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
}

// *********************************************************************************************
// *********************************************************************************************
// *********************************************************************************************

#define ERROR_FGO(msg) \
{ \
	if(msg) \
		MessageBox(hWnd, msg, APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
	return 0; \
}
// -----------------------------------------------------------------------------------------------

DWORD FAR PASCAL FilterGetOptions(HWND hWnd, HINSTANCE hInst, long lSamprate, WORD wChannels, WORD wBitsPerSample, DWORD dwOptions)
{
CRegistry	reg;
long		retVal;
BOOL		OpenDialog=FALSE;

/*	if(!reg.openCreateReg(HKEY_LOCAL_MACHINE,REGISTRY_PROGRAM_NAME  "\\FAAD"))
		ERROR_FGO("Can't open registry!")
	else
		if(OpenDialog=reg.getSetRegBool("OpenDialog",FALSE))
			reg.setRegBool("OpenDialog",FALSE);

	if(OpenDialog)
	{
	MY_DEC_CFG	*Cfg;
		if(!(Cfg=(MY_DEC_CFG *)malloc(sizeof(MY_DEC_CFG))))
			ERROR_FGO("Memory allocation error");
		ReadCfgDec(Cfg);
		Cfg->DecCfg.defSampleRate=lSamprate;
		switch(wBitsPerSample)
		{
		case 16:
			Cfg->DecCfg.outputFormat=FAAD_FMT_16BIT;
			break;
		case 24:
			Cfg->DecCfg.outputFormat=FAAD_FMT_24BIT;
			break;
		case 32:
			Cfg->DecCfg.outputFormat=FAAD_FMT_32BIT;
			break;
		default:
			ERROR_FGO("Invalid Bps");
		}
		Cfg->Channels=(BYTE)wChannels;
		retVal=DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_DECODER), (HWND)hWnd, (DLGPROC)DialogMsgProcDecoder, (DWORD)Cfg);
		WriteCfgDec(Cfg);
		FREE(Cfg);
	}
	else*/
		retVal=DialogBoxParam((HINSTANCE)hInst,(LPCSTR)MAKEINTRESOURCE(IDD_ENCODER), (HWND)hWnd, (DLGPROC)DIALOGMsgProc, dwOptions);

	if(retVal==-1)
		ERROR_FGO("DialogBoxParam");

	return retVal;
}
// *********************************************************************************************

// GetSuggestedSampleType() is called if OpenFilterOutput() returns NULL
void FAR PASCAL GetSuggestedSampleType(LONG *lplSamprate, WORD *lpwBitsPerSample, WORD *wChannels)
{
	*lplSamprate=0; // don't care
	*lpwBitsPerSample= *lpwBitsPerSample<=16 ? 0: 16;
	*wChannels= *wChannels<49 ? 0 : 48;
}
// *********************************************************************************************

void FAR PASCAL CloseFilterOutput(HANDLE hOutput)
{
	if(!hOutput)
		return;

MYOUTPUT *mo;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return);
	
	if(mo->aacFile)
	{
		fclose(mo->aacFile);
		mo->aacFile=0;
	}
	
	if(mo->hEncoder)
		faacEncClose(mo->hEncoder);
	
	FREE(mo->bitbuf)
	FREE(mo->buffer)
	
//	FREE(mi->dst_name);
	
	GlobalUnlock(hOutput);
	GlobalFree(hOutput);
}              
// *********************************************************************************************

#define ERROR_OFO(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
	if(hOutput) \
	{ \
		GlobalUnlock(hOutput); \
		CloseFilterOutput(hOutput); \
	} \
	return 0; \
}
// -----------------------------------------------------------------------------------------------

HANDLE FAR PASCAL OpenFilterOutput(LPSTR lpstrFilename,long lSamprate,WORD wBitsPerSample,WORD wChannels,long lSize, long far *lpChunkSize, DWORD dwOptions)
{
HANDLE			hOutput;
MYOUTPUT		*mo;
MY_ENC_CFG		cfg;
DWORD			samplesInput,
				maxBytesOutput;

//	if(wBitsPerSample!=8 && wBitsPerSample!=16) // 32 bit audio from cooledit is in unsupported format
//		return 0;
	if(wChannels>=49)	// FAAC supports max 48 tracks!
		return 0;

    if(!(hOutput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE|GMEM_ZEROINIT,sizeof(MYOUTPUT))))
		ERROR_OFO("Memory allocation error: hOutput");
	if(!(mo=(MYOUTPUT *)GlobalLock(hOutput)))
		ERROR_OFO("GlobalLock(hOutput)");

	// open the aac output file 
	if(!(mo->aacFile=fopen(lpstrFilename, "wb")))
		ERROR_OFO("Can't create file");

	// use bufferized stream
	setvbuf(mo->aacFile,NULL,_IOFBF,32767);

	// open the encoder library
	if(!(mo->hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
		ERROR_OFO("Can't open library");

	if(!(mo->bitbuf=(unsigned char *)malloc(maxBytesOutput*sizeof(unsigned char))))
		ERROR_OFO("Memory allocation error: output buffer");

	if(!(mo->buffer=(int32_t *)malloc(samplesInput*sizeof(int32_t))))
		ERROR_OFO("Memory allocation error: input buffer");

	ReadCfgEnc(&cfg);
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
			else
				myFormat->bitRate*=1000;

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
			ERROR_OFO("Unsupported parameters");
	}

	*lpChunkSize=samplesInput*(wBitsPerSample>>3);

//	mo->src_size=lSize;
	mo->Samprate=lSamprate;
	mo->BitsPerSample=wBitsPerSample;
	mo->Channels=wChannels;
	mo->samplesInput=samplesInput;
	mo->maxBytesOutput=maxBytesOutput;
//	mi->dst_name=strdup(lpstrFilename);

	// init flushing process
int bytesEncoded, tmp;

    bytesEncoded=faacEncEncode(mo->hEncoder, 0, 0, mo->bitbuf, maxBytesOutput); // initializes the flushing process
    if(bytesEncoded>0)
	{
		tmp=fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile);
		if(tmp!=bytesEncoded)
			ERROR_OFO("fwrite()");
	}

	GlobalUnlock(hOutput);

    return hOutput;
}
// *********************************************************************************************

#define ERROR_WFO(msg) \
{ \
	if(msg) \
		MessageBox(0, msg, APP_NAME " plugin", MB_OK|MB_ICONSTOP); \
	if(hOutput) \
		GlobalUnlock(hOutput); \
	return 0; \
}
// -----------------------------------------------------------------------------------------------

DWORD FAR PASCAL WriteFilterOutput(HANDLE hOutput, unsigned char far *bufIn, long lBytes)
{
	if(!hOutput)
		return 0;

int bytesWritten;
int bytesEncoded;
MYOUTPUT far *mo;

	GLOBALLOCK(mo,hOutput,MYOUTPUT,return 0);

int32_t *buf=mo->buffer;

	To32bit(buf,bufIn,mo->samplesInput,mo->BitsPerSample>>3,false);

	// call the actual encoding routine
	bytesEncoded=faacEncEncode(mo->hEncoder, (int32_t *)buf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
	if(bytesEncoded>0)
	{
		// write bitstream to aac file 
		bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->aacFile);
		if(bytesWritten!=bytesEncoded)
			ERROR_WFO("bytesWritten and bytesEncoded are different");
	}
	else
	{
		if(bytesEncoded<0)
			ERROR_WFO("faacEncEncode()");
		bytesWritten=lBytes ? 1 : 0; // bytesWritten==0 stops CoolEdit
	}
	
	GlobalUnlock(hOutput);
	return bytesWritten;
}
