#if !defined(AFX_PROPERTIESDUMMYPARENTDIALOG_H__442115CA_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PROPERTIESDUMMYPARENTDIALOG_H__442115CA_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PropertiesDummyParentDialog.h : header file
//

#include "SupportedPropertyPagesData.h"
#include "PropertiesTabParentDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CPropertiesDummyParentDialog dialog

class CPropertiesDummyParentDialog : public CDialog
{
// Construction
public:
	CPropertiesDummyParentDialog(bool bDestroyParentWindowOnDestruction, CWnd* pParent = NULL);   // standard constructor
	virtual ~CPropertiesDummyParentDialog();

// Dialog Data
	//{{AFX_DATA(CPropertiesDummyParentDialog)
	enum { IDD = IDD_PROPERTIESDUMMYPARENTDIALOG };
	CStatic	m_ctrlLabelDebugTag;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPropertiesDummyParentDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:

	void SetContent(const CSupportedPropertyPagesData &oPagesToShow, const TItemList<CJob*> &oJobsToConfigure, CJobListUpdatable *poListContainer);

	void DefineDestroyParentWindowOnDestrution(bool bDestroyParentWindowOnDestruction);

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPropertiesDummyParentDialog)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	bool m_bInitialized;

	CPropertiesTabParentDialog *m_poCurrentPropertiesDialog;

	// these two members are used if SetContent is called before this
	// dialog has been initialized
	bool m_bSetContentHasBeenCalled;
	CSupportedPropertyPagesData m_oPagesToShowBuffer;
	TItemList<CJob*> m_oJobsToConfigureBuffer;
	CJobListUpdatable *m_poListContainer;

	bool m_bDestroyParentWindowOnDestruction;

	CRect m_oCurrentSize;

	void ApplyNewSize(const CRect &oSize);

	void DestroyCurrentPropertiesPage();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTIESDUMMYPARENTDIALOG_H__442115CA_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
