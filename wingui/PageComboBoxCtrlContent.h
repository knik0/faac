// PageComboBoxCtrlContent.h: interface for the CPageComboBoxCtrlContent class.
// Author: Torsten Landmann
//
// is able to save the content of a combo box control including the 3rd state
// and perform merge operations so that data for several jobs can be
// merged
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PAGECOMBOBOXCTRLCONTENT_H__C245DF24_11A6_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_PAGECOMBOBOXCTRLCONTENT_H__C245DF24_11A6_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PageEditCtrlContent.h"

class CPageComboBoxCtrlContent : public CPageEditCtrlContent
	// I'd rather inherit from CPageEditCtrlContent as private but
	// that'd be too complicated since CAbstractPageCtrlContent implements
	// essential behaviour that must still be accessible from outside
{
public:
	CPageComboBoxCtrlContent();
	virtual ~CPageComboBoxCtrlContent();

	// note: you must not mix selection and text content
	// based management for one combo control to manage
	// at a time

	// data for combo controls that you wish to manage
	// based on their selection ids
	void SetContentSelection(int iSelectionId);
	int GetContentSelection() const;		// returns a negative value if in 3rd state or in case of errors

	// data for combo controls that you wish to manage
	// based on their currently selected/entered texts
	void SetContentText(const CString &oText);
	const CString& GetContentText() const;
	CString& GetContentText();
	// this method is only for drop down combo boxes; it fetches
	// the text of the current selection and applies it via
	// SetCurrentText(const CString&)
	void SetCurComboBoxSelectionText(CComboBox *poComboBox);

	// return if the previous setting was modified
	bool ApplyToJob(CString &oNativeJobPropertyTextString) const;
	bool ApplyToJob(long &lNativeJobPropertySelectionLong) const;

	// apply to variables that are defined in the class wizard
	void ApplyToComboBoxVariable(int &iSelectionVariable) const;
	void ApplyToComboBoxVariable(CString &oSelectionVariable) const;

	// important: this method is only for drop down combo boxes
	// that have been used with the text feature; note
	// that the behaviour is undefined if the string that must
	// be set doesn't exist in the combo box; this includes
	// the empty string that is attempted to be set when
	// the 3rd state is detected
	void ApplyToComboBoxPointer(CComboBox *poComboBox) const;

	// implementation of CAbstractPageCtrlContent method
	virtual CString GetHashString() const;

private:
	// this class does not need own data
};

#endif // !defined(AFX_PAGECOMBOBOXCTRLCONTENT_H__C245DF24_11A6_11D5_8402_0080C88C25BD__INCLUDED_)
