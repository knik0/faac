// JobList.cpp: implementation of the CJobList class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "JobList.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CJobList::CJobList()
{

}

CJobList::~CJobList()
{

}

long CJobList::AddJob(const CJob &oJob, long lDesiredIndex)
{
	return AddNewElem(oJob, lDesiredIndex);
}

CJob* CJobList::GetJob(long lIndex)
{
	CJob *poJob;
	return GetElemContent(lIndex, poJob) ? poJob : 0;
}

CJob* CJobList::GetNextJob(CBListReader &oReader, long *plIndex)
{
	CJob *poJob;
	return GetNextElemContent(oReader, poJob, plIndex) ? poJob : 0;
}