// PageRadioGroupCtrlContent.h: interface for the CPageRadioGroupCtrlContent class.
// Author: Torsten Landmann
//
// is able to save the content of a radio button group including the 3rd state
// and perform merge operations so that data for several jobs can be
// merged
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGERADIOGROUPCTRLCONTENT_H__9818D045_158E_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PAGERADIOGROUPCTRLCONTENT_H__9818D045_158E_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PageEditCtrlContent.h"

class CPageRadioGroupCtrlContent : public CPageEditCtrlContent
{
public:
	CPageRadioGroupCtrlContent();
	virtual ~CPageRadioGroupCtrlContent();

	// jobs should only call the SetContent(long) and ApplyToJob(long) methods,
	// which are inherited from CPageEditCtrlContent

	void GetFromRadioGroupVariable(int iVariable, long lNumberOfRadiosInGroup)			{ SetContent(iVariable, true); if (iVariable>=lNumberOfRadiosInGroup) SetIs3rdState(true); }
	void ApplyToRadioGroupVariable(int &iVariable) const								{ long lContent=-1; ApplyToJob(lContent); iVariable=lContent; }

	// implementation of CAbstractPageCtrlContent method
	virtual CString GetHashString() const;

private:
	// have no own data members
};

#endif // !defined(AFX_PAGERADIOGROUPCTRLCONTENT_H__9818D045_158E_11D5_8402_0080C88C25BD__INCLUDED_)
