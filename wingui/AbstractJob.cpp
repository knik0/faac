// AbstractJob.cpp: implementation of the CAbstractJob class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "AbstractJob.h"
#include "WindowUtil.h"

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
	m_lTotalNumberOfSubJobs(-1),
	m_eJobProcessingOutcome(eNotProcessed),
	m_lProcessingTime(0)
{

}

CAbstractJob::CAbstractJob(const CAbstractJob &oSource)
{
	*this=oSource;
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

void CAbstractJob::GetProcessingNumberInformation(long &lThisJobCountNumber, long &lTotalNumberOfJobs, long &lThisSubJobCountNumber, long &lTotalNumberOfSubJobs) const
{
	lThisJobCountNumber=m_lThisJobCountNumber;
	lTotalNumberOfJobs=m_lTotalNumberOfJobs;
	lThisSubJobCountNumber=m_lThisSubJobCountNumber;
	lTotalNumberOfSubJobs=m_lTotalNumberOfSubJobs;
}

void CAbstractJob::CopyAllJobNumberInfoFromJob(const CAbstractJob &oJob)
{
	oJob.GetProcessingNumberInformation(m_lThisJobCountNumber, m_lTotalNumberOfJobs, m_lThisSubJobCountNumber, m_lTotalNumberOfSubJobs);
}

CString CAbstractJob::GetJobProcessingAdditionalCaptionBarInformation() const
{
	// first get proper number info; must use method call for
	// that because the management might be subclassed
	long lThisJobCountNumber;
	long lTotalNumberOfJobs;
	long lThisSubJobCountNumber;
	long lTotalNumberOfSubJobs;
	GetProcessingNumberInformation(lThisJobCountNumber, lTotalNumberOfJobs, lThisSubJobCountNumber, lTotalNumberOfSubJobs);

	CString oJobInfo;
	if (lTotalNumberOfJobs>=0)
	{
		oJobInfo.Format(IDS_JobNofM, lThisJobCountNumber+1, lTotalNumberOfJobs);
	}
	if (lTotalNumberOfSubJobs>=0)
	{
		CString oSubJobInfo;
		oSubJobInfo.Format(IDS_SubJobNofM, lThisSubJobCountNumber+1, lTotalNumberOfSubJobs);

		oJobInfo+=CString(" - ")+oSubJobInfo;
	}

	return oJobInfo;
}

void CAbstractJob::SetProcessingOutcome(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingTime, const CString &oSupplementaryInfo)
{
	m_eJobProcessingOutcome=eJobProcessingOutcome;
	m_lProcessingTime=lProcessingTime;
	m_oSupplementaryInfo=oSupplementaryInfo;

	if (m_eJobProcessingOutcome==eError)
	{
		ASSERT(!m_oSupplementaryInfo.IsEmpty());
	}
}

void CAbstractJob::SetProcessingOutcomeCurTime(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingStartTime, const CString &oSupplementaryInfo)
{
	SetProcessingOutcome(eJobProcessingOutcome, ::GetTickCount()-lProcessingStartTime, oSupplementaryInfo);
}

void CAbstractJob::SetProcessingOutcome(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingTime, int iSupplementaryInfoStringResource)
{
	CString oSupplementaryInfo;
	oSupplementaryInfo.LoadString(iSupplementaryInfoStringResource);
	SetProcessingOutcome(eJobProcessingOutcome, lProcessingTime, oSupplementaryInfo);
}

void CAbstractJob::SetProcessingOutcomeCurTime(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingStartTime, int iSupplementaryInfoStringResource)
{
	CString oSupplementaryInfo;
	oSupplementaryInfo.LoadString(iSupplementaryInfoStringResource);
	SetProcessingOutcomeCurTime(eJobProcessingOutcome, lProcessingStartTime, oSupplementaryInfo);
}

void CAbstractJob::GetProcessingOutcome(EJobProcessingOutcome &eJobProcessingOutcome, long &lProcessingTime, CString &oSupplementaryInfo) const
{
	eJobProcessingOutcome=m_eJobProcessingOutcome;
	lProcessingTime=m_lProcessingTime;
	oSupplementaryInfo=m_oSupplementaryInfo;
}

CAbstractJob::EJobProcessingOutcome CAbstractJob::GetProcessingOutcomeSimple() const
{
	EJobProcessingOutcome eJobProcessingOutcome;
	long lProcessingTime;
	CString oSupplementaryInfo;
	GetProcessingOutcome(eJobProcessingOutcome, lProcessingTime, oSupplementaryInfo);
	return eJobProcessingOutcome;
}

CString CAbstractJob::GetProcessingOutcomeString() const
{
	CString oToReturn;

	// find out the outcome data
	EJobProcessingOutcome eJobProcessingOutcome;
	long lProcessingTime;
	CString oSupplementaryInfo;
	GetProcessingOutcome(eJobProcessingOutcome, lProcessingTime, oSupplementaryInfo);

	// find out a simple description for the outcome
	CString oSimpleOutcomeDesc;
	int iDescBuf;
	switch (eJobProcessingOutcome)
	{
	case eNotProcessed:
		{
			iDescBuf=IDS_OutcomeUnprocessed;
			break;
		}
	case eSuccessfullyProcessed:
		{
			iDescBuf=IDS_OutcomeSuccessfullyProcessed;
			break;
		}
	case ePartiallyProcessed:
		{
			iDescBuf=IDS_OutcomePartiallyProcessed;
			break;
		}
	case eUserAbort:
		{
			iDescBuf=IDS_OutcomeUserAbort;
			break;
		}
	case eError:
		{
			iDescBuf=IDS_OutcomeError;
			break;
		}
	default:
		{
			// unknown type of outcome
			ASSERT(false);
			iDescBuf=IDS_OutcomeError;
			break;
		}
	}
	oSimpleOutcomeDesc.LoadString(iDescBuf);

	if (oSupplementaryInfo.IsEmpty())
	{
		oToReturn=oSimpleOutcomeDesc;
	}
	else
	{
		oToReturn=oSimpleOutcomeDesc+": "+oSupplementaryInfo;
	}

	if (lProcessingTime>=10)
	{
		bool bIncludeMillis=true;
		if (lProcessingTime>=1000)
		{
			bIncludeMillis=false;
		}

		oToReturn+=_T(" (")+CWindowUtil::GetTimeDescription(lProcessingTime, bIncludeMillis)+_T(")");
	}

	return oToReturn;
}

void CAbstractJob::ResetProcessingOutcome()
{
	m_eJobProcessingOutcome=eNotProcessed;
	m_lProcessingTime=0;
	m_oSupplementaryInfo.Empty();
}
