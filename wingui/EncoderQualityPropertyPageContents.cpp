// EncoderQualityPropertyPageContents.cpp: implementation of the CEncoderQualityPropertyPageContents class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderQualityPropertyPageContents.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderQualityPropertyPageContents::CEncoderQualityPropertyPageContents()
{
}

CEncoderQualityPropertyPageContents::CEncoderQualityPropertyPageContents(const CEncoderQualityPropertyPageContents &oSource)
{
	*this=oSource;
}

CEncoderQualityPropertyPageContents::~CEncoderQualityPropertyPageContents()
{
}

CEncoderQualityPropertyPageContents& CEncoderQualityPropertyPageContents::operator*=(const CEncoderQualityPropertyPageContents &oRight)
{
	m_oBitRate*=oRight.m_oBitRate;
	m_oBandwidth*=oRight.m_oBandwidth;;
	m_oAllowMidSide*=oRight.m_oAllowMidSide;
	m_oUseTns*=oRight.m_oUseTns;
	m_oUseLtp*=oRight.m_oUseLtp;
	m_oUseLfe*=oRight.m_oUseLfe;
	m_oAacProfile*=oRight.m_oAacProfile;

	return *this;
}