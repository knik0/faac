#include <windows.h>
#include <shlobj.h>
#include <stdio.h>  // FILE *
#include "resource.h"
#include <faac.h>
#include "out.h"


#define PI_VER "v1.0beta"


extern void config_read();
extern void config_write();

int getwrittentime();


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
 BOOL  bStopEnc;

 unsigned char *inbuf;
 DWORD full_size; // size of decoded file needed to set the length of progress bar
 DWORD tagsize;
 DWORD bytes_read; // from file
 DWORD bytes_consumed; // by faadDecDecode
 DWORD bytes_into_buffer;
 DWORD bytes_Enc;
} MYOUTPUT;

Out_Module out;
char config_AACoutdir[MAX_PATH]="";
DWORD dwOptions;
static MYOUTPUT mo0,
                *mo=&mo0; // this is done to drag'n'drop code from CoolEdit plugin
static HBITMAP hBmBrowse=NULL;
static int srate, numchan, bps;
volatile int writtentime, w_offset;
static int last_pause=0;



BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ulReason, LPVOID lpReserved)
{
   switch(ulReason)
   {
      case DLL_PROCESS_ATTACH:
           DisableThreadLibraryCalls(hInst);
           if(!hBmBrowse)
            hBmBrowse=LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BROWSE));
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

static int CALLBACK WINAPI BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SetWindowText(hwnd,"Select Directory");
		SendMessage(hwnd,BFFM_SETSELECTION,(WPARAM)1,(LPARAM)config_AACoutdir);
	}
	return 0;
}


static BOOL CALLBACK DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
 switch(Message)
 {
  case WM_INITDIALOG:
	  {
      char buf[10];
	   SendMessage(GetDlgItem(hWndDlg, IDC_BTN_BROWSE), BM_SETIMAGE, IMAGE_BITMAP, (LPARAM) hBmBrowse);
       SetDlgItemText(hWndDlg, IDC_E_BROWSE, config_AACoutdir);

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
//        SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_SETCURSEL, (dwOptions>>6)&31, 0);
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
//      		 EnableWindow(GetDlgItem(hWndDlg, IDC_USELFE), Enabled);
      		 EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BITRATE), Enabled);
      		 EnableWindow(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), Enabled);
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
               faacEncCfg.aacObjectType=0;
              if(IsDlgButtonChecked(hWndDlg,IDC_RADIO_LOW))
			  {
               faacEncCfg.aacObjectType=LOW;
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
			  retVal|=(SendMessage(GetDlgItem(hWndDlg, IDC_CB_BITRATE), CB_GETCURSEL, 0, 0)&31)<<19;
              //retVal|=faacEncCfg.bitRate;
			  faacEncCfg.bandWidth=GetDlgItemInt(hWndDlg, IDC_CB_BANDWIDTH, 0, FALSE);
//			  retVal|=(SendMessage(GetDlgItem(hWndDlg, IDC_CB_BANDWIDTH), CB_GETCURSEL, 0, 0)&31)<<6;
			  retVal|=(faacEncCfg.bandWidth&0x0000ffff)<<1;

			  if(IsDlgButtonChecked(hWndDlg,IDC_CHK_AUTOCFG))
			   retVal|=1;

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



void config(HWND hWnd)
{
 dwOptions=DialogBox(out.hDllInstance, MAKEINTRESOURCE(IDD_COMPRESSION), hWnd, DIALOGMsgProc);
// dwOptions=DialogBoxParam((HINSTANCE)out.hDllInstance,(LPCSTR)MAKEINTRESOURCE(IDD_COMPRESSION), (HWND)hWnd, (DLGPROC)DIALOGMsgProc, dwOptions);
 config_write();
}

void about(HWND hwnd)
{
char buf[256];
 sprintf(buf,"AAC-MPEG4 encoder plug-in %s\nThis plugin uses FAAC encoder engine v%g\n\nCompiled on %s\n",
	         PI_VER,
             FAACENC_VERSION,
			 __DATE__);
 MessageBox(hwnd, buf, "About", MB_OK);
/*	MessageBox(hwnd,"AAC-MPEG4 encoder plug-in " PI_VER "\n"
					"This plugin uses FAAC encoder engine v1.5\n\n"
					"Compiled on " __DATE__ "\n","About",MB_OK);*/
}

void init()
{
 config_read();
}

void quit()
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

int open(int lSamprate, int wChannels, int wBitsPerSample, int bufferlenms, int prebufferms)
{
//HANDLE        hOutput;
faacEncHandle hEncoder;
FILE          *outfile;
unsigned char *bitbuf;
DWORD         maxBytesOutput;
long          samplesInput;
int           bytesEncoded;


	char *t,*p;
	char temp2[MAX_PATH],lpstrFilename[MAX_PATH];
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
		config(out.hMainWindow);
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

	if(!(mo->inbuf=(unsigned char*)malloc(sizeof(short)*1024*wChannels)))
	{
	 MessageBox(0, "Memory allocation error: output buffer", "FAAC interface", MB_OK);
     faacEncClose(hEncoder);
	 fclose(outfile);
	 free(bitbuf);
	 return -1;
	}

//	*lpChunkSize=samplesInput*2;

/*    hOutput=GlobalAlloc(GMEM_MOVEABLE|GMEM_SHARE,sizeof(MYOUTPUT));
    if(hOutput)
    {
	MYOUTPUT *mo;*/
	 //mo=(MYOUTPUT *)GlobalLock(hOutput);
	 mo->fFile=outfile;
//	 mo->lSize=lSize;
	 mo->lSamprate=lSamprate;
	 mo->wBitsPerSample=wBitsPerSample;
	 mo->wChannels=wChannels;
//	 mo->dwDataOffset=0; // ???
//	 mo->bWrittenHeader=0;
	 strcpy(mo->szNAME,lpstrFilename);

	 mo->hEncoder=hEncoder;
     mo->bitbuf=bitbuf;
	 mo->maxBytesOutput=maxBytesOutput;
	 mo->samplesInput=samplesInput*(sizeof(short));
	 mo->bStopEnc=0;

//	 GlobalUnlock(hOutput);
/*    }
	else
	{
	 MessageBox(0, "hOutput=NULL", "FAAC interface", MB_OK);
     faacEncClose(hEncoder);
	 fclose(outfile);
	 free(bitbuf);
	 free(mo->inbuf);
	 return -1;
	}*/

	if(dwOptions && !(dwOptions&1))
	{
     faacEncConfigurationPtr myFormat;
     myFormat=faacEncGetCurrentConfiguration(hEncoder);

	 myFormat->mpegVersion=(dwOptions>>29)&7;
	 myFormat->aacObjectType=(dwOptions>>27)&3;
	 myFormat->allowMidside=(dwOptions>>26)&1;
	 myFormat->useTns=(dwOptions>>25)&1;
	 myFormat->useLfe=(dwOptions>>24)&1;

	 switch((dwOptions>>19)&31)
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
	  myFormat->bandWidth=mo->lSamprate/2;

	 if(!faacEncSetConfiguration(hEncoder, myFormat))
	 {
      MessageBox(0, "Unsupported parameters", "FAAC interface", MB_OK);
      faacEncClose(hEncoder);
	  fclose(outfile);
	  free(bitbuf);
      free(mo->inbuf);
//      GlobalFree(hOutput);
	  return -1;
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

void close()
{
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

// CloseHandle(outfile);
}

int write(char *buf, int len)
{
int bytesWritten;
int bytesEncoded;
int k,i,shift=0;

//	int t;
	writtentime += len;

//	WriteFile(outfile,buf,len,&t,NULL);


/* if(hOutput)
 { 
 MYOUTPUT far *mo;
  mo=(MYOUTPUT far *)GlobalLock(hOutput);*/

  if(!mo->bStopEnc)
  {
//   if(writtentime>mo->lSize)
//	return 0;

   if(mo->bytes_into_buffer+len<mo->samplesInput)
   {
    memcpy(mo->inbuf+mo->bytes_into_buffer, buf, len);
	mo->bytes_into_buffer+=len;
	return 0;
   }
   else
   {
    memcpy(mo->inbuf+mo->bytes_into_buffer, buf, mo->samplesInput-mo->bytes_into_buffer);
    shift=mo->samplesInput-mo->bytes_into_buffer;
    mo->bytes_into_buffer+=shift;
	len-=shift;
    buf+=shift;

    bytesEncoded=faacEncEncode(mo->hEncoder, (short *)mo->inbuf, mo->samplesInput/sizeof(short), mo->bitbuf, mo->maxBytesOutput);
	mo->bytes_into_buffer=0;
    if(bytesEncoded<1) // end of flushing process
    {
     if(bytesEncoded<0)
      MessageBox(0, "faacEncEncode() failed", "FAAC interface", MB_OK);
     mo->bStopEnc=1;
//     GlobalUnlock(hOutput);
     return -1;
    }
   }

   if(mo->bytes_into_buffer)
   {
    if(bytesEncoded<0)
     MessageBox(0, "bytes_into_buffer>0", "FAAC interface", MB_OK);
    mo->bStopEnc=1;
//    GlobalUnlock(hOutput);
    return -1;
   }

// call the actual encoding routine
   k=len/mo->samplesInput;
   for(i=0; i<k; i++)
    bytesEncoded+=faacEncEncode(mo->hEncoder, (short *)(buf+i*mo->samplesInput), mo->samplesInput/sizeof(short), mo->bitbuf, mo->maxBytesOutput);
   memcpy(mo->inbuf+mo->bytes_into_buffer, buf, len%mo->samplesInput);
   mo->bytes_into_buffer+=len%mo->samplesInput;
   if(bytesEncoded<1) // end of flushing process
   {
    if(bytesEncoded<0)
     MessageBox(0, "faacEncEncode() failed", "FAAC interface", MB_OK);
    mo->bStopEnc=1;
//    GlobalUnlock(hOutput);
    return -1;
   }
// write bitstream to aac file 
   bytesWritten=fwrite(mo->bitbuf, 1, bytesEncoded, mo->fFile);
   if(bytesWritten!=bytesEncoded)
   {
    MessageBox(0, "bytesWritten and bytesEncoded are different", "FAAC interface", MB_OK);
    mo->bStopEnc=1;
//    GlobalUnlock(hOutput);
    return -1;
   }

//   GlobalUnlock(hOutput);
  }
// }


	Sleep(0);
	return 0;
}

int canwrite()
{
	return last_pause ? 0 : 16*1024*1024;
//	return last_pause ? 0 : mo->samplesInput;
//	return mo->samplesInput;
}

int isplaying()
{
	return 0;
}

int pause(int pause)
{
	int t=last_pause;
	last_pause=pause;
	return t;
}

void setvolume(int volume)
{
}

void setpan(int pan)
{
}

void flush(int t)
{
  int a;
  w_offset=0;
  a = t - getwrittentime();
  w_offset=a;
}
	
int getwrittentime()
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

Out_Module out = {
	OUT_VER,
	"Freeware AAC encoder " PI_VER,
	33,
	0, // hmainwindow
	0, // hdllinstance
	config,
	about,
	init,
	quit,
	open,
	close,
	write,
	canwrite,
	isplaying,
	pause,
	setvolume,
	setpan,
	flush,
	getwrittentime,
	getwrittentime
};

__declspec( dllexport ) Out_Module * winampGetOutModule()
{
	return &out;
}
