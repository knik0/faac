// PageCheckboxCtrlContent.cpp: implementation of the CPageCheckboxCtrlContent class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "PageCheckboxCtrlContent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPageCheckboxCtrlContent::CPageCheckboxCtrlContent(bool bCheckMark):
	m_bCheckMark(bCheckMark)
{

}

CPageCheckboxCtrlContent::~CPageCheckboxCtrlContent()
{

}


CString CPageCheckboxCtrlContent::GetHashString() const
{
	return "CPageCheckboxCtrlContent-"+CString(m_bCheckMark ? "T" : "F");
}

void CPageCheckboxCtrlContent::ApplyCheckCodeToButton(CButton *poButton) const
{
	if (Is3rdState())
	{
		poButton->SetButtonStyle(BS_AUTO3STATE);
		poButton->SetCheck(2);
	}
	else
	{
		poButton->SetCheck(m_bCheckMark ? 1 : 0);
	}
}

bool CPageCheckboxCtrlContent::ApplyToJob(bool &bBool) const
{
	bool bOld=bBool;
	if (!Is3rdState())
	{
		bBool=GetCheckMark();
	}
	return bOld!=bBool;
}