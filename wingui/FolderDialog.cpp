// FolderDialog.cpp: implementation of the CFolderDialog class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FolderDialog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFolderDialog::CFolderDialog(CWnd *poParent, const CString &oDlgCaption, unsigned int iFlags):
	m_oDlgCaption(oDlgCaption)
{
	m_sctBrowseParams.hwndOwner=*poParent;
	m_sctBrowseParams.pidlRoot=NULL;		// no special root directory (this entry is NOT the directory that is shown first)
	m_sctBrowseParams.pszDisplayName=0;
	m_sctBrowseParams.lpszTitle=0;
	m_sctBrowseParams.ulFlags=iFlags;
	m_sctBrowseParams.lpfn=NULL;			// no callback
	m_sctBrowseParams.lParam=0;
}

CFolderDialog::~CFolderDialog()
{

}

int CFolderDialog::DoModal()
{
	m_sctBrowseParams.pszDisplayName=m_oFolderName.GetBuffer(_MAX_PATH);
	m_sctBrowseParams.lpszTitle=m_oDlgCaption;				// better do that here

	LPITEMIDLIST lpSelectedItems;
	if ((lpSelectedItems=SHBrowseForFolder(&m_sctBrowseParams))!=NULL)
	{
		// OK pressed
		m_oFolderName.ReleaseBuffer();
		// unfortunately m_oFolderName only contains the name of the
		// directory selected and not its complete path; thus we have
		// some work left to do...

		// complicated but more flexible version of finding out the path;
		/*// find out the complete selected path; for this we'll need
		// the desktop interface (SHBrowseForFolder() returns the directory
		// position relatively to the desktop folder)
		IShellFolder *pifcFile;
		SHGetDesktopFolder(&pifcFile);

		// now ask the desktop interface for the relative position of the
		// folder returned
		// -- this parameter makes this version more flexible-+
		STRRET sctRet;			//							  |
								//							  V
		pifcFile->GetDisplayNameOf(lpSelectedItems, SHGDN_NORMAL, &sctRet);
		
		
		// fetch the path that the user has selected
		switch (sctRet.uType)
		{
		case STRRET_CSTR:
			{
				m_oFolderPath=sctRet.cStr;
				break;
			}
		case STRRET_WSTR:
			{
				m_oFolderPath=sctRet.pOleStr;
				break;
			}
		default:
		case STRRET_OFFSET:
			{
				ASSERT(false);
					// unkown or unhandleble return type
				AfxMessageBox(IDS_InvalidReturnType);

				break;
			}
		}*/

		// conventional way of finding out the complete folder path
		BOOL bSuccess=SHGetPathFromIDList(lpSelectedItems, m_oFolderPath.GetBuffer(_MAX_PATH));
		m_oFolderPath.ReleaseBuffer();

		// free buffer that was returned by SHBrowseForFolder()
		IMalloc *pifcMalloc;
		SHGetMalloc(&pifcMalloc);
		pifcMalloc->Free(lpSelectedItems);

		if (!bSuccess)
		{
			// error finding out the complete path
			m_bError=true;
			return IDCANCEL;
		}

		return IDOK;
	}
	else
	{
		// cancel pressed
		m_oFolderName.ReleaseBuffer();

		m_bError=false;
		return IDCANCEL;
	}
}