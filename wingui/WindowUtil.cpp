// WindowUtil.cpp: implementation of the CWindowUtil class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "WindowUtil.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CWindowUtil::CWindowUtil()
{

}

CWindowUtil::~CWindowUtil()
{

}

void CWindowUtil::DeleteAllListCtrlItems(CListCtrl *poListCtrl)
{
	poListCtrl->DeleteAllItems();
}

long CWindowUtil::AddListCtrlItem(CListCtrl *poListCtrl, const char *lpszText, long lParam)
{
	if (poListCtrl==0) return -1;

	LV_ITEM sctItem;
	sctItem.mask=LVIF_TEXT | LVIF_PARAM;
	sctItem.iItem=0;
	sctItem.lParam=lParam;
	sctItem.iSubItem=0;
	sctItem.pszText=(char*)lpszText;		// hope we can trust the caller
	return poListCtrl->InsertItem(&sctItem);
}

void CWindowUtil::SetListCtrlItem(CListCtrl *poListCtrl, long lItemId, int iSubItemId, const char *lpszText)
{
	if (poListCtrl==0) return;

	LV_ITEM sctItem;
	sctItem.mask=LVIF_TEXT;
	sctItem.iItem=lItemId;
	sctItem.iSubItem=iSubItemId;
	sctItem.pszText=(char*)lpszText;		// hope we can trust the caller
	poListCtrl->SetItem(&sctItem);
}

void CWindowUtil::AddListCtrlColumn(
	CListCtrl *poListCtrl,
	int iColumnCount, 
	const char *lpszText, 
	double dWidth)
{
	ASSERT(poListCtrl!=0);		// valid parameter?
	if (poListCtrl==0) return;

	CRect cRect;
	int iWidthBuf;

	// determine dimensions of report list
	poListCtrl->GetWindowRect(cRect);

	// calculate width of new column
	iWidthBuf=dWidth>1 ?
		((int) (dWidth)) :
		((int) (dWidth*cRect.Width()));

	// create new column
	poListCtrl->InsertColumn(iColumnCount,
				lpszText,
				LVCFMT_LEFT, iWidthBuf, iColumnCount);
}


TItemList<long> CWindowUtil::GetAllSelectedListCtrlItemLParams(CListCtrl *poListCtrl, bool bDisableNoSelectionErrorMsg)
{
	// buffer a pointer to the list control object
	CListCtrl* pListCtrl=poListCtrl;
	ASSERT(pListCtrl!=0);		// valid list parameter?
	if (pListCtrl==0) return TItemList<long>();

	// create the return item
	TItemList<long> oSelectedLParams;

	// get a list control read pointer
	POSITION hPos=pListCtrl->GetFirstSelectedItemPosition();
	if (hPos==0)
	{
		// user did not select an item
		if (!bDisableNoSelectionErrorMsg)
		{
			AfxMessageBox(IDS_NoListItemSelected);
		}
	}
	else
	{   
		// loop through all selected items and get their
		// referring lParams
		int nCurItem;
		long lCurItemData;
		while (hPos!=0)   
		{
			// fetch the next element
			nCurItem=pListCtrl->GetNextSelectedItem(hPos);

			// find out the item data of the current element;
			lCurItemData=pListCtrl->GetItemData(nCurItem);

			// give our element to the list
			oSelectedLParams.AddNewElem(lCurItemData);
		}
	}

	return oSelectedLParams;
}

int CWindowUtil::GetListCtrlItemIdByLParam(CListCtrl *poListCtrl, long lParam, int iStartAt)
{
	// find out the list control item that corresponds with the
	// given lParam
	LVFINDINFOA sctFindInfo;
	sctFindInfo.flags=LVFI_PARAM;
	sctFindInfo.lParam=lParam;
	return poListCtrl->FindItem(&sctFindInfo, iStartAt);
}

void CWindowUtil::SetListCtrlFullRowSelectStyle(CListCtrl *poListCtrl, bool bFullRowSelectStyle)
{
	if (poListCtrl==0)
	{
		ASSERT(false);
		return;
	}

	if (bFullRowSelectStyle)
	{
		ListView_SetExtendedListViewStyleEx(*poListCtrl, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
	}
	else
	{
		ListView_SetExtendedListViewStyleEx(*poListCtrl, LVS_EX_FULLROWSELECT, 0);
	}
}

void CWindowUtil::SetListCtrlCheckBoxStyle(CListCtrl *poListCtrl, bool bCheckboxStyle)
{
	if (poListCtrl==0)
	{
		ASSERT(false);
		return;
	}

	if (bCheckboxStyle)
	{
		ListView_SetExtendedListViewStyleEx(*poListCtrl, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
	}
	else
	{
		ListView_SetExtendedListViewStyleEx(*poListCtrl, LVS_EX_CHECKBOXES, 0);
	}
}


void CWindowUtil::ForceNumericContent(CEdit *poEdit, bool bAllowNegative)
{
	ASSERT(poEdit!=0);
	if (poEdit==0) return;


	CString oOldString;
	poEdit->GetWindowText(oOldString);
	CString oNewString(oOldString);
	oNewString.TrimLeft();
	bool bIsNegative=false;
	if (bAllowNegative)
	{
		if (oNewString.GetAt(0)=='-')
		{
			bIsNegative=true;
			oNewString.Delete(0);
		}
	}
	FilterString(oNewString, "0123456789");
	if (bIsNegative)
	{
		oNewString=CString("-")+oNewString;
	}
	if (oNewString.GetLength()<oOldString.GetLength())
	{
		DWORD dwCurSel=poEdit->GetSel();
		// have altered the text
		poEdit->SetWindowText(oNewString);
		poEdit->SetSel(dwCurSel);
	}
}

void CWindowUtil::FilterString(CString &oString, const CString &oAcceptedChars)
{
	long lCurPos=0;
	while (lCurPos<oString.GetLength())
	{
		if (oAcceptedChars.Find(oString.GetAt(lCurPos))!=-1)
		{
			// character is ok
			lCurPos++;
		}
		else
		{
			// character is not ok
			oString.Delete(lCurPos);
		}
	}
}

CString CWindowUtil::GetTimeDescription(long lMilliseconds, bool bIncludeMillis)
{
	long lSeconds=lMilliseconds/1000;
	lMilliseconds=lMilliseconds%1000;
	long lMinutes=lSeconds/60;
	lSeconds=lSeconds%60;
	long lHours=lMinutes/60;
	lMinutes=lMinutes%60;
	long lDays=lHours/24;
	lHours=lHours%24;

	CString oToReturn;
	if (lDays>0)
	{
		oToReturn.Format("%dd:%02d:%02d:%02d", lDays, lHours, lMinutes, lSeconds);
	}
	else
	{
		oToReturn.Format("%02d:%02d:%02d", lHours, lMinutes, lSeconds);
	}

	if (bIncludeMillis)
	{
		CString oMillis;
		oMillis.Format(":%03d", lMilliseconds);
		oToReturn+=oMillis;
	}
	return oToReturn;
}