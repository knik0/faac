// faac_winguiDlg.h : header file
// Author: Torsten Landmann
//
///////////////////////////////////

#if !defined(AFX_FAAC_WINGUIDLG_H__DFE38E67_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FAAC_WINGUIDLG_H__DFE38E67_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ListCtrlStateSaver.h"
#include "JobListUpdatable.h"

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiDlg dialog

class CFaac_winguiDlg : public CDialog, public CJobListUpdatable
{
// Construction
public:
	CFaac_winguiDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~CFaac_winguiDlg();

// Dialog Data
	//{{AFX_DATA(CFaac_winguiDlg)
	enum { IDD = IDD_FAAC_WINGUI_DIALOG };
	CListCtrl	m_ctrlListJobs;
	BOOL	m_bCheckRemoveProcessedJobs;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFaac_winguiDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

	CJobList* GetGlobalJobList()								{ return &((CFaac_winguiApp*)AfxGetApp())->GetGlobalJobList(); }
	CFaacWinguiProgramSettings* GetGlobalProgramSettings()		{ return &((CFaac_winguiApp*)AfxGetApp())->GetGlobalProgramSettings(); }

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CFaac_winguiDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButtonAddEncoderJob();
	afx_msg void OnClickListJobs(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownListJobs(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButtonDeleteJobs();
	afx_msg void OnButtonProcessSelected();
	afx_msg void OnRclickListJobs(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButtonSaveJobList();
	afx_msg void OnButtonLoadJobList();
	afx_msg void OnButtonDuplicateSelected();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	friend class CPropertiesTabParentDialog;

	// implementation to CJobListUpdatable;
	// this method refills the job list control;
	// if no explicit selection state is specified the current
	// selection is preserved;
	// bSimpleUpdate may be set to true; if it is, no items are added
	// or removed and only the info column is updated
	void ReFillInJobListCtrl(CListCtrlStateSaver *poSelectionStateToUse=0, bool bSimpleUpdate=true);

	TItemList<long> m_lLastJobListSelection;
	void OnJobListCtrlUserAction();

	// by defining the parameter you can define which new selection is
	// to be assumed; if 0 is specified the method determines the actual
	// selection
	void OnJobListCtrlSelChange(TItemList<long> *poNewSelection=0);

	// this is a very special variable; if it is <0 it affects nothing;
	// if, however, it is >=0 it is incremented each time a call to
	// ReFillInJobListCtrl()) is made; in this case the call is ignored
	// (no list control operation is done)
	int m_iListCtrlUpdatesCounter;

	// when this variable is !=0 it contains the current file to
	// propose when file interaction is done
	CString *m_poCurrentStandardFile;
	void ReleaseStandardFile()			{ if (m_poCurrentStandardFile!=0) delete m_poCurrentStandardFile; }
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FAAC_WINGUIDLG_H__DFE38E67_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
