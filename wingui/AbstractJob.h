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

class CAbstractJob  
{
public:
	CAbstractJob();
	virtual ~CAbstractJob();

	virtual CSupportedPropertyPagesData GetSupportedPropertyPages() const=0;

	// returns if the job was completely and successfully process
	virtual bool ProcessJob() const=0;

	// is used during the processing of the job to give the user a
	// feedback what exactly is done; may use up to 3 lines
	virtual CString GetDetailedDescriptionForStatusDialog() const=0;
};

#endif // !defined(AFX_ABSTRACTJOB_H__DFE38E74_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
