// EncoderId3PropertyPageContents.h: interface for the CEncoderId3PropertyPageContents class.
// Author: Torsten Landmann
//
// encapsulates the content of the property page "id3" including
// possible 3rd states of all controls
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERID3PROPERTYPAGECONTENTS_H__0C2E1C01_10AE_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERID3PROPERTYPAGECONTENTS_H__0C2E1C01_10AE_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AbstractPropertyPageContents.h"
#include "PageEditCtrlContent.h"
#include "PageCheckboxCtrlContent.h"
#include "PageComboBoxCtrlContent.h"

class CEncoderId3PropertyPageContents : public CAbstractPropertyPageContents  
{
public:
	CEncoderId3PropertyPageContents();
	virtual ~CEncoderId3PropertyPageContents();

	CPageEditCtrlContent m_oArtist;
	CPageEditCtrlContent m_oTrackNo;
	CPageEditCtrlContent m_oAlbum;
	CPageEditCtrlContent m_oYear;
	CPageEditCtrlContent m_oTitle;
	CPageCheckboxCtrlContent m_oCopyright;
	CPageEditCtrlContent m_oOriginalArtist;
	CPageEditCtrlContent m_oComposer;
	CPageEditCtrlContent m_oUrl;
	CPageComboBoxCtrlContent m_oGenre;
	CPageEditCtrlContent m_oEncodedBy;
	CPageEditCtrlContent m_oComment;

	// with this operator pages for several jobs can be "merged"
	CEncoderId3PropertyPageContents& operator*=(const CEncoderId3PropertyPageContents &oRight);
};

#endif // !defined(AFX_ENCODERID3PROPERTYPAGECONTENTS_H__0C2E1C01_10AE_11D5_8402_0080C88C25BD__INCLUDED_)
