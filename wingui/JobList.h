// JobList.h: interface for the CJobList class.
// Author: Torsten Landmann
//
// This class is used to save lists of jobs. Note the class
// CFileSerializableJobList, which is used to save and load job
// lists to and from files.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOBLIST_H__DFE38E75_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOBLIST_H__DFE38E75_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"
#include "Job.h"

class CJobList : public TItemList<CJob>  
{
public:
	CJobList();
	virtual ~CJobList();

	long AddJob(const CJob &oJob, long lDesiredIndex=-1);
	CJob* GetJob(long lIndex);
	CJob* GetNextJob(CBListReader &oReader, long *plIndex=0);
};

#endif // !defined(AFX_JOBLIST_H__DFE38E75_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
