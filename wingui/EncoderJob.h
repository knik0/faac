// EncoderJob.h: interface for the CEncoderJob class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENCODERJOB_H__DFE38E70_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
#define AFX_ENCODERJOB_H__DFE38E70_0E81_11D5_8402_0080C88C25BD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SourceTargetFilePair.h"
#include "Id3TagInfo.h"
#include "ConcreteJobBase.h"
#include "EncoderGeneralPropertyPageContents.h"
#include "EncoderQualityPropertyPageContents.h"
#include "EncoderId3PropertyPageContents.h"
//#include "JobList.h"		// ring include
class CJobList;

// do not derive this class from CJob;
// rather let CJob objects contain instances of this class
class CEncoderJob : public CConcreteJobBase
{
public:
	CEncoderJob();
	CEncoderJob(const CEncoderJob &oSource);		// copy constructor
	virtual ~CEncoderJob();

	enum EMpegVersion
	{
		eMpegVersion2=0,
		eMpegVersion4=1
	};

	enum EAacProfile
	{
		eAacProfileMain=0,			// the values of the enumerated constants
		eAacProfileLc=1,			// must reflect the order of the radio
		eAacProfileSsr=2,			// controls on the property page, starting with 0
		eAacProfileLtp=3
	};

	// property getters
	const CSourceTargetFilePair& GetFiles() const						{ return m_oFiles; }
	CSourceTargetFilePair& GetFiles()									{ return m_oFiles; }
	CSourceTargetFilePair* GetFilesPointer()							{ return &m_oFiles; }
	bool GetSourceFileFilterIsRecursive() const							{ return m_bSourceFileFilterIsRecursive; }
	const CId3TagInfo& GetTargetFileId3Info() const						{ return m_oTargetFileId3Info; }
	CId3TagInfo& GetTargetFileId3Info()									{ return m_oTargetFileId3Info; }
	CId3TagInfo* GetTargetFileId3InfoPointer()							{ return &m_oTargetFileId3Info; }
	bool GetAllowMidside() const										{ return m_bAllowMidSide; }
	bool GetUseTns() const												{ return m_bUseTns; }
	bool GetUseLtp() const												{ return m_bUseLtp; }
	bool GetUseLfe() const												{ return m_bUseLfe; }
	EAacProfile GetAacProfile() const									{ return m_eAacProfile; }
	long GetAacProfileL() const											{ return m_eAacProfile; }
	unsigned long GetBitRate() const									{ return m_ulBitRate; }
	long GetBitRateL() const											{ return (long)m_ulBitRate; }
	unsigned long GetBandwidth() const									{ return m_ulBandwidth; }
	long GetBandwidthL() const											{ return (long)m_ulBandwidth; }
	EMpegVersion GetMpegVersion() const									{ return m_eMpegVersion; }
	long GetMpegVersionL() const										{ return m_eMpegVersion; }
					
	// property setters
	void SetFiles(const CSourceTargetFilePair &oFiles)					{ m_oFiles=oFiles; }
	void SetSourceFileFilterIsRecursive(bool bIsRecursive)				{ m_bSourceFileFilterIsRecursive=bIsRecursive; }
	void SetTargetFileId3Info(const CId3TagInfo &oTargetFileId3Info)	{ m_oTargetFileId3Info=oTargetFileId3Info; }
	void SetAllowMidside(bool bAllowMidSide)							{ m_bAllowMidSide=bAllowMidSide; }
	void SetUseTns(bool bUseTns)										{ m_bUseTns=bUseTns; }
	void SetUseLtp(bool bUseLtp)										{ m_bUseLtp=bUseLtp; }
	void SetUseLfe(bool bUseLfe) 										{ m_bUseLfe=bUseLfe; }
	void SetAacProfile(EAacProfile eAacProfile)							{ m_eAacProfile=eAacProfile; }
	void SetAacProfile(long lAacProfile)								{ m_eAacProfile=(EAacProfile)lAacProfile; }
	void SetBitRate(unsigned long ulBitRate)							{ m_ulBitRate=ulBitRate; }
	void SetBitRate(long lBitRate)										{ ASSERT(lBitRate>=0); m_ulBitRate=(unsigned long)lBitRate; }
	void SetBandwidth(unsigned long ulBandwidth)						{ m_ulBandwidth=ulBandwidth; }
	void SetBandwidth(long lBandwidth)									{ ASSERT(lBandwidth>=0); m_ulBandwidth=lBandwidth; }
	void SetMpegVersion(EMpegVersion eMpegVersion)						{ m_eMpegVersion = eMpegVersion; }
	void SetMpegVersion(long lMpegVersion)								{ m_eMpegVersion = (EMpegVersion)lMpegVersion; }

	// no more up to date (also had to call base class version at least)
	// CEncoderJob& operator=(const CEncoderJob &oRight);

	// property page interaction
	CEncoderGeneralPropertyPageContents GetGeneralPageContents() const;
	void ApplyGeneralPageContents(const CEncoderGeneralPropertyPageContents &oPageContents);
	CEncoderQualityPropertyPageContents GetQualityPageContents() const;
	void ApplyQualityPageContents(const CEncoderQualityPropertyPageContents &oPageContents);
	CEncoderId3PropertyPageContents GetId3PageContents() const;
	void ApplyId3PageContents(const CEncoderId3PropertyPageContents &oPageContents);

	// implementations to CJobListCtrlDescribable
	virtual CString DescribeJobTypeShort() const;
	virtual CString DescribeJobTypeLong() const;
	virtual CString DescribeJob() const;

	// implementations to CAbstractJob
	virtual CSupportedPropertyPagesData GetSupportedPropertyPages() const;
	virtual bool ProcessJob(CJobProcessingDynamicUserInputInfo &oUserInputInfo);
	virtual CString GetDetailedDescriptionForStatusDialog() const;

	// implementations to CFileSerializable
	virtual bool PutToArchive(CArchive &oArchive) const;
	virtual bool GetFromArchive(CArchive &oArchive);

	static CString TranslateAacProfileToShortString(EAacProfile eAacProfile);
	static CString TranslateMpegVersionToShortString(EMpegVersion eMpegVersion);

	// when <this> is an encoder job with a filter as source file this method expands
	// this job to a list of single jobs to process;
	// returns false in case of errors but displays error messages on its own;
	// the bCreateDirectories parameter specifies if the target directory structure
	// is already to be created, normally the default value false should be appropriate;
	// the lNumberOfErrorJobs parameter returns the number of jobs that failed to be created;
	// note that this method only increments the previous value if errorneous jobs are
	// encountered, it does not reset it in any kind;
	bool ExpandFilterJob(CJobList &oTarget, long &lNumberOfErrorJobs, bool bCreateDirectories=false) const;

private:
	CSourceTargetFilePair m_oFiles;
	bool m_bSourceFileFilterIsRecursive;
	CId3TagInfo m_oTargetFileId3Info;

	// MPEG version
	EMpegVersion m_eMpegVersion;

	/* AAC profile */
	EAacProfile m_eAacProfile;
	/* Allow mid/side coding */
	bool m_bAllowMidSide;
	/* Use Temporal Noise Shaping */
	bool m_bUseTns;
	/* Use Long Term Prediction */
	bool m_bUseLtp;
	/* Use one of the channels as LFE channel */
	bool m_bUseLfe;

	/* bitrate / channel of AAC file */
	unsigned long m_ulBitRate;

	/* AAC file frequency bandwidth */
	unsigned long m_ulBandwidth;
};

#endif // !defined(AFX_ENCODERJOB_H__DFE38E70_0E81_11D5_8402_0080C88C25BD__INCLUDED_)
