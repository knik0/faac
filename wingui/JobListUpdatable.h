// JobListUpdatable.h: interface for the CJobListUpdatable class.
// Author: Torsten Landmann
//
// just an interface for better encapsulation
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOBLISTUPDATABLE_H__A1444E81_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOBLISTUPDATABLE_H__A1444E81_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ListCtrlStateSaver.h"

class CJobListUpdatable  
{
public:
	CJobListUpdatable();
	virtual ~CJobListUpdatable();

	// this method refills the job list control;
	// if no explicit selection state is specified the current
	// selection is preserved
	virtual void ReFillInJobListCtrl(CListCtrlStateSaver *poSelectionStateToUse=0, bool bSimpleUpdate=true)=0;
};

#endif // !defined(AFX_JOBLISTUPDATABLE_H__A1444E81_1546_11D5_8402_0080C88C25BD__INCLUDED_)
