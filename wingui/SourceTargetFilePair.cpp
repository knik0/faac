// SourceTargetFilePair.cpp: implementation of the CSourceTargetFilePair class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "SourceTargetFilePair.h"
#include "FilePathCalc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSourceTargetFilePair::CSourceTargetFilePair()
{

}

CSourceTargetFilePair::CSourceTargetFilePair(const CString &oSourceFileDirectory, const CString &oSourceFileName, const CString &oTargetFileDirectory, const CString &oTargetFileName):
	m_oSourceFileDirectory(oSourceFileName),
	m_oSourceFileName(oSourceFileName),
	m_oTargetFileDirectory(oTargetFileDirectory),
	m_oTargetFileName(oTargetFileName)
{
}

CSourceTargetFilePair::CSourceTargetFilePair(const CString &oSourceFilePath, const CString &oTargetFilePath)
{
	bool bSuccess=true;

	m_oSourceFileDirectory=oSourceFilePath;
	bSuccess&=SetCompleteSourceFilePath(oSourceFilePath);
	bSuccess&=SetCompleteTargetFilePath(oTargetFilePath);

	ASSERT(bSuccess);
}

CSourceTargetFilePair::CSourceTargetFilePair(const CSourceTargetFilePair &oSource)
{
	*this=oSource;
}

CSourceTargetFilePair::~CSourceTargetFilePair()
{

}

CString CSourceTargetFilePair::GetCompleteSourceFilePath() const
{
	CString oPath(GetSourceFileDirectory());
	CFilePathCalc::MakePath(oPath);
	return oPath+GetSourceFileName();
}

CString CSourceTargetFilePair::GetCompleteTargetFilePath() const
{
	CString oPath(GetTargetFileDirectory());
	CFilePathCalc::MakePath(oPath);
	return oPath+GetTargetFileName();
}

bool CSourceTargetFilePair::SetCompleteSourceFilePath(const CString &oPath)
{
	bool bSuccess=true;

	m_oSourceFileDirectory=oPath;
	bSuccess&=CFilePathCalc::MakePath(m_oSourceFileDirectory);
	bSuccess&=CFilePathCalc::ExtractFileName(oPath, m_oSourceFileName);

	return bSuccess;
}

bool CSourceTargetFilePair::SetCompleteTargetFilePath(const CString &oPath)
{
	bool bSuccess=true;

	m_oTargetFileDirectory=oPath;
	bSuccess&=CFilePathCalc::MakePath(m_oTargetFileDirectory);
	bSuccess&=CFilePathCalc::ExtractFileName(oPath, m_oTargetFileName);

	return bSuccess;
}

CSourceTargetFilePair& CSourceTargetFilePair::operator=(const CSourceTargetFilePair &oRight)
{
	m_oSourceFileDirectory=oRight.m_oSourceFileDirectory;
	m_oSourceFileName=oRight.m_oSourceFileName;

	m_oTargetFileDirectory=oRight.m_oTargetFileDirectory;
	m_oTargetFileName=oRight.m_oTargetFileName;

	return *this;
}

bool CSourceTargetFilePair::PutToArchive(CArchive &oArchive) const
{
	// put a class version flag
	int iVersion=1;
	oArchive << iVersion;

	oArchive << m_oSourceFileDirectory;
	oArchive << m_oSourceFileName;

	oArchive << m_oTargetFileDirectory;
	oArchive << m_oTargetFileName;

	return true;
}

bool CSourceTargetFilePair::GetFromArchive(CArchive &oArchive)
{
	// fetch the class version flag
	int iVersion;
	oArchive >> iVersion;

	switch (iVersion)
	{
	case 1:
		{
			oArchive >> m_oSourceFileDirectory;
			oArchive >> m_oSourceFileName;

			oArchive >> m_oTargetFileDirectory;
			oArchive >> m_oTargetFileName;

			return true;
		}
	default:
		{
			// unknown file format version
			return false;
		}
	}
}