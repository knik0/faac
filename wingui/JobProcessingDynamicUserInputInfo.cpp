// JobProcessingDynamicUserInputInfo.cpp: implementation of the CJobProcessingDynamicUserInputInfo class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "JobProcessingDynamicUserInputInfo.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CJobProcessingDynamicUserInputInfo::CJobProcessingDynamicUserInputInfo():
	m_eAutoCreateDirectories(eYes)
{

}

CJobProcessingDynamicUserInputInfo::~CJobProcessingDynamicUserInputInfo()
{

}

CJobProcessingDynamicUserInputInfo::EAutoCreateDirectories CJobProcessingDynamicUserInputInfo::GetAutoCreateDirectories(const CString &oCurrentDirToCreate, bool bReturnOnlyYesOrNo)
{
	switch (m_eAutoCreateDirectories)
	{
	case eNo:
	case eYes:
		{
			// must ask the user
			CAskCreateDirectoryDialog oDlg(oCurrentDirToCreate);
			m_eAutoCreateDirectories=TranslateFromAskDialog((CAskCreateDirectoryDialog::ECreateDirectoryDialogReturn)oDlg.DoModal());
			break;
		}
	default:
		{
			// don't need to ask the user
			break;
		}
	}

	EAutoCreateDirectories eToReturn(m_eAutoCreateDirectories);

	if (bReturnOnlyYesOrNo)
	{
		if (eToReturn==eAlways) eToReturn=eYes;
		if (eToReturn==eNever) eToReturn=eNo;
	}

	return eToReturn;
}

bool CJobProcessingDynamicUserInputInfo::GetAutoCreateDirectoryBool(const CString &oCurrentDirToCreate)
{
	return GetAutoCreateDirectories(oCurrentDirToCreate, true)==eYes;
}

CJobProcessingDynamicUserInputInfo::EAutoCreateDirectories CJobProcessingDynamicUserInputInfo::TranslateFromAskDialog(CAskCreateDirectoryDialog::ECreateDirectoryDialogReturn eDialogReturn)
{
	switch (eDialogReturn)
	{
	case CAskCreateDirectoryDialog::eNo:
		{
			return eNo;
		}
	case CAskCreateDirectoryDialog::eYes:
		{
			return eYes;
		}
	case CAskCreateDirectoryDialog::eAlways:
		{
			return eAlways;
		}
	default:
		{
			// unknown parameter value
			ASSERT(false);
		}
	case CAskCreateDirectoryDialog::eNever:
		{
			return eNever;
		}
	}
}