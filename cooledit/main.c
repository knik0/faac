#include <windows.h>
#include <stdlib.h>
#include "resource.h"
#include "filters.h" //CoolEdit
#include "faac.h"

extern faacEncConfiguration faacEncCfg;


BOOL WINAPI DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
   switch (fdwReason)
   {
      case DLL_PROCESS_ATTACH:
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

// Fill COOLQUERY structure with information regarding this file filter

__declspec(dllexport) short FAR PASCAL QueryCoolFilter(COOLQUERY far * cq)
{
	lstrcpy(cq->szName,"MPEG4-AAC Format");		
	lstrcpy(cq->szCopyright,"Freeware AAC-MPEG4 codec");// Compiled on: " __DATE__);
	lstrcpy(cq->szExt,"AAC");
	lstrcpy(cq->szExt2,"MP4"); 
	cq->lChunkSize=16384; 
	cq->dwFlags=QF_RATEADJUSTABLE|QF_CANLOAD|QF_CANSAVE|QF_HASOPTIONSBOX;
 	cq->Stereo8=0xFF; // supports all rates of stereo 8
 	cq->Stereo16=0xFF;
 	cq->Stereo24=0xFF;
 	cq->Stereo32=0xFF;
 	cq->Mono8=0xFF; // supports all rates of stereo 8
 	cq->Mono16=0xFF;
 	cq->Mono24=0xFF;
 	cq->Mono32=0xFF;
 	cq->Quad32=0xFF;
 	cq->Quad16=0xFF;
 	cq->Quad8=0xFF;
 	return C_VALIDLIBRARY;
}

__declspec(dllexport) BOOL FAR PASCAL DIALOGMsgProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam)
{
   switch(Message)
   {
   case WM_INITDIALOG:
         {             
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
			  CheckDlgButton(hWndDlg,IDC_RADIO1,TRUE);
			  break;
			 case 2:
			  CheckDlgButton(hWndDlg,IDC_RADIO2,TRUE);
			  break;
			 case 3:
			  CheckDlgButton(hWndDlg,IDC_RADIO3,TRUE);
			  break;
			 case 4:
			  CheckDlgButton(hWndDlg,IDC_RADIO4,TRUE);
			  break;
			 case 5:
			  CheckDlgButton(hWndDlg,IDC_RADIO5,TRUE);
			  break;
			 case 6:
			  CheckDlgButton(hWndDlg,IDC_RADIO6,TRUE);
			  break;
			 case 7:
			  CheckDlgButton(hWndDlg,IDC_RADIO7,TRUE);
			  break;
			 case 8:
			  CheckDlgButton(hWndDlg,IDC_RADIO8,TRUE);
			  break;
			 case 9:
			  CheckDlgButton(hWndDlg,IDC_RADIO9,TRUE);
			  break;
			 default:
			  CheckDlgButton(hWndDlg,IDC_RADIO1,TRUE);
			  CheckDlgButton(hWndDlg,IDC_RADIO3,TRUE);
			  CheckDlgButton(hWndDlg,IDC_RADIO7,TRUE);
			  break;
	         }	         
         }         
         break; // End of WM_INITDIALOG                                 

    case WM_CLOSE:
         // Closing the Dialog behaves the same as Cancel               
         PostMessage(hWndDlg, WM_COMMAND, IDCANCEL, 0L);
         break; // End of WM_CLOSE                                      

    case WM_COMMAND:
	{
		switch(LOWORD(wParam))
        {
		    case IDOK:
            	 {
				 char szTemp[10];
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO1))
            	   faacEncCfg.mpegVersion=4;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO2))
            	   faacEncCfg.mpegVersion=2;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO3))
            	   faacEncCfg.aacObjectType=0;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO4))
            	   faacEncCfg.aacObjectType=1;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO5))
            	   faacEncCfg.aacObjectType=2;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO6))
            	   faacEncCfg.aacObjectType=3;
/*            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO7))
            	   faacEncCfg=;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO8))
            	   faacEncCfg=;
            	  if(IsDlgButtonChecked(hWndDlg,IDC_RADIO9))
            	   faacEncCfg=;*/

                  faacEncCfg.allowMidside=IsDlgButtonChecked(hWndDlg, IDC_ALLOWMIDSIDE) == BST_CHECKED ? 1 : 0;
                  faacEncCfg.useTns=IsDlgButtonChecked(hWndDlg, IDC_USETNS) == BST_CHECKED ? 1 : 0;
                  faacEncCfg.useLfe=IsDlgButtonChecked(hWndDlg, IDC_USELFE) == BST_CHECKED ? 1 : 0;

				  GetDlgItemText(hWndDlg, IDC_CB_BITRATE, szTemp, sizeof(szTemp));
				  faacEncCfg.bitRate=atoi(szTemp);
				  GetDlgItemText(hWndDlg, IDC_CB_BANDWIDTH, szTemp, sizeof(szTemp));
				  faacEncCfg.bandWidth=atoi(szTemp);
                  
				  EndDialog(hWndDlg, (short)2);
            	 }
                 break;
            case IDCANCEL:
                 // Ignore data values entered into the controls        
                 // and dismiss the dialog window returning FALSE       
                 EndDialog(hWndDlg, FALSE);
                 break;
           }
         break;    // End of WM_COMMAND                                 
	}
    default:
        return FALSE;
   }
 return TRUE;
}//  End of DIALOGSMsgProc                                      
