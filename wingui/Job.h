// Job.h: interface for the CJob class.
// Author: Torsten Landmann
//
// represents a certain block of work (e.g. one encoding job or one
// decoding job)
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOB_H__DFE38E6F_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOB_H__DFE38E6F_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"
#include "EncoderJob.h"
#include "ConcreteJobBase.h"

class CJob : public CGenericSortable, public CConcreteJobBase
{
public:
	enum EJobType
	{
		eUndefined,
		eEncoderJob,
	};

	CJob();
	CJob(const CEncoderJob &oEncoderJob);
	CJob(const CJob &oSource);		// copy constructor
	virtual ~CJob();

	// each of the following methods overwrites the settings
	// of a previous such method call
	void SetEncoderJob(const CEncoderJob &oEncoderJob);

	EJobType GetJobType() const							{ return m_eJobType; }
	// each of the following methods returns 0 if the content
	// of this object doesn't match the method called
	const CEncoderJob* GetEncoderJob() const			{ return m_eJobType==eEncoderJob ? (CEncoderJob*)m_poJob : 0; }
	CEncoderJob* GetEncoderJob()						{ return m_eJobType==eEncoderJob ? (CEncoderJob*)m_poJob : 0; }

	CJob& operator=(const CJob &oRight);


	// implementations to CJobListCtrlDescribable
	virtual CString DescribeJobTypeShort() const;
	virtual CString DescribeJobTypeLong() const;
	virtual CString DescribeJob() const;

	// implementations to CAbstractJob
	virtual CSupportedPropertyPagesData GetSupportedPropertyPages() const;
	virtual bool ProcessJob(CJobProcessingDynamicUserInputInfo &oUserInputInfo);
	virtual CString GetDetailedDescriptionForStatusDialog() const;
	// overrides to CAbstractJob
	virtual void SetJobNumberInfo(long lThisJobCountNumber, long lTotalNumberOfJobs);
	virtual void SetSubJobNumberInfo(long lThisSubJobCountNumber, long lTotalNumberOfSubJobs);
	virtual void GetProcessingNumberInformation(long &lThisJobCountNumber, long &lTotalNumberOfJobs, long &lThisSubJobCountNumber, long &lTotalNumberOfSubJobs) const;
	virtual void SetProcessingOutcome(EJobProcessingOutcome eJobProcessingOutcome, long lProcessingTime, const CString &oSupplementaryInfo);
	virtual void GetProcessingOutcome(EJobProcessingOutcome &eJobProcessingOutcome, long &lProcessingTime, CString &oSupplementaryInfo) const;
	virtual void ResetProcessingOutcome();

	// implementations to CFileSerializable
	virtual bool PutToArchive(CArchive &oArchive) const;
	virtual bool GetFromArchive(CArchive &oArchive);

private:
	EJobType m_eJobType;

	// this member contains an object of the type defined by
	// m_eJobType
	CConcreteJobBase *m_poJob;

	void ResetContent();
};

#endif // !defined(AFX_JOB_H__DFE38E6F_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
