#if !defined(AFX_ASKCREATEDIRECTORYDIALOG_H__5D3060C2_1CA9_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ASKCREATEDIRECTORYDIALOG_H__5D3060C2_1CA9_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AskCreateDirectoryDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAskCreateDirectoryDialog dialog

class CAskCreateDirectoryDialog : public CDialog
{
// Construction
public:
	CAskCreateDirectoryDialog(const CString &oDirectory, CWnd* pParent = NULL);   // standard constructor
	virtual ~CAskCreateDirectoryDialog();

// Dialog Data
	//{{AFX_DATA(CAskCreateDirectoryDialog)
	enum { IDD = IDD_ASKCREATEDIRECTORYDIALOG };
	CString	m_oLabelTargetDir;
	//}}AFX_DATA

	// one of the following values is returned by DoModal()
	enum ECreateDirectoryDialogReturn
	{
		eNo,
		eYes,
		eAlways,
		eNever,
	};


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAskCreateDirectoryDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAskCreateDirectoryDialog)
	afx_msg void OnButtonYes();
	afx_msg void OnButtonNo();
	afx_msg void OnButtonAlways();
	afx_msg void OnButtonNever();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ASKCREATEDIRECTORYDIALOG_H__5D3060C2_1CA9_11D5_8402_0080C88C25BD__INCLUDED_)
