// EncoderGeneralPropertyPageContents.cpp: implementation of the CEncoderGeneralPropertyPageContents class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderGeneralPropertyPageContents.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderGeneralPropertyPageContents::CEncoderGeneralPropertyPageContents()
{

}

CEncoderGeneralPropertyPageContents::CEncoderGeneralPropertyPageContents(const CEncoderGeneralPropertyPageContents &oSource)
{
	*this=oSource;
}

CEncoderGeneralPropertyPageContents::~CEncoderGeneralPropertyPageContents()
{

}

CEncoderGeneralPropertyPageContents& CEncoderGeneralPropertyPageContents::operator*=(
	const CEncoderGeneralPropertyPageContents &oRight)
{
	m_oSourceDirectory*=oRight.m_oSourceDirectory;
	m_oSourceFile*=oRight.m_oSourceFile;
	m_oTargetDirectory*=oRight.m_oTargetDirectory;
	m_oTargetFile*=oRight.m_oTargetFile;
	m_oSourceFileFilterIsRecursive*=oRight.m_oSourceFileFilterIsRecursive;

	m_oSourceFilterRecursiveCheckboxVisible*=oRight.m_oSourceFilterRecursiveCheckboxVisible;

	return *this;
}