// SourceTargetFilePair.h: interface for the CSourceTargetFilePair class.
// Author: Torsten Landmann
//
// encapsulates the source and target information of an encoding or
// decoding job
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SOURCETARGETFILEPAIR_H__DFE38E71_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_SOURCETARGETFILEPAIR_H__DFE38E71_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FilePathCalc.h"
#include "FileSerializable.h"

class CSourceTargetFilePair : public CFileSerializable
{
public:
	CSourceTargetFilePair();
	CSourceTargetFilePair(const CString &oSourceFileDirectory, const CString &oSourceFileName, const CString &oTargetFileDirectory, const CString &oTargetFileName);
	CSourceTargetFilePair(const CString &oSourceFilePath, const CString &oTargetFilePath);
	CSourceTargetFilePair(const CSourceTargetFilePair &oSource);		// copy constructor
	virtual ~CSourceTargetFilePair();

	// property getters
	const CString& GetSourceFileDirectory() const						{ return m_oSourceFileDirectory; }
	CString& GetSourceFileDirectory()									{ return m_oSourceFileDirectory; }
	const CString& GetSourceFileName() const							{ return m_oSourceFileName; }
	CString& GetSourceFileName()										{ return m_oSourceFileName; }
	const CString& GetTargetFileDirectory() const						{ return m_oTargetFileDirectory; }
	CString& GetTargetFileDirectory()									{ return m_oTargetFileDirectory; }
	const CString& GetTargetFileName() const							{ return m_oTargetFileName; }
	CString& GetTargetFileName()										{ return m_oTargetFileName; }

	// property setters
	void SetSourceFileDirectory(const CString& oSourceFileDirectory)	{ m_oSourceFileDirectory=oSourceFileDirectory; CFilePathCalc::MakePath(m_oSourceFileDirectory); }
	void SetSourceFileName(const CString& oSourceFileName)				{ m_oSourceFileName=oSourceFileName; }
	void SetTargetFileDirectory(const CString& oTargetFileDirectory)	{ m_oTargetFileDirectory=oTargetFileDirectory; CFilePathCalc::MakePath(m_oTargetFileDirectory); }
	void SetTargetFileName(const CString& oTargetFileName)				{ m_oTargetFileName=oTargetFileName; }

	// functionality
	CString GetCompleteSourceFilePath() const;
	CString GetCompleteTargetFilePath() const;
	bool SetCompleteSourceFilePath(const CString &oPath);		// returns false if the argument could not be evaluated properly
	bool SetCompleteTargetFilePath(const CString &oPath);		// returns false if the argument could not be evaluated properly

	CSourceTargetFilePair& operator=(const CSourceTargetFilePair &oRight);

	// implementations to CFileSerializable
	virtual bool PutToArchive(CArchive &oArchive) const;
	virtual bool GetFromArchive(CArchive &oArchive);

private:
	CString m_oSourceFileDirectory;
	CString m_oSourceFileName;

	CString m_oTargetFileDirectory;
	CString m_oTargetFileName;
};

#endif // !defined(AFX_SOURCETARGETFILEPAIR_H__DFE38E71_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
