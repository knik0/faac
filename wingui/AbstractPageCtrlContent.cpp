// AbstractPageCtrlContent.cpp: implementation of the CAbstractPageCtrlContent class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "AbstractPageCtrlContent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAbstractPageCtrlContent::CAbstractPageCtrlContent():
	m_b3rdStateContent(true)			// it's essential that this is initialized to true;
										// otherwise changes could be commited from pages even
										// if there had been detected errors
{

}

CAbstractPageCtrlContent::~CAbstractPageCtrlContent()
{

}

CAbstractPageCtrlContent& CAbstractPageCtrlContent::operator*=(
	const CAbstractPageCtrlContent &oRight)
{
	if (Is3rdState()) return *this;		// 3rd state can't be left by multiplication operator

	if (GetHashString()!=oRight.GetHashString())
	{
		SetIs3rdState(true);
	}

	return *this;
}
