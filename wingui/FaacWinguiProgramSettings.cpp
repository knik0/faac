// FaacWinguiProgramSettings.cpp: implementation of the CFaacWinguiProgramSettings class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FaacWinguiProgramSettings.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFaacWinguiProgramSettings::CFaacWinguiProgramSettings():
	m_oDefaultEncSourceDir(""),
	m_oDefaultEncTargetDir(""),
	m_lDefaultEncBitRate(64000),
	m_lDefaultEncBandwith(18000),
	m_bDefaultEncAllowMidSide(true),
	m_bDefaultEncUseTns(true),
	m_bDefaultEncUseLtp(true),
	m_bDefaultEncUseLfe(false),
	m_eDefaultEncAacProfile(CEncoderJob::eAacProfileMain),
	m_bDefaultEncCopyrightBit(false),
	m_oDefaultEncEncodedBy(""),
	m_oDefaultEncComment("")
{
	// use the temporary directory as target by standard if not otherwise
	// specified
	if (m_oDefaultEncTargetDir.IsEmpty())
	{
		CString oTempDir;
		strcpy(oTempDir.GetBuffer(1000), getenv("TEMP"));
		oTempDir.ReleaseBuffer();
		m_oDefaultEncTargetDir=oTempDir;
	}
	if (m_oDefaultEncTargetDir.IsEmpty())
	{
		CString oTempDir;
		strcpy(oTempDir.GetBuffer(1000), getenv("TMP"));
		oTempDir.ReleaseBuffer();
		m_oDefaultEncTargetDir=oTempDir;
	}
}

CFaacWinguiProgramSettings::~CFaacWinguiProgramSettings()
{

}

void CFaacWinguiProgramSettings::ApplyToJob(CJob &oJob) const
{
	CEncoderJob *poEncoderJob=oJob.GetEncoderJob();
	if (poEncoderJob==0)
	{
		ASSERT(false);
		return;
	}

	ApplyToJob(*poEncoderJob);
}

void CFaacWinguiProgramSettings::ApplyToJob(CEncoderJob &oJob) const
{
	oJob.GetFiles().SetSourceFileDirectory(m_oDefaultEncSourceDir);
	oJob.GetFiles().SetTargetFileDirectory(m_oDefaultEncTargetDir);
	oJob.SetBitRate(m_lDefaultEncBitRate);
	oJob.SetBandwidth(m_lDefaultEncBandwith);
	oJob.SetAllowMidside(m_bDefaultEncAllowMidSide);
	oJob.SetUseTns(m_bDefaultEncUseTns);
	oJob.SetUseLtp(m_bDefaultEncUseLtp);
	oJob.SetUseLfe(m_bDefaultEncUseLfe);
	oJob.SetAacProfile(m_eDefaultEncAacProfile);
	oJob.GetTargetFileId3Info().SetCopyright(m_bDefaultEncCopyrightBit);
	oJob.GetTargetFileId3Info().SetEncodedBy(m_oDefaultEncEncodedBy);
	oJob.GetTargetFileId3Info().SetComment(m_oDefaultEncComment);
}
