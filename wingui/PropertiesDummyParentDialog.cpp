// PropertiesDummyParentDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "PropertiesDummyParentDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPropertiesDummyParentDialog dialog


CPropertiesDummyParentDialog::CPropertiesDummyParentDialog(
	bool bDestroyParentWindowOnDestruction, CWnd* pParent /*=NULL*/):
	m_bDestroyParentWindowOnDestruction(bDestroyParentWindowOnDestruction),
	m_poCurrentPropertiesDialog(0),
	m_bInitialized(false),
	m_bSetContentHasBeenCalled(false)
{
	//{{AFX_DATA_INIT(CPropertiesDummyParentDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	Create(CPropertiesDummyParentDialog::IDD, pParent);
}

CPropertiesDummyParentDialog::~CPropertiesDummyParentDialog()
{
	DestroyCurrentPropertiesPage();

	/*if (m_bDestroyParentWindowOnDestruction)
	{
		if (GetParent()!=0)
		{
			GetParent()->DestroyWindow();
		}
	}*/
}

void CPropertiesDummyParentDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPropertiesDummyParentDialog)
	DDX_Control(pDX, IDC_LABELDEBUGTAG, m_ctrlLabelDebugTag);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPropertiesDummyParentDialog, CDialog)
	//{{AFX_MSG_MAP(CPropertiesDummyParentDialog)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPropertiesDummyParentDialog message handlers

void CPropertiesDummyParentDialog::SetContent(
	const CSupportedPropertyPagesData &oPagesToShow,
	const TItemList<CJob*> &oJobsToConfigure,
	CJobListUpdatable *poListContainer)
{
	if (m_bInitialized)
	{
		m_bSetContentHasBeenCalled=false;

		DestroyCurrentPropertiesPage();

		m_poCurrentPropertiesDialog=new CPropertiesTabParentDialog(oPagesToShow, oJobsToConfigure, poListContainer, this);
		ApplyNewSize(m_oCurrentSize);
		m_poCurrentPropertiesDialog->ShowWindow(SW_SHOW);
	}
	else
	{
		m_bSetContentHasBeenCalled=true;

		m_oPagesToShowBuffer=oPagesToShow;
		m_oJobsToConfigureBuffer=oJobsToConfigure;
		m_poListContainer=poListContainer;
	}
}

void CPropertiesDummyParentDialog::DefineDestroyParentWindowOnDestrution(bool bDestroyParentWindowOnDestruction)
{
	m_bDestroyParentWindowOnDestruction=bDestroyParentWindowOnDestruction;
}

void CPropertiesDummyParentDialog::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	m_oCurrentSize=CRect(0, 0, cx, cy);
	
	// TODO: Add your message handler code here
	ApplyNewSize(m_oCurrentSize);
}

BOOL CPropertiesDummyParentDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_bInitialized=true;

	if (m_bSetContentHasBeenCalled)
	{
		SetContent(m_oPagesToShowBuffer, m_oJobsToConfigureBuffer, m_poListContainer);
	}
	ApplyNewSize(m_oCurrentSize);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPropertiesDummyParentDialog::ApplyNewSize(const CRect &oSize)
{
	if (m_bInitialized)
	{
		// dialog has been initialized

		if (m_poCurrentPropertiesDialog!=0)
		{
			m_poCurrentPropertiesDialog->MoveWindow(oSize);
		}

		CRect oLabelRect;
		m_ctrlLabelDebugTag.GetWindowRect(&oLabelRect);
		CRect oDebugLabelRect(oSize.BottomRight(), oLabelRect.Size());
		oDebugLabelRect.OffsetRect(-oLabelRect.Size().cx, -oLabelRect.Size().cy);
		m_ctrlLabelDebugTag.MoveWindow(oDebugLabelRect);
	}
}

void CPropertiesDummyParentDialog::DestroyCurrentPropertiesPage()
{
	if (m_poCurrentPropertiesDialog!=0)
	{
		delete m_poCurrentPropertiesDialog;
		m_poCurrentPropertiesDialog=0;
	}
}