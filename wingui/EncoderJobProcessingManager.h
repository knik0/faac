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
	CEncoderJobProcessingManager(CEncoderJob *poJobToProcess, CJobProcessingDynamicUserInputInfo &oUserInputInfo);
	virtual ~CEncoderJobProcessingManager();

	// encoder jobs should call this dialog before creating and starting the status dialog
	// for this processing manager; if this member returns false you can stop processing
	// of this job; this case is identical with that the the DoModal() function of the
	// status returns something different from IDOK
	bool MayStartProcessingWithStatusDialog();

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
	CEncoderJob *m_poJobToProcess;
	CProcessingStatusDialogInfoFeedbackCallbackInterface *m_poInfoTarget;
	CJobProcessingDynamicUserInputInfo &m_oUserInputInfo;

	// returns true if the job has been completely processed
	bool DoProcessing();

	void WriteProgress(long lOperationStartTickCount, long lMaxSteps, long lCurSteps);

	static int GetAacProfileConstant(CEncoderJob::EAacProfile eAacProfile);
	static int GetMpegVersionConstant(CEncoderJob::EMpegVersion eMpegVersion);

	// opens an archive that writes in the specified file, 0 in
	// case of errors;
	// the caller must specify two pointers that are both initialized
	// by this method; the caller must delete these two objects when
	// he's finished - first the archive, then the file;
	// this method returns false in case of errors; then neither of
	// the two pointers must be tried to be deleted
	static bool OpenOutputFileArchive(const CString &oFileName, CFile* &poFile, CArchive* &poArchive);
};

#endif // !defined(AFX_ENCODERJOBPROCESSINGMANAGER_H__A1444E93_1546_11D5_8402_0080C88C25BD__INCLUDED_)
