// FolderDialog.h: interface for the CFolderDialog class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FOLDERDIALOG_H__57C79485_9C5F_11D3_A724_007250C10000__INCLUDED_)
#define AFX_FOLDERDIALOG_H__57C79485_9C5F_11D3_A724_007250C10000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CFolderDialog  
{
public:
	// for a flag description see BROWSEINFO in the MSDN
	CFolderDialog(CWnd *poParent, const CString &oDlgCaption, unsigned int iFlags=0);
	virtual ~CFolderDialog();

	// write to this member if you want to modify the parameters
	// provided at construction before calling DoModal()
	BROWSEINFO m_sctBrowseParams;

	// read these members after DoModal() return with IDOK
	CString m_oFolderName;
	CString m_oFolderPath;

	// read this member after DoModal() return with IDCANCEL
	bool m_bError;

	// call this member to show the dialog
	int DoModal();

private:
	CString m_oDlgCaption;
};

#endif // !defined(AFX_FOLDERDIALOG_H__57C79485_9C5F_11D3_A724_007250C10000__INCLUDED_)
