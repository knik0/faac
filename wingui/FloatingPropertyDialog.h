// Author: Torsten Landmann
//
// This class represents the floating property dialog used
// to configure the jobs
//
////////////////////////////////////////////////////////

#if !defined(AFX_FLOATINGPROPERTYDIALOG_H__442115C9_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FLOATINGPROPERTYDIALOG_H__442115C9_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FloatingPropertyDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFloatingPropertyDialog dialog

class CFloatingPropertyDialog : public CDialog
{
// Construction
public:
	CFloatingPropertyDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CFloatingPropertyDialog();

// Dialog Data
	//{{AFX_DATA(CFloatingPropertyDialog)
	enum { IDD = IDD_FLOATINGPROPERTYDIALOG };
	CStatic	m_ctrlLabelDebugTag;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFloatingPropertyDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	// this member creates a singelton dummy dialog that's embedded
	// in any floating window; the creation of this floating window
	// is task of this method;
	// note that this member works somewhat asynchrounous; on creation
	// of the CPropertiesDummyParentDialog window it calls the
	static void CreateFloatingPropertiesDummyParentDialog();

	// this member properly cleans up any floating property window, if there's
	// any
	static void InvalidFloatingPropertiesDialog();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFloatingPropertyDialog)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	static CFloatingPropertyDialog *m_poFloatingPropertyDialogSingleton;

	CPropertiesDummyParentDialog *m_poPropertiesDummyDialog;

	CRect m_oCurrentSize;

	void ApplyNewSize(const CRect &oSize);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FLOATINGPROPERTYDIALOG_H__442115C9_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
