// ProcessJobStatusDialog.cpp : implementation file
//

#include "stdafx.h"
#include "faac_wingui.h"
#include "ProcessJobStatusDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef SW_VIS_CODE
#define SW_VIS_CODE(bVisible)		((bVisible) ? SW_SHOW : SW_HIDE)
#endif

CProcessJobStatusDialog *CProcessJobStatusDialog::m_poDialog=0;

/////////////////////////////////////////////////////////////////////////////
// CProcessJobStatusDialog dialog


CProcessJobStatusDialog::CProcessJobStatusDialog(CProcessingStartStopPauseInteractable *poClient, CWnd* pParent /*=NULL*/)
	: CDialog(CProcessJobStatusDialog::IDD, pParent),
	m_poClient(poClient)
{
	//{{AFX_DATA_INIT(CProcessJobStatusDialog)
	m_oLabelBottomStatusText = _T("");
	m_oLabelTopStatusText = _T("");
	//}}AFX_DATA_INIT
}

CProcessJobStatusDialog::~CProcessJobStatusDialog()
{
}

void CProcessJobStatusDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CProcessJobStatusDialog)
	DDX_Control(pDX, IDC_BUTTONPAUSE, m_ctrlButtonPause);
	DDX_Control(pDX, IDC_BUTTONCONTINUE, m_ctrlButtonContinue);
	DDX_Control(pDX, IDC_BUTTONABORT, m_ctrlButtonAbort);
	DDX_Control(pDX, IDC_PROGRESS1, m_ctrlProgressBar);
	DDX_Text(pDX, IDC_LABELBOTTOMSTATUSTEXT, m_oLabelBottomStatusText);
	DDX_Text(pDX, IDC_LABELTOPSTATUSTEXT, m_oLabelTopStatusText);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CProcessJobStatusDialog, CDialog)
	//{{AFX_MSG_MAP(CProcessJobStatusDialog)
	ON_BN_CLICKED(IDC_BUTTONABORT, OnButtonAbort)
	ON_BN_CLICKED(IDC_BUTTONPAUSE, OnButtonPause)
	ON_BN_CLICKED(IDC_BUTTONCONTINUE, OnButtonContinue)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProcessJobStatusDialog message handlers

BOOL CProcessJobStatusDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_ctrlProgressBar.SetRange32(0, 10000);

	// install a timer to initiiate the processing
	ASSERT(m_poDialog==0);
	m_poDialog=this;
	::SetTimer(*this, 100, 200, TimerProc);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CProcessJobStatusDialog::SetStatus(double dProgress, const CString &oTopStatusText, const CString &oBottomStatusText)
{
	m_ctrlProgressBar.SetPos((int)dProgress*100);
	m_oLabelTopStatusText=oTopStatusText;
	m_oLabelBottomStatusText=oBottomStatusText;
	UpdateData(FALSE);
}

void CProcessJobStatusDialog::SetAvailableActions(bool bStop, bool bPause)
{
	m_ctrlButtonAbort.ShowWindow(SW_VIS_CODE(bStop));
	m_ctrlButtonPause.ShowWindow(SW_VIS_CODE(bPause));
	m_ctrlButtonContinue.ShowWindow(SW_VIS_CODE(bPause));
}

void CProcessJobStatusDialog::ProcessUserMessages()
{
	MSG msg;
	while (::PeekMessage(&msg, /**this*/0, 0, 0, PM_REMOVE)) 
	{ 
		::TranslateMessage(&msg);
		::DispatchMessage(&msg); 
	}
}

void CProcessJobStatusDialog::ReturnToCaller(bool bSuccess)
{
	if (bSuccess)
	{
		CDialog::OnOK();
	}
	else
	{
		CDialog::OnCancel();
	}
}

void CALLBACK EXPORT CProcessJobStatusDialog::TimerProc(
	   HWND hWnd,      // handle of CWnd that called SetTimer
	   UINT nMsg,      // WM_TIMER
	   UINT nIDEvent,  // timer identification
	   DWORD dwTime    // system time
	)
{
	// kill the timer
	::KillTimer(hWnd, nIDEvent);

	if (m_poDialog==0)
	{
		// ignore timer
		return;
	}

	ASSERT(m_poDialog!=0);

	// initiate the processing
	CProcessJobStatusDialog *poDialog=m_poDialog;
	m_poDialog=0;
	poDialog->m_poClient->Start(poDialog);
}

void CProcessJobStatusDialog::OnButtonAbort() 
{
	// TODO: Add your control notification handler code here
	m_poClient->Stop();
}

void CProcessJobStatusDialog::OnButtonPause() 
{
	// TODO: Add your control notification handler code here
	m_poClient->Pause();
}

void CProcessJobStatusDialog::OnButtonContinue() 
{
	// TODO: Add your control notification handler code here
	m_poClient->Start(0);
}
