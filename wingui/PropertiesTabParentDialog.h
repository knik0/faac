#if !defined(AFX_PROPERTIESTABPARENTDIALOG_H__442115C1_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PROPERTIESTABPARENTDIALOG_H__442115C1_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PropertiesTabParentDialog.h : header file
//

#include "SupportedPropertyPagesData.h"
#include "TItemList.h"
#include "Job.h"
#include "JobListUpdatable.h"

/////////////////////////////////////////////////////////////////////////////
// CPropertiesTabParentDialog dialog

class CPropertiesTabParentDialog : public CDialog
{
// Construction
public:
	CPropertiesTabParentDialog(const CSupportedPropertyPagesData &oPagesToShow, const TItemList<CJob*> &oJobsToConfigure, CJobListUpdatable *poListContainer, CWnd* pParent = NULL);   // standard constructor
	virtual ~CPropertiesTabParentDialog();

// Dialog Data
	//{{AFX_DATA(CPropertiesTabParentDialog)
	enum { IDD = IDD_PROPERTIESTABPARENTDIALOG };
	CStatic	m_ctrlLabelDebugTag;
	CStatic	m_ctrlLabelNoSelection;
	CTabCtrl	m_ctrlTab;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPropertiesTabParentDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPropertiesTabParentDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	CSupportedPropertyPagesData m_oPagesToShow;
	TItemList<CJob*> m_oJobsToConfigure;
	CJobListUpdatable *m_poListContainer;

	// if this dialog has already been initialized
	bool m_bInitialized;

	bool m_bTabsAdded;
	void AddTabs();
	bool AddTab(EPropertyPageType ePageType, int iTabId);	// also creates the tab page; returns false in case of errors
	bool ShowPage(int iId);

	CDialog *m_poCurrentPage;

	CRect m_oCurrentSize;
	CRect m_oCurrentPageSize;

	// this is just saved for the user's convenience
	static int s_iLastTabPage;

	void ApplyNewSize(const CRect &oSize, const CRect &oPageSize);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTIESTABPARENTDIALOG_H__442115C1_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
