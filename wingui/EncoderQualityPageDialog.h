// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERQUALITYPAGEDIALOG_H__7B47B264_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERQUALITYPAGEDIALOG_H__7B47B264_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EncoderQualityPageDialog.h : header file
//

#include "JobListUpdatable.h"
#include "JobListsToConfigureSaver.h"

/////////////////////////////////////////////////////////////////////////////
// CEncoderQualityPageDialog dialog

class CEncoderQualityPageDialog : public CDialog
{
// Construction
public:
	CEncoderQualityPageDialog(const TItemList<CJob*> &oJobsToConfigure, CJobListUpdatable *poListContainer, CWnd* pParent = NULL);   // standard constructor
	virtual ~CEncoderQualityPageDialog();

// Dialog Data
	//{{AFX_DATA(CEncoderQualityPageDialog)
	enum { IDD = IDD_ENCODERQUALITYPAGEDIALOG };
	CButton	m_ctrlCheckUseTns;
	CButton	m_ctrlCheckUseLtp;
	CEdit	m_ctrlEditBandwidth;
	CEdit	m_ctrlEditBitRate;
	CButton	m_ctrlCheckUseLfe;
	CButton	m_ctrlCheckMidSide;
	CString	m_oEditBandwidth;
	CString	m_oEditBitRate;
	int		m_iRadioAacProfile;
	int		m_iRadioMpegVersion;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CEncoderQualityPageDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	// returns false in case of errors
	bool GetPageContents(CEncoderQualityPropertyPageContents &oTarget);
	void ApplyPageContents(const CEncoderQualityPropertyPageContents &oPageContents);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CEncoderQualityPageDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateEditBitRate();
	afx_msg void OnUpdateEditBandwidth();
	afx_msg void OnKillfocusEditBandWidth();
	afx_msg void OnKillfocusEditBitRate();
	afx_msg void OnCheckMidSide();
	afx_msg void OnCheckUseLfe();
	afx_msg void OnCheckUseLtp();
	afx_msg void OnCheckUseTns();
	afx_msg void OnRadioAacProfileLc();
	afx_msg void OnRadioAacProfileMain();
	afx_msg void OnRadioAacProfileSsr();
	afx_msg void OnRadioAacProfileLtp();
	afx_msg void OnRadioMpegVersion2();
	afx_msg void OnRadioMpegVersion4();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	bool m_bInitialized;

	TItemList<CJob*> m_oJobsToConfigure;

	CJobListUpdatable *m_poListContainer;

	CEncoderQualityPropertyPageContents ParseJobs();
	void ModifyJobs(const CEncoderQualityPropertyPageContents &oPageContents);

	// called when changes have been made in the dialog and are to
	// be synchronized with both "data holders"
	void UpdateJobs(bool bFinishCheckBoxSessions=true, bool bDlgDestructUpdate=false);
	bool m_bIgnoreUpdates;


	// these members are used for managing the check box controls and the
	// changes that are made with them in the dialog
	enum ETypeOfCheckBox
	{
		eNone,
		eAllowMidSide,
		eUseTns,
		eUseLtp,
		eUseLfe,
	} m_eCurCheckBox;
	CJobListsToConfigureSaver m_oCheckStateChangeStateSaver;
	void ProcessCheckBoxClick(CButton *poCheckBox, ETypeOfCheckBox eTypeOfCheckBox);
	void FinishCurrentCheckBoxSessionIfNecessary();
	void FinishCheckBoxSessionIfNecessary(CButton *poCheckBox);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ENCODERQUALITYPAGEDIALOG_H__7B47B264_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
