// AbstractJob.h: interface for the CAbstractJob class.
// Author: Torsten Landmann
//
// This is the base class for all job types
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ABSTRACTJOB_H__DFE38E74_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ABSTRACTJOB_H__DFE38E74_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SupportedPropertyPagesData.h"
#include "JobProcessingDynamicUserInputInfo.h"

class CAbstractJob  
{
public:
	CAbstractJob();
	CAbstractJob(const CAbstractJob &oSource);		// copy constructor
	virtual ~CAbstractJob();

	virtual CSupportedPropertyPagesData GetSupportedPropertyPages() const=0;

	// returns if the job was completely and successfully process
	virtual bool ProcessJob(CJobProcessingDynamicUserInputInfo &oUserInputInfo)=0;

	// is used during the processing of the job to give the user a
	// feedback what exactly is done; may use up to 3 lines
	virtual CString GetDetailedDescriptionForStatusDialog() const=0;

	// job processing information; all numbers are zero based;
	// subclasses may overwrite the first three members, if they do they
	// should overwrite them all together
	virtual void SetJobNumberInfo(long lThisJobCountNumber, long lTotalNumberOfJobs);
	virtual void SetSubJobNumberInfo(long lThisSubJobCountNumber, long lTotalNumberOfSubJobs);
	virtual void GetProcessingNumberInformation(long &lThisJobCountNumber, long &lTotalNumberOfJobs, long &lThisSubJobCountNumber, long &lTotalNumberOfSubJobs) const;
	void CopyAllJobNumberInfoFromJob(const CAbstractJob &oJob);
	CString GetJobProcessingAdditionalCaptionBarInformation() const;

	// job processing outcome information
	enum EJobProcessingOutcome
	{
		eNotProcessed,
		eSuccessfullyProcessed,
		ePartiallyProcessed,		// only for filter jobs
		eUserAbort,
		eError,
	};
	// note that supplementary info is mandatory if eOutcome==eError;
	// the processing time is specified in milliseconds;
	// SetProcessingOutcomeCurTime() requires that the second parameter is the
	// value that ::GetTickCount() returned immediately before the job processing began
	virtual void SetProcessingOutcome(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingTime, const CString &oSupplementaryInfo);
	void SetProcessingOutcome(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingTime, int iSupplementaryInfoStringResource);
	void SetProcessingOutcomeCurTime(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingStartTime, const CString &oSupplementaryInfo);
	void SetProcessingOutcomeCurTime(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingStartTime, int iSupplementaryInfoStringResource);
	virtual void GetProcessingOutcome(EJobProcessingOutcome &eJobProcessingOutcome, long &lProcessingTime, CString &oSupplementaryInfo) const;
	EJobProcessingOutcome GetProcessingOutcomeSimple() const;
	CString GetProcessingOutcomeString() const;
	// puts the job back to the "not processed" state
	virtual void ResetProcessingOutcome();

private:
	long m_lThisJobCountNumber;
	long m_lTotalNumberOfJobs;
	long m_lThisSubJobCountNumber;
	long m_lTotalNumberOfSubJobs;

	EJobProcessingOutcome m_eJobProcessingOutcome;
	long m_lProcessingTime;		// only valid if the outcome was success or user abort
	CString m_oSupplementaryInfo;
};

#endif // !defined(AFX_ABSTRACTJOB_H__DFE38E74_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
