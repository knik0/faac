// JobListsToConfigureSaver.cpp: implementation of the CJobListsToConfigureSaver class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "JobListsToConfigureSaver.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CJobListsToConfigureSaver::CJobListsToConfigureSaver()
{

}

CJobListsToConfigureSaver::CJobListsToConfigureSaver(TItemList<CJob*> &oJobs)
{
	SaveJobs(oJobs);
}

CJobListsToConfigureSaver::~CJobListsToConfigureSaver()
{

}

void CJobListsToConfigureSaver::SaveJobs(TItemList<CJob*> &oJobs)
{
	m_oSavedJobs.DeleteAll();
	CBListReader oReader(oJobs);
	CJob *poCurJob;
	long lCurJobIndex;
	while (oJobs.GetNextElemContent(oReader, poCurJob, &lCurJobIndex))
	{
		m_oSavedJobs.AddJob(*poCurJob, lCurJobIndex);
	}
}

void CJobListsToConfigureSaver::RestoreJobs(TItemList<CJob*> &oJobs)
{
	CBListReader oReader(oJobs);
	CJob *poCurJob;
	long lCurJobIndex;
	while (oJobs.GetNextElemContent(oReader, poCurJob, &lCurJobIndex))
	{
		// find the referring job in the list of saved jobs
		CJob *poSavedJob=m_oSavedJobs.GetJob(lCurJobIndex);
		if (poSavedJob==0)
		{
			// looks like you have modified the list structur between
			// the load and save operation
			ASSERT(false);
		}
		else
		{
			// copy in the saved content for the current job
			*poCurJob=*poSavedJob;
		}
	}
}