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
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

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
#define HE_MAX_BITRATE_PER_CH 48000  /* above ceiling LC wins: SBR costs up to 1 MOS on transients */
/* quantqual == totalBitrate/1280 (see faacEncApplyConfig); derived to stay in sync with HE_MAX_BITRATE_PER_CH. */
#define HE_VBR_QUANTQUAL_MAX  (2 * HE_MAX_BITRATE_PER_CH / 1280)

#if (defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64) && !defined(PACKAGE_VERSION)
#include "win32_ver.h"
#endif

/* Rate control tuning constants */
#define RC_DAMPING_FACTOR      0.6f   /* Control loop damping */

/* Bounds on the peak limiter's quality scale factor: the ceiling guarantees
 * each retry makes progress, the floor keeps one outsized frame from
 * collapsing quality to MINQUAL in a single step. */
#define PEAK_BACKOFF_CEILING   0.85f
#define PEAK_BACKOFF_FLOOR     0.10f
#define PEAK_MAX_RETRIES       12

static char *libfaacName = PACKAGE_VERSION;
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://faac.sourceforge.net/)\n"
  " Copyright (C) 1999-2001, Menno Bakker\n"
  " Copyright (C) 2002-2017, Krzysztof Nikiel\n"
  " Copyright (C) 2004, Dan Villiom P. Christiansen\n"
  " Copyright (C) 2005-2026, Fabian Greffrath\n"
  " Copyright (C) 2026, Nils Schimmelmann\n";

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

/* Element-to-channel mapping is fixed for the session once InitElements has
 * run, so cache which channels are LFE here instead of rescanning
 * hEncoder->elements[] for every channel on every frame. */
static void RefreshLfeMap(faacEncStruct *hEncoder)
{
    memset(hEncoder->isLfeChannel, 0, sizeof(hEncoder->isLfeChannel));
    for (int e = 0; e < hEncoder->numElements; e++) {
        if (hEncoder->elements[e].type == ID_LFE)
            hEncoder->isLfeChannel[hEncoder->elements[e].channels[0]] = true;
    }
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
        pBitStream = OpenBitStream((uint32_t)*pSizeOfDecoderSpecificInfo, *ppBuffer);
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
        case INPUT_24BIT:
        case INPUT_32BIT:
        case INPUT_FLOAT:
            break;
        default:
            return 0;
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
            /* Below 44.1 kHz, SBR has less core bandwidth to extend from, so the
             * ceiling ramps down toward 20000 bps/ch at the HE_MIN_SAMPLE_RATE floor. */
            unsigned int max_he_rate = 0;
            if (hEncoder->sampleRate >= 44100) {
                max_he_rate = HE_MAX_BITRATE_PER_CH;
            } else if (hEncoder->sampleRate >= HE_MIN_SAMPLE_RATE) {
                max_he_rate = 20000 + (unsigned int)((hEncoder->sampleRate - 32000) *
                              (HE_MAX_BITRATE_PER_CH - 20000) / (44100 - 32000));
            }
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
            /* Scale initial quality seed by sample-rate frame duration factor (44100 / sampleRate)
             * so low sampling rates (e.g. 16 kHz) start at appropriate quality scale factors for
             * fast rate-control convergence on short audio clips. */
            float rateFactor = 44100.0f / (float)hEncoder->sampleRate;
            /* Precise target-bitrate quality seeding curve: maps bitRate to optimal initial quantqual
             * for rapid rate-control convergence without early overshoot or undershoot. */
            float bps = (float)config->bitRate;
            float q_seed;
            if (bps <= 16000.0f) {
                q_seed = 10.0f + 22.0f * (bps / 16000.0f);
            } else if (bps <= 64000.0f) {
                q_seed = 32.0f + 68.0f * ((bps - 16000.0f) / 48000.0f);
            } else {
                q_seed = bps / 640.0f;
            }
            /* Boost initial seed for mono speech streams */
            if (hEncoder->numChannels == 1 && bps >= 32000.0f) q_seed *= 2.5f;
            config->quantqual = q_seed * (float)hEncoder->numChannels * rateFactor;
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
        unsigned long sbr_bitrate = hEncoder->config.bitRate ? (hEncoder->config.bitRate * hEncoder->numChannels) : ((unsigned long)hEncoder->config.quantqual * 1280);
        SbrContextUpdateConfig(sCtx, hEncoder->numChannels, sbr_bitrate, &hEncoder->fft_tables);
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

    hEncoder->config.maxBitRate = config->maxBitRate;

    /* Peak-limiter retry scratch: allocated for all encoders to enforce ISO 6144 bits/ch frame ceiling. */
    {
        unsigned int ch;
        for (ch = 0; ch < hEncoder->numChannels; ch++) {
            if (!hEncoder->peakSnap[ch])
                hEncoder->peakSnap[ch] = (int *)AllocMemory(2 * MAX_SCFAC_BANDS * sizeof(int));
            if (!hEncoder->peakSnap[ch])
                return 0;
        }
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

    InitElements(hEncoder->elements, &hEncoder->numElements, (int)hEncoder->numChannels, hEncoder->config.useLfe);
    RefreshLfeMap(hEncoder);

    /* Initialize adaptive bit reservoir for ABR mode */
    if (hEncoder->config.bitRate > 0) {
        int desbits = (int)((unsigned long long)hEncoder->numChannels * hEncoder->config.bitRate * FRAME_LEN / hEncoder->sampleRate);
        int maxReservoirBits = (int)max(0, (int)(AAC_MAX_BITS_PER_CH * hEncoder->numChannels) - desbits);
        hEncoder->bitReservoirCap = min(maxReservoirBits, 2 * desbits);
        hEncoder->bitReservoir = hEncoder->bitReservoirCap / 2;
    } else {
        hEncoder->bitReservoirCap = 0;
        hEncoder->bitReservoir = 0;
    }

    return 1;
}

#ifdef FAAC_STATS
faacEncStats g_faacStats;
#endif

faacEncHandle faacEncOpen(unsigned long sampleRate,
                                  unsigned int numChannels,
                                  unsigned long *inputSamples,
                                  unsigned long *maxOutputBytes)
{
#ifdef FAAC_STATS
    memset(&g_faacStats, 0, sizeof(faacEncStats));
    g_faacStats.minReservoirRatio = 100.0f;
#endif
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
    InitElements(hEncoder->elements, &hEncoder->numElements, (int)hEncoder->numChannels, (bool)hEncoder->config.useLfe);
    RefreshLfeMap(hEncoder);

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

    if (spch == 0) return 0;
    if (hEncoder->inputFifoFill + spch > hEncoder->inputFifoCap) return -1;

    for (channel = 0; channel < numChannels; channel++) {
        float *dst = hEncoder->inputFifo[channel] + hEncoder->inputFifoFill;
        switch (hEncoder->config.inputFormat) {
            case INPUT_16BIT: {
                short *src = (short *)inputBuffer + hEncoder->config.channel_map[channel];
                for (i = 0; i < spch; i++) { dst[i] = (float)*src; src += numChannels; }
                break;
            }
            case INPUT_24BIT: {
                const uint8_t *src_base = (const uint8_t *)inputBuffer;
                for (i = 0; i < spch; i++) {
                    const uint8_t *src = src_base + (i * numChannels + hEncoder->config.channel_map[channel]) * 3;
#if defined(WORDS_BIGENDIAN) && WORDS_BIGENDIAN
                    int32_t s = ((int32_t)src[0] << 16) | ((int32_t)src[1] << 8) | (int32_t)src[2];
#else
                    int32_t s = (int32_t)src[0] | ((int32_t)src[1] << 8) | ((int32_t)src[2] << 16);
#endif
                    if (s & 0x800000) s |= (int32_t)0xff000000;
                    dst[i] = (1.0f / 256.0f) * (float)s;
                }
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
            default: return -1;
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

    if (n > hEncoder->inputFifoFill) n = hEncoder->inputFifoFill;
    rem = hEncoder->inputFifoFill - n;
    if (rem)
        for (channel = 0; channel < numChannels; channel++)
            memmove(hEncoder->inputFifo[channel], hEncoder->inputFifo[channel] + n, rem * sizeof(float));
    hEncoder->inputFifoFill = rem;
}

int faacEncClose(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel;

    if (!hEncoder) return 0;

#ifdef FAAC_STATS
    if (g_faacStats.totalFrames > 0)
    {
        double qavg = g_faacStats.totalQuality / g_faacStats.totalFrames;
        double tr = 100.0 * g_faacStats.transientFrames / g_faacStats.totalFrames;
        double tns = g_faacStats.longBlocks > 0 ? 100.0 * g_faacStats.longBlocksTNS / g_faacStats.longBlocks : 0.0;
        double ms = g_faacStats.totalBands > 0 ? 100.0 * g_faacStats.msBands / g_faacStats.totalBands : 0.0;
        double is = g_faacStats.totalBands > 0 ? 100.0 * g_faacStats.isBands / g_faacStats.totalBands : 0.0;
        double pns = g_faacStats.totalBands > 0 ? 100.0 * g_faacStats.pnsBands / g_faacStats.totalBands : 0.0;
        double att_avg = g_faacStats.attackCount > 0 ? g_faacStats.totalAttack / g_faacStats.attackCount : 0.0;
        float att_max = g_faacStats.maxAttack;

        fprintf(stderr, "\n--- Encoder Diagnostics ---\n");
        fprintf(stderr, " Quality             : Qavg    = %6.2f\n", qavg);

        if (g_faacStats.sbrFrames > 0)
        {
            double sbr_tr = 100.0 * g_faacStats.sbrTransientFrames / g_faacStats.sbrFrames;
            fprintf(stderr, " Transients & Grid   : Core Tr = %5.1f%% (%u/%u) | SBR Grid = %5.1f%% Var | Attack = %.1fx avg, %.1fx max\n",
                    tr, g_faacStats.transientFrames, g_faacStats.totalFrames, sbr_tr, att_avg, att_max);
        }
        else
        {
            fprintf(stderr, " Transients & Grid   : Core Tr = %5.1f%% (%u/%u) | Attack = %.1fx avg, %.1fx max\n",
                    tr, g_faacStats.transientFrames, g_faacStats.totalFrames, att_avg, att_max);
        }

        if (g_faacStats.shortChannels > 0)
        {
            double grp_avg = (double)g_faacStats.shortGroupSum / g_faacStats.shortChannels;
            double split = 100.0 * g_faacStats.shortSplitChannels / g_faacStats.shortChannels;
            fprintf(stderr, " Short Grouping      : Groups  = %5.2f avg/ch | Split = %5.1f%% of %u short ch\n",
                    grp_avg, split, g_faacStats.shortChannels);
        }

        double peak_retry_pct = 100.0 * g_faacStats.peakRetryFrames / g_faacStats.totalFrames;

        if (g_faacStats.sbrFrames > 0)
        {
            double sbr_invf = g_faacStats.sbrInvfCount > 0 ? (double)g_faacStats.sbrInvfSum / g_faacStats.sbrInvfCount : 0.0;
            fprintf(stderr, " Tool Allocation     : M/S     = %5.1f%% | I/S = %5.1f%% | PNS = %5.1f%% | TNS = %5.1f%% | INVF = %.2f\n",
                    ms, is, pns, tns, sbr_invf);
        }
        else
        {
            fprintf(stderr, " Tool Allocation     : M/S     = %5.1f%% | I/S = %5.1f%% | PNS = %5.1f%% | TNS = %5.1f%%\n",
                    ms, is, pns, tns);
        }
        if (g_faacStats.reservoirFrames > 0 || g_faacStats.peakRetryFrames > 0)
        {
            if (g_faacStats.reservoirFrames > 0 && g_faacStats.peakRetryFrames > 0)
            {
                double res_fill = g_faacStats.totalReservoirRatio / g_faacStats.reservoirFrames;
                fprintf(stderr, " Rate Control & Cap  : Reservoir Fill = %5.1f%% (min %5.1f%%, max %5.1f%%) | Peak Limit Retries = %5.1f%% (%u/%u)\n",
                        res_fill, g_faacStats.minReservoirRatio, g_faacStats.maxReservoirRatio,
                        peak_retry_pct, g_faacStats.peakRetryFrames, g_faacStats.totalFrames);
            }
            else if (g_faacStats.reservoirFrames > 0)
            {
                double res_fill = g_faacStats.totalReservoirRatio / g_faacStats.reservoirFrames;
                fprintf(stderr, " Rate Control & Cap  : Reservoir Fill = %5.1f%% (min %5.1f%%, max %5.1f%%)\n",
                        res_fill, g_faacStats.minReservoirRatio, g_faacStats.maxReservoirRatio);
            }
            else
            {
                fprintf(stderr, " Rate Control & Cap  : Peak Limit Retries = %5.1f%% (%u/%u)\n",
                        peak_retry_pct, g_faacStats.peakRetryFrames, g_faacStats.totalFrames);
            }
        }
        fprintf(stderr, "---------------------------\n");
    }
#endif

    PsyEnd(hEncoder->psyInfo, hEncoder->numChannels);
    FilterBankEnd(hEncoder);
    fft_terminate(&hEncoder->fft_tables);

    for (channel = 0; channel < hEncoder->numChannels; channel++)
	{
        int buf;
        for (buf = 0; buf < 4; buf++) {
            if (hEncoder->audioFIFO[channel][buf])
                FreeMemory(hEncoder->audioFIFO[channel][buf]);
        }
		if (hEncoder->inputFifo[channel])
			FreeMemory (hEncoder->inputFifo[channel]);
        if (hEncoder->peakSnap[channel])
            FreeMemory(hEncoder->peakSnap[channel]);
    }

    if (hEncoder->ascCache) free(hEncoder->ascCache);

    if (hEncoder->sbrContext) {
        SbrContextEnd(hEncoder->sbrContext);
        hEncoder->sbrContext = NULL;
    }

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

/* Admission gate: TNS shapes noise along the temporal envelope, so a window
 * with no envelope discontinuity has nothing for it to do, but the LPC gate
 * only discovers that after normalization, autocorrelation and Levinson-Durbin
 * have run. Screening on the envelope first skips that work for frames headed
 * for rejection anyway.
 *
 * Scaled to PsyGetAttack's statistic (largest relative energy jump between
 * adjacent sub-blocks). Not portable to a different sub-block count/size --
 * the same transient reads as a smaller jump with fewer, longer sub-blocks. */
#define TNS_ATTACK_MIN 0.5f

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
    BitStream *bitStream;

    CoderInfo *coderInfo = hEncoder->coderInfo;
    unsigned int numChannels = hEncoder->numChannels;
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
    int flushing = (samplesInput == 0 || inputBuffer == NULL);

    if (samplesInput > 0 && inputBuffer != NULL)
    {
        if (appendInputFifo(hEncoder, inputBuffer, samplesInput) < 0)
            return -1;
    }

    /* A 0-byte return is ambiguous during end-of-stream flushing. While flushing,
     * absorb no-output pipeline priming ticks internally until an encoded frame
     * is produced or core lookahead delay is fully drained. */
    do {
        int realPerCh;          /* real (non-padded) input samples/ch in this frame */
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

        /* SBR's coded-payload ring (frameFIFO) trails the core FIFO by one
         * extra tick, so HE-AAC needs one more flush tick than LC to drain. */
        unsigned int flushBudget = (hEncoder->config.aacObjectType == HE_V1) ?
            SBR_FRAME_FIFO : (LOOKAHEAD_DEPTH + 1);
        if (hEncoder->flushFrame > flushBudget)
            return 0;

        /* HE-AAC: run SBR + downsample first; the core then encodes heHalfRate.
         * Flush frames (realPerCh == 0) included -- the SBR payload runs
         * SBR_FRAME_FIFO-1 frames behind, so the pipeline has to keep ticking
         * through the drain or the tail access units re-emit stale envelopes. */
        float *heHalfRate[MAX_CHANNELS] = {0};
        if (hEncoder->config.aacObjectType == HE_V1 && SbrContextIsPresent(hEncoder->sbrContext))
            doHEAACFrame(hEncoder, (unsigned int)realPerCh, heHalfRate);

        /* Update current sample buffers */
        for (channel = 0; channel < numChannels; channel++)
        {
            float *tmp = hEncoder->audioFIFO[channel][FIFO_PAST];
            hEncoder->audioFIFO[channel][FIFO_PAST]   = hEncoder->audioFIFO[channel][FIFO_CURR];
            hEncoder->audioFIFO[channel][FIFO_CURR]   = hEncoder->audioFIFO[channel][FIFO_AHEAD1];
            hEncoder->audioFIFO[channel][FIFO_AHEAD1]  = hEncoder->audioFIFO[channel][FIFO_AHEAD2];
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
                memcpy(hEncoder->audioFIFO[channel][FIFO_AHEAD2], hEncoder->inputFifo[channel], spc * sizeof(float));
                if (spc < FRAME_LEN)
                    memset(hEncoder->audioFIFO[channel][FIFO_AHEAD2] + spc, 0, (FRAME_LEN - spc) * sizeof(float));
            }

            /* LFE's block_type is always forced to ONLY_LONG_WINDOW in PsyCalculate,
             * so the transient analysis below would be discarded -- skip it. */
            if (!hEncoder->isLfeChannel[channel])
            {
                /* Shared detector replacement on HE: skip half-rate PsyBufferUpdate. */
                if (hEncoder->config.aacObjectType != HE_V1 || !SbrContextIsAnalysisValid(hEncoder->sbrContext))
                {
                    PsyBufferUpdate(&hEncoder->gpsyInfo, &hEncoder->psyInfo[channel],
                        hEncoder->audioFIFO[channel][FIFO_AHEAD1],
                        hEncoder->audioFIFO[channel][FIFO_AHEAD2]);
                }
            }
        }

        /* Drop the consumed frame from the FIFO front (both the LC copy and the
         * HE doHEAACFrame read the leading frameSamplesPerCh samples). */
        if (realPerCh > 0)
            consumeInputFifo(hEncoder, frameSamplesPerCh);

        if (hEncoder->frameNum > LOOKAHEAD_DEPTH)
            break;
    } while (flushing);

    if (!flushing && hEncoder->frameNum <= LOOKAHEAD_DEPTH) /* Still filling up the buffers */
        return 0;

    /* Psychoacoustics */
    /* Shared detector replacement on HE: skip half-rate PsyCalculate. */
    if (hEncoder->config.aacObjectType != HE_V1 || !SbrContextIsAnalysisValid(hEncoder->sbrContext))
        PsyCalculate(hEncoder->elements, hEncoder->numElements, hEncoder->psyInfo, numChannels);

    BlockSwitch(hEncoder, coderInfo, hEncoder->psyInfo, numChannels);

#ifdef FAAC_STATS
    g_faacStats.totalFrames++;
    if (coderInfo[0].block_type == ONLY_SHORT_WINDOW || coderInfo[0].block_type == LONG_SHORT_WINDOW)
    {
        g_faacStats.transientFrames++;
    }
#endif

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
        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbs;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].sfbn; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_short[sb];
            }
            coderInfo[channel].sfb_offset[sb] = offset;
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

    /* Funnelled through one call site so BlocGroup stays a single inlined copy. */
    for (int e = 0; e < hEncoder->numElements; e++)
    {
        AACElement *el = &hEncoder->elements[e];
        int l = el->channels[0];
        int r = (el->type == ID_CPE) ? el->channels[1] : -1;
        CoderInfo *a = NULL, *b = NULL;
        float *xa = NULL, *xb = NULL;

        if (coderInfo[l].block_type == ONLY_SHORT_WINDOW)
        {
            a = &coderInfo[l];
            xa = hEncoder->freqBuff[l];
            if (r >= 0 && coderInfo[r].block_type == ONLY_SHORT_WINDOW)
            {
                b = &coderInfo[r];
                xb = hEncoder->freqBuff[r];
            }
        }
        else if (r >= 0 && coderInfo[r].block_type == ONLY_SHORT_WINDOW)
        {
            a = &coderInfo[r];
            xa = hEncoder->freqBuff[r];
        }

        if (a)
        {
            BlocGroup(a, xa, b, xb, &hEncoder->aacquantCfg);
#ifdef FAAC_STATS
            /* Everything downstream scales with groups.n * sfbn, so this one
             * number covers both the throughput and the bitrate axis. */
            {
                unsigned int nch = b ? 2 : 1;
                g_faacStats.shortChannels += nch;
                g_faacStats.shortGroupSum += (unsigned long)a->groups.n * nch;
                if (a->groups.n > 1)
                    g_faacStats.shortSplitChannels += nch;
            }
#endif
        }
    }

    /* Perform TNS analysis and filtering */
    for (channel = 0; channel < numChannels; channel++) {
        if (!hEncoder->isLfeChannel[channel] && useTns) {
            float attack = PsyGetAttack(&hEncoder->psyInfo[channel]);

#ifdef FAAC_STATS
            if (attack > 0.0f && isfinite(attack)) {
                g_faacStats.totalAttack += attack;
                if (attack > g_faacStats.maxAttack) {
                    g_faacStats.maxAttack = attack;
                }
                g_faacStats.attackCount++;
            }
            if (coderInfo[channel].block_type != ONLY_SHORT_WINDOW) {
                g_faacStats.longBlocks++;
            }
#endif

            /* No envelope available (HE-AAC skips PsyBufferUpdate) means no
               basis to reject on, so admit and let the LPC gates decide. */
            if (attack > 0.0f && attack < TNS_ATTACK_MIN) {
                coderInfo[channel].tnsInfo.tnsDataPresent = 0;
                continue;
            }
            TnsEncode(&(coderInfo[channel].tnsInfo),
                      coderInfo[channel].sfbn,
                      coderInfo[channel].block_type,
                      coderInfo[channel].sfb_offset,
                      hEncoder->freqBuff[channel]);
        } else {
            coderInfo[channel].tnsInfo.tnsDataPresent = 0;      /* TNS not used for LFE */
        }
    }

    for (int e = 0; e < hEncoder->numElements; e++) {
      // reduce LFE bandwidth
		if (hEncoder->elements[e].type == ID_LFE)
		{
                    coderInfo[hEncoder->elements[e].channels[0]].sfbn = 3;
		}
	}

    /* Clear each channel's section state before AACstereo pre-loads intensity
     * bands and BlocQuant resolves the rest. */
    for (channel = 0; channel < numChannels; channel++)
        ResetCoderSections(&coderInfo[channel]);

    AACstereo(coderInfo, hEncoder->elements, hEncoder->numElements, hEncoder->freqBuff,
              (float)hEncoder->aacquantCfg.quality/DEFQUAL, jointmode, hEncoder->sampleRate,
              hEncoder->config.bandWidth);

    /* AACstereo has already consumed freqBuff in place and BlocQuant
     * accumulates into sf[] while reading book[], so a retry can re-run
     * neither. Snapshot what they produce -- book, sf, and the sfbn the CPE fix
     * below rewrites -- so a retry restarts from identical state. */
    unsigned long long peakBits = 0;
    float baseQuality = hEncoder->aacquantCfg.quality;
    int sfbnSnap[MAX_CHANNELS];
    int attempt;

    /* ISO/IEC 14496-3 standard frame limit: 6144 bits per channel */
    peakBits = (unsigned long long)numChannels * AAC_MAX_BITS_PER_CH;

    /* If output format is ADTS (outputFormat == 1), respect the 13-bit ADTS
     * container frame length limit (ADTS_MAX_FRAME_SIZE = 8191 bytes = 65528 bits). */
    if (hEncoder->config.outputFormat == 1)
    {
        unsigned long long adtsPeakBits = (unsigned long long)ADTS_MAX_FRAME_SIZE * 8;
        if (adtsPeakBits < peakBits)
            peakBits = adtsPeakBits;
    }

    if (hEncoder->config.maxBitRate)
    {
        /* maxBitRate is whole-stream, so no channel factor here. For HE-AAC
         * sampleRate is the halved core rate, which is what makes FRAME_LEN
         * cover the right span of output samples. */
        unsigned long long userPeakBits = (unsigned long long)hEncoder->config.maxBitRate
            * FRAME_LEN / hEncoder->sampleRate;
        if (userPeakBits < peakBits)
            peakBits = userPeakBits;
    }

    for (channel = 0; channel < numChannels; channel++) {
        memcpy(hEncoder->peakSnap[channel], coderInfo[channel].book,
               MAX_SCFAC_BANDS * sizeof(int));
        memcpy(hEncoder->peakSnap[channel] + MAX_SCFAC_BANDS, coderInfo[channel].sf,
               MAX_SCFAC_BANDS * sizeof(int));
        sfbnSnap[channel] = coderInfo[channel].sfbn;
    }

    /* Retry while the frame busts peakBits. The search is bounded, not
     * exact-fit: an exact fit can fail to terminate on pathological input. */
    for (attempt = 0; attempt <= PEAK_MAX_RETRIES; attempt++)
    {
        for (channel = 0; channel < numChannels; channel++) {
            BlocQuant(&coderInfo[channel], hEncoder->freqBuff[channel],
                      &(hEncoder->aacquantCfg));
        }

        // fix max_sfb in CPE mode
        for (int e = 0; e < hEncoder->numElements; e++)
        {
            if (hEncoder->elements[e].type == ID_CPE)
            {
                CoderInfo *cil, *cir;

                cil = &coderInfo[hEncoder->elements[e].channels[0]];
                cir = &coderInfo[hEncoder->elements[e].channels[1]];

                cil->sfbn = cir->sfbn = max(cil->sfbn, cir->sfbn);
            }
        }

        /* Write the AAC bitstream; the write doubles as the size probe. */
        bitStream = OpenBitStream(bufferSize, outputBuffer);
        if (!bitStream)
            return -1;

        if (WriteBitstream(hEncoder, coderInfo, hEncoder->elements, hEncoder->numElements, bitStream) < 0)
            return -1;

        /* Close the bitstream and return the number of bytes written */
        frameBytes = CloseBitStream(bitStream);

        if (!peakBits || (unsigned long long)frameBytes * 8 <= peakBits
            || hEncoder->aacquantCfg.quality <= MINQUAL)
            break;

        /* Aim at the budget rather than stepping down by a fixed factor: rate
         * control can park quality anywhere up to MAXQUAL (5000), and a fixed
         * halving needs ~9 passes to cross that to MINQUAL (10), more than any
         * sane retry budget. Frame bits grow sub-linearly with quality, so
         * scaling by the bit ratio undershoots the budget and converges in a
         * pass or two. */
        float scale = (float)peakBits / (float)((unsigned long long)frameBytes * 8);
        if (scale > PEAK_BACKOFF_CEILING) scale = PEAK_BACKOFF_CEILING;
        if (scale < PEAK_BACKOFF_FLOOR)   scale = PEAK_BACKOFF_FLOOR;
        hEncoder->aacquantCfg.quality *= scale;
        if (hEncoder->aacquantCfg.quality < MINQUAL)
            hEncoder->aacquantCfg.quality = MINQUAL;

        for (channel = 0; channel < numChannels; channel++) {
            memcpy(coderInfo[channel].book, hEncoder->peakSnap[channel],
                   MAX_SCFAC_BANDS * sizeof(int));
            memcpy(coderInfo[channel].sf, hEncoder->peakSnap[channel] + MAX_SCFAC_BANDS,
                   MAX_SCFAC_BANDS * sizeof(int));
            coderInfo[channel].sfbn = sfbnSnap[channel];
        }
    }

    /* The cap is per frame, so the backoff must not outlive it: left sticky,
     * one hard frame drags the rest of the stream down, and with bitRate == 0
     * the rate controller below never runs to claw the quality back. */
    hEncoder->aacquantCfg.quality = baseQuality;

#ifdef FAAC_STATS
    if (attempt > 0)
    {
        g_faacStats.peakRetryFrames++;
    }
    g_faacStats.totalQuality += hEncoder->aacquantCfg.quality;
#endif

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

        /* Compute total stream Perceptual Entropy (PE) across channels */
        float totalPE = 0.0f;
        for (channel = 0; channel < numChannels; channel++) {
            totalPE += hEncoder->psyInfo[channel].pe;
        }

        /* Update adaptive bit reservoir balance and compute effective frame bits for rate control */
        int effectiveBits = totalBits;
        int diff = desbits - totalBits;

        if (diff < 0) {
            int excess = -diff;
            /* Adaptive burst draw ceiling: 0.5 * desbits for low bitrates (<=48k stereo / <=24k mono), 1.0 * desbits for high bitrates */
            int drawLimit = (hEncoder->config.bitRate <= 24000) ? (desbits / 2) : desbits;
            int maxDraw = (excess < drawLimit) ? excess : drawLimit;
            /* Data-driven PE complexity threshold: PE_THRESH_PER_CH per channel naturally captures high-entropy transients.
             * Bypassing low-entropy frames prevents quality scale-factor inflation and overshoot. */
            if (totalPE > (PE_THRESH_PER_CH * (float)numChannels) && hEncoder->bitReservoir > 0) {
                int absorbed = (maxDraw < hEncoder->bitReservoir) ? maxDraw : hEncoder->bitReservoir;
                effectiveBits = totalBits - absorbed;
                hEncoder->bitReservoir -= absorbed;
            } else {
                hEncoder->bitReservoir += diff;
                if (hEncoder->bitReservoir < 0) hEncoder->bitReservoir = 0;
            }
        } else {
            /* Simple frames replenish the reservoir without penalizing feedback rate control */
            int space = hEncoder->bitReservoirCap - hEncoder->bitReservoir;
            int deposited = (diff < space) ? diff : space;
            hEncoder->bitReservoir += deposited;
            effectiveBits = totalBits;
        }

        if (effectiveBits > sbrBits)
            fix = (float)(desbits - sbrBits) / (float)(effectiveBits - sbrBits);
        else
            fix = 1.0f;

        /* Apply adaptive damping: accelerate rate control recovery when reservoir is depleted or full */
        float damping = RC_DAMPING_FACTOR;
        if (hEncoder->bitReservoirCap > 0) {
            float fillRatio = (float)hEncoder->bitReservoir / (float)hEncoder->bitReservoirCap;
            if (fillRatio < 0.25f || fillRatio > 0.75f)
                damping = 0.85f;

#ifdef FAAC_STATS
            {
                float fillPct = fillRatio * 100.0f;
                g_faacStats.totalReservoirRatio += fillPct;
                if (fillPct < g_faacStats.minReservoirRatio) g_faacStats.minReservoirRatio = fillPct;
                if (fillPct > g_faacStats.maxReservoirRatio) g_faacStats.maxReservoirRatio = fillPct;
                g_faacStats.reservoirFrames++;
            }
#endif

            /* Additive reservoir proportional correction to eliminate long-term drift */
            float resErr = fillRatio - 0.5f;
            /* Adaptive gain adjustment for HE-AAC to compensate for fixed SBR payload bit offset */
            float kp = (hEncoder->config.aacObjectType == HE_V1 && hEncoder->config.bitRate <= 32000) ? 0.12f : 0.08f;
            fix += kp * resErr;
        }

        /* Apply damping to the quality adjustment */
        fix = (fix - 1.0f) * damping + 1.0f;

        /* Skip small adjustments (< 0.5%) to reduce quality scale update math and keep quality steady */
        if (fabsf(fix - 1.0f) > 0.005f) {
            fix = (fix < 0.80f) ? 0.80f : ((fix > 1.20f) ? 1.20f : fix);
            hEncoder->aacquantCfg.quality *= fix;
        }

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
