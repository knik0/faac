// AbstractJob.cpp: implementation of the CAbstractJob class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "AbstractJob.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAbstractJob::CAbstractJob():
	m_lThisJobCountNumber(-1),
	m_lTotalNumberOfJobs(-1),
	m_lThisSubJobCountNumber(-1),
	m_lTotalNumberOfSubJobs(-1)
{

}

CAbstractJob::~CAbstractJob()
{

}

void CAbstractJob::SetJobNumberInfo(long lThisJobCountNumber, long lTotalNumberOfJobs)
{
	m_lThisJobCountNumber=lThisJobCountNumber;
	m_lTotalNumberOfJobs=lTotalNumberOfJobs;
}

void CAbstractJob::SetSubJobNumberInfo(long lThisSubJobCountNumber, long lTotalNumberOfSubJobs)
{
	m_lThisSubJobCountNumber=lThisSubJobCountNumber;
	m_lTotalNumberOfSubJobs=lTotalNumberOfSubJobs;
}

void CAbstractJob::CopyAllJobNumberInfoFromJob(const CAbstractJob &oJob)
{
	m_lThisJobCountNumber=oJob.m_lThisJobCountNumber;
	m_lTotalNumberOfJobs=oJob.m_lTotalNumberOfJobs;
	m_lThisSubJobCountNumber=oJob.m_lThisSubJobCountNumber;
	m_lTotalNumberOfSubJobs=oJob.m_lTotalNumberOfSubJobs;
}

CString CAbstractJob::GetJobProcessingAdditionalCaptionBarInformation() const
{
	CString oJobInfo;
	if (m_lTotalNumberOfJobs>=0)
	{
		oJobInfo.Format(IDS_JobNofM, m_lThisJobCountNumber+1, m_lTotalNumberOfJobs);
	}
	if (m_lTotalNumberOfSubJobs>=0)
	{
		CString oSubJobInfo;
		oSubJobInfo.Format(IDS_SubJobNofM, m_lThisSubJobCountNumber+1, m_lTotalNumberOfSubJobs);

		oJobInfo+=CString(" - ")+oSubJobInfo;
	}

	return oJobInfo;
}
