// Id3TagInfo.cpp: implementation of the CId3TagInfo class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "Id3TagInfo.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CId3TagInfo::CId3TagInfo():
	m_lTrackNo(-1),
	m_lYear(-1)
{
}

CId3TagInfo::CId3TagInfo(const CId3TagInfo &oSource)
{
	*this=oSource;
}

CId3TagInfo::~CId3TagInfo()
{
}


CId3TagInfo& CId3TagInfo::operator=(const CId3TagInfo &oRight)
{
	m_oArtist=oRight.m_oArtist;
	m_lTrackNo=oRight.m_lTrackNo;
	m_oAlbum=oRight.m_oAlbum;
	m_lYear=oRight.m_lYear;
	m_oSongTitle=oRight.m_oSongTitle;
	m_bCopyright=oRight.m_bCopyright;
	m_oOriginalArtist=oRight.m_oOriginalArtist;
	m_oComposer=oRight.m_oComposer;
	m_oUrl=oRight.m_oUrl;
	m_oGenre=oRight.m_oGenre;
	m_oEncodedBy=oRight.m_oEncodedBy;
	m_oComment=oRight.m_oComment;

	return *this;
}

bool CId3TagInfo::PutToArchive(CArchive &oArchive) const
{
	// put a class version flag
	int iVersion=1;
	oArchive << iVersion;

	oArchive << m_oArtist;
	oArchive << m_lTrackNo;
	oArchive << m_oAlbum;
	oArchive << m_lYear;
	oArchive << m_oSongTitle;
	CFileSerializable::SerializeBool(oArchive, m_bCopyright);
	oArchive << m_oOriginalArtist;
	oArchive << m_oComposer;
	oArchive << m_oUrl;
	oArchive << m_oGenre;
	oArchive << m_oEncodedBy;
	oArchive << m_oComment;

	return true;
}

bool CId3TagInfo::GetFromArchive(CArchive &oArchive)
{
	// fetch the class version flag
	int iVersion;
	oArchive >> iVersion;

	switch (iVersion)
	{
	case 1:
		{
			oArchive >> m_oArtist;
			oArchive >> m_lTrackNo;
			oArchive >> m_oAlbum;
			oArchive >> m_lYear;
			oArchive >> m_oSongTitle;
			CFileSerializable::DeSerializeBool(oArchive, m_bCopyright);
			oArchive >> m_oOriginalArtist;
			oArchive >> m_oComposer;
			oArchive >> m_oUrl;
			oArchive >> m_oGenre;
			oArchive >> m_oEncodedBy;
			oArchive >> m_oComment;

			return true;
		}
	default:
		{
			// unknown file format version
			return false;
		}
	}
}