// FileListQueryManager.h: interface for the CFileListQueryManager class.
// Author: Torsten Landmann
//
// this class is a wrapper around CFileDialog and is used to let the user
// select several files at a time
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILELISTQUERYMANAGER_H__A1444E8A_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FILELISTQUERYMANAGER_H__A1444E8A_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"

class CFileListQueryManager  
{
public:
	CFileListQueryManager();
	virtual ~CFileListQueryManager();

	static TItemList<CString> GetFilePaths(bool m_bOpenMode=true, const CString *poDefaultFile=0, const CString &oDefaultExtension="", const CString &oExtensions="All Files (*.*)|*.*||");
};

#endif // !defined(AFX_FILELISTQUERYMANAGER_H__A1444E8A_1546_11D5_8402_0080C88C25BD__INCLUDED_)
