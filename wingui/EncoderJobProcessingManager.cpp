// EncoderJobProcessingManager.cpp: implementation of the CEncoderJobProcessingManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "faac_wingui.h"
#include "EncoderJobProcessingManager.h"
#include "WindowUtil.h"

#include <sndfile.h>
#include "faac.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//#define DUMMY_ENCODERJOB_PROCESSING

// constants copied from reference implementation in VC project faacgui
#define PCMBUFSIZE 1024
#define BITBUFSIZE 8192

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEncoderJobProcessingManager::CEncoderJobProcessingManager(const CEncoderJob *poJobToProcess):
	m_poJobToProcess(poJobToProcess),
	m_poInfoTarget(0),
	m_eCurrentWorkingState(eInitial)
{
}

CEncoderJobProcessingManager::~CEncoderJobProcessingManager()
{
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
			m_poInfoTarget->SetAvailableActions(true, false);
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

	long lMaxCount=250*64000/(5+m_poJobToProcess->GetBitRate());
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
				return false;
			}
		}
	}

	m_eCurrentWorkingState=eCleanup;

	return true;
}

#else
bool CEncoderJobProcessingManager::DoProcessing()
{
	long lStartTimeMillis=::GetTickCount();
	const CEncoderJob *poJob=m_poJobToProcess;
	bool bInterrupted=false;

	// TEMP: added to make it work, these values need to be used
	unsigned long inputSamples, maxOutputSize;
	//

	SNDFILE *infile;
	SF_INFO sfinfo;

	// open the input file
	if ((infile = sf_open_read(poJob->GetFiles().GetCompleteSourceFilePath(), &sfinfo)) != NULL)
	{
		// determine input file parameters
		unsigned int sampleRate = sfinfo.samplerate;
		unsigned int numChannels = sfinfo.channels;

		// open and setup the encoder
		faacEncHandle hEncoder = faacEncOpen(sampleRate, numChannels, &inputSamples,
			&maxOutputSize);
		if (hEncoder)
		{
			HANDLE hOutfile;

			// set encoder configuration
			faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(hEncoder);

			config->allowMidside = poJob->GetAllowMidside() ? 1 : 0;
			config->useTns = poJob->GetUseTns() ? 1 : 0;
			config->useLtp = poJob->GetUseLtp() ? 1 : 0;
			config->useLfe = poJob->GetUseLfe() ? 1 : 0;
			config->bitRate = poJob->GetBitRate();
			config->bandWidth = poJob->GetBandwidth();
			config->aacProfile = GetAacProfileConstant(poJob->GetAacProfile());

			if (!faacEncSetConfiguration(hEncoder, config))
			{
				faacEncClose(hEncoder);
				sf_close(infile);

				AfxMessageBox("faacEncSetConfiguration failed!", MB_OK | MB_ICONSTOP);

				return false;
			}

			// open the output file
			hOutfile = CreateFile(poJob->GetFiles().GetCompleteTargetFilePath(), GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hOutfile != INVALID_HANDLE_VALUE)
			{
				UINT startTime = GetTickCount(), lastUpdated = 0;
				DWORD totalBytesRead = 0;

				unsigned int bytesInput = 0, bytesConsumed = 0;
				DWORD numberOfBytesWritten = 0;
				short *pcmbuf;
				unsigned char *bitbuf;

				pcmbuf = (short*)malloc(PCMBUFSIZE*numChannels*sizeof(short));
				bitbuf = (unsigned char*)malloc(BITBUFSIZE*sizeof(unsigned char));

				while (true)
				{
					int bytesWritten;
					int samplesToRead = PCMBUFSIZE;

					bytesInput = sf_read_short(infile, pcmbuf, numChannels*PCMBUFSIZE) * sizeof(short);
					
					//SendDlgItemMessage (hWnd, IDC_PROGRESS, PBM_SETPOS, (unsigned long)((float)totalBytesRead * 1024.0f / (sfinfo.samples*2*numChannels)), 0);
					
					totalBytesRead += bytesInput;

					// call the actual encoding routine
					bytesWritten = faacEncEncode(hEncoder,
						pcmbuf,
						bytesInput/2,
						bitbuf,
						BITBUFSIZE);

					switch (m_eCurrentWorkingState)
					{
					case eRunning:
						{
							// just report our current state and process waiting window messages
							WriteProgress(lStartTimeMillis, (sfinfo.samples*2*numChannels), totalBytesRead);
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
						break;
					}

					if (!bytesInput && !bytesWritten)
					{
						// all done, bail out
						break;
					}

					if (bytesWritten < 0)
					{
						AfxMessageBox("faacEncEncodeFrame failed!", MB_OK | MB_ICONSTOP);
						bInterrupted=true;
						break;
					}

					WriteFile(hOutfile, bitbuf, bytesWritten, &numberOfBytesWritten, NULL);
				}

				CloseHandle(hOutfile);
				if (pcmbuf) free(pcmbuf);
				if (bitbuf) free(bitbuf);
			}

			faacEncClose(hEncoder);
		}

		sf_close(infile);
		MessageBeep(1);
	}
	else
	{
		AfxMessageBox("Couldn't open input file!", MB_OK | MB_ICONSTOP);
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
	long lEstimateEntireTime=(long)((double)(lElapsedTime)/(dProgress/100));
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
	default:
		{
			ASSERT(false);
		}
	case CEncoderJob::eAacProfileMain:
		{
			return MAIN;
		}
	case CEncoderJob::eAacProfileSsr:
		{
			return SSR;
		}
	}
}