// SupportedPropertyPagesData.cpp: implementation of the CSupportedPropertyPagesData class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "SupportedPropertyPagesData.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSupportedPropertyPagesData::CSupportedPropertyPagesData()
{

}

CSupportedPropertyPagesData::CSupportedPropertyPagesData(
	const CSupportedPropertyPagesData &oSource)
{
	*this=oSource;
}

CSupportedPropertyPagesData::~CSupportedPropertyPagesData()
{

}

void CSupportedPropertyPagesData::ShowPage(EPropertyPageType ePageType)
{
	if (!IsPageVisible(ePageType))
	{
		AddNewElem(ePageType);
	}
}

void CSupportedPropertyPagesData::HidePage(EPropertyPageType ePageType)
{
	DeleteAllElemsWithContent(ePageType);
}

bool CSupportedPropertyPagesData::IsPageVisible(EPropertyPageType ePageType) const
{
	return FindContent(ePageType);
}

CSupportedPropertyPagesData& CSupportedPropertyPagesData::operator=(const CSupportedPropertyPagesData &oRight)
{
	// call inherited version
	TItemList<EPropertyPageType>::operator=(oRight);

	// have no own members to copy
	return *this;
}

CSupportedPropertyPagesData CSupportedPropertyPagesData::operator*(
	const CSupportedPropertyPagesData &oRight)
{
	CSupportedPropertyPagesData oToReturn(*this);
	oToReturn*=oRight;
	return oToReturn;
}

CSupportedPropertyPagesData& CSupportedPropertyPagesData::operator*=(
	const CSupportedPropertyPagesData &oRight)
{
	if (this!=&oRight)
	{
		// call inherited version
		TItemList<EPropertyPageType>::operator*=(oRight);

		// have no own members to intersect
	}
	return *this;
}

long CSupportedPropertyPagesData::GetNumberOfPages() const
{
	TItemList<EPropertyPageType> oTempList(*this);
	oTempList.RemoveDoubleElements();

	return oTempList.GetNumber();
}

CString CSupportedPropertyPagesData::GetPageDescriptionShort(EPropertyPageType ePageType)
{
	int iStringId=0;
	switch (ePageType)
	{
	case ePageTypeEncoderGeneral:
		{
			iStringId=IDS_EncoderGeneralShort;
			break;
		}
	case ePageTypeEncoderQuality:
		{
			iStringId=IDS_EncoderQualityShort;
			break;
		}
	case ePageTypeEncoderID3:
		{
			iStringId=IDS_EncoderId3Short;
			break;
		}
	default:
		{
			iStringId=IDS_TabUndefined;
			break;
		}
	}

	CString oToReturn;
	oToReturn.LoadString(iStringId);
	return oToReturn;
}
