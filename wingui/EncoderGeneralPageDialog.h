#if !defined(AFX_ENCODERGENERALPAGEDIALOG_H__442115C6_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERGENERALPAGEDIALOG_H__442115C6_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EncoderGeneralPageDialog.h : header file
//

#include "JobListUpdatable.h"
#include "JobListsToConfigureSaver.h"

/////////////////////////////////////////////////////////////////////////////
// CEncoderGeneralPageDialog dialog

class CEncoderGeneralPageDialog : public CDialog
{
// Construction
public:
	CEncoderGeneralPageDialog(const TItemList<CJob*> &oJobsToConfigure, CJobListUpdatable *poListContainer, CWnd* pParent = NULL);   // standard constructor
	virtual ~CEncoderGeneralPageDialog();

// Dialog Data
	//{{AFX_DATA(CEncoderGeneralPageDialog)
	enum { IDD = IDD_ENCODERGENERALPAGEDIALOG };
	CButton	m_ctrlCheckRecursive;
	CButton	m_ctrlButtonBrowseTargetFile;
	CButton	m_ctrlButtonBrowseSourceFile;
	CString	m_oEditSourceDir;
	CString	m_oEditSourceFile;
	CString	m_oEditTargetDir;
	CString	m_oEditTargetFile;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEncoderGeneralPageDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	// returns false in case of errors
	bool GetPageContents(CEncoderGeneralPropertyPageContents &oTarget);
	void ApplyPageContents(const CEncoderGeneralPropertyPageContents &oPageContents);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEncoderGeneralPageDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnKillfocusEditSourceDir();
	afx_msg void OnKillfocusEditSourceFile();
	afx_msg void OnKillfocusEditTargetDir();
	afx_msg void OnKillfocusEditTargetFile();
	afx_msg void OnButtonBrowseSourceDir();
	afx_msg void OnButtonBrowseTargetDir();
	afx_msg void OnButtoBrowseSourceFile();
	afx_msg void OnButtonBrowseTargetFile();
	afx_msg void OnChangeEditSourceFile();
	afx_msg void OnCheckRecursive();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	bool m_bInitialized;

	TItemList<CJob*> m_oJobsToConfigure;

	CJobListUpdatable *m_poListContainer;

	CEncoderGeneralPropertyPageContents ParseJobs();
	void ModifyJobs(const CEncoderGeneralPropertyPageContents &oPageContents);


	// called when changes have been made in the dialog and are to
	// be synchronized with both "data holders"
	void UpdateJobs(bool bFinishCheckBoxSessions=true, bool bDlgDestructUpdate=false);
	bool m_bIgnoreUpdates;

	// these members are used for managing the check box controls and the
	// changes that are made with them in the dialog
	enum ETypeOfCheckBox
	{
		eNone,
		eRecursive,
	} m_eCurCheckBox;
	CJobListsToConfigureSaver m_oCheckStateChangeStateSaver;
	void ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox);
	void FinishCurrentCheckBoxSessionIfNecessary();
	void FinishCheckBoxSessionIfNecessary(CButton *poCheckBox);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ENCODERGENERALPAGEDIALOG_H__442115C6_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
