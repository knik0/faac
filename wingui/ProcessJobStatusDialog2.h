#if !defined(AFX_PROCESSJOBSTATUSDIALOG_H__A1444E8F_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PROCESSJOBSTATUSDIALOG_H__A1444E8F_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessJobStatusDialog.h : header file
//

#include "ProcessingStatusDialogInfoFeedbackCallbackInterface.h"
#include "ProcessingStartStopPauseInteractable.h"

/////////////////////////////////////////////////////////////////////////////
// CProcessJobStatusDialog dialog

class CProcessJobStatusDialog : public CDialog, public CProcessingStatusDialogInfoFeedbackCallbackInterface
{
// Construction
public:
	CProcessJobStatusDialog(CProcessingStartStopPauseInteractable *poClient, CWnd* pParent = NULL);   // standard constructor
	virtual ~CProcessJobStatusDialog();

// Dialog Data
	//{{AFX_DATA(CProcessJobStatusDialog)
	enum { IDD = IDD_PROCESSJOBSTATUSDIALOG };
	CButton	m_ctrlButtonPause;
	CButton	m_ctrlButtonContinue;
	CButton	m_ctrlButtonAbort;
	CProgressCtrl	m_ctrlProgressBar;
	CString	m_oLabelBottomStatusText;
	CString	m_oLabelTopStatusText;
	//}}AFX_DATA

	// implementation to interface CProcessingStatusDialogInfoFeedbackCallbackInterface
	virtual void SetStatus(double dProgress, const CString &oTopStatusText, const CString &oBottomStatusText);
	virtual void SetAvailableActions(bool bStop, bool bPause);
	virtual void ProcessUserMessages();
	virtual void ReturnToCaller(bool bSuccess);



// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProcessJobStatusDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CProcessJobStatusDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonAbort();
	afx_msg void OnButtonPause();
	afx_msg void OnButtonContinue();
	afx_msg LRESULT OnStartProcessing(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CProcessingStartStopPauseInteractable *m_poClient;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSJOBSTATUSDIALOG_H__A1444E8F_1546_11D5_8402_0080C88C25BD__INCLUDED_)
