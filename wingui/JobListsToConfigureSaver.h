// JobListsToConfigureSaver.h: interface for the CJobListsToConfigureSaver class.
// Author: Torsten Landmann
//
// This class is able to save jobs that are available only as pointers
// so that their contents can be restored later; note that it is assumed
// that the structure of the list (number and indexes of elements) does
// not change between the save and the restore operation
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOBLISTSTOCONFIGURESAVER_H__A1444E82_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOBLISTSTOCONFIGURESAVER_H__A1444E82_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Job.h"
#include "JobList.h"

class CJobListsToConfigureSaver  
{
public:
	CJobListsToConfigureSaver();
	CJobListsToConfigureSaver(TItemList<CJob*> &oJobs);	// implies the save operation
	virtual ~CJobListsToConfigureSaver();

	// you can assume that both methods accept a const parameter; the
	// only reason that it isn't really a const parameter is that
	// we need a read session
	void SaveJobs(TItemList<CJob*> &oJobs);
	void RestoreJobs(TItemList<CJob*> &oJobs);

private:
	CJobList m_oSavedJobs;
};

#endif // !defined(AFX_JOBLISTSTOCONFIGURESAVER_H__A1444E82_1546_11D5_8402_0080C88C25BD__INCLUDED_)
