// RecursiveDirectoryTraverser.h: interface for the CRecursiveDirectoryTraverser class.
// Author: Torsten Landmann
//
// This class is made for finding files within a directory tree.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_RECURSIVEDIRECTORYTRAVERSER_H__99D7AE61_17EB_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_RECURSIVEDIRECTORYTRAVERSER_H__99D7AE61_17EB_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TItemList.h"

class CRecursiveDirectoryTraverser  
{
public:
	CRecursiveDirectoryTraverser();
	virtual ~CRecursiveDirectoryTraverser();

	// the filter string may be a directory name, a file, or a directory
	// name with a file filter;
	// returns -1 in case of errors
	static int CountMatchingFiles(const CString &oFilterString);

	// this method finds all files that are in the specified directory
	// and match the filter; it can walk recursively through the entire
	// subtree returning all files in the subtree that match the filter;
	// in this case directory names are NOT matched against the filter;
	// return value: a list of complete paths of the files found;
	// this method displays error messages and returns an empty list in case
	// of errors
	static TItemList<CString> FindFiles(const CString &oRootDirectory, const CString &oFileNameFilter, bool bRecursive);

	// returns if the directory was found or could be created. if false is
	// returned this mostly means that there is a concurring file to 
	// the first directory that needs to be created
	static bool MakeSureDirectoryExists(const CString &oDirectoryPath);

private:

	// example:
	//	before:
	//		oExistingPath=="c:\somewhere\hello\"
	//		oCreatablePath="somewhereelse\super\"
	//	after:
	//		oExistingPath=="c:\somewhere\hello\somewhereelse"
	//		oCreatablePath="super\"
	//////
	// the method physically creates the directory on the disk;
	// returns false in case of errors
	static bool CreateOneDirectory(CString &oExistingPath, CString &oCreatablePath);
};

#endif // !defined(AFX_RECURSIVEDIRECTORYTRAVERSER_H__99D7AE61_17EB_11D5_8402_0080C88C25BD__INCLUDED_)
