// FloatingPropertyDialog.cpp : implementation file
// Author: Torsten Landmann
//
///////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FloatingPropertyDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CFloatingPropertyDialog *CFloatingPropertyDialog::m_poFloatingPropertyDialogSingleton=0;

/////////////////////////////////////////////////////////////////////////////
// CFloatingPropertyDialog dialog


CFloatingPropertyDialog::CFloatingPropertyDialog(CWnd* pParent /*=NULL*/):
	m_poPropertiesDummyDialog(0)
{
	//{{AFX_DATA_INIT(CFloatingPropertyDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	Create(CFloatingPropertyDialog::IDD, pParent);
}

CFloatingPropertyDialog::~CFloatingPropertyDialog()
{
	if (m_poPropertiesDummyDialog!=0)
	{
		delete m_poPropertiesDummyDialog;
		m_poPropertiesDummyDialog=0;

		((CFaac_winguiApp*)AfxGetApp())->SetGlobalPropertiesDummyParentDialogSingleton(0);
	}
}

void CFloatingPropertyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFloatingPropertyDialog)
	DDX_Control(pDX, IDC_LABELDEBUGTAG, m_ctrlLabelDebugTag);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFloatingPropertyDialog, CDialog)
	//{{AFX_MSG_MAP(CFloatingPropertyDialog)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFloatingPropertyDialog message handlers

void CFloatingPropertyDialog::CreateFloatingPropertiesDummyParentDialog()
{
	if (m_poFloatingPropertyDialogSingleton==0)
	{
		// memory leak reported here is ok (leak is created immediately before
		// application exit)
		m_poFloatingPropertyDialogSingleton=new CFloatingPropertyDialog;
	}
}

void CFloatingPropertyDialog::InvalidFloatingPropertiesDialog()
{
	if (m_poFloatingPropertyDialogSingleton!=0)
	{
		delete m_poFloatingPropertyDialogSingleton;
		m_poFloatingPropertyDialogSingleton=0;
	}
}

void CFloatingPropertyDialog::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	m_oCurrentSize=CRect(0, 0, cx, cy);
	ApplyNewSize(m_oCurrentSize);
}

BOOL CFloatingPropertyDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	ShowWindow(SW_SHOW);
	m_poPropertiesDummyDialog=new CPropertiesDummyParentDialog(true, this);
	((CFaac_winguiApp*)AfxGetApp())->SetGlobalPropertiesDummyParentDialogSingleton(m_poPropertiesDummyDialog);
	ApplyNewSize(m_oCurrentSize);
	m_poPropertiesDummyDialog->ShowWindow(SW_SHOW);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFloatingPropertyDialog::ApplyNewSize(const CRect &oSize)
{
	if (m_poPropertiesDummyDialog!=0)
	{
		// dialog has been initialized

		m_poPropertiesDummyDialog->MoveWindow(oSize);

		CRect oLabelRect;
		m_ctrlLabelDebugTag.GetWindowRect(&oLabelRect);
		CRect oDebugLabelRect(oSize.BottomRight(), oLabelRect.Size());
		oDebugLabelRect.OffsetRect(-oLabelRect.Size().cx, -oLabelRect.Size().cy);
		m_ctrlLabelDebugTag.MoveWindow(oDebugLabelRect);
	}
}