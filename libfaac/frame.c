/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "frame.h"
#include "coder.h"
#include "channels.h"
#include "bitstream.h"
#include "filtbank.h"
#include "quantize.h"
#include "util.h"
#include "tns.h"
#include "stereo.h"
#include "sbr.h"
#include "resample.h"

#if (defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64) && !defined(PACKAGE_VERSION)
#include "win32_ver.h"
#endif

/* Rate control tuning constants */
#define RC_DEADBAND_THRESHOLD  0.05  /* +/- 5% deadband */
#define RC_DAMPING_FACTOR      0.6   /* Control loop damping */

static char *libfaacName = PACKAGE_VERSION;
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://faac.sourceforge.net/)\n"
  " Copyright (C) 1999,2000,2001  Menno Bakker\n"
  " Copyright (C) 2002,2003,2017  Krzysztof Nikiel\n"
  "This software is based on the ISO MPEG-4 reference source code.\n";

static const psymodellist_t psymodellist[] = {
  {&psymodel2, "knipsycho psychoacoustic"},
  {NULL}
};

static SR_INFO srInfo[12+1];

static unsigned int CalcBandwidth(unsigned long bitRate, unsigned long sampleRate)
{
    const unsigned int nyquist = sampleRate / 2;
    unsigned int bw;

    if (!bitRate) return nyquist;

    if (bitRate <= 16000) {
        /* Segment 1: Telephony (4kHz to 6kHz) */
        bw = 4000 + (bitRate / 8);
    }
    else if (bitRate <= 32000) {
        /* Segment 2: Low-tier (6kHz to 11kHz)
         */
        bw = 6000 + ((bitRate - 16000) * 5 / 16);
    }
    else if (bitRate <= 64000) {
        /* Segment 3: Mid-tier expansion (11kHz to 18.5kHz)
         */
        bw = 11000 + ((bitRate - 32000) * 15 / 64);
    }
    else if (bitRate <= 128000) {
        /* Segment 4: High-fidelity catch-up (18.5kHz to 20kHz) */
        bw = 18500 + ((bitRate - 64000) * 3 / 128);
    }
    else {
        /* Segment 5: Transparency plateau (20kHz+) */
        bw = 20000 + ((bitRate - 128000) / 16);
        if (bw > 20000) bw = 20000;
    }

    /* Safety clamp to Shannon-Nyquist limit */
    return (bw > nyquist) ? nyquist : bw;
}

static void HeAacBuffersFree(faacEncStruct *hEncoder)
{
    int channel;
    for (channel = 0; channel < MAX_CHANNELS; channel++) {
        if (hEncoder->heFullRatePtr[channel]) {
            FreeMemory(hEncoder->heFullRatePtr[channel]);
            hEncoder->heFullRatePtr[channel] = NULL;
        }
        if (hEncoder->heHalfRatePtr[channel]) {
            FreeMemory(hEncoder->heHalfRatePtr[channel]);
            hEncoder->heHalfRatePtr[channel] = NULL;
        }
    }
}

static int HeAacBuffersAlloc(faacEncStruct *hEncoder)
{
    unsigned int i;
    for (i = 0; i < hEncoder->numChannels; i++) {
        if (!hEncoder->heFullRatePtr[i]) {
            hEncoder->heFullRatePtr[i] = (faac_real *)AllocMemory(2 * FRAME_LEN * sizeof(faac_real));
            if (!hEncoder->heFullRatePtr[i]) {
                HeAacBuffersFree(hEncoder);
                return 0;
            }
        }
        if (!hEncoder->heHalfRatePtr[i]) {
            hEncoder->heHalfRatePtr[i] = (faac_real *)AllocMemory(FRAME_LEN * sizeof(faac_real));
            if (!hEncoder->heHalfRatePtr[i]) {
                HeAacBuffersFree(hEncoder);
                return 0;
            }
        }
    }
    return 1;
}

int FAACAPI faacEncGetVersion( char **faac_id_string,
			      				char **faac_copyright_string)
{
  if (faac_id_string)
    *faac_id_string = libfaacName;

  if (faac_copyright_string)
    *faac_copyright_string = libCopyright;

  return FAAC_CFG_VERSION;
}


int FAACAPI faacEncGetDecoderSpecificInfo(faacEncHandle hpEncoder,unsigned char** ppBuffer,unsigned long* pSizeOfDecoderSpecificInfo)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    BitStream* pBitStream = NULL;

    if((hEncoder == NULL) || (ppBuffer == NULL) || (pSizeOfDecoderSpecificInfo == NULL)) {
        return -1;
    }

    if(hEncoder->config.mpegVersion == MPEG2){
        return -2; /* not supported */
    }

    if (hEncoder->config.aacObjectType == HE_AAC) {
        /* HE-AAC explicit backward-compatible AudioSpecificConfig (5 bytes / 37 bits):
         *   5 bits: AOT = 2 (AAC-LC core)
         *   4 bits: samplingFrequencyIndex for Fs/2 (core rate)
         *   4 bits: channelConfiguration
         *   GASpecificConfig: frameLengthFlag(1) + dependsOnCoreCoder(1) + extensionFlag(1)
         *   SBR extension: syncWord(11=0x2b7) + extAOT(5=5) + sbrPresentFlag(1) + extSRIdx(4)
         *  Total: 5+4+4+1+1+1+11+5+1+4 = 37 bits → 5 bytes */
        *pSizeOfDecoderSpecificInfo = 5;
        *ppBuffer = malloc(5);
        if (*ppBuffer == NULL) return -3;
        memset(*ppBuffer, 0, 5);
        pBitStream = OpenBitStream(5, *ppBuffer);
        PutBit(pBitStream, LOW,                       5); /* AOT = AAC-LC (core) */
        PutBit(pBitStream, hEncoder->sampleRateIdx,   4); /* Fs/2 (already set) */
        PutBit(pBitStream, hEncoder->numChannels,     4);
        /* GASpecificConfig */
        PutBit(pBitStream, 0, 1); /* frameLengthFlag */
        PutBit(pBitStream, 0, 1); /* dependsOnCoreCoder */
        PutBit(pBitStream, 0, 1); /* extensionFlag */
        /* SBR backward-compatible extension */
        PutBit(pBitStream, 0x2b7,                    11); /* extensionSyncWord */
        PutBit(pBitStream, 5,                         5); /* extensionAudioObjectType = SBR */
        PutBit(pBitStream, 1,                         1); /* sbrPresentFlag */
        PutBit(pBitStream, hEncoder->fullSampleRateIdx, 4); /* full Fs index */
        CloseBitStream(pBitStream);
        return 0;
    }

    *pSizeOfDecoderSpecificInfo = 2;
    *ppBuffer = malloc(2);

    if(*ppBuffer != NULL){

        memset(*ppBuffer,0,*pSizeOfDecoderSpecificInfo);
        pBitStream = OpenBitStream(*pSizeOfDecoderSpecificInfo, *ppBuffer);
        PutBit(pBitStream, hEncoder->config.aacObjectType, 5);
        PutBit(pBitStream, hEncoder->sampleRateIdx, 4);
        PutBit(pBitStream, hEncoder->numChannels, 4);
        CloseBitStream(pBitStream);

        return 0;
    } else {
        return -3;
    }
}


faacEncConfigurationPtr FAACAPI faacEncGetCurrentConfiguration(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    faacEncConfigurationPtr config = &(hEncoder->config);

    return config;
}

int FAACAPI faacEncSetConfiguration(faacEncHandle hpEncoder,
                                    faacEncConfigurationPtr config)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    int i;
    int maxqual = hEncoder->config.outputFormat ? MAXQUALADTS : MAXQUAL;

    hEncoder->config.jointmode = config->jointmode;
    hEncoder->config.useLfe = config->useLfe;
    hEncoder->config.useTns = config->useTns;
    hEncoder->config.aacObjectType = config->aacObjectType;
    hEncoder->config.mpegVersion = config->mpegVersion;
    hEncoder->config.outputFormat = config->outputFormat;
    hEncoder->config.inputFormat = config->inputFormat;
    hEncoder->config.shortctl = config->shortctl;

    assert((hEncoder->config.outputFormat == 0) || (hEncoder->config.outputFormat == 1));

    switch( hEncoder->config.inputFormat )
    {
        case FAAC_INPUT_16BIT:
        //case FAAC_INPUT_24BIT:
        case FAAC_INPUT_32BIT:
        case FAAC_INPUT_FLOAT:
            break;

        default:
            return 0;
            break;
    }

    if (hEncoder->config.aacObjectType != LOW &&
        hEncoder->config.aacObjectType != HE_AAC &&
        hEncoder->config.aacObjectType != AAC_AUTO)
        return 0;

    if (!hEncoder->sampleRate || !hEncoder->numChannels)
        return 0;

    /* Clamp bitrate against the full (pre-halved) core sample rate so the cap
     * stays correct regardless of HE-AAC's 2:1 downsample. */
    {
        unsigned long fullRate = hEncoder->fullSampleRate ? hEncoder->fullSampleRate
                                                          : hEncoder->sampleRate;
        if (config->bitRate > (MaxBitrate(fullRate) / hEncoder->numChannels))
            config->bitRate = MaxBitrate(fullRate) / hEncoder->numChannels;
    }

    /* Resolve AAC_AUTO before the half-rate switch so the picked type drives
     * everything downstream. Crossover: HE-AAC below 48 kbps/ch, LC at/above. */
    if (hEncoder->config.aacObjectType == AAC_AUTO) {
        unsigned long rate_per_ch = config->bitRate;
        int pick_heaac = (rate_per_ch > 0 && rate_per_ch < 48000);
        hEncoder->config.aacObjectType = pick_heaac ? HE_AAC : LOW;
        config->aacObjectType = hEncoder->config.aacObjectType;
    }

    /* HE-AAC: switch core codec to half sample rate (first time only) */
    if (hEncoder->config.aacObjectType == HE_AAC &&
        hEncoder->fullSampleRate == 0) {
        hEncoder->fullSampleRate    = hEncoder->sampleRate;
        hEncoder->fullSampleRateIdx = hEncoder->sampleRateIdx;
        hEncoder->sampleRate        = hEncoder->sampleRate / 2;
        hEncoder->sampleRateIdx     = GetSRIndex(hEncoder->sampleRate);
        hEncoder->srInfo            = &srInfo[hEncoder->sampleRateIdx];
        /* Force MPEG4 (SBR requires it) and disable PNS (not valid with SBR) */
        hEncoder->config.mpegVersion = MPEG4;
        hEncoder->config.pnslevel    = 0;
    }

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);
#if 0
    if (config->bitRate < MinBitrate())
        return 0;
#endif

    if (config->bitRate && !config->bandWidth)
    {
        config->bandWidth = CalcBandwidth(config->bitRate, hEncoder->sampleRate);

        if (!config->quantqual)
        {
            config->quantqual = (faac_real)config->bitRate * hEncoder->numChannels / 1280;
            if (config->quantqual > DEFQUAL)
                config->quantqual = (config->quantqual - DEFQUAL) * 3.0 + DEFQUAL;
        }
    }

    if (!config->quantqual)
        config->quantqual = DEFQUAL;

    hEncoder->config.bitRate = config->bitRate;

    if (!config->bandWidth)
    {
        config->bandWidth = CalcBandwidth(config->bitRate, hEncoder->sampleRate);
    }

    hEncoder->config.bandWidth = config->bandWidth;

    // check bandwidth
    if (hEncoder->config.bandWidth < 100)
		hEncoder->config.bandWidth = 100;
    if (hEncoder->config.bandWidth > (hEncoder->sampleRate / 2))
		hEncoder->config.bandWidth = hEncoder->sampleRate / 2;

    if (config->quantqual > maxqual)
        config->quantqual = maxqual;
    if (config->quantqual < MINQUAL)
        config->quantqual = MINQUAL;

    hEncoder->config.quantqual = config->quantqual;

    if (config->mpegVersion == MPEG2)
        config->pnslevel = 0;
    if (config->pnslevel < 0)
        config->pnslevel = 0;
    if (config->pnslevel > 10)
        config->pnslevel = 10;
    hEncoder->aacquantCfg.pnslevel = config->pnslevel;
    /* set quantization quality */
    hEncoder->aacquantCfg.quality = config->quantqual;
    CalcBW(&hEncoder->config.bandWidth,
              hEncoder->sampleRate,
              hEncoder->srInfo,
              &hEncoder->aacquantCfg);

    // reset psymodel
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);
    if (config->psymodelidx >= (sizeof(psymodellist) / sizeof(psymodellist[0]) - 1))
		config->psymodelidx = (sizeof(psymodellist) / sizeof(psymodellist[0])) - 2;

    hEncoder->config.psymodelidx = config->psymodelidx;
    hEncoder->psymodel = (psymodel_t *)psymodellist[hEncoder->config.psymodelidx].ptr;
    hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
			hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
			hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
			hEncoder->srInfo->num_cb_short);

	/* load channel_map */
	for( i = 0; i < MAX_CHANNELS; i++ )
		hEncoder->config.channel_map[i] = config->channel_map[i];

    /* HE-AAC: initialise resampler, SBR state and buffers (after sample-rate switch) */
    if (hEncoder->config.aacObjectType == HE_AAC) {
        if (!hEncoder->resampler)
            hEncoder->resampler = ResampleOpen(hEncoder->numChannels);
        if (!hEncoder->sbrInfo)
            hEncoder->sbrInfo = SBRInit(hEncoder->numChannels,
                                        hEncoder->fullSampleRate,
                                        hEncoder->sampleRate);

        if (!HeAacBuffersAlloc(hEncoder))
            return 0;
    } else {
        /* If switched away from HE-AAC, free these buffers */
        HeAacBuffersFree(hEncoder);
    }

    /* OK */
    return 1;
}

faacEncHandle FAACAPI faacEncOpen(unsigned long sampleRate,
                                  unsigned int numChannels,
                                  unsigned long *inputSamples,
                                  unsigned long *maxOutputBytes)
{
    unsigned int channel;
    faacEncStruct* hEncoder;

    if (numChannels > MAX_CHANNELS)
	return NULL;

    *inputSamples = FRAME_LEN*numChannels;
    *maxOutputBytes = ADTS_FRAMESIZE;

    hEncoder = (faacEncStruct*)AllocMemory(sizeof(faacEncStruct));
    SetMemory(hEncoder, 0, sizeof(faacEncStruct));

    hEncoder->numChannels = numChannels;
    hEncoder->sampleRate = sampleRate;
    hEncoder->sampleRateIdx = GetSRIndex(sampleRate);

    /* Initialize variables to default values */
    hEncoder->frameNum = 0;
    hEncoder->flushFrame = 0;

    /* Default configuration */
    hEncoder->config.version = FAAC_CFG_VERSION;
    hEncoder->config.name = libfaacName;
    hEncoder->config.copyright = libCopyright;
    hEncoder->config.mpegVersion = MPEG4;
    hEncoder->config.aacObjectType = AAC_AUTO;
    hEncoder->config.jointmode = JOINT_IS;
    hEncoder->config.pnslevel = 4;
    hEncoder->config.useLfe = 1;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 64000;
    hEncoder->config.bandWidth = CalcBandwidth(hEncoder->config.bitRate, sampleRate);
    hEncoder->config.quantqual = 0;
    hEncoder->config.psymodellist = (psymodellist_t *)psymodellist;
    hEncoder->config.psymodelidx = 0;
    hEncoder->psymodel =
      (psymodel_t *)hEncoder->config.psymodellist[hEncoder->config.psymodelidx].ptr;
    hEncoder->config.shortctl = SHORTCTL_NORMAL;

    HeAacBuffersFree(hEncoder);

	/* default channel map is straight-through */
	for( channel = 0; channel < MAX_CHANNELS; channel++ )
		hEncoder->config.channel_map[channel] = channel;

    hEncoder->config.outputFormat = ADTS_STREAM;

    /*
        be compatible with software which assumes 24bit in 32bit PCM
    */
    hEncoder->config.inputFormat = FAAC_INPUT_32BIT;

    /* find correct sampling rate depending parameters */
    hEncoder->srInfo = &srInfo[hEncoder->sampleRateIdx];

    for (channel = 0; channel < numChannels; channel++)
	{
        hEncoder->coderInfo[channel].prev_window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].block_type = ONLY_LONG_WINDOW;
        hEncoder->coderInfo[channel].groups.n = 1;
        hEncoder->coderInfo[channel].groups.len[0] = 1;

        hEncoder->sampleBuff[channel] = NULL;
    }

    /* Initialize coder functions */
	fft_initialize( &hEncoder->fft_tables );

	hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
        hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
        hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short);

    FilterBankInit(hEncoder);

    TnsInit(hEncoder);

    QuantizeInit();

    /* Return handle */
    return hEncoder;
}

int FAACAPI faacEncClose(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel;

    /* Deinitialize coder functions */
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);

    FilterBankEnd(hEncoder);

    fft_terminate(&hEncoder->fft_tables);

    /* Free remaining buffer memory */
    for (channel = 0; channel < hEncoder->numChannels; channel++)
	{
		if (hEncoder->sampleBuff[channel])
			FreeMemory(hEncoder->sampleBuff[channel]);
		if (hEncoder->next3SampleBuff[channel])
			FreeMemory (hEncoder->next3SampleBuff[channel]);
    }

    /* Free HE-AAC state */
    if (hEncoder->resampler) {
        ResampleClose(hEncoder->resampler);
        hEncoder->resampler = NULL;
    }
    if (hEncoder->sbrInfo) {
        SBREnd(hEncoder->sbrInfo);
        hEncoder->sbrInfo = NULL;
    }
    HeAacBuffersFree(hEncoder);

    /* Free handle */
    if (hEncoder)
		FreeMemory(hEncoder);

    BlocStat();

    return 0;
}

/* HE-AAC pre-processing: deinterleave full-rate input, run SBR analysis,
 * then 2:1 downsample → FRAME_LEN per channel.  Marked cold+noinline so the
 * compiler places this in .text.unlikely, keeping it out of the L1 icache
 * on the AAC-LC path. */
__attribute__((cold, noinline))
static int doHEAACPreprocess(faacEncStruct *hEncoder,
                              int32_t *inputBuffer,
                              unsigned int samplesInput,
                              faac_real *heHalfRate[MAX_CHANNELS])
{
    unsigned int channel, i;
    unsigned int numChannels = hEncoder->numChannels;
    int full_spch = (int)(samplesInput / numChannels);

    /* HE-AAC buffer is sized for 2*FRAME_LEN full-rate samples per channel.
     * Partial frames at end-of-file (full_spch < 2*FRAME_LEN) are acceptable;
     * overflow into out-of-bounds memory is not. */
    if (full_spch <= 0 || full_spch > 2 * FRAME_LEN)
        return -1;

    for (channel = 0; channel < numChannels; channel++) {
        faac_real *fullRate = hEncoder->heFullRatePtr[channel];

        switch (hEncoder->config.inputFormat) {
            case FAAC_INPUT_16BIT: {
                short *src = (short *)inputBuffer
                             + hEncoder->config.channel_map[channel];
                for (i = 0; i < (unsigned)full_spch; i++) {
                    fullRate[i] = (faac_real)*src;
                    src += numChannels;
                }
                break;
            }
            case FAAC_INPUT_32BIT: {
                int32_t *src = (int32_t *)inputBuffer
                               + hEncoder->config.channel_map[channel];
                for (i = 0; i < (unsigned)full_spch; i++) {
                    fullRate[i] = (1.0f/256) * (faac_real)*src;
                    src += numChannels;
                }
                break;
            }
            case FAAC_INPUT_FLOAT: {
                float *src = (float *)inputBuffer
                             + hEncoder->config.channel_map[channel];
                for (i = 0; i < (unsigned)full_spch; i++) {
                    fullRate[i] = (faac_real)*src;
                    src += numChannels;
                }
                break;
            }
            default:
                break;
        }
        heHalfRate[channel] = hEncoder->heHalfRatePtr[channel];
    }

    SBRAnalysis(hEncoder->sbrInfo, hEncoder->heFullRatePtr, numChannels, full_spch);
    Resample2to1(hEncoder->resampler, hEncoder->heFullRatePtr, full_spch, hEncoder->heHalfRatePtr);
    return 0;
}

int FAACAPI faacEncEncode(faacEncHandle hpEncoder,
                          int32_t *inputBuffer,
                          unsigned int samplesInput,
                          unsigned char *outputBuffer,
                          unsigned int bufferSize
                          )
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel, i;
    int sb, frameBytes;
    unsigned int offset;
    BitStream *bitStream; /* bitstream used for writing the frame to */

    /* local copy's of parameters */
    ChannelInfo *channelInfo = hEncoder->channelInfo;
    CoderInfo *coderInfo = hEncoder->coderInfo;
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int useLfe = hEncoder->config.useLfe;
    unsigned int useTns = hEncoder->config.useTns;
    unsigned int jointmode = hEncoder->config.jointmode;
    unsigned int bandWidth = hEncoder->config.bandWidth;
    unsigned int shortctl = hEncoder->config.shortctl;
    int maxqual = hEncoder->config.outputFormat ? MAXQUALADTS : MAXQUAL;

    /* Increase frame number */
    hEncoder->frameNum++;

    if (samplesInput == 0)
        hEncoder->flushFrame++;

    /* After 4 flush frames all samples have been encoded,
       return 0 bytes written */
    if (hEncoder->flushFrame > 4)
        return 0;

    /* Determine the channel configuration */
    GetChannelInfo(channelInfo, numChannels, useLfe);

    /* HE-AAC pre-processing (cold path): deinterleave, SBR analysis, 2:1 downsample.
     * heHalfRate[ch] is non-NULL only when HE-AAC preprocessing ran successfully. */
    faac_real *heHalfRate[MAX_CHANNELS] = {0};

    if (hEncoder->config.aacObjectType == HE_AAC &&
        hEncoder->sbrInfo && hEncoder->resampler && samplesInput > 0)
        if (doHEAACPreprocess(hEncoder, inputBuffer, samplesInput, heHalfRate) < 0)
            return -1;

    /* Update current sample buffers */
    for (channel = 0; channel < numChannels; channel++)
	{
		faac_real *tmp;


		if (!hEncoder->sampleBuff[channel])
			hEncoder->sampleBuff[channel] = (faac_real*)AllocMemory(FRAME_LEN*sizeof(faac_real));

		tmp = hEncoder->sampleBuff[channel];

		hEncoder->sampleBuff[channel]	= hEncoder->next3SampleBuff[channel];
		hEncoder->next3SampleBuff[channel]	= tmp;

        if (samplesInput == 0)
        {
            /* start flushing*/
            for (i = 0; i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        }
        else if (hEncoder->config.aacObjectType == HE_AAC && heHalfRate[channel])
        {
            /* HE-AAC: load pre-computed half-rate samples */
            memcpy(hEncoder->next3SampleBuff[channel], heHalfRate[channel],
                   FRAME_LEN * sizeof(faac_real));
        }
        else
        {
			int samples_per_channel = samplesInput/numChannels;

            /* handle the various input formats and channel remapping */
            switch( hEncoder->config.inputFormat )
			{
                case FAAC_INPUT_16BIT:
					{
						short *input_channel = (short*)inputBuffer + hEncoder->config.channel_map[channel];

						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (faac_real)*input_channel;
							input_channel += numChannels;
						}
					}
                    break;

                case FAAC_INPUT_32BIT:
					{
						int32_t *input_channel = (int32_t*)inputBuffer + hEncoder->config.channel_map[channel];

						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (1.0/256) * (faac_real)*input_channel;
							input_channel += numChannels;
						}
					}
                    break;

                case FAAC_INPUT_FLOAT:
					{
						float *input_channel = (float*)inputBuffer + hEncoder->config.channel_map[channel];

						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (faac_real)*input_channel;
							input_channel += numChannels;
						}
					}
                    break;

                default:
                    return -1; /* invalid input format */
                    break;
            }

            for (i = (int)(samplesInput/numChannels); i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
		}

		/* Psychoacoustics */
		/* Update buffers and run FFT on new samples */
		/* LFE psychoacoustic can run without it */
		if (channelInfo[channel].type != ELEMENT_LFE)
		{
			hEncoder->psymodel->PsyBufferUpdate(
					&hEncoder->fft_tables,
					&hEncoder->gpsyInfo,
					&hEncoder->psyInfo[channel],
					hEncoder->next3SampleBuff[channel],
					bandWidth,
					hEncoder->srInfo->cb_width_short,
					hEncoder->srInfo->num_cb_short);
		}
    }

    if (hEncoder->frameNum <= 3) /* Still filling up the buffers */
        return 0;

    /* Psychoacoustics */
    hEncoder->psymodel->PsyCalculate(channelInfo, &hEncoder->gpsyInfo, hEncoder->psyInfo,
        hEncoder->srInfo->cb_width_long, hEncoder->srInfo->num_cb_long,
        hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short, numChannels, (faac_real)hEncoder->aacquantCfg.quality / DEFQUAL);

    hEncoder->psymodel->BlockSwitch(coderInfo, hEncoder->psyInfo, numChannels);

    /* force block type */
    if (shortctl == SHORTCTL_NOSHORT)
    {
		for (channel = 0; channel < numChannels; channel++)
		{
			coderInfo[channel].block_type = ONLY_LONG_WINDOW;
		}
    }
    else if ((hEncoder->frameNum <= 4) || (shortctl == SHORTCTL_NOLONG))
    {
		for (channel = 0; channel < numChannels; channel++)
		{
			coderInfo[channel].block_type = ONLY_SHORT_WINDOW;
		}
    }

    /* AAC Filterbank, MDCT with overlap and add */
    for (channel = 0; channel < numChannels; channel++) {
        FilterBank(hEncoder,
            &coderInfo[channel],
            hEncoder->sampleBuff[channel],
            hEncoder->freqBuff[channel],
            hEncoder->overlapBuff[channel],
            MOVERLAPPED);
    }

    for (channel = 0; channel < numChannels; channel++) {
        channelInfo[channel].msInfo.is_present = 0;

        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbs;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].sfbn; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_short[sb];
            }
            coderInfo[channel].sfb_offset[sb] = offset;
            BlocGroup(hEncoder->freqBuff[channel], coderInfo + channel, &hEncoder->aacquantCfg);
        } else {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbl;

            coderInfo[channel].groups.n = 1;
            coderInfo[channel].groups.len[0] = 1;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].sfbn; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_long[sb];
            }
            coderInfo[channel].sfb_offset[sb] = offset;
        }
    }

    /* Perform TNS analysis and filtering */
    for (channel = 0; channel < numChannels; channel++) {
        if ((channelInfo[channel].type != ELEMENT_LFE) && (useTns)) {
            TnsEncode(&(coderInfo[channel].tnsInfo),
                      coderInfo[channel].sfbn,
                      coderInfo[channel].sfbn,
                      coderInfo[channel].block_type,
                      coderInfo[channel].sfb_offset,
                      hEncoder->freqBuff[channel], hEncoder->gpsyInfo.sharedWorkBuffLong);
        } else {
            coderInfo[channel].tnsInfo.tnsDataPresent = 0;      /* TNS not used for LFE */
        }
    }

    for (channel = 0; channel < numChannels; channel++) {
      // reduce LFE bandwidth
		if (channelInfo[channel].type == ELEMENT_LFE)
		{
                    coderInfo[channel].sfbn = 3;
		}
	}

    AACstereo(coderInfo, channelInfo, hEncoder->freqBuff, numChannels,
              (faac_real)hEncoder->aacquantCfg.quality/DEFQUAL, jointmode);

    for (channel = 0; channel < numChannels; channel++) {
        BlocQuant(&coderInfo[channel], hEncoder->freqBuff[channel],
                  &(hEncoder->aacquantCfg));
    }

    // fix max_sfb in CPE mode
    for (channel = 0; channel < numChannels; channel++)
    {
		if (channelInfo[channel].present
				&& (channelInfo[channel].type == ELEMENT_CPE)
				&& (channelInfo[channel].ch_is_left))
		{
			CoderInfo *cil, *cir;

			cil = &coderInfo[channel];
			cir = &coderInfo[channelInfo[channel].paired_ch];

                        cil->sfbn = cir->sfbn = max(cil->sfbn, cir->sfbn);
		}
    }
    /* Write the AAC bitstream */
    bitStream = OpenBitStream(bufferSize, outputBuffer);

    if (WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels) < 0)
        return -1;

    /* Close the bitstream and return the number of bytes written */
    frameBytes = CloseBitStream(bitStream);

    /* Adjust quality to get correct average bitrate */
    if (hEncoder->config.bitRate)
    {
        int desbits = numChannels * (hEncoder->config.bitRate * FRAME_LEN)
            / hEncoder->sampleRate;
        int totalBits = frameBytes * 8;
        /* When HE-AAC is active, frameBytes also contains the SBR fill element.
         * The quantiser quality only controls AAC-LC core bits, so compare the
         * target against core bits only — otherwise SBR overhead starves the
         * core at exactly the bitrates where SBR should leave it headroom. */
        if (hEncoder->config.aacObjectType == HE_AAC && hEncoder->sbrInfo) {
            int id_aac = (numChannels > 1) ? ID_CPE : ID_SCE;
            int sbr_bits = SBRWriteBitstream(hEncoder->sbrInfo, NULL, id_aac, 0);
            if (sbr_bits > 0 && sbr_bits < totalBits)
                totalBits -= sbr_bits;
        }
        faac_real fix = (faac_real)desbits / (faac_real)totalBits;

        if (fix < (1.0 - RC_DEADBAND_THRESHOLD)) {
            fix += RC_DEADBAND_THRESHOLD;
        } else if (fix > (1.0 + RC_DEADBAND_THRESHOLD)) {
            fix -= RC_DEADBAND_THRESHOLD;
        } else {
            fix = 1.0;
        }

        /* Apply damping to the quality adjustment */
        fix = (fix - 1.0) * RC_DAMPING_FACTOR + 1.0;
        // printf("q: %.1f(f:%.4f)\n", hEncoder->aacquantCfg.quality, fix);

        hEncoder->aacquantCfg.quality *= fix;

        if (hEncoder->aacquantCfg.quality > maxqual)
            hEncoder->aacquantCfg.quality = maxqual;
        if (hEncoder->aacquantCfg.quality < MINQUAL)
            hEncoder->aacquantCfg.quality = MINQUAL;
    }

    return frameBytes;
}


/* Scalefactorband data table for 1024 transform length */
static SR_INFO srInfo[12+1] =
{
    { 96000, 41, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
        }
    }, { 88200, 41, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
        }
    }, { 64000, 47, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 12, 12, 12, 16, 16, 16, 20, 24, 24, 28,
            36, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
            40, 40, 40, 40, 40
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 32
        }
    }, { 48000, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16
        }
    }, { 44100, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16
        }
    }, { 32000, 51, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28,
            28, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32
        },{
            4,  4,  4,  4,  4,  8,  8,  8,  12, 12, 12, 16, 16, 16
        }
    }, { 24000, 47, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 20
        }
    }, { 22050, 47, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 20
        }
    }, { 16000, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 12000, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 11025, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 8000, 40, 15,
        {
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16,
            16, 16, 16, 16, 16, 16, 20, 20, 20, 20, 24, 24, 24, 28,
            28, 32, 36, 36, 40, 44, 48, 52, 56, 60, 64, 80
        }, {
            4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 12, 16, 20, 20
        }
    },
    { -1 }
};
