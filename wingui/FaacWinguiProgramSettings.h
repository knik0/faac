// FaacWinguiProgramSettings.h: interface for the CFaacWinguiProgramSettings class.
// Author: Torsten Landmann
//
// container for program defaults
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FAACWINGUIPROGRAMSETTINGS_H__A1444E89_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FAACWINGUIPROGRAMSETTINGS_H__A1444E89_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Job.h"
#include "EncoderJob.h"

class CFaacWinguiProgramSettings  
{
public:
	CFaacWinguiProgramSettings();
	virtual ~CFaacWinguiProgramSettings();

	CString m_oDefaultEncSourceDir;
	CString m_oDefaultEncTargetDir;
	long m_lDefaultEncBitRate;
	long m_lDefaultEncBandwith;
	bool m_bDefaultEncAllowMidSide;
	bool m_bDefaultEncUseTns;
	bool m_bDefaultEncUseLtp;
	bool m_bDefaultEncUseLfe;
	CEncoderJob::EAacProfile m_eDefaultEncAacProfile;
	bool m_bDefaultEncCopyrightBit;
	CString m_oDefaultEncEncodedBy;
	CString m_oDefaultEncComment;

	// that's just necessary quite often: copy the defaults to a (new) job
	void ApplyToJob(CJob &oJob) const;
	void ApplyToJob(CEncoderJob &oJob) const;
};

#endif // !defined(AFX_FAACWINGUIPROGRAMSETTINGS_H__A1444E89_1546_11D5_8402_0080C88C25BD__INCLUDED_)
