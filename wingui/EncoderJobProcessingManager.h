// EncoderJobProcessingManager.h: interface for the CEncoderJobProcessingManager class.
// Author: Torsten Landmann
//
// manages the actual processing of one encoder job at a time. Note that this
// class doesn't properly process encoder jobs that have a file mask set
// as input file. Callers must filter out these jobs before using this class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERJOBPROCESSINGMANAGER_H__A1444E93_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERJOBPROCESSINGMANAGER_H__A1444E93_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "EncoderJob.h"
#include "ProcessingStartStopPauseInteractable.h"

class CEncoderJobProcessingManager : public CProcessingStartStopPauseInteractable  
{
public:
	CEncoderJobProcessingManager(const CEncoderJob *poJobToProcess);
	virtual ~CEncoderJobProcessingManager();

	virtual void Start(CProcessingStatusDialogInfoFeedbackCallbackInterface *poInfoTarget);

	virtual void Stop();
	virtual void Pause();

	enum ECurrentWorkingState
	{
		eInitial,
		eRunning,
		ePaused,
		eStopped,
		eCleanup,
	} m_eCurrentWorkingState;

private:
	const CEncoderJob *m_poJobToProcess;
	CProcessingStatusDialogInfoFeedbackCallbackInterface *m_poInfoTarget;

	// returns true if the job has been completely processed
	bool DoProcessing();

	void WriteProgress(long lOperationStartTickCount, long lMaxSteps, long lCurSteps);

	static int GetAacProfileConstant(CEncoderJob::EAacProfile eAacProfile);
};

#endif // !defined(AFX_ENCODERJOBPROCESSINGMANAGER_H__A1444E93_1546_11D5_8402_0080C88C25BD__INCLUDED_)
