#include <windows.h>
#include <shlobj.h>
#include <stdio.h>  // FILE *
#include "resource.h"
#include "faac.h"
#include "out.h"


#define PI_VER "v1.0 beta2"

extern void config_read();
extern void config_write();

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
 FILE  *fFile;         
 DWORD lSize;
 long  lSamprate;
 WORD  wBitsPerSample;
 WORD  wChannels;
// DWORD dwDataOffset;
 //BOOL  bWrittenHeader;
 char  szNAME[256];

 faacEncHandle hEncoder;
 unsigned char *bitbuf;
 DWORD maxBytesOutput;
 long  samplesInput;
 BYTE  bStopEnc;

 unsigned char *inbuf;
 DWORD full_size; // size of decoded file needed to set the length of progress bar
 DWORD tagsize;
 DWORD bytes_read; // from file
 DWORD bytes_consumed; // by faadDecDecode
 DWORD bytes_into_buffer;
 DWORD bytes_Enc;
} MYOUTPUT;

char config_AACoutdir[MAX_PATH]="";
DWORD dwOptions;
static MYOUTPUT mo0,
                *mo=&mo0; // this is done to copy'n'paste code from CoolEdit plugin
static HBITMAP hBmBrowse=NULL;
static int srate, numchan, bps;
volatile int writtentime, w_offset;
static int last_pause=0;


Out_Module out = {
	OUT_VER,
	"Freeware AAC encoder " PI_VER,
	34,
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

__declspec( dllexport ) Out_Module * winampGetOutModule()
{
	return &out;
}



BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ulReason, LPVOID lpReserved)
{
	switch(ulReason)
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls(hInst);
			if(!hBmBrowse)
				hBmBrowse=LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BROWSE));
			/* 	Code from LibMain inserted here.  Return TRUE to keep the
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
			/* 	Called each time a thread is created in a process that has
				already loaded (attached to) this DLL. Does not get called
				for each thread that exists in the process before it loaded
				the DLL.
 
				Do thread-specific initialization here.
			*/
			break;
 
		case DLL_THREAD_DETACH:
			/* 	Same as above, but called when a thread in the process
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
			/* 	Code from _WEP inserted here.  This code may (like the
				LibMain) not be necessary.  Check to make certain that the
				operating system is not doing it for you.
			*/
			break;
	}
 
	/* 	The return value is only used for DLL_PROCESS_ATTACH; all other
		conditions are ignored.  */
	return TRUE;   // successful DLL_PROCESS_ATTACH
}



static int CALLBACK WINAPI BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SetWindowText(hwnd,"Select Directory");
		SendMessage(hwnd,BFFM_SETSELECTION,(WPARAM)1,(LPARAM)config_AACoutdir);
	}
	return 0;
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

static BOOL CALLBACK DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		case WM_INITDIALOG:
		{
			char buf[10];
			int br;
			char szTemp[64];

			if(dwOptions)
			{
				char Enabled=!(dwOptions&1);
				CheckDlgButton(hWndDlg,IDC_CHK_AUTOCFG, dwOptions&1);
				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG4), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MPEG2), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_MAIN), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LOW), Enabled);
//				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_ALLOWMIDSIDE), Enabled);
				EnableWindow(GetDlgItem(hWndDlg, IDC_USETNS), Enabled);
//				EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
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
							DISABLE_LTP
							break;
					}

				CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, (dwOptions>>26)&1);
				CheckDlgButton(hWndDlg, IDC_USETNS, (dwOptions>>25)&1);
				CheckDlgButton(hWndDlg, IDC_USELFE, (dwOptions>>24)&1);

				for(br=0; br<=((dwOptions>>16)&255) ; br++)
				{
					if(br == ((dwOptions>>16)&255))
						sprintf(szTemp, "%d", br*1000);
				}

				SetDlgItemText(hWndDlg, IDC_CB_BITRATE, szTemp);

				for(br=0; br<=((dwOptions>>1)&0x0000ffff) ; br++)
				{
					if(br == ((dwOptions>>1)&0x0000ffff))
						sprintf(szTemp, "%d", br);
				}

				SetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, szTemp);

				break;
			} // End dwOptions

			CheckDlgButton(hWndDlg, IDC_ALLOWMIDSIDE, TRUE);
			CheckDlgButton(hWndDlg, IDC_USETNS, TRUE);
			CheckDlgButton(hWndDlg, IDC_USELFE, FALSE);
       
			switch((long)lParam)
			{
				case IDC_RADIO_MPEG4:
					CheckDlgButton(hWndDlg,IDC_RADIO_MPEG4,TRUE);
					EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), !IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG));
					break;
				case IDC_RADIO_MPEG2:
					CheckDlgButton(hWndDlg,IDC_RADIO_MPEG2,TRUE);
					DISABLE_LTP
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
       			// 	Closing the Dialog behaves the same as Cancel               
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
//					EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_SSR), Enabled);
					EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled);
					EnableWindow(GetDlgItem(hWndDlg, IDC_ALLOWMIDSIDE), Enabled);
					EnableWindow(GetDlgItem(hWndDlg, IDC_USETNS), Enabled);
//					EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
					EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled);
					EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled);
					if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_MPEG4))
						EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), Enabled);
					else
						DISABLE_LTP
				}
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
					idlist = SHBrowseForFolder( &bi );
					if (idlist) 
						SHGetPathFromIDList( idlist, config_AACoutdir );

					SetDlgItemText(hWndDlg, IDC_E_BROWSE, config_AACoutdir);
				}
					break;

				case IDC_E_BROWSE:
					GetDlgItemText(hWndDlg, IDC_E_BROWSE, config_AACoutdir,256);
					break;

				case IDOK:
				{
					char szTemp[64];
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
			  
					GetDlgItemText(hWndDlg, IDC_CB_BITRATE, szTemp, sizeof(szTemp));
					faacEncCfg.bitRate = atoi(szTemp);
					retVal|=((faacEncCfg.bitRate/1000)&255)<<16;
					GetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, szTemp, sizeof(szTemp));
					faacEncCfg.bandWidth = atoi(szTemp);
					retVal|=(faacEncCfg.bandWidth&0x0000ffff)<<1;

					if(IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG))
						retVal|=1;

					EndDialog(hWndDlg, retVal);
				}
					break;

				case IDCANCEL:
				// 	Ignore data values entered into the controls        
				// 	and dismiss the dialog window returning FALSE       
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

				case IDC_RADIO_MPEG4:
					EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), !IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG));
					break;

				case IDC_RADIO_MPEG2:
					EnableWindow(GetDlgItem(hWndDlg, IDC_RADIO_LTP), FALSE);
					DISABLE_LTP
					break;
			}
				break; // End of WM_COMMAND                                 
		default: return FALSE;
	}
 return TRUE;
} // End of DIALOGSMsgProc                                      



void Config(HWND hWnd)
{
	dwOptions=DialogBox(out.hDllInstance, MAKEINTRESOURCE(IDD_COMPRESSION), hWnd, DIALOGMsgProc);
//	dwOptions=DialogBoxParam((HINSTANCE)out.hDllInstance,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)DIALOGMsgProc, dwOptions);
	config_write();
}

void About(HWND hwnd)
{
	char buf[256];
	sprintf(buf,"AAC-MPEG4 encoder plug-in %s\nThis plugin uses FAAC encoder engine v%g\n\nCompiled on %s\n",
		PI_VER,
		FAACENC_VERSION,
		__DATE__);
	MessageBox(hwnd, buf, "About", MB_OK);
}

void Init()
{
	config_read();
}

void Quit()
{
}

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

int Open(int lSamprate, int wChannels, int wBitsPerSample, int bufferlenms, int prebufferms)
{
//	HANDLE		hOutput;
	faacEncHandle	hEncoder;
	FILE		*outfile;
	unsigned char	*bitbuf;
	DWORD		maxBytesOutput;
	long		samplesInput;
	int		bytesEncoded;
	int		br;

	char		*t,*p;
	char		temp2[MAX_PATH],lpstrFilename[MAX_PATH];
	GetWindowText(out.hMainWindow,temp2,sizeof(temp2));
	t=temp2;

	t=scanstr_back(temp2,"-",NULL);
	if (t) t[-1]=0;

	if (temp2[0] && temp2[1] == '.')
	{
		char *p1,*p2;
		p1=lpstrFilename;
		p2=temp2;
		while (*p2) *p1++ = *p2++;
		*p1=0;
		p1 = temp2+1;
		p2 = lpstrFilename;
		while (*p2) *p1++ = *p2++;
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

	w_offset = writtentime = 0;
	numchan = wChannels;
	srate = lSamprate;
	bps = wBitsPerSample;



	/* open the aac output file */
	if(!(outfile=fopen(lpstrFilename, "wb")))
	{
		MessageBox(0, "Can't create file", "FAAC interface", MB_OK);
		return -1;
	}

	/* open the encoder library */
	if(!(hEncoder=faacEncOpen(lSamprate, wChannels, &samplesInput, &maxBytesOutput)))
	{
		MessageBox(0, "Can't init library", "FAAC interface", MB_OK);
		fclose(outfile);
		return -1;
	}

	if(!(bitbuf=(unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char))))
	{
		MessageBox(0, "Memory allocation error: output buffer", "FAAC interface", MB_OK);
		faacEncClose(hEncoder);
		fclose(outfile);
		return -1;
	}

	if(!(mo->inbuf=(unsigned char*)malloc(samplesInput*sizeof(short))))
	{
		MessageBox(0, "Memory allocation error: output buffer", "FAAC interface", MB_OK);
		faacEncClose(hEncoder);
		fclose(outfile);
		free(bitbuf);
		return -1;
	}

	mo->fFile=outfile;
//	mo->lSize=lSize;
	mo->lSamprate=lSamprate;
	mo->wBitsPerSample=wBitsPerSample;
	mo->wChannels=wChannels;
	strcpy(mo->szNAME,lpstrFilename);

	mo->hEncoder=hEncoder;
	mo->bitbuf=bitbuf;
	mo->maxBytesOutput=maxBytesOutput;
	mo->samplesInput=samplesInput;
	mo->bStopEnc=0;


	if(dwOptions && !(dwOptions&1))
	{
		faacEncConfigurationPtr myFormat;
		myFormat=faacEncGetCurrentConfiguration(hEncoder);

		myFormat->mpegVersion=(dwOptions>>29)&7;
		myFormat->aacObjectType=(dwOptions>>27)&3;
		myFormat->allowMidside=(dwOptions>>26)&1;
		myFormat->useTns=(dwOptions>>25)&1;
		myFormat->useLfe=(dwOptions>>24)&1;

		for(br=0; br<=((dwOptions>>16)&255) ; br++)
		{
			if(br == ((dwOptions>>16)&255))
				myFormat->bitRate=br*1000;
		}

		myFormat->bandWidth=(dwOptions>>1)&0x0000ffff;
		if(!myFormat->bandWidth)
			myFormat->bandWidth=mo->lSamprate/2;

		if(!faacEncSetConfiguration(hEncoder, myFormat))
		{
			MessageBox(0, "Unsupported parameters", "FAAC interface", MB_OK);
			faacEncClose(hEncoder);
			fclose(outfile);
			free(bitbuf);
			free(mo->inbuf);
//			GlobalFree(hOutput);
			return -1;
		}
/*		{
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
			free(mo->inbuf);
			GlobalFree(hOutput);
			return -1;
		}*/
	}

	bytesEncoded=faacEncEncode(hEncoder, 0, 0, bitbuf, maxBytesOutput); // initializes the flushing process
	if(bytesEncoded>0)
		fwrite(bitbuf, 1, bytesEncoded, outfile);

	return 0;
}

void Close()
{
//	Following code crashes winamp. why???

	if(mo->bytes_into_buffer)
	{
		int bytesEncoded;
		bytesEncoded=faacEncEncode(mo->hEncoder, (short *)mo->inbuf, mo->bytes_into_buffer/sizeof(short), mo->bitbuf, mo->maxBytesOutput);
		if(bytesEncoded>0)
			fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
	}

	if(mo->hEncoder)
		faacEncClose(mo->hEncoder);
	if(mo->fFile)
		fclose(mo->fFile);
	if(mo->bitbuf)
		free(mo->bitbuf);
	if(mo->inbuf)
		free(mo->inbuf);

//	CloseHandle(outfile);
}

int Write(char *buf, int len)
{
	int bytesWritten;
	int bytesEncoded;
	int k,i,shift=0;

	writtentime += len;
  
	if(!mo->bStopEnc)
	{
		if(mo->bytes_into_buffer+len<mo->samplesInput*sizeof(short))
		{
			memcpy(mo->inbuf+mo->bytes_into_buffer, buf, len);
			mo->bytes_into_buffer+=len;
			return 0;
		}
		else
			if(mo->bytes_into_buffer)
			{
				shift=mo->samplesInput*sizeof(short)-mo->bytes_into_buffer;
				memcpy(mo->inbuf+mo->bytes_into_buffer, buf, shift);
				mo->bytes_into_buffer+=shift;
				buf+=shift;
				len-=shift;

				bytesEncoded=faacEncEncode(mo->hEncoder, (short *)mo->inbuf, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
				mo->bytes_into_buffer=0;
				if(bytesEncoded<1) // end of flushing process
				{
					if(bytesEncoded<0)
					{
						MessageBox(0, "faacEncEncode() failed", "FAAC interface", MB_OK);
						mo->bStopEnc=1;
					}
					bytesWritten=len ? 0 : -1;
					return bytesWritten;
				}
//				write bitstream to aac file 
				bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
				if(bytesWritten!=bytesEncoded)
				{
					MessageBox(0, "bytesWritten and bytesEncoded are different", "FAAC interface", MB_OK);
					mo->bStopEnc=1;
					return -1;
				}
			}

//		call the actual encoding routine
		k=len/(mo->samplesInput*sizeof(short));
		for(i=0; i<k; i++)
		{
			bytesEncoded+=faacEncEncode(mo->hEncoder, ((short *)buf)+i*mo->samplesInput, mo->samplesInput, mo->bitbuf, mo->maxBytesOutput);
			if(bytesEncoded<1) // end of flushing process
			{
				if(bytesEncoded<0)
				{
					MessageBox(0, "faacEncEncode() failed", "FAAC interface", MB_OK);
					mo->bStopEnc=1;
				}
				bytesWritten=len ? 0 : -1;
				return bytesWritten;
			}
//		write bitstream to aac file 
		bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
		if(bytesWritten!=bytesEncoded)
		{
			MessageBox(0, "bytesWritten and bytesEncoded are different", "FAAC interface", MB_OK);
			mo->bStopEnc=1;
			return -1;
		}
	}

	mo->bytes_into_buffer=len%(mo->samplesInput*sizeof(short));
	memcpy(mo->inbuf, buf+k*mo->samplesInput*sizeof(short), mo->bytes_into_buffer);
  }

  Sleep(0);
  return 0;
}

int CanWrite()
{
	return last_pause ? 0 : 16*1024*1024;
//	return last_pause ? 0 : mo->samplesInput;
}

int IsPlaying()
{
	return 0;
}

int Pause(int pause)
{
	int t=last_pause;
	last_pause=pause;
	return t;
}

void SetVolume(int volume)
{
}

void SetPan(int pan)
{
}

void Flush(int t)
{
	int a;
	w_offset=0;
	a = t - GetWrittenTime();
	w_offset=a;
}
	
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
