/*
FAAD - codec plugin for Cooledit
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
#include "filters.h" //CoolEdit
#include "defines.h"



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
	lstrcpy(cq->szName, APP_NAME " Format");		
	lstrcpy(cq->szCopyright, APP_NAME " codec");
	lstrcpy(cq->szExt,"AAC");
	lstrcpy(cq->szExt2,"MP4"); 
	cq->lChunkSize=16384; 
	cq->dwFlags=QF_RATEADJUSTABLE|QF_CANLOAD|QF_CANSAVE|QF_HASOPTIONSBOX;
 	cq->Mono8=0xFF;
 	cq->Mono16=0xFF;
 	cq->Mono24=0xFF;
 	cq->Mono32=0xFF;
 	cq->Stereo8=0xFF; // supports all rates ???
 	cq->Stereo16=0xFF;
 	cq->Stereo24=0xFF;
 	cq->Stereo32=0xFF;
 	cq->Quad8=0xFF;
 	cq->Quad16=0xFF;
 	cq->Quad32=0xFF;
 	return C_VALIDLIBRARY;
}
