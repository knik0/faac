// faac_wingui.cpp : Defines the class behaviors for the application.
// Author: MSVC6.0 Wizard and Torsten Landmann :)
//
///////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "faac_winguiDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiApp

BEGIN_MESSAGE_MAP(CFaac_winguiApp, CWinApp)
	//{{AFX_MSG_MAP(CFaac_winguiApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiApp construction

CFaac_winguiApp::CFaac_winguiApp():
	m_poCurPropertiesDummyParentDialogSingletonContainer(0)
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CFaac_winguiApp object

CFaac_winguiApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiApp initialization

BOOL CFaac_winguiApp::InitInstance()
{
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CFaac_winguiDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	PerformAppCleanup();

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

void CFaac_winguiApp::SetGlobalPropertiesDummyParentDialogSingleton(
	CPropertiesDummyParentDialog *poPropertyContainer)
{
	if (m_poCurPropertiesDummyParentDialogSingletonContainer!=0)
	{
		//m_poCurPropertiesDummyParentDialogSingletonContainer->DestroyWindow();
		delete m_poCurPropertiesDummyParentDialogSingletonContainer;
		m_poCurPropertiesDummyParentDialogSingletonContainer=0;
	}

	m_poCurPropertiesDummyParentDialogSingletonContainer=poPropertyContainer;
}

CPropertiesDummyParentDialog* CFaac_winguiApp::GetGlobalPropertiesDummyParentDialogSingleton()
{
	return m_poCurPropertiesDummyParentDialogSingletonContainer;
}

void CFaac_winguiApp::PerformAppCleanup()
{
	if (m_poCurPropertiesDummyParentDialogSingletonContainer!=0)
	{
		delete m_poCurPropertiesDummyParentDialogSingletonContainer;
		m_poCurPropertiesDummyParentDialogSingletonContainer=0;
	}
}