// FilePathCalc.cpp: implementation of the CFilePathCalc class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FilePathCalc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFilePathCalc::CFilePathCalc()
{

}

CFilePathCalc::~CFilePathCalc()
{

}

bool CFilePathCalc::GetRelativePosition(const CString &oFrom, const CString &oTo, CString &oRelativePath)
{
	CString oFromPath(oFrom);
	CString oToPath(oTo);

	oFromPath.MakeLower();
	oToPath.MakeLower();

	// extract raw paths
	{
		if (!MakePath(oFromPath) || !MakePath(oToPath))
		{
			return false;
		}
	}

	// compare drive letters
	{
		CString oFromDrive;
		CString oToDrive;
		if (!ExtractDrive(oFromPath, oFromDrive) ||
			!ExtractDrive(oToPath, oToDrive))
		{
			return false;
		}

		if (oFromDrive!=oToDrive)
		{
			// drive letters are different

			oRelativePath=oTo;
			return true;
		}
	}

	// determine relative path
	{
		CString oUpPath;
		CString oDownPath;
		CString oBuf1, oBuf2;

		// make paths to same depth
		while (CountBackSlashes(oFromPath)>CountBackSlashes(oToPath))
		{
			oUpPath+="..\\";
			if (!RemoveOneDirFromEnd(oFromPath, oBuf1))
			{
				return false;
			}
		}
		while (CountBackSlashes(oToPath)>CountBackSlashes(oFromPath))
		{
			if (!RemoveOneDirFromEnd(oToPath, oBuf2))
			{
				return false;
			}

			oDownPath=oBuf2+oDownPath;
		}

		// keep track how deep we must go back to find
		// the directory from which we can go towards
		// the target directory
		while (oFromPath!=oToPath)
		{
			oUpPath+="..\\";

			if (!RemoveOneDirFromEnd(oFromPath, oBuf1))
			{
				return false;
			}
			if (!RemoveOneDirFromEnd(oToPath, oBuf2))
			{
				return false;
			}

			oDownPath=oBuf2+oDownPath;
		}

		CString oToFileName;
		if (!ExtractFileName(oTo, oToFileName))
		{
			return false;
		}

		oRelativePath=oUpPath+oDownPath+oToFileName;
	}

	return true;
}

bool CFilePathCalc::ApplyRelativePath(
	const CString &oRelativePath, CString &oAbsolutePath)
{
	CString oRelativePathCpy(oRelativePath);
	if (oRelativePathCpy.Find(':')==1)
	{
		// relative path contains a drive specificiation

		oAbsolutePath=oRelativePath;

		return true;
	}

	// delete ".\"s
	{
		while (oRelativePathCpy.Find(".\\")==0)
		{
			oRelativePathCpy.Delete(0, 2);
		}
	}

	if (oRelativePathCpy.Find('\\')==0)
	{
		// relative path begins with a back slash

		CString oDrive;
		if (!ExtractDrive(oAbsolutePath, oDrive))
		{
			return false;
		}

		oAbsolutePath=oDrive+oRelativePathCpy;
		
		return true;
	}

	CString oBuf1;
	while (oRelativePathCpy.Find("..\\")!=-1)
	{
		// delete first three characters
		oRelativePathCpy.Delete(0, 3);

		if (!RemoveOneDirFromEnd(oAbsolutePath, oBuf1))
		{
			return false;
		}
	}

	oAbsolutePath+=oRelativePathCpy;

	return true;
}


bool CFilePathCalc::MakePath(CString &oStr, bool bAssumeDirectory)
{
	long lCharPos, lCharPos2;

	if ((lCharPos=oStr.Find(':'))!=-1)
	{
		// there is a colon in the string

		if (oStr.Find(':', lCharPos+1)!=-1)
		{
			// error case: more than one colon in the string
			return false;
		}

		if (oStr.GetAt(lCharPos+1)!='\\')
		{
			// the colon is not followed by a back slash
			if (oStr.GetLength()>lCharPos+2)
			{
				// error case
				// something like "c:windows" was passed on
				return false;
			}
			else
			{
				// something like "c:" was passed on
				oStr+="\\";
				return true;
			}
		}
		else
		{
			// the colon is followed by a back slash

			if ((lCharPos=oStr.ReverseFind('.'))!=-1)
			{
				// the path description contains at least one dot

				if ((lCharPos2=oStr.ReverseFind('\\'))!=-1)
				{
					if (lCharPos2>lCharPos || bAssumeDirectory)
					{
						// there is a back slash behind the last
						// dot or we shall assume there is one

						if (oStr.GetAt(oStr.GetLength()-1)!='\\')
						{
							// last character is not yet a backslash but
							// it must become one
							
							// append a back slash
							oStr+='\\';
							
							return true;
						}

						// path name is ok
						return true;
					}
					else
					{
						// there is no back slash behind the last
						// dot -> the path contains a file name;

						// remove the file name;
						// terminate behind last back slash
						oStr.GetBuffer(0);
						oStr.SetAt(lCharPos2+1, '\0');
						oStr.ReleaseBuffer();
						return true;
					}
				}
				else
				{
					// error case: no backslash in path name
					return false;
				}
			}
			else
			{
				// the path description does not contain a dot

				if (oStr.GetAt(oStr.GetLength()-1)!='\\')
				{
					// last character is not a back slash

					// append a back slash
					oStr+='\\';
					return true;
				}
				else
				{
					// last character is a back slash
					return true;
				}
			}
		}
	}
	else
	{
		// error case: no colon in string
		return false;
	}
}

bool CFilePathCalc::ExtractFileName(const CString &oPath, CString &oFileName)
{
	if (&oPath!=&oFileName)
	{
		oFileName=oPath;
	}

	long lBackSlashPos=oFileName.ReverseFind('\\');
	if (lBackSlashPos==-1)
	{
		// the path did not contain a back slash

		if (oFileName.Find(':')!=-1)
		{
			// the path contains a drive letter

			// delete first two characters
			oFileName.Delete(0, 2);

			return true;
		}
		else
		{
			// the path did not contain a colon

			return true;
		}
	}
	else
	{
		// the path contains at least one backslash

		// delete all characters up to the last back slash
		oFileName.Delete(0, lBackSlashPos+1);

		return true;
	}
}

bool CFilePathCalc::ExtractDrive(const CString &oPath, CString &oDrive)
{
	if (&oPath!=&oDrive)
	{
		oDrive=oPath;
	}

	if (oDrive.Find(':')==-1)
	{
		return false;
	}

	oDrive.GetBuffer(0);
	oDrive.SetAt(2, '\0');
	oDrive.ReleaseBuffer();

	return true;
}

bool CFilePathCalc::SplitFileAndExtension(const CString &oFileName, CString &oSimpleFileName, CString &oExtension)
{
	int iLastBackslashPos=oFileName.ReverseFind('\\');
	int iLastDotPos=oFileName.ReverseFind('.');
	int iStringLength=oFileName.GetLength();
	
	oSimpleFileName=oFileName;
	if (iLastBackslashPos>=0 && iLastDotPos>=0 && iLastBackslashPos>iLastDotPos)
	{
		// the last dot is BEFORE the last backslash
		oExtension.Empty();
		return true;
	}
	
	if (iLastDotPos==-1)
	{
		// there is no dot	
		oExtension.Empty();
		return true;
	}
	else
	{
		// there is a dot
		oSimpleFileName=oFileName;
		oExtension=oSimpleFileName.Mid(iLastDotPos+1, iStringLength-iLastDotPos-1);
		oSimpleFileName.Delete(iLastDotPos, iStringLength-iLastDotPos);
		return true;
	}

	return false;
}

bool CFilePathCalc::IsValidFileMask(const CString &oFileMask, bool bAcceptDirectories)
{
	CString oFileName;
	if (!ExtractFileName(oFileMask, oFileName)) return false;
	if (!bAcceptDirectories && oFileName!=oFileMask) return false;		// contains a directory?

	if (oFileName.IsEmpty()) return false;		// "too short"

	// make sure there's at least one joker
	CString oFileNameTemp(oFileName);
	FilterString(oFileNameTemp, "*?");
	if (oFileNameTemp.IsEmpty()) return false;

	CString oTemp(oFileName);
	FilterStringInverse(oTemp, "/\\:\"<>|");		// these two are invalid for files but not for filters: ?*
	if (oFileName!=oTemp) return false;			// file path contained invalid characters
	
	// make sure no characters are between an asterisk and the next period
	int iCurPos=oFileName.Find('*');
	while (iCurPos>=0 && iCurPos<oFileName.GetLength()-1)
	{
		if (oFileName[iCurPos+1]!='.') return false;
		iCurPos=oFileName.Find('*', iCurPos+1);
	}

	// make sure the file doesn't end with a period
	if (oFileName[oFileName.GetLength()-1]=='.') return false;

	// make sure the extension doesn't contain spaces
	{
		CString oFileNameRaw;
		CString oExtension;
		if (!SplitFileAndExtension(oFileName, oFileNameRaw, oExtension)) return false;
		FilterString(oExtension, " ");
		if (!oExtension.IsEmpty()) return false;		// extension contains spaces
	}

	// passed all tests
	return true;
}

bool CFilePathCalc::RemoveOneDirFromEnd(CString &oPath, CString &oRemoved)
{
	oRemoved=oPath;

	// delete last back slash
	oPath.GetBuffer(0);
	oPath.SetAt(oPath.ReverseFind('\\'), '\0');
	oPath.ReleaseBuffer();
	
	long lLastBackSlashPos=
		oPath.ReverseFind('\\');
	if (lLastBackSlashPos==-1)
	{
		// error: no further back slash
		return false;
	}

	// truncate behind last back slash
	oPath.GetBuffer(0);
	oPath.SetAt(lLastBackSlashPos+1, '\0');
	oPath.ReleaseBuffer();

	oRemoved.Delete(0, lLastBackSlashPos+1);

	return true;
}

long CFilePathCalc::CountBackSlashes(const CString &oPath)
{
	long lCount=0;
	long lCurPos=0;
	CString oDummy(oPath);
	while ((lCurPos=oDummy.Find('\\', lCurPos+1))!=-1)
	{
		lCount++;
	}
	
	return lCount;
}

void CFilePathCalc::FilterString(CString &oString, const CString &oAcceptedChars)
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

void CFilePathCalc::FilterStringInverse(CString &oString, const CString &oCharsToRemove)
{
	long lCurPos=0;
	while (lCurPos<oString.GetLength())
	{
		if (oCharsToRemove.Find(oString.GetAt(lCurPos))==-1)
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