// PageEditCtrlContent.h: interface for the CPageEditCtrlContent class.
// Author: Torsten Landmann
//
// is able to save the content of an edit control including the 3rd state
// and perform merge operations so that data for several jobs can be
// merged
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGEEDITCTRLCONTENT_H__7B47B267_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PAGEEDITCTRLCONTENT_H__7B47B267_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AbstractPageCtrlContent.h"

class CPageEditCtrlContent : public CAbstractPageCtrlContent
{
public:
	CPageEditCtrlContent();
	CPageEditCtrlContent(const CString &oEditCtrlContent);
	virtual ~CPageEditCtrlContent();

	// setting content automatically disables the 3rd state
	void SetContent(const CString &oEditCtrlContent);
	void SetContent(long lEditCtrlContent, bool bNegativeIs3rdState=false);
	const CString& GetContent() const;
	CString& GetContent();

	void ApplyToJob(CString &oNativeJobPropertyString) const;
	void ApplyToJob(long &lNativeJobPropertyLong) const;

	// implementation of CAbstractPageCtrlContent method
	virtual CString GetHashString() const;

private:
	CString m_oEditCtrlContent;
	static CString s_oEmptyString;
};

#endif // !defined(AFX_PAGEEDITCTRLCONTENT_H__7B47B267_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
