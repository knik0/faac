// FileListQueryManager.cpp: implementation of the CFileListQueryManager class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FileListQueryManager.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileListQueryManager::CFileListQueryManager()
{

}

CFileListQueryManager::~CFileListQueryManager()
{

}

TItemList<CString> CFileListQueryManager::GetFilePaths(bool m_bOpenMode, const CString *poDefaultFile, const CString &oDefaultExtension, const CString &oExtensions)
{
	TItemList<CString> toReturn;
	if (!m_bOpenMode)
	{
		// not supported
		ASSERT(false);
		return toReturn;
	}

	CFileDialog oOpenDialog(
		m_bOpenMode? TRUE : FALSE,					// file open mode
		oDefaultExtension,							// default extension
		(poDefaultFile!=0 ? *poDefaultFile : (LPCTSTR)0),	// default file
		OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT,
		oExtensions);

	// for multi selection we must set our own target pointer
	int iFileBufferSize=8096;
	oOpenDialog.m_ofn.lpstrFile=new char[iFileBufferSize];
	oOpenDialog.m_ofn.nMaxFile=iFileBufferSize;
	// the dialog crashes if we don't zero out the buffer
	memset(oOpenDialog.m_ofn.lpstrFile, 0, iFileBufferSize);

	if (oOpenDialog.DoModal()==IDOK)
	{
		// the files have been opened	
		POSITION position=oOpenDialog.GetStartPosition();
		while (position!=0)
		{
			CString oCurFile=oOpenDialog.GetNextPathName(position);
			toReturn.AddNewElem(oCurFile);
		}
	}
	else
	{
		// user abort or error
		if (CommDlgExtendedError()!=0)
		{
			// an error occured
			AfxMessageBox(IDS_ErrorDuringFileSelection);
		}
	}

	delete[] oOpenDialog.m_ofn.lpstrFile;

	return toReturn;
}
