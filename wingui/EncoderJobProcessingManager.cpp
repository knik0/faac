// EncoderJobProcessingManager.cpp: implementation of the CEncoderJobProcessingManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderJobProcessingManager.h"
#include "WindowUtil.h"
#include "RecursiveDirectoryTraverser.h"

#include <sndfile.h>
#include "faac.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//#define DUMMY_ENCODERJOB_PROCESSING

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderJobProcessingManager::CEncoderJobProcessingManager(
	CEncoderJob *poJobToProcess, CJobProcessingDynamicUserInputInfo &oUserInputInfo):

	m_poJobToProcess(poJobToProcess),
	m_poInfoTarget(0),
	m_eCurrentWorkingState(eInitial),
	m_oUserInputInfo(oUserInputInfo)
{
}

CEncoderJobProcessingManager::~CEncoderJobProcessingManager()
{
}

bool CEncoderJobProcessingManager::MayStartProcessingWithStatusDialog()
{
	long lStartTimeMillis=::GetTickCount();
	CEncoderJob *poJob=m_poJobToProcess;
	// make sure the target directory exists at all
	{
		CString oTargetFileDir(poJob->GetFiles().GetTargetFileDirectory());
		if (oTargetFileDir.GetLength()<1)
		{
			CString oError;
			oError.LoadString(IDS_InvalidTargetDirectory);
			poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, oError);
			return false;
		}
		
		while (oTargetFileDir.GetAt(oTargetFileDir.GetLength()-1)=='\\')
		{
			oTargetFileDir.Delete(oTargetFileDir.GetLength()-1);
		}
		if (CRecursiveDirectoryTraverser::CountMatchingFiles(oTargetFileDir)<1)
		{
			// the target directory doesn't exist;
			// there are two possibilities: create it
			// or
			// abort with an error
			CString oTargetDir(poJob->GetFiles().GetTargetFileDirectory());
			if (m_oUserInputInfo.GetAutoCreateDirectoryBool(oTargetDir))
			{
				if (!CRecursiveDirectoryTraverser::MakeSureDirectoryExists(oTargetDir))
				{
					// directory couldn't be created;
					// log the error
					CString oError;
					oError.Format(IDS_ErrorCreatingDirectory, poJob->GetFiles().GetTargetFileDirectory());
					poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, oError);
					return false;
				}
			}
			else
			{
				// the directory doesn't exist and the user refused to create it
				poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, IDS_UserRefusedToCreateTargetDirectory);
				return false;
			}
		}
		else
		{
			// the directory already exists so everything is ok up to here
		}
	}

	return true;
}

void CEncoderJobProcessingManager::Start(
	CProcessingStatusDialogInfoFeedbackCallbackInterface *poInfoTarget)
{
	if (poInfoTarget!=0)
	{
		m_poInfoTarget=poInfoTarget;
	}

	switch (m_eCurrentWorkingState)
	{
	case eInitial:
		{
			// initialize the status dialog
			{
				// define supported buttons
				m_poInfoTarget->SetAvailableActions(true, false);

				// set the dialog caption
				m_poInfoTarget->SetAdditionalCaptionInfo(m_poJobToProcess->GetJobProcessingAdditionalCaptionBarInformation());
			}

			// initialize ourselves and run the job
			m_eCurrentWorkingState=eRunning;
			m_poInfoTarget->ReturnToCaller(DoProcessing());
			break;
		}
	case ePaused:
		{
			m_eCurrentWorkingState=eRunning;
			break;
		}
	default:
		{
			// call to Start() is invalid except in the above two cases
			ASSERT(false);
		}
	}
}

void CEncoderJobProcessingManager::Stop()
{
	switch (m_eCurrentWorkingState)
	{
	case eRunning:
	case ePaused:
		{
			m_eCurrentWorkingState=eStopped;
			break;
		}
	case eCleanup:
		{
			// ignore
			break;
		}
	default:
		{
			// call to Stop() is invalid except in the above two cases
			ASSERT(false);
		}
	}
}

void CEncoderJobProcessingManager::Pause()
{
	switch (m_eCurrentWorkingState)
	{
	case eRunning:
		{
			m_eCurrentWorkingState=ePaused;
			break;
		}
	default:
		{
			// call to Pause() is invalid except in the above case
			ASSERT(false);
		}
	}
}

#ifdef DUMMY_ENCODERJOB_PROCESSING
bool CEncoderJobProcessingManager::DoProcessing()
{
	long lStartTimeMillis=::GetTickCount();

	const CEncoderJob *poJob=m_poJobToProcess;

	long lMaxCount=250*64000/(5+poJob->GetBitRate());
	for (long lPos=0; lPos<lMaxCount; lPos++)
	{
		long lMultiplicationDummy;
		for (long lPosInner=0; lPosInner<10000000; lPosInner++)
		{
			// just a pause to simulate work
			lMultiplicationDummy=234985;
			lMultiplicationDummy*=301872;
		}

		switch (m_eCurrentWorkingState)
		{
		case eRunning:
			{
				// just report our current state
				WriteProgress(lStartTimeMillis, lMaxCount, lPos);
				m_poInfoTarget->ProcessUserMessages();
				break;
			}
		case ePaused:
			{
				// must wait
				while (m_eCurrentWorkingState==ePaused)
				{
					// be idle
					m_poInfoTarget->ProcessUserMessages();
					Sleep(200);
				}
				break;
			}
		case eStopped:
			{
				// must interrupt
				poJob->SetProcessingOutcomeCurTime(CAbstractJob::eUserAbort, lStartTimeMillis, "");
				return false;
			}
		}
	}

	m_eCurrentWorkingState=eCleanup;

	poJob->SetProcessingOutcomeCurTime(CAbstractJob::eSuccessfullyProcessed, lStartTimeMillis, "");

	return true;
}

#else

bool CEncoderJobProcessingManager::DoProcessing()
{
	long lStartTimeMillis=::GetTickCount();
	CEncoderJob *poJob=m_poJobToProcess;
	bool bInterrupted=false;

#ifdef _DEBUG
	// make sure preprocessing has been done properly to warn the programmer if not
	if (!MayStartProcessingWithStatusDialog())
	{
		// you should call MayStartProcessingWithStatusDialog() before starting
		// a status dialog driven processing of the job
		ASSERT(false);
	}
#endif

	SNDFILE *phInFile;
	SF_INFO sctSfInfo;

	// open the input file
	if ((phInFile=sf_open_read(poJob->GetFiles().GetCompleteSourceFilePath(), &sctSfInfo)) != NULL)
	{
		// determine input file parameters
		long lSampleRate=sctSfInfo.samplerate;
		long lNumChannels=sctSfInfo.channels;

		// open and setup the encoder
		unsigned long ulInputSamplesPerLoopCycle;
		unsigned long ulMaxLoopCycleOutputSize;
		faacEncHandle hEncoder=faacEncOpen(lSampleRate, lNumChannels, &ulInputSamplesPerLoopCycle, &ulMaxLoopCycleOutputSize);
		if (hEncoder!=0)
		{
			// set encoder configuration
			faacEncConfigurationPtr pEncConfig=faacEncGetCurrentConfiguration(hEncoder);

			pEncConfig->allowMidside = poJob->GetAllowMidside() ? 1 : 0;
			pEncConfig->useTns = poJob->GetUseTns() ? 1 : 0;
//			pEncConfig->useLtp = poJob->GetUseLtp() ? 1 : 0;
			pEncConfig->useLfe = poJob->GetUseLfe() ? 1 : 0;
			pEncConfig->bitRate = poJob->GetBitRate();
			pEncConfig->bandWidth = poJob->GetBandwidth();
			pEncConfig->mpegVersion = GetMpegVersionConstant(poJob->GetMpegVersion());
			pEncConfig->aacObjectType = GetAacProfileConstant(poJob->GetAacProfile());

			/* temp fix for MPEG4 LTP object type */
//			if (pEncConfig->aacObjectType == 1)
//				pEncConfig->aacObjectType = 3;

			if (!faacEncSetConfiguration(hEncoder, pEncConfig))
			{
				faacEncClose(hEncoder);
				sf_close(phInFile);

				poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, IDS_FaacEncSetConfigurationFailed);

				return false;
			}

			// open the output file
			CFile *poFile;
			CArchive *poTargetFileOutputArchive;
			if (OpenOutputFileArchive(poJob->GetFiles().GetCompleteTargetFilePath(), poFile, poTargetFileOutputArchive))
			{
				long lStartTime=GetTickCount();
				long lLastUpdated=0;
				long lTotalBytesRead = 0;

				long lSamplesInput=0;
				long lBytesConsumed=0;
				short *parsPcmBuf;
				char *parcBitBuf;

				parsPcmBuf=new short[ulInputSamplesPerLoopCycle];
				parcBitBuf=new char[ulMaxLoopCycleOutputSize];

				while (true)
				{
					long lBytesWritten;

					lSamplesInput=sf_read_short(phInFile, parsPcmBuf, ulInputSamplesPerLoopCycle);
					
					lTotalBytesRead+=lSamplesInput*sizeof(short);

					// call the actual encoding routine
					lBytesWritten=faacEncEncode(
						hEncoder,
						parsPcmBuf,
						lSamplesInput,
						parcBitBuf,
						ulMaxLoopCycleOutputSize);

					switch (m_eCurrentWorkingState)
					{
					case eRunning:
						{
							// just report our current state and process waiting window messages
							WriteProgress(lStartTimeMillis, (sctSfInfo.samples*sizeof(short)*lNumChannels), lTotalBytesRead);
							m_poInfoTarget->ProcessUserMessages();
							break;
						}
					case ePaused:
						{
							// must wait
							while (m_eCurrentWorkingState==ePaused)
							{
								// be idle
								m_poInfoTarget->ProcessUserMessages();
								Sleep(200);
							}
							break;
						}
					case eStopped:
						{
							// must interrupt
							bInterrupted=true;
							break;
						}
					}
					
					if (bInterrupted) 
					{
						// Stop Pressed
						poJob->SetProcessingOutcomeCurTime(CAbstractJob::eUserAbort, lStartTimeMillis, "");
						break;
					}

					if (lSamplesInput==0 && lBytesWritten==0)
					{
						// all done, bail out
						poJob->SetProcessingOutcomeCurTime(CAbstractJob::eSuccessfullyProcessed, lStartTimeMillis, "");
						break;
					}

					if (lBytesWritten < 0)
					{
						poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, IDS_FaacEncEncodeFrameFailed);
						bInterrupted=true;
						break;
					}

					poTargetFileOutputArchive->Write(parcBitBuf, lBytesWritten);
				}

				// close the target file
				if (poTargetFileOutputArchive!=0) delete poTargetFileOutputArchive;
				if (poFile!=0) delete poFile;
				if (parsPcmBuf!=0) delete[] parsPcmBuf;
				if (parcBitBuf!=0) delete[] parcBitBuf;
			}

			faacEncClose(hEncoder);
		}

		sf_close(phInFile);
		//MessageBeep(1);		// no more done here
	}
	else
	{
		poJob->SetProcessingOutcomeCurTime(CAbstractJob::eError, lStartTimeMillis, IDS_CouldntOpenInputFile);
		bInterrupted=true;
	}

	return !bInterrupted;
}
#endif

void CEncoderJobProcessingManager::WriteProgress(long lOperationStartTickCount, long lMaxSteps, long lCurSteps)
{
	long lCurTime=::GetTickCount();
	double dProgress=100.*lCurSteps/lMaxSteps;
	if (dProgress>100) dProgress=100;		// just security
	if (dProgress<0) dProgress=0;			// just security
	CString oTopStatusText=m_poJobToProcess->GetDetailedDescriptionForStatusDialog();

	long lElapsedTime=lCurTime-lOperationStartTickCount;
	long lEstimateEntireTime=(long)((100.*lElapsedTime)/dProgress);
	long lETA=lEstimateEntireTime-lElapsedTime;
	CString oElapsedTime(CWindowUtil::GetTimeDescription(lElapsedTime));
	CString oEntireTime(CWindowUtil::GetTimeDescription(lEstimateEntireTime));
	CString oETA(CWindowUtil::GetTimeDescription(lETA));
	CString oBottomStatusText;
	oBottomStatusText.Format("%.1f %%\n\n%s / %s   -   %s", dProgress, oElapsedTime, oEntireTime, oETA);

	m_poInfoTarget->SetStatus(dProgress, oTopStatusText, oBottomStatusText);
}

int CEncoderJobProcessingManager::GetAacProfileConstant(CEncoderJob::EAacProfile eAacProfile)
{
	switch (eAacProfile)
	{
	case CEncoderJob::eAacProfileLc:
		{
			return LOW;
		}
	case CEncoderJob::eAacProfileMain:
		{
			return MAIN;
		}
	case CEncoderJob::eAacProfileSsr:
		{
			return SSR;
		}
	case CEncoderJob::eAacProfileLtp:
		{
			return LTP;
		}
	default:
		{
			ASSERT(false);
		}
	}
}

int CEncoderJobProcessingManager::GetMpegVersionConstant(CEncoderJob::EMpegVersion eMpegVersion)
{
	switch (eMpegVersion)
	{
	case CEncoderJob::eMpegVersion2:
		return MPEG2;
	case CEncoderJob::eMpegVersion4:
		return MPEG4;
	default:
		ASSERT(false);
	}
}

bool CEncoderJobProcessingManager::OpenOutputFileArchive(const CString &oFileName, CFile* &poFile, CArchive* &poArchive)
{
	try
	{
		poFile=0;
		poArchive=0;

		// open the file
		poFile=new CFile(oFileName, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite);
		poArchive=new CArchive(poFile, CArchive::store);

		return true;
	}
	catch (...)
	{
		// error opening the file for exclusive writing
		if (poArchive!=0)
		{
			delete poArchive;
		}
		if (poFile!=0)
		{
			delete poFile;
		}
		return false;
	}
}