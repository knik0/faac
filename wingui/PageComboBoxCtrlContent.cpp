// PageComboBoxCtrlContent.cpp: implementation of the CPageComboBoxCtrlContent class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "PageComboBoxCtrlContent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPageComboBoxCtrlContent::CPageComboBoxCtrlContent()
{

}

CPageComboBoxCtrlContent::~CPageComboBoxCtrlContent()
{

}

void CPageComboBoxCtrlContent::SetContentSelection(int iSelectionId)
{
	CString oSelection;
	oSelection.Format("%d", iSelectionId);
	SetContentText(oSelection);
}

int CPageComboBoxCtrlContent::GetContentSelection() const
{
	CString oSelectionString(GetContentText());
	if (oSelectionString.IsEmpty()) return -1;		// 3rd state
	int iSelectionId;
	if (sscanf(oSelectionString, "%d", &iSelectionId)!=1)
	{
		// error while parsing
		return -1;
	}

	return iSelectionId;
}

void CPageComboBoxCtrlContent::SetContentText(const CString &oText)
{
	SetContent(oText);
}

const CString& CPageComboBoxCtrlContent::GetContentText() const
{
	return GetContent();
}

CString& CPageComboBoxCtrlContent::GetContentText()
{
	return GetContent();
}

void CPageComboBoxCtrlContent::SetCurComboBoxSelectionText(CComboBox *poComboBox)
{
	int iCurSelection=poComboBox->GetCurSel();
	if (iCurSelection<0)
	{
		SetIs3rdState(true);
		return;
	}

	CString oText;
	poComboBox->GetLBText(iCurSelection, oText);
	SetContent(oText);
}

bool CPageComboBoxCtrlContent::ApplyToJob(CString &oNativeJobPropertyTextString) const
{
	CString oOld(oNativeJobPropertyTextString);
	if (!Is3rdState())
	{
		oNativeJobPropertyTextString=GetContentText();
	}
	return oOld!=oNativeJobPropertyTextString;
}

bool CPageComboBoxCtrlContent::ApplyToJob(long &lNativeJobPropertySelectionLong) const
{
	long lOld(lNativeJobPropertySelectionLong);
	int iId=GetContentSelection();
	if (iId>=0)
	{
		lNativeJobPropertySelectionLong=GetContentSelection();
	}
	return lOld!=lNativeJobPropertySelectionLong;
}

void CPageComboBoxCtrlContent::ApplyToComboBoxVariable(int &iSelectionVariable) const
{
	int iId=GetContentSelection();
	if (iId>=0)
	{
		iSelectionVariable=GetContentSelection();
	}
}

void CPageComboBoxCtrlContent::ApplyToComboBoxVariable(CString &oSelectionVariable) const
{
	if (!Is3rdState())
	{
		oSelectionVariable=GetContentText();
	}
}

void CPageComboBoxCtrlContent::ApplyToComboBoxPointer(CComboBox *poComboBox) const
{
	// important: this method is only for drop down combo boxes
	// that have been used with the text feature
	CString oStringToSelect;
	if (Is3rdState())
	{
		oStringToSelect.Empty();
	}
	else
	{
		oStringToSelect=GetContent();
	}

	int iSelection=poComboBox->FindStringExact(0, oStringToSelect);
	if (iSelection>=0)
	{
		poComboBox->SetCurSel(iSelection);
	}
}

CString CPageComboBoxCtrlContent::GetHashString() const
{
	return CString("CPageComboBoxCtrlContent:")+CPageEditCtrlContent::GetHashString();
}