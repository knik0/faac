// FileMaskAssembler.cpp: implementation of the CFileMaskAssembler class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "FileMaskAssembler.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFileMaskAssembler::CFileMaskAssembler()
{

}

CFileMaskAssembler::~CFileMaskAssembler()
{

}

CString CFileMaskAssembler::GetFileMask(TItemList<int> &oFilterStringEntries)
{
	// assemble the filter string
	CString oFilter;
	long lCurIndex=1;
	int iCurItem;
	while (oFilterStringEntries.GetElemContent(lCurIndex++, iCurItem))
	{
		// accept aac files
		CString oBuf;
		oBuf.LoadString(iCurItem);
		oFilter+=oBuf+"|";
	}
	oFilter+="||";
	return oFilter;
}
