// JobProcessingDynamicUserInputInfo.h: interface for the CJobProcessingDynamicUserInputInfo class.
// Author: Torsten Landmann
//
// This class asks questions to the user and/or saves the referring answers for
// future reuse during the processing of a job list 
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_JOBPROCESSINGDYNAMICUSERINPUTINFO_H__5D3060C1_1CA9_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_JOBPROCESSINGDYNAMICUSERINPUTINFO_H__5D3060C1_1CA9_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "AskCreateDirectoryDialog.h"

class CJobProcessingDynamicUserInputInfo  
{
public:
	CJobProcessingDynamicUserInputInfo();
	virtual ~CJobProcessingDynamicUserInputInfo();

	enum EAutoCreateDirectories
	{
		eNo,
		eYes,
		eAlways,
		eNever,
	};

	// finds out if the necessary directory for a target
	// file is to be created (if not usually the
	// processing of the file will fail)
	EAutoCreateDirectories GetAutoCreateDirectories(const CString &oCurrentDirToCreate, bool bReturnOnlyYesOrNo=true);
	// this is an easier to use version of the last member
	bool GetAutoCreateDirectoryBool(const CString &oCurrentDirToCreate);

private:
	EAutoCreateDirectories m_eAutoCreateDirectories;
	static EAutoCreateDirectories TranslateFromAskDialog(CAskCreateDirectoryDialog::ECreateDirectoryDialogReturn eDialogReturn);
};

#endif // !defined(AFX_JOBPROCESSINGDYNAMICUSERINPUTINFO_H__5D3060C1_1CA9_11D5_8402_0080C88C25BD__INCLUDED_)
