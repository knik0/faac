// PropertiesTabParentDialog.cpp : implementation file
// Author: Torsten Landmann
//
///////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "PropertiesTabParentDialog.h"
#include "EncoderGeneralPageDialog.h"
#include "EncoderQualityPageDialog.h"
#include "EncoderId3PageDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int CPropertiesTabParentDialog::s_iLastTabPage=0;

/////////////////////////////////////////////////////////////////////////////
// CPropertiesTabParentDialog dialog


CPropertiesTabParentDialog::CPropertiesTabParentDialog(
	const CSupportedPropertyPagesData &oPagesToShow,
	const TItemList<CJob*> &oJobsToConfigure,
	CJobListUpdatable *poListContainer,
	CWnd* pParent /*=NULL*/):
	m_oPagesToShow(oPagesToShow),
	m_oJobsToConfigure(oJobsToConfigure),
	m_bTabsAdded(false),
	m_bInitialized(false),
	m_poCurrentPage(0),
	m_poListContainer(poListContainer)
{
	//{{AFX_DATA_INIT(CPropertiesTabParentDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	Create(CPropertiesTabParentDialog::IDD, pParent);
}

CPropertiesTabParentDialog::~CPropertiesTabParentDialog()
{
	if (m_poCurrentPage!=0)
	{
		delete m_poCurrentPage;
		m_poCurrentPage=0;
	}
}

void CPropertiesTabParentDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPropertiesTabParentDialog)
	DDX_Control(pDX, IDC_LABELDEBUGTAG, m_ctrlLabelDebugTag);
	DDX_Control(pDX, IDC_LABELNOSELECTION, m_ctrlLabelNoSelection);
	DDX_Control(pDX, IDC_TAB1, m_ctrlTab);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPropertiesTabParentDialog, CDialog)
	//{{AFX_MSG_MAP(CPropertiesTabParentDialog)
	ON_WM_SIZE()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelchangeTab1)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPropertiesTabParentDialog message handlers


BOOL CPropertiesTabParentDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_bInitialized=true;

	ApplyNewSize(m_oCurrentSize, m_oCurrentPageSize);

	if (m_oPagesToShow.GetNumberOfPages()>0)
	{
		m_ctrlTab.ShowWindow(SW_SHOW);
		m_ctrlLabelNoSelection.ShowWindow(SW_HIDE);

		AddTabs();

		// restore last selected tab page
		if (s_iLastTabPage>=m_oPagesToShow.GetNumberOfPages())
		{
			s_iLastTabPage=0;
		}
		m_ctrlTab.SetCurSel(s_iLastTabPage);
		ShowPage(s_iLastTabPage);
	}
	else
	{
		m_ctrlTab.ShowWindow(SW_HIDE);
		m_ctrlLabelNoSelection.ShowWindow(SW_SHOW);
	}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPropertiesTabParentDialog::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	m_oCurrentSize=CRect(0, 0, cx, cy);
	m_oCurrentPageSize=m_oCurrentSize;
	m_oCurrentPageSize.left+=6;
	m_oCurrentPageSize.top+=25;
	m_oCurrentPageSize.right-=6;
	m_oCurrentPageSize.bottom-=6;
	
	ApplyNewSize(m_oCurrentSize, m_oCurrentPageSize);
}

void CPropertiesTabParentDialog::AddTabs()
{
	if (m_bTabsAdded) return;

	// first insert the tabs

	int iCurTabPage=0;
	m_oPagesToShow.Sort();
	CBListReader oReader(m_oPagesToShow);
	EPropertyPageType eCurrentPageType;
	while (m_oPagesToShow.GetNextElemContent(oReader, eCurrentPageType))
	{
		if (AddTab(eCurrentPageType, iCurTabPage))
		{
			iCurTabPage++;
		}
	}
	m_bTabsAdded=true;
}

bool CPropertiesTabParentDialog::AddTab(EPropertyPageType ePageType, int iTabId)
{
	// create the tab page
	m_ctrlTab.InsertItem(iTabId, CSupportedPropertyPagesData::GetPageDescriptionShort(ePageType));

	return true;
}


bool CPropertiesTabParentDialog::ShowPage(int iId)
{
	if (m_poCurrentPage!=0)
	{
		delete m_poCurrentPage;
		m_poCurrentPage=0;
	}

	EPropertyPageType ePageType;
	if (!m_oPagesToShow.GetPosContent(iId, ePageType))
	{
		// to many tabs
		ASSERT(false);
		return false;
	}

	switch (ePageType)
	{
	case ePageTypeEncoderGeneral:
		{
			m_poCurrentPage=new CEncoderGeneralPageDialog(m_oJobsToConfigure, m_poListContainer, this);
			break;
		}
	case ePageTypeEncoderQuality:
		{
			m_poCurrentPage=new CEncoderQualityPageDialog(m_oJobsToConfigure, m_poListContainer, this);
			break;
		}
	case ePageTypeEncoderID3:
		{
			m_poCurrentPage=new CEncoderId3PageDialog(m_oJobsToConfigure, m_poListContainer, this);
			break;
		}
	default:
		{
			// unknown type of property page
			ASSERT(false);
			return false;
		}
	}
	m_poCurrentPage->MoveWindow(m_oCurrentPageSize);
	m_poCurrentPage->ShowWindow(SW_SHOW);

	s_iLastTabPage=iId;

	return true;
}

void CPropertiesTabParentDialog::OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here

	ShowPage(m_ctrlTab.GetCurSel());
	
	*pResult = 0;
}

void CPropertiesTabParentDialog::ApplyNewSize(const CRect &oSize, const CRect &oPageSize)
{
	// TODO: Add your message handler code here
	if (m_bInitialized)
	{
		// resize the tab control
		m_ctrlTab.MoveWindow(oSize);

		// resize the page
		if (m_poCurrentPage!=0)
		{
			m_poCurrentPage->MoveWindow(oPageSize);
		}

		// move debug tag label
		CRect oLabelRect;
		m_ctrlLabelDebugTag.GetWindowRect(&oLabelRect);
		CRect oDebugLabelRect(oSize.BottomRight(), oLabelRect.Size());
		oDebugLabelRect.OffsetRect(-oLabelRect.Size().cx, -oLabelRect.Size().cy);
		m_ctrlLabelDebugTag.MoveWindow(oDebugLabelRect);

		// move no selection label
		m_ctrlLabelNoSelection.GetWindowRect(oLabelRect);
		CPoint oCurLabelCenter(oLabelRect.CenterPoint());
		CPoint oDesiredLabelCenter(oSize.CenterPoint());
		CPoint oOffset(oDesiredLabelCenter-oCurLabelCenter);
		oLabelRect.OffsetRect(oOffset);
		m_ctrlLabelNoSelection.MoveWindow(oLabelRect);
	}
}
