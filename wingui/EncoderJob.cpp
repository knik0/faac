// EncoderJob.cpp: implementation of the CEncoderJob class.
// Author: Torsten Landmann
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderJob.h"
#include "EncoderJobProcessingManager.h"
#include "ProcessJobStatusDialog.h"
#include "RecursiveDirectoryTraverser.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderJob::CEncoderJob():
	m_bAllowMidSide(false),
	m_bUseTns(false),
	m_bUseLtp(false),
	m_bUseLfe(false),
	m_eAacProfile(eAacProfileMain),
	m_ulBitRate(128000),
	m_ulBandwidth(22500)
{

}

CEncoderJob::CEncoderJob(const CEncoderJob &oSource)
{
	*this=oSource;
}

CEncoderJob::~CEncoderJob()
{

}


/*CEncoderJob& CEncoderJob::operator=(const CEncoderJob &oRight)
{
	m_oFiles=oRight.m_oFiles;
	m_bSourceFileFilterIsRecursive=oRight.m_bSourceFileFilterIsRecursive;
	m_oTargetFileId3Info=oRight.m_oTargetFileId3Info;

	m_bAllowMidSide=oRight.m_bAllowMidSide;
	m_bUseTns=oRight.m_bUseTns;
	m_bUseLtp=oRight.m_bUseLtp;
	m_bUseLfe=oRight.m_bUseLfe;
	m_eAacProfile=oRight.m_eAacProfile;

	m_ulBitRate=oRight.m_ulBitRate;

	m_ulBandwidth=oRight.m_ulBandwidth;

	return *this;
}*/

CString CEncoderJob::DescribeJobTypeShort() const
{
	CString oEncoderJob;
	oEncoderJob.LoadString(IDS_EncoderJobShort);
	return oEncoderJob;
}

CString CEncoderJob::DescribeJobTypeLong() const
{
	CString oEncoderJob;
	oEncoderJob.LoadString(IDS_EncoderJobLong);
	return oEncoderJob;
}

CString CEncoderJob::DescribeJob() const
{
	CString oToReturn;
	oToReturn.Format("%s - %s - %.0f kbit/s - %.0f kHz", GetFiles().GetSourceFileName(), TranslateAacProfileToShortString(GetAacProfile()), (double)GetBitRate()/1000, (double)GetBandwidth()/1000);
	if (GetAllowMidside())
	{
		CString oMidSide;
		oMidSide.LoadString(IDS_MidSide);
		oToReturn+=CString(" - ")+oMidSide;
	}
	if (GetUseTns())
	{
		CString oTns;
		oTns.LoadString(IDS_UseTns);
		oToReturn+=CString(" - ")+oTns;
	}
	if (GetUseLtp())
	{
		CString oLtp;
		oLtp.LoadString(IDS_UseLtp);
		oToReturn+=CString(" - ")+oLtp;
	}
	if (GetUseLfe())
	{
		CString oLfe;
		oLfe.LoadString(IDS_UseLfe);
		oToReturn+=CString(" - ")+oLfe;
	}
	return oToReturn;
}

CSupportedPropertyPagesData CEncoderJob::GetSupportedPropertyPages() const
{
	CSupportedPropertyPagesData toReturn;
	toReturn.ShowPage(ePageTypeEncoderGeneral);
	toReturn.ShowPage(ePageTypeEncoderQuality);
	toReturn.ShowPage(ePageTypeEncoderID3);

	return toReturn;
}

bool CEncoderJob::ProcessJob() const
{
	if (!CFilePathCalc::IsValidFileMask(GetFiles().GetSourceFileName()))
	{
		// it's a regular single file encoder job

		// create the manager that manages and performs the processing
		CEncoderJobProcessingManager oManager(this);

		// create the status dialog
		CProcessJobStatusDialog oDlg(&oManager);
		return oDlg.DoModal()==IDOK;
	}
	else
	{
		// it's a filter encoder job

		CJobList oExpandedJobList;
		if (!ExpandFilterJob(oExpandedJobList)) return false;

		CBListReader oReader(oExpandedJobList);
		CJob *poCurrentJob;
		while ((poCurrentJob=oExpandedJobList.GetNextJob(oReader))!=0)
		{
			// make sure the target directory for the current job exists
			CString oTargetDir(poCurrentJob->GetEncoderJob()->GetFiles().GetTargetFileDirectory());
			if (!CRecursiveDirectoryTraverser::MakeSureDirectoryExists(oTargetDir))
			{
				CString oError;
				oError.Format(IDS_ErrorCreatingNestedEncoderJob, poCurrentJob->GetEncoderJob()->GetFiles().GetCompleteSourceFilePath(), GetFiles().GetCompleteSourceFilePath());
				AfxMessageBox("Error creating target directory", MB_OK | MB_ICONSTOP);		// XXX insert resource string and ask user what to do
				return false;
			}

			// process the job
			if (!poCurrentJob->GetEncoderJob()->ProcessJob())
			{
				CString oError;
				oError.Format(IDS_ErrorCreatingNestedEncoderJob, poCurrentJob->GetEncoderJob()->GetFiles().GetCompleteSourceFilePath(), GetFiles().GetCompleteSourceFilePath());
				if (AfxMessageBox(oError, MB_YESNO)!=IDYES)
				{
					return false;
				}
			}
		}

		/*// first find out all files that actually belong to the job
		TItemList<CString> oFiles=
			CRecursiveDirectoryTraverser::FindFiles(
			GetFiles().GetSourceFileDirectory(),
			GetFiles().GetSourceFileName(),
			GetSourceFileFilterIsRecursive());

		if (oFiles.GetNumber()==0)
		{
			CString oError;
			oError.Format(IDS_FilterDidntFindFiles, GetFiles().GetCompleteSourceFilePath());
			AfxMessageBox(oError);
			return false;
		}

		long lTotalNumberOfSubJobsToProcess=oFiles.GetNumber();
		long lCurSubJobCount=0;

		CBListReader oReader(oFiles);
		CString oCurrentFilePath;
		while (oFiles.GetNextElemContent(oReader, oCurrentFilePath))
		{
			CEncoderJob oNewJob(*this);
			oNewJob.SetSubJobNumberInfo(lCurSubJobCount++, lTotalNumberOfSubJobsToProcess);
			if (!oNewJob.GetFiles().SetCompleteSourceFilePath(oCurrentFilePath))
			{
				CString oError;
				oError.Format(IDS_ErrorCreatingNestedEncoderJob, oCurrentFilePath, GetFiles().GetCompleteSourceFilePath());
				if (AfxMessageBox(oError, MB_YESNO)!=IDYES)
				{
					return false;
				}
			}
			// assemble the target file name and apply it to the new job
			{
				// find out the long name of the source file directory
				CString oSourceFileDir;
				{
					oSourceFileDir=GetFiles().GetSourceFileDirectory();
					CString oTemp;
					LPTSTR pDummy;
					::GetFullPathName(oSourceFileDir,
						MAX_PATH, oTemp.GetBuffer(MAX_PATH),
						&pDummy);
					oTemp.ReleaseBuffer();
					if (oTemp[oTemp.GetLength()-1]=='\\')
					{
						oTemp.Delete(oTemp.GetLength()-1);
					}

					oSourceFileDir=oTemp;
				}

				// find out the suffix to append to the target directory
				// for our particular file
				CString oDirSuffix;
				{
					CString oFileDir(oCurrentFilePath);
					CFilePathCalc::MakePath(oFileDir);
					int iLength=oFileDir.GetLength();
					oDirSuffix=oFileDir.Right(iLength-oSourceFileDir.GetLength());
					oDirSuffix.Delete(0);
				}

				// determine the target directory for that particular file
				CString oTargetDir(GetFiles().GetTargetFileDirectory());
				CFilePathCalc::MakePath(oTargetDir, true);
				oTargetDir+=oDirSuffix;
				if (!CRecursiveDirectoryTraverser::MakeSureDirectoryExists(oTargetDir))
				{
					CString oError;
					oError.Format(IDS_ErrorCreatingNestedEncoderJob, oCurrentFilePath, GetFiles().GetCompleteSourceFilePath());
					AfxMessageBox("Error creating target directory", MB_OK | MB_ICONSTOP);		// XXX insert resource string and ask user what to do
					return false;
				}

				CString oSourceFileName;
				CFilePathCalc::ExtractFileName(oCurrentFilePath, oSourceFileName);
				CString oSourceFileNameRaw;
				CString oSourceFileExtension;
				CFilePathCalc::SplitFileAndExtension(oSourceFileName, oSourceFileNameRaw, oSourceFileExtension);
				oNewJob.GetFiles().SetTargetFileDirectory(oTargetDir);
				CString oDefaultExtension;
				oDefaultExtension.LoadString(IDS_EndTargetFileStandardExtension);
				oNewJob.GetFiles().SetTargetFileName(oSourceFileNameRaw+"."+oDefaultExtension);
			}

			// process the new job
			if (!oNewJob.ProcessJob())
			{
				CString oError;
				oError.Format(IDS_ErrorCreatingNestedEncoderJob, oCurrentFilePath, GetFiles().GetCompleteSourceFilePath());
				if (AfxMessageBox(oError, MB_YESNO)!=IDYES)
				{
					return false;
				}
			}
		}*/

		return true;
	}
}

CString CEncoderJob::GetDetailedDescriptionForStatusDialog() const
{
	CString oToReturn;
	oToReturn.Format(IDS_DetailedEncoderJobDescriptionString, GetFiles().GetCompleteSourceFilePath(), GetFiles().GetCompleteTargetFilePath(), TranslateAacProfileToShortString(GetAacProfile()), GetBitRate(), GetBandwidth());
	if (GetAllowMidside())
	{
		CString oMidSide;
		oMidSide.LoadString(IDS_MidSide);
		oToReturn+=CString(" - ")+oMidSide;
	}
	if (GetUseTns())
	{
		CString oTns;
		oTns.LoadString(IDS_UseTns);
		oToReturn+=CString(" - ")+oTns;
	}
	if (GetUseLtp())
	{
		CString oLtp;
		oLtp.LoadString(IDS_UseLtp);
		oToReturn+=CString(" - ")+oLtp;
	}
	if (GetUseLfe())
	{
		CString oLfe;
		oLfe.LoadString(IDS_UseLfe);
		oToReturn+=CString(" - ")+oLfe;
	}
	return oToReturn;
}

bool CEncoderJob::PutToArchive(CArchive &oArchive) const
{
	// put a class version flag
	int iVersion=1;
	oArchive << iVersion;

	if (!m_oFiles.PutToArchive(oArchive)) return false;
	CFileSerializable::SerializeBool(oArchive, m_bSourceFileFilterIsRecursive);
	if (!m_oTargetFileId3Info.PutToArchive(oArchive)) return false;

	oArchive << m_eAacProfile;
	CFileSerializable::SerializeBool(oArchive, m_bAllowMidSide);
	CFileSerializable::SerializeBool(oArchive, m_bUseTns);
	CFileSerializable::SerializeBool(oArchive, m_bUseLtp);
	CFileSerializable::SerializeBool(oArchive, m_bUseLfe);
	oArchive << m_ulBitRate;
	oArchive << m_ulBandwidth;

	return true;
}

bool CEncoderJob::GetFromArchive(CArchive &oArchive)
{
	// fetch the class version flag
	int iVersion;
	oArchive >> iVersion;

	switch (iVersion)
	{
	case 1:
		{
			if (!m_oFiles.GetFromArchive(oArchive)) return false;
			CFileSerializable::DeSerializeBool(oArchive, m_bSourceFileFilterIsRecursive);
			if (!m_oTargetFileId3Info.GetFromArchive(oArchive)) return false;

			oArchive >> (int&)m_eAacProfile;
			CFileSerializable::DeSerializeBool(oArchive, m_bAllowMidSide);
			CFileSerializable::DeSerializeBool(oArchive, m_bUseTns);
			CFileSerializable::DeSerializeBool(oArchive, m_bUseLtp);
			CFileSerializable::DeSerializeBool(oArchive, m_bUseLfe);
			oArchive >> m_ulBitRate;
			oArchive >> m_ulBandwidth;

			return true;
		}
	default:
		{
			// unknown file format version
			return false;
		}
	}
}

CString CEncoderJob::TranslateAacProfileToShortString(EAacProfile eAacProfile)
{
	int iStringId;
	switch (eAacProfile)
	{
	case eAacProfileLc:
		{
			iStringId=IDS_AacProfileLc;
			break;
		}
	case eAacProfileMain:
		{
			iStringId=IDS_AacProfileMain;
			break;
		}
	case eAacProfileSsr:
		{
			iStringId=IDS_AacProfileSsr;
			break;
		}
	default:
		{
			// unknown aac profile
			ASSERT(false);
			iStringId=0;
			break;
		}
	}

	CString oToReturn;
	oToReturn.Format(iStringId);
	return oToReturn;
}

bool CEncoderJob::ExpandFilterJob(CJobList &oTarget, bool bCreateDirectories) const
{
	if (!CFilePathCalc::IsValidFileMask(GetFiles().GetSourceFileName()))
	{
		ASSERT(false);		// not a filter job
		return false;
	}
	else
	{
		// it's a filter encoder job

		oTarget.DeleteAll();

		// first find out all files that actually belong to the job
		TItemList<CString> oFiles=
			CRecursiveDirectoryTraverser::FindFiles(
			GetFiles().GetSourceFileDirectory(),
			GetFiles().GetSourceFileName(),
			GetSourceFileFilterIsRecursive());

		if (oFiles.GetNumber()==0)
		{
			CString oError;
			oError.Format(IDS_FilterDidntFindFiles, GetFiles().GetCompleteSourceFilePath());
			AfxMessageBox(oError);
			return false;
		}

		long lTotalNumberOfSubJobsToProcess=oFiles.GetNumber();
		long lCurSubJobCount=0;

		CBListReader oReader(oFiles);
		CString oCurrentFilePath;
		while (oFiles.GetNextElemContent(oReader, oCurrentFilePath))
		{
			CEncoderJob oNewJob(*this);
			oNewJob.SetSubJobNumberInfo(lCurSubJobCount++, lTotalNumberOfSubJobsToProcess);
			if (!oNewJob.GetFiles().SetCompleteSourceFilePath(oCurrentFilePath))
			{
				CString oError;
				oError.Format(IDS_ErrorCreatingNestedEncoderJob, oCurrentFilePath, GetFiles().GetCompleteSourceFilePath());
				if (AfxMessageBox(oError, MB_YESNO)!=IDYES)
				{
					return false;
				}
			}
			// assemble the target file name and apply it to the new job
			{
				// find out the long name of the source file directory
				CString oSourceFileDir;
				{
					oSourceFileDir=GetFiles().GetSourceFileDirectory();
					CString oTemp;
					LPTSTR pDummy;
					::GetFullPathName(oSourceFileDir,
						MAX_PATH, oTemp.GetBuffer(MAX_PATH),
						&pDummy);
					oTemp.ReleaseBuffer();
					if (oTemp[oTemp.GetLength()-1]=='\\')
					{
						oTemp.Delete(oTemp.GetLength()-1);
					}

					oSourceFileDir=oTemp;
				}

				// find out the suffix to append to the target directory
				// for our particular file
				CString oDirSuffix;
				{
					CString oFileDir(oCurrentFilePath);
					CFilePathCalc::MakePath(oFileDir);
					int iLength=oFileDir.GetLength();
					oDirSuffix=oFileDir.Right(iLength-oSourceFileDir.GetLength());
					oDirSuffix.Delete(0);
				}

				// determine the target directory for that particular file
				CString oTargetDir(GetFiles().GetTargetFileDirectory());
				CFilePathCalc::MakePath(oTargetDir, true);
				oTargetDir+=oDirSuffix;
				if (bCreateDirectories)
				{
					if (!CRecursiveDirectoryTraverser::MakeSureDirectoryExists(oTargetDir))
					{
						CString oError;
						oError.Format(IDS_ErrorCreatingNestedEncoderJob, oCurrentFilePath, GetFiles().GetCompleteSourceFilePath());
						AfxMessageBox("Error creating target directory", MB_OK | MB_ICONSTOP);		// XXX insert resource string and ask user what to do
						return false;
					}
				}

				CString oSourceFileName;
				CFilePathCalc::ExtractFileName(oCurrentFilePath, oSourceFileName);
				CString oSourceFileNameRaw;
				CString oSourceFileExtension;
				CFilePathCalc::SplitFileAndExtension(oSourceFileName, oSourceFileNameRaw, oSourceFileExtension);
				oNewJob.GetFiles().SetTargetFileDirectory(oTargetDir);
				CString oDefaultExtension;
				oDefaultExtension.LoadString(IDS_EndTargetFileStandardExtension);
				oNewJob.GetFiles().SetTargetFileName(oSourceFileNameRaw+"."+oDefaultExtension);
			}

			oTarget.AddJob(oNewJob);
		}

		return true;
	}
}

CEncoderGeneralPropertyPageContents CEncoderJob::GetGeneralPageContents() const
{
	CEncoderGeneralPropertyPageContents oToReturn;
	oToReturn.m_oSourceDirectory.SetContent(GetFiles().GetSourceFileDirectory());
	oToReturn.m_oSourceFile.SetContent(GetFiles().GetSourceFileName());
	oToReturn.m_oTargetDirectory.SetContent(GetFiles().GetTargetFileDirectory());
	oToReturn.m_oTargetFile.SetContent(GetFiles().GetTargetFileName());
	oToReturn.m_oSourceFileFilterIsRecursive.SetCheckMark(GetSourceFileFilterIsRecursive());

	// save if this job would have the "recursive" check box enabled
	oToReturn.m_oSourceFilterRecursiveCheckboxVisible.SetCheckMark(CFilePathCalc::IsValidFileMask(GetFiles().GetSourceFileName()));

	return oToReturn;
}

void CEncoderJob::ApplyGeneralPageContents(const CEncoderGeneralPropertyPageContents &oPageContents)
{
	// note: the use of getters on the righthand side is correct since they return references
	oPageContents.m_oSourceDirectory.ApplyToJob(GetFiles().GetSourceFileDirectory());
	oPageContents.m_oSourceFile.ApplyToJob(GetFiles().GetSourceFileName());
	oPageContents.m_oTargetDirectory.ApplyToJob(GetFiles().GetTargetFileDirectory());
	oPageContents.m_oTargetFile.ApplyToJob(GetFiles().GetTargetFileName());
	oPageContents.m_oSourceFileFilterIsRecursive.ApplyToJob(m_bSourceFileFilterIsRecursive);

	// ignore oPageContents.m_oSourceFilterRecursiveCheckboxVisible
}

CEncoderQualityPropertyPageContents CEncoderJob::GetQualityPageContents() const
{
	CEncoderQualityPropertyPageContents oToReturn;
	oToReturn.m_oBitRate.SetContent(GetBitRateL());
	oToReturn.m_oBandwidth.SetContent(GetBandwidthL());
	oToReturn.m_oAllowMidSide.SetCheckMark(GetAllowMidside());
	oToReturn.m_oUseTns.SetCheckMark(GetUseTns());
	oToReturn.m_oUseLtp.SetCheckMark(GetUseLtp());
	oToReturn.m_oUseLfe.SetCheckMark(GetUseLfe());
	oToReturn.m_oAacProfile.SetContent(GetAacProfileL(), true);

	return oToReturn;
}

void CEncoderJob::ApplyQualityPageContents(const CEncoderQualityPropertyPageContents &oPageContents)
{
	// note: the use of getters on the righthand side is correct since they return references
	oPageContents.m_oBitRate.ApplyToJob((long&)m_ulBitRate);
	oPageContents.m_oBandwidth.ApplyToJob((long&)m_ulBandwidth);
	oPageContents.m_oAllowMidSide.ApplyToJob(m_bAllowMidSide);
	oPageContents.m_oUseTns.ApplyToJob(m_bUseTns);
	oPageContents.m_oUseLtp.ApplyToJob(m_bUseLtp);
	oPageContents.m_oUseLfe.ApplyToJob(m_bUseLfe);
	long lTemp=-1;
	oPageContents.m_oAacProfile.ApplyToJob(lTemp);
	if (lTemp>=0) SetAacProfile(lTemp);
}

CEncoderId3PropertyPageContents CEncoderJob::GetId3PageContents() const
{
	CEncoderId3PropertyPageContents oToReturn;
	oToReturn.m_oArtist.SetContent(GetTargetFileId3Info().GetArtist());
	oToReturn.m_oTrackNo.SetContent(GetTargetFileId3Info().GetTrackNo(), true);
	oToReturn.m_oAlbum.SetContent(GetTargetFileId3Info().GetAlbum());
	oToReturn.m_oYear.SetContent(GetTargetFileId3Info().GetYear(), true);
	oToReturn.m_oTitle.SetContent(GetTargetFileId3Info().GetSongTitle());
	oToReturn.m_oCopyright.SetCheckMark(GetTargetFileId3Info().GetCopyright());
	oToReturn.m_oOriginalArtist.SetContent(GetTargetFileId3Info().GetOriginalArtist());
	oToReturn.m_oComposer.SetContent(GetTargetFileId3Info().GetComposer());
	oToReturn.m_oUrl.SetContent(GetTargetFileId3Info().GetUrl());
	oToReturn.m_oGenre.SetContentText(GetTargetFileId3Info().GetGenre());
	oToReturn.m_oEncodedBy.SetContent(GetTargetFileId3Info().GetEncodedBy());
	oToReturn.m_oComment.SetContent(GetTargetFileId3Info().GetComment());

	return oToReturn;
}

void CEncoderJob::ApplyId3PageContents(const CEncoderId3PropertyPageContents &oPageContents)
{
	// note: the use of getters on the righthand side is correct since they return references
	oPageContents.m_oArtist.ApplyToJob(GetTargetFileId3Info().GetArtist());
	oPageContents.m_oTrackNo.ApplyToJob(GetTargetFileId3Info().GetTrackNoRef());
	oPageContents.m_oAlbum.ApplyToJob(GetTargetFileId3Info().GetAlbum());
	oPageContents.m_oYear.ApplyToJob(GetTargetFileId3Info().GetYearRef());
	oPageContents.m_oTitle.ApplyToJob(GetTargetFileId3Info().GetSongTitle());
	oPageContents.m_oCopyright.ApplyToJob(GetTargetFileId3Info().GetCopyrightRef());
	oPageContents.m_oOriginalArtist.ApplyToJob(GetTargetFileId3Info().GetOriginalArtist());
	oPageContents.m_oComposer.ApplyToJob(GetTargetFileId3Info().GetComposer());
	oPageContents.m_oUrl.ApplyToJob(GetTargetFileId3Info().GetUrl());
	oPageContents.m_oGenre.ApplyToJob(GetTargetFileId3Info().GetGenre());
	oPageContents.m_oEncodedBy.ApplyToJob(GetTargetFileId3Info().GetEncodedBy());
	oPageContents.m_oComment.ApplyToJob(GetTargetFileId3Info().GetComment());
}