// ProcessingStatusDialogInfoFeedbackCallbackInterface.h: interface for the CProcessingStatusDialogInfoFeedbackCallbackInterface class.
// Author: Torsten Landmann
//
// This class is an interface to enable a process to update the status
// dialog that displays the progress and further information
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROCESSINGSTATUSDIALOGINFOFEEDBACKCALLBACKINTERFACE_H__A1444E91_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PROCESSINGSTATUSDIALOGINFOFEEDBACKCALLBACKINTERFACE_H__A1444E91_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CProcessingStatusDialogInfoFeedbackCallbackInterface  
{
public:
	CProcessingStatusDialogInfoFeedbackCallbackInterface();
	virtual ~CProcessingStatusDialogInfoFeedbackCallbackInterface();

	// progress must be given in percent
	virtual void SetStatus(double dProgress, const CString &oTopStatusText, const CString &oBottomStatusText)=0;
	virtual void SetAvailableActions(bool bStop, bool bPause)=0;
	virtual void ProcessUserMessages()=0;
	virtual void SetAdditionalCaptionInfo(const CString &oAdditionalInfo)=0;

	// the return value is the status code to return to the caller;
	// if bSuccess then IDOK is returned, IDCANCEL otherwise
	virtual void ReturnToCaller(bool bSuccess)=0;
};

#endif // !defined(AFX_PROCESSINGSTATUSDIALOGINFOFEEDBACKCALLBACKINTERFACE_H__A1444E91_1546_11D5_8402_0080C88C25BD__INCLUDED_)
