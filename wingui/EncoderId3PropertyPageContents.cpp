// EncoderId3PropertyPageContents.cpp: implementation of the CEncoderId3PropertyPageContents class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderId3PropertyPageContents.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderId3PropertyPageContents::CEncoderId3PropertyPageContents()
{

}

CEncoderId3PropertyPageContents::~CEncoderId3PropertyPageContents()
{

}

CEncoderId3PropertyPageContents& CEncoderId3PropertyPageContents::operator*=(const CEncoderId3PropertyPageContents &oRight)
{
	m_oArtist*=oRight.m_oArtist;
	m_oTrackNo*=oRight.m_oTrackNo;
	m_oAlbum*=oRight.m_oAlbum;
	m_oYear*=oRight.m_oYear;
	m_oTitle*=oRight.m_oTitle;
	m_oCopyright*=oRight.m_oCopyright;
	m_oOriginalArtist*=oRight.m_oOriginalArtist;
	m_oComposer*=oRight.m_oComposer;
	m_oUrl*=oRight.m_oUrl;
	m_oGenre*=oRight.m_oGenre;
	m_oEncodedBy*=oRight.m_oEncodedBy;
	m_oComment*=oRight.m_oComment;

	return *this;
}