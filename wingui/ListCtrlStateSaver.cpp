// ListCtrlStateSaver.cpp: implementation of the CListCtrlStateSaver class.
// Copyright 1999, Torsten Landmann
// successfully compiled and tested under MSVC 6.0
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ListCtrlStateSaver.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CListCtrlStateSaver::CListCtrlStateSaver()
{

}

CListCtrlStateSaver::CListCtrlStateSaver(CListCtrl *poListCtrl)
{
	SaveState(poListCtrl);
}

CListCtrlStateSaver::~CListCtrlStateSaver()
{

}

void CListCtrlStateSaver::SaveState(CListCtrl *poListCtrl)
{
	m_bIsSaving=true;
	m_oSelectedItemLParams.DeleteAll();
	m_oCheckedItemLParams.DeleteAll();
	m_lFocusedItemLParam=-1;		// no selection or don't know the selection yet

	if (poListCtrl!=0)
	{
		GoThroughEntireList(poListCtrl);
	}
}

void CListCtrlStateSaver::RestoreState(CListCtrl *poListCtrl)
{
	m_bIsSaving=false;

	if (poListCtrl!=0)
	{
		GoThroughEntireList(poListCtrl);
	}
}

void CListCtrlStateSaver::UnselectAll()
{
	m_oSelectedItemLParams.DeleteAll();
}

void CListCtrlStateSaver::UncheckAll()
{
	m_oCheckedItemLParams.DeleteAll();
}

void CListCtrlStateSaver::SetToSelected(long lParam, bool bSelected)
{
	if (bSelected)
	{
		AddSelectedItem(lParam);
	}
	else
	{
		m_oSelectedItemLParams.DeleteAllElemsWithContent(lParam);
	}
}

void CListCtrlStateSaver::SetToSelected(TItemList<long> oLParams, bool bSelected)
{
	HRead hRead=oLParams.BeginRead();
	long lCurLParam;
	while (oLParams.GetNextElemContent(hRead, lCurLParam))
	{
		SetToSelected(lCurLParam, bSelected);
	}
	oLParams.EndRead(hRead);
}

void CListCtrlStateSaver::SetToChecked(long lParam, bool bChecked)
{
	if (bChecked)
	{
		AddCheckedItem(lParam);
	}
	else
	{
		m_oCheckedItemLParams.DeleteAllElemsWithContent(lParam);
	}
}

void CListCtrlStateSaver::SetToChecked(TItemList<long> oLParams, bool bSelected)
{
	HRead hRead=oLParams.BeginRead();
	long lCurLParam;
	while (oLParams.GetNextElemContent(hRead, lCurLParam))
	{
		SetToChecked(lCurLParam, bSelected);
	}
	oLParams.EndRead(hRead);
}

long CListCtrlStateSaver::GetFocusedItemLParam() const
{
	return m_lFocusedItemLParam;
}

bool CListCtrlStateSaver::IsSelected(long lItemLParam) const
{
	long lDummy;
	return m_oSelectedItemLParams.FindContent(lItemLParam, lDummy);
}

bool CListCtrlStateSaver::IsChecked(long lItemLParam) const
{
	long lDummy;
	return m_oCheckedItemLParams.FindContent(lItemLParam, lDummy);
}

long CListCtrlStateSaver::GetNumberOfSelectedItems() const
{
	return m_oSelectedItemLParams.GetNumber();
}

long CListCtrlStateSaver::GetNumberOfCheckedItems() const
{
	return m_oCheckedItemLParams.GetNumber();
}

void CListCtrlStateSaver::AddSelectedItem(long lParam)
{
	long lId;
	if (!m_oSelectedItemLParams.FindContent(lParam, lId))
	{
		m_oSelectedItemLParams.AddNewElem(lParam);
	}
}

void CListCtrlStateSaver::AddCheckedItem(long lParam)
{
	long lId;
	if (!m_oCheckedItemLParams.FindContent(lParam, lId))
	{
		m_oCheckedItemLParams.AddNewElem(lParam);
	}
}

bool CListCtrlStateSaver::IsSelected(CListCtrl *poListCtrl, int iItemId)
{
	UINT uiItemState=ListView_GetItemState(*poListCtrl, iItemId, LVIS_SELECTED);
	return uiItemState!=0;
}

bool CListCtrlStateSaver::IsInSelectedItemsList(long lParam)
{
	long lId;
	return m_oSelectedItemLParams.FindContent(lParam, lId);
}

bool CListCtrlStateSaver::IsChecked(CListCtrl *poListCtrl, int iItemId)
{
	return ListView_GetCheckState(*poListCtrl, iItemId)!=0;
}

bool CListCtrlStateSaver::IsInCheckedItemsList(long lParam)
{
	long lId;
	return m_oCheckedItemLParams.FindContent(lParam, lId);
}

bool CListCtrlStateSaver::IsFocused(CListCtrl *poListCtrl, int iItemId)
{
	UINT uiItemState=ListView_GetItemState(*poListCtrl, iItemId, LVIS_FOCUSED);
	return uiItemState!=0;
}

void CListCtrlStateSaver::SetSelectedState(CListCtrl *poListCtrl, int iItemId, bool bSelected)
{
	if (bSelected)
	{
		ListView_SetItemState(*poListCtrl, iItemId, LVIS_SELECTED, LVIS_SELECTED);
	}
	else
	{
		ListView_SetItemState(*poListCtrl, iItemId, 0, LVIS_SELECTED);
	}
}

void CListCtrlStateSaver::SetCheckedState(CListCtrl *poListCtrl, int iItemId, bool bChecked)
{
	// macro as written at topic "Extended list view styles"
	// in the msdn
#ifndef ListView_SetCheckState
   #define ListView_SetCheckState(hwndLV, i, fCheck) \
      ListView_SetItemState(hwndLV, i, \
      INDEXTOSTATEIMAGEMASK((fCheck)+1), LVIS_STATEIMAGEMASK)
#endif

	if (bChecked)
	{
		ListView_SetCheckState(*poListCtrl, iItemId, TRUE);
	}
	else
	{
		ListView_SetCheckState(*poListCtrl, iItemId, FALSE);
	}
}

void CListCtrlStateSaver::SetToFocused(CListCtrl *poListCtrl, int iItemId)
{
	ListView_SetItemState(*poListCtrl, iItemId, LVIS_FOCUSED, LVIS_FOCUSED);
}

void CListCtrlStateSaver::GoThroughEntireList(CListCtrl *poListCtrl)
{
	long lNumberOfItems=poListCtrl->GetItemCount();
	for (int iCurItemId=0; iCurItemId<lNumberOfItems; iCurItemId++)
	{
		long lCurItemLParam=poListCtrl->GetItemData(iCurItemId);
		if (m_bIsSaving)
		{
			// save the item's state

			if (IsSelected(poListCtrl, iCurItemId))
			{
				AddSelectedItem(lCurItemLParam);
			}

			if (IsChecked(poListCtrl, iCurItemId))
			{
				AddCheckedItem(lCurItemLParam);
			}

			if (IsFocused(poListCtrl, iCurItemId))
			{
				m_lFocusedItemLParam=lCurItemLParam;
			}
		}
		else
		{
			// restore the item's state
			SetSelectedState(poListCtrl, iCurItemId, IsInSelectedItemsList(lCurItemLParam));
			SetCheckedState(poListCtrl, iCurItemId, IsInCheckedItemsList(lCurItemLParam));

			if (m_lFocusedItemLParam==lCurItemLParam)
			{
				SetToFocused(poListCtrl, iCurItemId);
			}
		}
	}

	if (!m_bIsSaving && m_lFocusedItemLParam<0)
	{
		// no focused item
		SetToFocused(poListCtrl, -1);
	}
}