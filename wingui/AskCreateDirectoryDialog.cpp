// AskCreateDirectoryDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "AskCreateDirectoryDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAskCreateDirectoryDialog dialog


CAskCreateDirectoryDialog::CAskCreateDirectoryDialog(const CString &oDirectory, CWnd* pParent /*=NULL*/)
	: CDialog(CAskCreateDirectoryDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAskCreateDirectoryDialog)
	m_oLabelTargetDir = _T("");
	//}}AFX_DATA_INIT
	m_oLabelTargetDir=oDirectory;
}

CAskCreateDirectoryDialog::~CAskCreateDirectoryDialog()
{
}

void CAskCreateDirectoryDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAskCreateDirectoryDialog)
	DDX_Text(pDX, IDC_LABELTARGETDIR, m_oLabelTargetDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAskCreateDirectoryDialog, CDialog)
	//{{AFX_MSG_MAP(CAskCreateDirectoryDialog)
	ON_BN_CLICKED(IDC_BUTTONYES, OnButtonYes)
	ON_BN_CLICKED(IDC_BUTTONNO, OnButtonNo)
	ON_BN_CLICKED(IDC_BUTTONALWAYS, OnButtonAlways)
	ON_BN_CLICKED(IDC_BUTTONNEVER, OnButtonNever)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAskCreateDirectoryDialog message handlers

void CAskCreateDirectoryDialog::OnButtonYes() 
{
	// TODO: Add your control notification handler code here
	EndDialog(eYes);
}

void CAskCreateDirectoryDialog::OnButtonNo() 
{
	// TODO: Add your control notification handler code here
	EndDialog(eNo);
}

void CAskCreateDirectoryDialog::OnButtonAlways() 
{
	// TODO: Add your control notification handler code here
	EndDialog(eAlways);
}

void CAskCreateDirectoryDialog::OnButtonNever() 
{
	// TODO: Add your control notification handler code here
	EndDialog(eNever);
}
