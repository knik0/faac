// ListCtrlStateSaver.h: interface for the CListCtrlStateSaver class.
// Copyright 1999, Torsten Landmann
// successfully compiled and tested under MSVC 6.0
//
// This class is able to easily preserve the state of a list control
// during refills of that control. It preserves information on which
// items are selected and which items are checked. It does not preserve
// the order.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LISTCTRLSTATESAVER_H__CAAC66A6_72CB_11D3_A724_C0C04EC10107__INCLUDED_)
#define AFX_LISTCTRLSTATESAVER_H__CAAC66A6_72CB_11D3_A724_C0C04EC10107__INCLUDED_

#include "TItemList.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CListCtrlStateSaver  
{
public:
	CListCtrlStateSaver();		// when using this constructor you can configure the list control using SetToSelected() and public methods below
	CListCtrlStateSaver(CListCtrl *poListCtrl);
	virtual ~CListCtrlStateSaver();

	void SaveState(CListCtrl *poListCtrl);
	void RestoreState(CListCtrl *poListCtrl);		// not a const member; however you can assume it is one

	// for manual list control configuration; take effect
	// on RestoreState()
	void UnselectAll();
	void UncheckAll();
	void SetToSelected(long lParam, bool bSelected=true);
	void SetToSelected(TItemList<long> oLParams, bool bSelected=true);
	void SetToChecked(long lParam, bool bChecked=true);
	void SetToChecked(TItemList<long> oLParams, bool bSelected=true);

	long GetFocusedItemLParam() const;		// -1 if there's no focused item
	bool IsSelected(long lItemLParam) const;
	bool IsChecked(long lItemLParam) const;

	long GetNumberOfSelectedItems() const;
	long GetNumberOfCheckedItems() const;

private:
	long m_lFocusedItemLParam;
	TItemList<long> m_oSelectedItemLParams;
	TItemList<long> m_oCheckedItemLParams;

	void AddSelectedItem(long lParam);
	void AddCheckedItem(long lParam);

	static bool IsSelected(CListCtrl *poListCtrl, int iItemId);
	bool IsInSelectedItemsList(long lParam);
	static bool IsChecked(CListCtrl *poListCtrl, int iItemId);
	bool IsInCheckedItemsList(long lParam);
	static bool IsFocused(CListCtrl *poListCtrl, int iItemId);

	void SetSelectedState(CListCtrl *poListCtrl, int iItemId, bool bSelected);
	void SetCheckedState(CListCtrl *poListCtrl, int iItemId, bool bChecked);
	void SetToFocused(CListCtrl *poListCtrl, int iItemId);

	bool m_bIsSaving;

	void GoThroughEntireList(CListCtrl *poListCtrl);
};

#endif // !defined(AFX_LISTCTRLSTATESAVER_H__CAAC66A6_72CB_11D3_A724_C0C04EC10107__INCLUDED_)
