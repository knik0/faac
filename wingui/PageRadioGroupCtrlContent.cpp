// PageRadioGroupCtrlContent.cpp: implementation of the CPageRadioGroupCtrlContent class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "PageRadioGroupCtrlContent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPageRadioGroupCtrlContent::CPageRadioGroupCtrlContent()
{

}

CPageRadioGroupCtrlContent::~CPageRadioGroupCtrlContent()
{

}

CString CPageRadioGroupCtrlContent::GetHashString() const
{
	return CString("CPageRadioGroupCtrlContent:")+CPageEditCtrlContent::GetHashString();
}