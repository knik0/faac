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

/* HE-AAC auto-mode thresholds; tuned via ViSQOL on a 49-clip corpus. */
#define HE_MIN_SAMPLE_RATE    32000  /* Fs/2 < 16 kHz below this → core too narrow for SBR */
#define HE_MIN_BITRATE_PER_CH 12000  /* below floor HE wins by an ever-widening margin */
#define HE_MAX_BITRATE_PER_CH 28000  /* above ceiling LC wins: SBR costs up to 1 MOS on transients */
#define HE_VBR_QUANTQUAL_MAX  60     /* quality ≤60 ≈ ≤100 kbps; HE saves bits */

#if (defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64) && !defined(PACKAGE_VERSION)
#include "win32_ver.h"
#endif

/* Rate control tuning constants */
#define RC_DEADBAND_THRESHOLD  0.05f  /* +/- 5% deadband */
#define RC_DAMPING_FACTOR      0.6f   /* Control loop damping */

static char *libfaacName = PACKAGE_VERSION;
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://faac.sourceforge.net/)\n"
  " Copyright (C) 1999,2000,2001  Menno Bakker\n"
  " Copyright (C) 2002,2003,2017  Krzysztof Nikiel\n"
  "This software is based on the ISO MPEG-4 reference source code.\n";

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

int faacEncGetVersion( char **faac_id_string,
			      				char **faac_copyright_string)
{
  if (faac_id_string)
    *faac_id_string = libfaacName;

  if (faac_copyright_string)
    *faac_copyright_string = libCopyright;

  return FAAC_CFG_VERSION;
}


int faacEncGetDecoderSpecificInfo(faacEncHandle hpEncoder,unsigned char** ppBuffer,unsigned long* pSizeOfDecoderSpecificInfo)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    BitStream* pBitStream = NULL;

    if((hEncoder == NULL) || (ppBuffer == NULL) || (pSizeOfDecoderSpecificInfo == NULL)) {
        return -1;
    }

    if(hEncoder->config.mpegVersion == MPEG2){
        return -2; /* not supported */
    }

    if (hEncoder->config.aacObjectType == HE_V1 && hEncoder->sbrContext) {
        return SbrContextGetASC(hEncoder->sbrContext, hEncoder->sampleRateIdx, hEncoder->numChannels, ppBuffer, pSizeOfDecoderSpecificInfo);
    }

    *pSizeOfDecoderSpecificInfo = 2;
    *ppBuffer = (unsigned char *)malloc(2);

    if(*ppBuffer != NULL){
        memset(*ppBuffer,0,*pSizeOfDecoderSpecificInfo);
        pBitStream = OpenBitStream((int)*pSizeOfDecoderSpecificInfo, *ppBuffer);
        if (!pBitStream) {
            free(*ppBuffer);
            *ppBuffer = NULL;
            return -3;
        }
        PutBit(pBitStream, hEncoder->config.aacObjectType, 5);
        PutBit(pBitStream, hEncoder->sampleRateIdx, 4);
        PutBit(pBitStream, hEncoder->numChannels, 4);
        CloseBitStream(pBitStream);

        return 0;
    } else {
        return -3;
    }
}


/* Configuration worker behind faac_encoder_open(): validates the config,
 * resolves AUTO/HE-AAC, and (re)initializes the encoder for it. Returns 1 on
 * success, 0 on failure. */
int faacEncApplyConfig(faacEncStruct* hEncoder,
                       faacEncConfigurationPtr config)
{
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

    /* If this handle was previously resolved to HE-AAC, restore the native Fs so
     * object-type resolution below always starts from a consistent base (needed
     * when a later call toggles between LC and HE-AAC). */
    SbrContextRestoreRate(hEncoder->sbrContext, &hEncoder->sampleRate, &hEncoder->sampleRateIdx, &hEncoder->srInfo);

    switch( hEncoder->config.inputFormat )
    {
        case INPUT_16BIT:
        //case INPUT_24BIT:
        case INPUT_32BIT:
        case INPUT_FLOAT:
            break;

        default:
            return 0;
            break;
    }

    /* Only LC, HE-AAC v1, and AUTO (which resolves to one of them) are
     * supported object types. */
    if (hEncoder->config.aacObjectType != LOW &&
        hEncoder->config.aacObjectType != HE_V1 &&
        hEncoder->config.aacObjectType != AUTO)
        return 0;

    /* Check for correct bitrate */
    if (!hEncoder->sampleRate || !hEncoder->numChannels)
        return 0;
    /* Clamp against the full (pre-downsample) rate: for an already-resolved
     * HE-AAC handle sampleRate is the halved core rate. */
    {
        unsigned long fullRate = SbrContextGetFullRate(hEncoder->sbrContext, hEncoder->sampleRate);
        if (config->bitRate > (MaxBitrate(fullRate) / hEncoder->numChannels))
            config->bitRate = MaxBitrate(fullRate) / hEncoder->numChannels;
    }

    /* Resolve AUTO to LC or HE-AAC. HE-AAC wins for low rates, but only
     * at Fs >= 32 kHz so the Fs/2 core stays >= 16 kHz; below that the
     * narrow-band core + SBR reconstruction collapses. */
    if (hEncoder->config.aacObjectType == AUTO) {
        unsigned long rate_per_ch = config->bitRate;
        int rate_ok;
        if (rate_per_ch > 0) {
            /* Threshold scales with Fs: (3/4)*Fs - 4 kHz gives ~20 kbps at 32 kHz,
             * saturating at HE_MAX_BITRATE_PER_CH for Fs ≥ 44.1 kHz. */
            unsigned int max_he_rate = (unsigned int)(hEncoder->sampleRate * 3 / 4 - 4000);
            if (max_he_rate > HE_MAX_BITRATE_PER_CH) max_he_rate = HE_MAX_BITRATE_PER_CH;
            rate_ok = (rate_per_ch >= HE_MIN_BITRATE_PER_CH && rate_per_ch <= max_he_rate);
        } else {
            rate_ok = (config->quantqual <= HE_VBR_QUANTQUAL_MAX);
        }
        hEncoder->config.aacObjectType = (rate_ok && hEncoder->sampleRate >= HE_MIN_SAMPLE_RATE) ? HE_V1 : LOW;
        config->aacObjectType = hEncoder->config.aacObjectType;
    }

    if (hEncoder->config.aacObjectType == HE_V1 && hEncoder->sampleRate < HE_MIN_SAMPLE_RATE)
        return 0;

    /* HE-AAC: encode the core as AAC-LC; SBR rebuilds the top octave. The core
     * runs dual-rate at Fs/2; the original rate is kept for SBR and the ASC.
     * (Single-rate SBR is not supported: decoders unconditionally reconstruct
     * the SBR band table from 2*core_rate, so a full-Fs core is undecodeable.) */
    if (hEncoder->config.aacObjectType == HE_V1) {
        hEncoder->config.mpegVersion = MPEG4;
        if (!hEncoder->sbrContext)
            hEncoder->sbrContext = SbrContextInit(hEncoder->numChannels);

        if (!hEncoder->sbrContext)
            return 0;

        SbrContextResolveRate(hEncoder->sbrContext, &hEncoder->sampleRate, &hEncoder->sampleRateIdx, &hEncoder->srInfo);
    }

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);

    if (config->bitRate && !config->bandWidth)
    {
        config->bandWidth = CalcBandwidth(config->bitRate, hEncoder->sampleRate);

        if (!config->quantqual)
        {
            config->quantqual = (float)config->bitRate * hEncoder->numChannels / 1280;
            if (config->quantqual > DEFQUAL)
                config->quantqual = (config->quantqual - DEFQUAL) * 3.0f + DEFQUAL;
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

    if (config->quantqual > (unsigned long)maxqual)
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

    if (hEncoder->config.aacObjectType == HE_V1) {
        SBRContext *sCtx = hEncoder->sbrContext;
        SbrContextUpdateConfig(sCtx, hEncoder->numChannels, hEncoder->config.bitRate * hEncoder->numChannels, &hEncoder->fft_tables);
        /* kx * Fs / (2*64): each QMF band is Fs/(2*SBR_QMF_BANDS_64) Hz wide.
         * Matching core bandwidth to the SBR crossover avoids a gap or overlap. */
        hEncoder->config.bandWidth = SbrContextGetXOverBandwidth(sCtx);
    } else {
        if (hEncoder->sbrContext) {
            SbrContextEnd(hEncoder->sbrContext);
            hEncoder->sbrContext = NULL;
        }
    }

    /* Input FIFO: holds one frame plus up to one full incoming chunk of leftover.
     * HE-AAC frames are 2*FRAME_LEN (the dual-rate core runs at Fs/2), LC is
     * FRAME_LEN. Sizing covers the largest frame the resolved object type could
     * need so toggling SBR across SetConfiguration calls never reallocs. */
    {
        unsigned int cap = 2 * faacFrameSamples(hEncoder);
        unsigned int channel;
        for (channel = 0; channel < hEncoder->numChannels; channel++)
            if (!hEncoder->inputFifo[channel])
            {
                hEncoder->inputFifo[channel] =
                    (float *)AllocMemory(cap * sizeof(float));
                if (!hEncoder->inputFifo[channel]) return 0;
            }
        hEncoder->inputFifoCap  = cap;
        hEncoder->inputFifoFill = 0;
    }

    CalcBW(&hEncoder->config.bandWidth,
              hEncoder->sampleRate,
              hEncoder->srInfo,
              &hEncoder->aacquantCfg);

    // reset psymodel
    PsyEnd(hEncoder->psyInfo, hEncoder->numChannels);
    PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
			hEncoder->sampleRate);

	/* load channel_map */
	for( i = 0; i < MAX_CHANNELS; i++ )
		hEncoder->config.channel_map[i] = config->channel_map[i];

    /* OK */
    return 1;
}

faacEncHandle faacEncOpen(unsigned long sampleRate,
                                  unsigned int numChannels,
                                  unsigned long *inputSamples,
                                  unsigned long *maxOutputBytes)
{
    unsigned int channel;
    faacEncStruct* hEncoder;

    if (numChannels < 1 || numChannels > MAX_CHANNELS)
	return NULL;

    *inputSamples = FRAME_LEN*numChannels;
    *maxOutputBytes = ADTS_FRAMESIZE;

    hEncoder = (faacEncStruct*)AllocMemory(sizeof(faacEncStruct));
    if (!hEncoder) return NULL;
    SetMemory(hEncoder, 0, sizeof(faacEncStruct));

    hEncoder->numChannels = numChannels;
    hEncoder->sampleRate = sampleRate;
    hEncoder->sampleRateIdx = GetSRIndex(sampleRate);

    /* Initialize variables to default values */
    hEncoder->frameNum = 0;
    hEncoder->flushFrame = 0;

    /* Default configuration */
    hEncoder->config.mpegVersion = MPEG4;
    hEncoder->config.aacObjectType = LOW;
    hEncoder->config.jointmode = JOINT_MIXED;
    hEncoder->config.pnslevel = 4;
    hEncoder->config.useLfe = 1;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 64000;
    hEncoder->config.bandWidth = CalcBandwidth(hEncoder->config.bitRate, sampleRate);
    hEncoder->config.quantqual = 0;
    hEncoder->config.shortctl = SHORTCTL_NORMAL;

	/* default channel map is straight-through */
	for( channel = 0; channel < MAX_CHANNELS; channel++ )
		hEncoder->config.channel_map[channel] = channel;

    hEncoder->config.outputFormat = ADTS_STREAM;

    /*
        be compatible with software which assumes 24bit in 32bit PCM
    */
    hEncoder->config.inputFormat = INPUT_32BIT;

    /* find correct sampling rate depending parameters */
    hEncoder->srInfo = &srInfo[hEncoder->sampleRateIdx];

    for (channel = 0; channel < numChannels; channel++)
	{
        int buf;
        hEncoder->coderInfo[channel].prev_window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].block_type = ONLY_LONG_WINDOW;
        hEncoder->coderInfo[channel].groups.n = 1;
        hEncoder->coderInfo[channel].groups.len[0] = 1;

        for (buf = 0; buf < 4; buf++) {
            hEncoder->audioFIFO[channel][buf] = (float*)AllocMemory(FRAME_LEN*sizeof(float));
            if (!hEncoder->audioFIFO[channel][buf])
            {
                faacEncClose(hEncoder);
                return NULL;
            }
            memset(hEncoder->audioFIFO[channel][buf], 0, FRAME_LEN*sizeof(float));
        }
    }

    /* Initialize coder functions */
	fft_initialize( &hEncoder->fft_tables );

	PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
        hEncoder->sampleRate);

    FilterBankInit(hEncoder);

    TnsInit(hEncoder);

    QuantizeInit();

    /* Return handle */
    return hEncoder;
}


/* Append the caller's (interleaved) input to the per-channel input FIFO,
 * de-interleaving and converting to float once here so the rest of the
 * encoder is agnostic to the input format. samplesInput may be any count that
 * fits the FIFO; returns -1 on overflow or an invalid format. */
static int appendInputFifo(faacEncStruct *hEncoder, int32_t *inputBuffer,
                           unsigned int samplesInput)
{
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int spch = samplesInput / numChannels;
    unsigned int channel, i;

    if (spch == 0)
        return 0;
    if (hEncoder->inputFifoFill + spch > hEncoder->inputFifoCap)
        return -1;

    for (channel = 0; channel < numChannels; channel++) {
        float *dst = hEncoder->inputFifo[channel] + hEncoder->inputFifoFill;
        switch (hEncoder->config.inputFormat) {
            case INPUT_16BIT: {
                short *src = (short *)inputBuffer + hEncoder->config.channel_map[channel];
                for (i = 0; i < spch; i++) { dst[i] = (float)*src; src += numChannels; }
                break;
            }
            case INPUT_32BIT: {
                int32_t *src = (int32_t *)inputBuffer + hEncoder->config.channel_map[channel];
                for (i = 0; i < spch; i++) { dst[i] = (1.0f/256) * (float)*src; src += numChannels; }
                break;
            }
            case INPUT_FLOAT: {
                float *src = (float *)inputBuffer + hEncoder->config.channel_map[channel];
                for (i = 0; i < spch; i++) { dst[i] = (float)*src; src += numChannels; }
                break;
            }
            default:
                return -1;
        }
    }
    hEncoder->inputFifoFill += spch;
    return 0;
}

/* Drop n samples/channel from the front of the FIFO, shifting the leftover down. */
static void consumeInputFifo(faacEncStruct *hEncoder, unsigned int n)
{
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int channel, rem;

    if (n > hEncoder->inputFifoFill)
        n = hEncoder->inputFifoFill;
    rem = hEncoder->inputFifoFill - n;
    if (rem)
        for (channel = 0; channel < numChannels; channel++)
            memmove(hEncoder->inputFifo[channel],
                    hEncoder->inputFifo[channel] + n,
                    rem * sizeof(float));
    hEncoder->inputFifoFill = rem;
}

int faacEncClose(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel;

    /* Deinitialize coder functions */
    PsyEnd(hEncoder->psyInfo, hEncoder->numChannels);

    FilterBankEnd(hEncoder);

    fft_terminate(&hEncoder->fft_tables);

    /* Free remaining buffer memory */
    for (channel = 0; channel < hEncoder->numChannels; channel++)
	{
        int buf;
        for (buf = 0; buf < 4; buf++) {
            if (hEncoder->audioFIFO[channel][buf])
                FreeMemory(hEncoder->audioFIFO[channel][buf]);
        }
		if (hEncoder->inputFifo[channel])
			FreeMemory (hEncoder->inputFifo[channel]);
    }

    if (hEncoder->ascCache)
        free(hEncoder->ascCache);

    if (hEncoder->sbrContext) {
        SbrContextEnd(hEncoder->sbrContext);
        hEncoder->sbrContext = NULL;
    }

    /* Free handle */
    if (hEncoder)
		FreeMemory(hEncoder);

    return 0;
}

/* HE-AAC per-frame front end: take one assembled full-rate frame from the FIFO
 * front (realPerCh real samples/ch, the rest silence-padded), run SBR analysis
 * on it, then 2:1 downsample to produce the AAC-LC core signal. The FIFO is not
 * consumed here; the caller drops the frame after the core has read heHalfRate.
 * Cold path, kept out of the LC fast path. */
#if defined(__GNUC__)
__attribute__((cold, noinline))
#endif
static void doHEAACFrame(faacEncStruct *hEncoder, unsigned int realPerCh,
                         float *heHalfRate[MAX_CHANNELS])
{
    SbrContextProcessFrame(hEncoder->sbrContext, hEncoder->numChannels, (int)realPerCh, hEncoder->inputFifo, heHalfRate);
}

int faacEncEncode(faacEncHandle hpEncoder,
                          int32_t *inputBuffer,
                          unsigned int samplesInput,
                          unsigned char *outputBuffer,
                          unsigned int bufferSize
                          )
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel;
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
    unsigned int shortctl = hEncoder->config.shortctl;
    int maxqual = hEncoder->config.outputFormat ? MAXQUALADTS : MAXQUAL;

    /* The input FIFO decouples the caller's chunk size from the encoder frame
     * size: append whatever we were handed, then emit at most one frame. A frame
     * is mult*FRAME_LEN samples/channel (mult==2 for HE-AAC, whose dual-rate core
     * runs at Fs/2; 1 for LC). While fewer than a full frame is
     * buffered we just return 0 without touching any per-frame state, so the
     * encoder behaves identically regardless of the caller's chunk size. */
    unsigned int frameSamplesPerCh = faacFrameSamples(hEncoder);
    int flushing = (samplesInput == 0);
    int realPerCh;          /* real (non-padded) input samples/ch in this frame */

    if (samplesInput > 0)
        if (appendInputFifo(hEncoder, inputBuffer, samplesInput) < 0)
            return -1;

    if (hEncoder->inputFifoFill >= frameSamplesPerCh)
        realPerCh = (int)frameSamplesPerCh;           /* full frame ready */
    else if (flushing && hEncoder->inputFifoFill > 0)
        realPerCh = (int)hEncoder->inputFifoFill;     /* final partial frame */
    else if (flushing)
        realPerCh = 0;                                /* drain core lookahead */
    else
        return 0;                                     /* accumulating */

    /* Increase frame number */
    hEncoder->frameNum++;

    /* A pure (FIFO-empty) flush frame pushes silence to drain the core's
     * algorithmic delay; a final partial frame still carries real samples and is
     * counted like a data frame, matching the pre-FIFO behaviour. */
    if (realPerCh == 0)
        hEncoder->flushFrame++;

    /* After LOOKAHEAD_DEPTH + 1 flush frames all samples have been encoded,
       return 0 bytes written */
    if (hEncoder->flushFrame > (LOOKAHEAD_DEPTH + 1))
        return 0;

    /* Determine the channel configuration */
    GetChannelInfo(channelInfo, numChannels, useLfe);

    /* HE-AAC: run SBR + downsample first; the core then encodes heHalfRate. */
    float *heHalfRate[MAX_CHANNELS] = {0};
    if (realPerCh > 0 && hEncoder->config.aacObjectType == HE_V1 && SbrContextIsPresent(hEncoder->sbrContext))
        doHEAACFrame(hEncoder, (unsigned int)realPerCh, heHalfRate);

    /* Update current sample buffers */
    for (channel = 0; channel < numChannels; channel++)
	{
		float *tmp;

		tmp = hEncoder->audioFIFO[channel][FIFO_PAST];
		hEncoder->audioFIFO[channel][FIFO_PAST]  = hEncoder->audioFIFO[channel][FIFO_CURR];
		hEncoder->audioFIFO[channel][FIFO_CURR]  = hEncoder->audioFIFO[channel][FIFO_AHEAD1];
		hEncoder->audioFIFO[channel][FIFO_AHEAD1] = hEncoder->audioFIFO[channel][FIFO_AHEAD2];
		hEncoder->audioFIFO[channel][FIFO_AHEAD2] = tmp;

        if (realPerCh == 0)
        {
            /* start flushing*/
            memset(hEncoder->audioFIFO[channel][FIFO_AHEAD2], 0, FRAME_LEN * sizeof(float));
        }
        else if (hEncoder->config.aacObjectType == HE_V1 && heHalfRate[channel])
        {
            /* core feeds on the SBR-downsampled signal, not the raw input */
            memcpy(hEncoder->audioFIFO[channel][FIFO_AHEAD2], heHalfRate[channel], FRAME_LEN * sizeof(float));
        }
        else
        {
            /* LC: take one frame from the FIFO front (already float),
             * silence-padding a short final frame. */
            unsigned int spc = ((unsigned int)realPerCh < FRAME_LEN) ? (unsigned int)realPerCh : FRAME_LEN;

            memcpy(hEncoder->audioFIFO[channel][FIFO_AHEAD2], hEncoder->inputFifo[channel],
                   spc * sizeof(float));
            if (spc < FRAME_LEN)
                memset(hEncoder->audioFIFO[channel][FIFO_AHEAD2] + spc, 0, (FRAME_LEN - spc) * sizeof(float));
		}

		/* Psychoacoustics */
		/* Update buffers and run FFT on new samples */
		/* LFE psychoacoustic can run without it */
		if (channelInfo[channel].type != ELEMENT_LFE)
		{
            /* Shared detector replacement on HE: skip half-rate PsyBufferUpdate. */
            if (hEncoder->config.aacObjectType != HE_V1 || !SbrContextIsAnalysisValid(hEncoder->sbrContext))
            {
                PsyBufferUpdate(
                        &hEncoder->gpsyInfo,
                        &hEncoder->psyInfo[channel],
                        hEncoder->audioFIFO[channel][FIFO_AHEAD1],
                        hEncoder->audioFIFO[channel][FIFO_AHEAD2]);
            }
		}
    }

    /* Drop the consumed frame from the FIFO front (both the LC copy and the
     * HE doHEAACFrame read the leading frameSamplesPerCh samples). */
    if (realPerCh > 0)
        consumeInputFifo(hEncoder, frameSamplesPerCh);

    if (hEncoder->frameNum <= LOOKAHEAD_DEPTH) /* Still filling up the buffers */
        return 0;

    /* Psychoacoustics */
    /* Shared detector replacement on HE: skip half-rate PsyCalculate. */
    if (hEncoder->config.aacObjectType != HE_V1 || !SbrContextIsAnalysisValid(hEncoder->sbrContext))
        PsyCalculate(channelInfo, hEncoder->psyInfo, numChannels);

    BlockSwitch(hEncoder, coderInfo, hEncoder->psyInfo, numChannels);

    /* force block type */
    if (shortctl == SHORTCTL_NOSHORT)
    {
		for (channel = 0; channel < numChannels; channel++)
		{
			coderInfo[channel].block_type = ONLY_LONG_WINDOW;
		}
    }
    else if ((hEncoder->frameNum <= (LOOKAHEAD_DEPTH + 1)) || (shortctl == SHORTCTL_NOLONG))
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
            hEncoder->audioFIFO[channel][FIFO_PAST],
            hEncoder->audioFIFO[channel][FIFO_CURR],
            hEncoder->freqBuff[channel]);
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

    /* Clear each channel's section state before AACstereo pre-loads intensity
     * bands and BlocQuant resolves the rest. */
    for (channel = 0; channel < numChannels; channel++)
        if (channelInfo[channel].present)
            ResetCoderSections(&coderInfo[channel]);

    AACstereo(coderInfo, channelInfo, hEncoder->freqBuff, numChannels,
              (float)hEncoder->aacquantCfg.quality/DEFQUAL, jointmode, hEncoder->sampleRate);

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
    if (!bitStream)
        return -1;

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
        int sbrBits = 0;
        float fix;

        /* Exclude SBR's fixed overhead from the core budget so the rate
         * controller doesn't starve the core to pay for SBR. */
        sbrBits = SbrContextGetBits(hEncoder->sbrContext, NULL, (int)numChannels, (int)hEncoder->config.aacObjectType, 0);

        if (totalBits > sbrBits)
            fix = (float)(desbits - sbrBits) / (float)(totalBits - sbrBits);
        else
            fix = 1.0f;

        if (fix < (1.0f - RC_DEADBAND_THRESHOLD)) {
            fix += RC_DEADBAND_THRESHOLD;
        } else if (fix > (1.0f + RC_DEADBAND_THRESHOLD)) {
            fix -= RC_DEADBAND_THRESHOLD;
        } else {
            fix = 1.0f;
        }

        /* Apply damping to the quality adjustment */
        fix = (fix - 1.0f) * RC_DAMPING_FACTOR + 1.0f;
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
SR_INFO srInfo[12+1] =
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
{ -1, 0, 0, {0}, {0} }
};
