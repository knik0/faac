// FileMaskAssembler.h: interface for the CFileMaskAssembler class.
// Author: Torsten Landmann
//
// This is an interesting class. It assembles a file mask for CFileDialog
// from a list of integers. These integers point to entries in the
// string table and have the following format: "Wav Files (*.wav)|*.wav"
// without quotation marks; All list items are processed from index 1 until
// the first missing index is found;
// the string that's returned can be used without modification (provided
// the substrings specified were okay)
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILEMASKASSEMBLER_H__A1444E8D_1546_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_FILEMASKASSEMBLER_H__A1444E8D_1546_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CFileMaskAssembler  
{
public:
	CFileMaskAssembler();
	virtual ~CFileMaskAssembler();

	// see topmost comment in this file
	static CString GetFileMask(TItemList<int> &oFilterStringEntries);
};

#endif // !defined(AFX_FILEMASKASSEMBLER_H__A1444E8D_1546_11D5_8402_0080C88C25BD__INCLUDED_)
