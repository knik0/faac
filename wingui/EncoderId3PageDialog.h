#if !defined(AFX_ENCODERID3PAGEDIALOG_H__442115D0_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERID3PAGEDIALOG_H__442115D0_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EncoderId3PageDialog.h : header file
//

#include "JobListUpdatable.h"
#include "JobListsToConfigureSaver.h"

/////////////////////////////////////////////////////////////////////////////
// CEncoderId3PageDialog dialog

class CEncoderId3PageDialog : public CDialog
{
// Construction
public:
	CEncoderId3PageDialog(const TItemList<CJob*> &oJobsToConfigure, CJobListUpdatable *poListContainer, CWnd* pParent = NULL);   // standard constructor
	virtual ~CEncoderId3PageDialog();

// Dialog Data
	//{{AFX_DATA(CEncoderId3PageDialog)
	enum { IDD = IDD_ENCODERID3PAGEDIALOG };
	CEdit	m_ctrlEditYear;
	CEdit	m_ctrlEditTrack;
	CComboBox	m_ctrlComboGenre;
	CButton	m_ctrlCheckCopyright;
	CString	m_oEditAlbum;
	CString	m_oEditArtist;
	CString	m_oEditComment;
	CString	m_oEditComposer;
	CString	m_oEditEncodedBy;
	CString	m_oEditOriginalArtist;
	CString	m_oEditTitle;
	CString	m_oEditTrack;
	CString	m_oEditUrl;
	CString	m_oEditYear;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEncoderId3PageDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
public:
	// returns false in case of errors
	bool GetPageContents(CEncoderId3PropertyPageContents &oTarget);
	void ApplyPageContents(const CEncoderId3PropertyPageContents &oPageContents);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEncoderId3PageDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnKillfocusComboGenre();
	afx_msg void OnKillfocusEditAlbum();
	afx_msg void OnKillfocusEditArtist();
	afx_msg void OnKillfocusEditComment();
	afx_msg void OnKillfocusEditComposer();
	afx_msg void OnKillfocusEditEncodedBy();
	afx_msg void OnKillfocusEditOriginalArtist();
	afx_msg void OnKillfocusEditTitle();
	afx_msg void OnKillfocusEditTrack();
	afx_msg void OnKillfocusEditUrl();
	afx_msg void OnKillfocusEditYear();
	afx_msg void OnCheckCopyright();
	afx_msg void OnUpdateEditTrack();
	afx_msg void OnUpdateEditYear();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	bool m_bInitialized;

	TItemList<CJob*> m_oJobsToConfigure;

	CJobListUpdatable *m_poListContainer;

	CEncoderId3PropertyPageContents ParseJobs();
	void ModifyJobs(const CEncoderId3PropertyPageContents &oPageContents);

	// called when changes have been made in the dialog and are to
	// be synchronized with both "data holders"
	void UpdateJobs(bool bFinishCheckBoxSessions=true, bool bDlgDestructUpdate=false);
	bool m_bIgnoreUpdates;


	// these members are used for managing the check box controls and the
	// changes that are made with them in the dialog
	enum ETypeOfCheckBox
	{
		eNone,
		eCopyright,
	} m_eCurCheckBox;
	CJobListsToConfigureSaver m_oCheckStateChangeStateSaver;
	void ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox);
	void FinishCurrentCheckBoxSessionIfNecessary();
	void FinishCheckBoxSessionIfNecessary(CButton *poCheckBox);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ENCODERID3PAGEDIALOG_H__442115D0_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
