// SupportedPropertyPagesData.h: interface for the CSupportedPropertyPagesData class.
// Author: Torsten Landmann
//
// This class is a wrapper around a simple TItemList<long>. It provides some
// basic functionality to save which pages of a set of property pages shall
// be visible.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SUPPORTEDPROPERTYPAGESDATA_H__442115C2_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_SUPPORTEDPROPERTYPAGESDATA_H__442115C2_0FD4_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"

enum EPropertyPageType
{
	ePageTypeEncoderGeneral,
	ePageTypeEncoderQuality,
	ePageTypeEncoderID3,
};

class CSupportedPropertyPagesData : public TItemList<EPropertyPageType>
{
public:
	CSupportedPropertyPagesData();
	CSupportedPropertyPagesData(const CSupportedPropertyPagesData &oSource);
	virtual ~CSupportedPropertyPagesData();

	void ShowPage(EPropertyPageType ePageType);
	void HidePage(EPropertyPageType ePageType);
	bool IsPageVisible(EPropertyPageType ePageType) const;
	long GetNumberOfPages() const;

	// multiplication intersects two data objects;
	// this is useful to determine the pages that are supported by
	// a certain selection of different jobs
	CSupportedPropertyPagesData& operator=(const CSupportedPropertyPagesData &oRight);
	CSupportedPropertyPagesData operator*(const CSupportedPropertyPagesData &oRight);
	CSupportedPropertyPagesData& operator*=(const CSupportedPropertyPagesData &oRight);

	static CString GetPageDescriptionShort(EPropertyPageType ePageType);
};

#endif // !defined(AFX_SUPPORTEDPROPERTYPAGESDATA_H__442115C2_0FD4_11D5_8402_0080C88C25BD__INCLUDED_)
