// ProcessingStartStopPauseInteractable.h: interface for the CProcessingStartStopPauseInteractable class.
// Author: Torsten Landmann
//
// this class is an interface to start, stop or pause processes (i.e. job processing)
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROCESSINGSTARTSTOPPAUSEINTERACTABLE_H__A1444E90_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PROCESSINGSTARTSTOPPAUSEINTERACTABLE_H__A1444E90_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ProcessingStatusDialogInfoFeedbackCallbackInterface.h"

class CProcessingStartStopPauseInteractable  
{
public:
	CProcessingStartStopPauseInteractable();
	virtual ~CProcessingStartStopPauseInteractable();

	// the flow of the following three methods is as follows:
	// at the beginning there is a call to Start() WITH valid parameter
	// Stop() and Pause() can only occur after Start() has been called at least once;
	// When Stop() has been sent the called object should cleanup;
	// when Pause() has been sent the called object should be idle() until
	// a new call to Start() has taken place; the second call may contain
	// a 0 parameter, so the previous value is to be used

	virtual void Start(CProcessingStatusDialogInfoFeedbackCallbackInterface *poInfoTarget)=0;

	virtual void Stop()=0;
	virtual void Pause()=0;
};

#endif // !defined(AFX_PROCESSINGSTARTSTOPPAUSEINTERACTABLE_H__A1444E90_1546_11D5_8402_0080C88C25BD__INCLUDED_)
