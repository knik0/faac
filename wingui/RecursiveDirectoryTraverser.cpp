// RecursiveDirectoryTraverser.cpp: implementation of the CRecursiveDirectoryTraverser class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "RecursiveDirectoryTraverser.h"
#include "FilePathCalc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRecursiveDirectoryTraverser::CRecursiveDirectoryTraverser()
{

}

CRecursiveDirectoryTraverser::~CRecursiveDirectoryTraverser()
{

}

int CRecursiveDirectoryTraverser::CountMatchingFiles(const CString &oFilterString)
{
	int iToReturn=0;

	CFileFind oFileFind;
	CString oSearchMask=oFilterString;
	
	if (oFileFind.FindFile(oSearchMask))
	{		
		iToReturn++;
		while (oFileFind.FindNextFile())
		{
			iToReturn++;
		}
	}

	oFileFind.Close();

	return iToReturn;
}

TItemList<CString> CRecursiveDirectoryTraverser::FindFiles(const CString &oRootDirectory, const CString &oFileNameFilter, bool bRecursive)
{
	TItemList<CString> oToReturn;
	CString oRootDir(oRootDirectory);
	if (!CFilePathCalc::MakePath(oRootDir, true) || !CFilePathCalc::IsValidFileMask(oFileNameFilter))
	{
		CString oError;
		oError.Format(IDS_SearchParametersInvalid, oRootDir, oFileNameFilter);
		AfxMessageBox(oError, MB_OK | MB_ICONSTOP);
		return oToReturn;
	}

	CFileFind oFileFind;
	CString oSearchMask=
		// "c:\\eigene dateien\\standard.apf";
		oRootDir+oFileNameFilter;

	
	if (oFileFind.FindFile(oSearchMask))
	{	
		CString oFileName;
		while (oFileFind.FindNextFile())
		{
			oFileName=oFileFind.GetFilePath();
			oToReturn.AddNewElem(oFileName);
		}
	}

	oFileFind.Close();

	// if requested traverse child directories
	if (bRecursive)
	{
		oSearchMask=oRootDir+"*";
		if (oFileFind.FindFile(oSearchMask))
		{
		
			CString oFileName;
			while (oFileFind.FindNextFile())
			{
				if (oFileFind.IsDirectory())
				{
					if (oFileFind.GetFileName()!="." &&
						oFileFind.GetFileName()!="..")
					{
						oToReturn+=FindFiles(oFileFind.GetFilePath(), oFileNameFilter, bRecursive);
					}
				}
			}
		}
	}

	return oToReturn;
}

bool CRecursiveDirectoryTraverser::MakeSureDirectoryExists(const CString &oDirectoryPath)
{
	CString oLocalPath(oDirectoryPath);
	CFilePathCalc::MakePath(oLocalPath);
	
	// first split the desired path in existing and "creatable" path
	CString oExistingPath;
	CString oCreatablePath;
	{
		oLocalPath.Delete(oLocalPath.GetLength()-1);
		while (CountMatchingFiles(oLocalPath)<1)
		{
			int iLastBackslashPos=oLocalPath.ReverseFind('\\');
			if (iLastBackslashPos>=0)
			{
				int iLength=oLocalPath.GetLength();
				oLocalPath.Delete(iLastBackslashPos, iLength-iLastBackslashPos);
			}
			else
			{
				// no more backslashes
				oCreatablePath=oDirectoryPath;
				break;
			}
		}
		if (oCreatablePath.IsEmpty())
		{
			// must determine the path to create
			oExistingPath=oLocalPath;
			CFilePathCalc::MakePath(oExistingPath, true);
			oCreatablePath=oDirectoryPath;
			CFilePathCalc::MakePath(oCreatablePath, true);

			// now we must remove directories from oCreatablePath as long
			// the string matches with oExistingPath
			long lExistingPos=0;
			while (!oCreatablePath.IsEmpty() &&
				!(lExistingPos==oExistingPath.GetLength()) &&
				oExistingPath.GetAt(lExistingPos)==oCreatablePath[0])
			{
				lExistingPos++;
				oCreatablePath.Delete(0);
			}

			// now the two paths are complete and we can begin creating the
			// directories
			while (!oCreatablePath.IsEmpty())
			{
				if (!CreateOneDirectory(oExistingPath, oCreatablePath))
				{
					// XXX might be useful to clean up already created directories
					// here
					return false;
				}
			}
		}
	}
	return true;
}

bool CRecursiveDirectoryTraverser::CreateOneDirectory(CString &oExistingPath, CString &oCreatablePath)
{
	// extract the first substring from the creatable path
	int iBackslashpos=oCreatablePath.Find('\\');
	if (iBackslashpos<=0) return false;

	CString oNextDirectoryToCreate(oCreatablePath.Left(iBackslashpos));

	// first try to create the directory
	if (!::CreateDirectory(
		oExistingPath+oNextDirectoryToCreate, 0))
	{
		return false;
	}

	// directory was successfully created
	oCreatablePath.Delete(0, iBackslashpos+1);
	oExistingPath+=oNextDirectoryToCreate+"\\";

	return true;
}