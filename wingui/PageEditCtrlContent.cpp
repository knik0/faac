// PageEditCtrlContent.cpp: implementation of the CPageEditCtrlContent class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "PageEditCtrlContent.h"
#include "WindowUtil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

CString CPageEditCtrlContent::s_oEmptyString="";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPageEditCtrlContent::CPageEditCtrlContent()
{
}

CPageEditCtrlContent::CPageEditCtrlContent(const CString &oEditCtrlContent):
	m_oEditCtrlContent(oEditCtrlContent)
{
}

CPageEditCtrlContent::~CPageEditCtrlContent()
{

}

void CPageEditCtrlContent::SetContent(const CString &oEditCtrlContent)
{
	if (oEditCtrlContent.IsEmpty())
	{
		SetIs3rdState(true);
	}
	else
	{
		SetIs3rdState(false);
		m_oEditCtrlContent=oEditCtrlContent;
	}
}

void CPageEditCtrlContent::SetContent(long lEditCtrlContent, bool bNegativeIs3rdState)
{
	if (bNegativeIs3rdState && lEditCtrlContent<0)
	{
		SetIs3rdState(true);
		return;
	}

	m_oEditCtrlContent.Format("%d", lEditCtrlContent);
	SetIs3rdState(false);
}

const CString& CPageEditCtrlContent::GetContent() const
{
	if (Is3rdState())
	{
		return s_oEmptyString;
	}
	else
	{
		return m_oEditCtrlContent;
	}
}

CString& CPageEditCtrlContent::GetContent()
{
	if (Is3rdState())
	{
		return s_oEmptyString;
	}
	else
	{
		return m_oEditCtrlContent;
	}
}

bool CPageEditCtrlContent::ApplyToJob(CString &oNativeJobPropertyString) const
{
	CString oOld(oNativeJobPropertyString);
	if (!Is3rdState())
	{
		oNativeJobPropertyString=GetContent();
	}
	return oOld!=oNativeJobPropertyString;
}

bool CPageEditCtrlContent::ApplyToJob(long &lNativeJobPropertyLong) const
{
	long lOld(lNativeJobPropertyLong);
	if (!Is3rdState())
	{
		CString oContent(GetContent());
		CWindowUtil::FilterString(oContent, "0123456789");
		long lNumber;
		if (sscanf(oContent, "%d", &lNumber)==1)
		{
			lNativeJobPropertyLong=lNumber;
		}
	}
	return lOld!=lNativeJobPropertyLong;
}

CString CPageEditCtrlContent::GetHashString() const
{
	return "CPageEditCtrlContent-"+m_oEditCtrlContent;
}
