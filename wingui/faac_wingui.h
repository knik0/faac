// faac_wingui.h : main header file for the FAAC_WINGUI application
// Author: MSVC6.0 Wizard and Torsten Landmann :)
// 
/////////////////////////////////////////////////////////////////////

#if !defined(AFX_FAAC_WINGUI_H__DFE38E65_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FAAC_WINGUI_H__DFE38E65_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "JobList.h"
#include "PropertiesDummyParentDialog.h"
#include "FaacWinguiProgramSettings.h"
#include "FloatingPropertyDialog.h"

/////////////////////////////////////////////////////////////////////////////
// CFaac_winguiApp:
// See faac_wingui.cpp for the implementation of this class
//

class CFaac_winguiApp : public CWinApp
{
public:
	CFaac_winguiApp();
	virtual ~CFaac_winguiApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFaac_winguiApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

	const CJobList& GetGlobalJobList() const			{ return m_oGlobalJobList; }
	CJobList& GetGlobalJobList()						{ return m_oGlobalJobList; }
	const CFaacWinguiProgramSettings& GetGlobalProgramSettings() const		{ return m_oGlobalProgramSettings; }
	CFaacWinguiProgramSettings& GetGlobalProgramSettings()					{ return m_oGlobalProgramSettings; }

	// these two members are not an exact pair (getter/setter); however they
	// work closely together
	void SetGlobalPropertiesDummyParentDialogSingleton(CPropertiesDummyParentDialog *poPropertyContainer, CFloatingPropertyDialog *poFloatingPropertiesDialog=0);
	CPropertiesDummyParentDialog* GetGlobalPropertiesDummyParentDialogSingleton();
	bool HaveFloatingProperties() const;
	CFloatingPropertyDialog* GetFloatingPropertiesDialog() const;

// Implementation

	//{{AFX_MSG(CFaac_winguiApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	// this list is the global job list within this application
	CJobList m_oGlobalJobList;

	// this object contains all global program settings
	CFaacWinguiProgramSettings m_oGlobalProgramSettings;

	// global program settings
	CFaacWinguiProgramSettings m_oProgramSettings;

	// this member saves a pointer to the entry point to display
	// properties of jobs
	CPropertiesDummyParentDialog *m_poCurPropertiesDummyParentDialogSingletonContainer;
	CFloatingPropertyDialog *m_poFloatingPropertiesDialog;


	// something like the destructor of the application
	void PerformAppCleanup();
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FAAC_WINGUI_H__DFE38E65_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
