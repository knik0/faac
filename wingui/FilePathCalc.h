// FilePathCalc.h: interface for the CFilePathCalc class.
// this class is used to provide access to certain procedures
// for handling file paths
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FILEPATHCALC_H__B9F42A01_49AE_11D3_A724_50DB50C10057__INCLUDED_)
#define AFX_FILEPATHCALC_H__B9F42A01_49AE_11D3_A724_50DB50C10057__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CFilePathCalc  
{
public:
	CFilePathCalc();
	virtual ~CFilePathCalc();

	// oFrom may be a path or a file name
	// oTo should be a file name
	// examples:
	// 1.:	oFrom:	"c:\windows\system"
	//		oTo:	"c:\windows\media"
	//			->						"..\media"
	//
	// 2.:	oFrom:	"c:\windows\system\test.txt"
	//		oTo:	"a:\test.txt"
	//			->						"a:\test.txt"
	//
	// 3.:	oFrom:	"c:\windows\system.txt"
	//		oTo:	"c:\windows\system.txt"
	//			->						"system.txt"
	//
	// 4.:	oFrom:	"c:\windows\system.txt\"
	//		oTo:	"c:\windows\system.txt"
	//			->						"..\system.txt"
	static bool GetRelativePosition(const CString &oFrom, const CString &oTo, CString &oRelativePath);

	// oAbsolute path must be finished by a back slash
	// 1.:	oAbsolutePath:	"c:\windows\"
	//		oRelativePath:	"..\windos"
	//			->						"c:\windos"
	//
	// 2.:	oAbsolutePath:	"c:\windows\new\"
	//		oRelativePath:	"..\windos\blah.txt"
	//			->						"c:\windows\windos\blah.txt"
	//
	static bool ApplyRelativePath(const CString &oRelativePath, CString &oAbsolutePath);

	// "c:\windows\blah.txt"	->	"c:\windows\"
	// "c:\windows"				->	"c:\windows\"
	// "c:"						->	"c:\"
	// "c:windows"				-> error
	// "windows"				-> error
	// "windows\blah.txt"		-> error
	// "c:\windows\blah.txt\"	-> "c:\windows\blah.txt\"	// in this case blah.txt should be a directory
	// return value: success
	// make sure you NOTE: the string MUST contain a
	// drive specification
	// if bAssumeDirectory==true then the following happens:
	// "c:\windows\blah.txt"	->	"c:\windows\blah.txt\"
	static bool MakePath(CString &oStr, bool bAssumeDirectory=false);

	// extracts the file name from a path
	// "c:\windows\blah.txt"	->	"blah.txt"
	// "c:\windows\blah.txt\"	-> error
	// "c:blah.txt"				->	"blah.txt"
	// "blah.txt"				->	"blah.txt"
	// source and target string parameter may reference the
	// same object
	static bool ExtractFileName(const CString &oPath, CString &oFileName);

	// "c:\windows\blah.txt"	->	"c:"
	// "\windows\blah.txt"		-> error
	// source and target string parameter may reference the
	// same object
	static bool ExtractDrive(const CString &oPath, CString &oDrive);

	// "blah.txt"				->	"blah", "txt"
	// "blah"					->	"blah", ""
	// "blah."					->	"blah", ""
	// "somewhere\blah.txt"		->	"somewhere\blah", "txt"
	// source and target string parameter may reference the
	// same object; the target objects may be the same object
	// (however behaviour is undefined in this case)
	static bool SplitFileAndExtension(const CString &oFilePath, CString &oSimpleFileName, CString &oExtension);

	// bAcceptDirectories==false
	//	""						-> false
	//	"*.txt"					-> true
	//	"*.something"			-> true
	//	"*"						-> true
	//	"*.*"					-> true
	//	"*.*.*"					-> true
	//	"*..txt"				-> false
	//	"?something.txt"		-> true
	//	"s?omething.*"			-> true
	//	"something.t xt"		-> false
	//	"something.txt"			-> false	(!)
	//	"\*.*"					-> false
	//	"c:\temp\*.*"			-> false
	// bAcceptDirectories==true
	//	""						-> false
	//	"*.txt"					-> true
	//	"*.something"			-> true
	//	"*"						-> true
	//	"*.*"					-> true
	//	"*.*.*"					-> true
	//	"*..txt"				-> false
	//	"?something.txt"		-> true
	//	"s?omething.*"			-> true
	//	"something.t xt"		-> false
	//	"something.txt"			-> false	(!)
	//	"\*.*"					-> true
	//	"c:\temp\*.*"			-> true
	static bool IsValidFileMask(const CString &oFileMask, bool bAcceptDirectories=false);

private:
	// oPath must be a directory description finished with
	// a back slash;
	// "c:\windows\system\"		->	"c:\windows\"
	// "c:\"					-> error
	static bool RemoveOneDirFromEnd(CString &oPath, CString &oRemoved);

	// counts the number of back slashs in the string
	// and thus determines the depth of the path
	static long CountBackSlashes(const CString &oPath);

	// deletes all unaccepted chars from the string
	static void FilterString(CString &oString, const CString &oAcceptedChars);

	// deletes all specified chars from the string
	static void FilterStringInverse(CString &oString, const CString &oCharsToRemove);
};

#endif // !defined(AFX_FILEPATHCALC_H__B9F42A01_49AE_11D3_A724_50DB50C10057__INCLUDED_)
