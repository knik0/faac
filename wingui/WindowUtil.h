// WindowUtil.h: interface for the CWindowUtil class.
// Author: Torsten Landmann
//
// some functionality to assist in work especially with windows controls
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WINDOWUTIL_H__3B6C58DA_0CE8_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_WINDOWUTIL_H__3B6C58DA_0CE8_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"

class CWindowUtil  
{
public:
	CWindowUtil();
	virtual ~CWindowUtil();

	static void DeleteAllListCtrlItems(CListCtrl *poListCtrl);
	static long AddListCtrlItem(CListCtrl *poListCtrl, const char *lpszText, long lParam);
	static void SetListCtrlItem(CListCtrl *poListCtrl, long lItemId, int iSubItemId, const char *lpszText);
	static void AddListCtrlColumn(CListCtrl *poListCtrl, int iColumnCount, const char *lpszText, double dWidth);	// for dWidth: <1: percent of the width of the list control; >1 width in pixels
	static TItemList<long> GetAllSelectedListCtrlItemLParams(CListCtrl *poListCtrl, bool bDisableNoSelectionErrorMsg);
	static int GetListCtrlItemIdByLParam(CListCtrl *poListCtrl, long lParam, int iStartAt=-1);		// returns a negative value if none is found

	static void ForceNumericContent(CEdit *poEdit, bool bAllowNegative);

	static void FilterString(CString &oString, const CString &oAcceptedChars);

	static CString GetTimeDescription(long lMilliseconds, bool bIncludeMillis=false);
};

#endif // !defined(AFX_WINDOWUTIL_H__3B6C58DA_0CE8_11D5_8402_0080C88C25BD__INCLUDED_)
