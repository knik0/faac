// Id3TagInfo.h: interface for the CId3TagInfo class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ID3TAGINFO_H__DFE38E72_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ID3TAGINFO_H__DFE38E72_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FileSerializable.h"

class CId3TagInfo : public CFileSerializable
{
public:
	CId3TagInfo();
	CId3TagInfo(const CId3TagInfo &oSource);	// copy constructor
	virtual ~CId3TagInfo();

	// getters
	const CString& GetArtist() const				{ return m_oArtist; }
	CString &GetArtist()							{ return m_oArtist; }
	long GetTrackNo() const							{ return m_lTrackNo; }
	long& GetTrackNoRef()							{ return m_lTrackNo; }
	const CString& GetAlbum() const					{ return m_oAlbum; }
	CString &GetAlbum()								{ return m_oAlbum; }
	long GetYear() const							{ return m_lYear; }
	long& GetYearRef()								{ return m_lYear; }
	const CString &GetSongTitle() const				{ return m_oSongTitle; }
	CString& GetSongTitle()							{ return m_oSongTitle; }
	bool GetCopyright() const						{ return m_bCopyright; }
	bool& GetCopyrightRef()							{ return m_bCopyright; }
	const CString& GetOriginalArtist() const		{ return m_oOriginalArtist; }
	CString &GetOriginalArtist()					{ return m_oOriginalArtist; }
	const CString &GetComposer() const				{ return m_oComposer; }
	CString& GetComposer()							{ return m_oComposer; }
	const CString &GetUrl() const					{ return m_oUrl; }
	CString& GetUrl()								{ return m_oUrl; }
	const CString &GetGenre() const					{ return m_oGenre; }
	CString& GetGenre()								{ return m_oGenre; }
	const CString &GetEncodedBy() const				{ return m_oEncodedBy; }
	CString& GetEncodedBy()							{ return m_oEncodedBy; }
	const CString& GetComment() const				{ return m_oComment; }
	CString &GetComment()							{ return m_oComment; }
			
	// setters
	void SetArtist(const CString &oArtist)					{ m_oArtist=oArtist; }
	void SetTrackNo(long lTrackNo)							{ m_lTrackNo=lTrackNo; }
	void SetAlbum(const CString &oAlbum)					{ m_oAlbum=oAlbum; }
	void SetYear(long lYear)								{ m_lYear=lYear; }
	void SetSongTitle(const CString &oSongTitle)			{ m_oSongTitle=oSongTitle; }
	void SetCopyright(bool bCopyright)						{ m_bCopyright=bCopyright; }
	void SetOriginalArtist(const CString &oOriginalArtist)	{ m_oOriginalArtist=oOriginalArtist; }
	void SetComposer(const CString &oComposer)				{ m_oComposer=oComposer; }
	void SetUrl(const CString &oUrl)						{ m_oUrl=oUrl; }
	void SetGenre(const CString &oGenre)					{ m_oGenre=oGenre; }
	void SetEncodedBy(const CString &oEncodedBy)			{ m_oEncodedBy=oEncodedBy; }
	void SetComment(const CString &oComment)				{ m_oComment=oComment; }

	CId3TagInfo& operator=(const CId3TagInfo &oRight);

	// implementations to CFileSerializable
	virtual bool PutToArchive(CArchive &oArchive) const;
	virtual bool GetFromArchive(CArchive &oArchive);
													
private:
	CString m_oArtist;
	long m_lTrackNo;
	CString m_oAlbum;
	long m_lYear;
	CString m_oSongTitle;
	bool m_bCopyright;
	CString m_oOriginalArtist;
	CString m_oComposer;
	CString m_oUrl;
	CString m_oGenre;
	CString m_oEncodedBy;
	CString m_oComment;
	// further to come (don't forget to update our assignment operator)

};

#endif // !defined(AFX_ID3TAGINFO_H__DFE38E72_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
