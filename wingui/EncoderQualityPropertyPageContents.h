// EncoderQualityPropertyPageContents.h: interface for the CEncoderQualityPropertyPageContents class.
// Author: Torsten Landmann
//
// encapsulates the content of the property page "quality" including
// possible 3rd states of all controls
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERQUALITYPROPERTYPAGECONTENTS_H__7B47B26A_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERQUALITYPROPERTYPAGECONTENTS_H__7B47B26A_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AbstractPropertyPageContents.h"
#include "PageEditCtrlContent.h"
#include "PageCheckboxCtrlContent.h"
#include "PageRadioGroupCtrlContent.h"

class CEncoderQualityPropertyPageContents : public CAbstractPropertyPageContents  
{
public:
	CEncoderQualityPropertyPageContents();
	CEncoderQualityPropertyPageContents(const CEncoderQualityPropertyPageContents &oSource);	// copy constructor
	virtual ~CEncoderQualityPropertyPageContents();

	CPageEditCtrlContent m_oBitRate;
	CPageEditCtrlContent m_oBandwidth;
	CPageCheckboxCtrlContent m_oAllowMidSide;
	CPageCheckboxCtrlContent m_oUseTns;
	CPageCheckboxCtrlContent m_oUseLtp;
	CPageCheckboxCtrlContent m_oUseLfe;
	CPageRadioGroupCtrlContent m_oAacProfile;

	// with this operator pages for several jobs can be "merged"
	CEncoderQualityPropertyPageContents& operator*=(const CEncoderQualityPropertyPageContents &oRight);
};

#endif // !defined(AFX_ENCODERQUALITYPROPERTYPAGECONTENTS_H__7B47B26A_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
