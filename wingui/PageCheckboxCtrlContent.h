// PageCheckboxCtrlContent.h: interface for the CPageCheckboxCtrlContent class.
// Author: Torsten Landmann
//
// is able to save the content of a checkbox including the 3rd state
// and perform merge operations so that data for several jobs can be
// merged
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGECHECKBOXCTRLCONTENT_H__7B47B269_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PAGECHECKBOXCTRLCONTENT_H__7B47B269_0FF8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AbstractPageCtrlContent.h"

class CPageCheckboxCtrlContent : public CAbstractPageCtrlContent  
{
public:
	CPageCheckboxCtrlContent(bool bCheckMark=false);
	virtual ~CPageCheckboxCtrlContent();

	// setting content automatically disables the 3rd state
	void SetCheckMark(bool bCheckMark)		{ SetIs3rdState(false); m_bCheckMark=bCheckMark; }
	bool GetCheckMark() const				{ return m_bCheckMark; }

	// accepts values returned by CButton::GetCheck()
	void SetCheckCode(int iCheckCode)		{ SetIs3rdState(iCheckCode==2); m_bCheckMark=iCheckCode==1; }
	int GetCheckCode() const				{ if (Is3rdState()) return 2; else return m_bCheckMark ? 1 : 0; }
	void ApplyCheckCodeToButton(CButton *poButton) const;		// this member should prefered over GetCheckCode() because it also enables the 3rd state, if necessary

	// returns if the existing value has been modified
	bool ApplyToJob(bool &bBool) const;

	// implementation of CAbstractPageCtrlContent method
	virtual CString GetHashString() const;

private:
	bool m_bCheckMark;
};

#endif // !defined(AFX_PAGECHECKBOXCTRLCONTENT_H__7B47B269_0FF8_11D5_8402_0080C88C25BD__INCLUDED_)
