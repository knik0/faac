// EncoderGeneralPropertyPageContents.h: interface for the CEncoderGeneralPropertyPageContents class.
// Author: Torsten Landmann
//
// encapsulates the content of the property page "general" including
// possible 3rd states of all controls
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERGENERALPROPERTYPAGECONTENTS_H__7B47B266_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERGENERALPROPERTYPAGECONTENTS_H__7B47B266_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AbstractPropertyPageContents.h"
#include "PageEditCtrlContent.h"
#include "PageCheckboxCtrlContent.h"

class CEncoderGeneralPropertyPageContents : public CAbstractPropertyPageContents  
{
public:
	CEncoderGeneralPropertyPageContents();
	CEncoderGeneralPropertyPageContents(const CEncoderGeneralPropertyPageContents &oSource);	// copy constructor
	virtual ~CEncoderGeneralPropertyPageContents();

	CPageEditCtrlContent m_oSourceDirectory;
	CPageEditCtrlContent m_oSourceFile;
	CPageEditCtrlContent m_oTargetDirectory;
	CPageEditCtrlContent m_oTargetFile;
	CPageCheckboxCtrlContent m_oSourceFileFilterIsRecursive;

	// not represented by real controls; just a tracker
	CPageCheckboxCtrlContent m_oSourceFilterRecursiveCheckboxVisible;

	// with this operator pages for several jobs can be "merged"
	CEncoderGeneralPropertyPageContents& operator*=(const CEncoderGeneralPropertyPageContents &oRight);
};

#endif // !defined(AFX_ENCODERGENERALPROPERTYPAGECONTENTS_H__7B47B266_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
