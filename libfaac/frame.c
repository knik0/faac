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
#include "ratecontrol.h"

/* HE-AAC auto-mode thresholds; tuned via ViSQOL on a 49-clip corpus. */
#define HE_MIN_SAMPLE_RATE    32000  /* Fs/2 < 16 kHz below this → core too narrow for SBR */
/* HE only wins harder as the rate falls: the further below Nyquist the LC core
 * lands, the more spectrum SBR is rescuing. 8000 is HE-AAC's design floor and
 * the lowest rate measured. */
#define HE_MIN_BITRATE_PER_CH 8000
/* Crossover measured against the LC curve at 48 kHz: HE still leads at
 * 32000 per channel and ties at 48000, with no rung measured between. At
 * 44.1 kHz it already ties at 32000, so the ceiling reaches this value
 * only at HE_MAX_SAMPLE_RATE. Either side moving (a wider LC core, a
 * better SBR) re-opens this constant. */
#define HE_MAX_BITRATE_PER_CH 32000
#define HE_MAX_SAMPLE_RATE    48000
/* Frozen, not derived: quantqual doesn't map onto a bitrate ceiling cleanly
 * (the two are off by 2-4.5x across the range), so this is set by measurement.
 * Deriving it from HE_MAX_BITRATE_PER_CH instead would flip -q 42+ to LC for
 * 13.1% more bits. Re-measure it with a -q sweep whenever the ABR crossover
 * moves. */
#define HE_VBR_QUANTQUAL_MAX  75

/* Top of the bandwidth curve: widening past it loses at every reachable rate,
 * the band above holds ~0.006% of programme energy and sits at the edge of
 * hearing. VBR, having no rate, codes at the top. */
#define BANDWIDTH_CEILING     18750

#if (defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64) && !defined(PACKAGE_VERSION)
#include "win32_ver.h"
#endif

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

static unsigned int CalcBandwidth(unsigned long bitRate)
{
    /* Anchors land on long-block band edges, so CalcBW()'s snap is a no-op and
     * these constants are the cutoff you actually get. Divisors are powers of
     * two. LC in effect: HE reaches here too, but faacEncApplyConfig then
     * overwrites bandWidth with the SBR crossover, which the core must meet. */
    if (bitRate <= 12000) {
        /* Unmeasured below here; ramp rather than extend the plateau down.
         * bitRate is unsigned, so guard the subtraction. */
        return (bitRate > 8000) ? 9250 + ((bitRate - 8000) * 5 / 4) : 9250;
    }
    if (bitRate <= 32000) {
        /* Flat by measurement: the optimum does not move across this range. */
        return 14250;
    }
    if (bitRate <= 48000)
        return 14250 + ((bitRate - 32000) * 9 / 32);
    return BANDWIDTH_CEILING;
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
        BitStream bs;
        InitBitStream(&bs, *ppBuffer, 2); /* zeroes the buffer, so the 3 trailing pad bits need no write */
        PutBit(&bs, hEncoder->config.aacObjectType, 5);
        PutBit(&bs, hEncoder->sampleRateIdx,        4);
        PutBit(&bs, hEncoder->numChannels,          4);
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
    /* Resolve AUTO to LC or HE-AAC. HE-AAC wins for low rates, but only
     * at Fs >= 32 kHz so the Fs/2 core stays >= 16 kHz; below that the
     * narrow-band core + SBR reconstruction collapses. */
    if (hEncoder->config.aacObjectType == AUTO) {
        unsigned long rate_per_ch = config->bitRate;
        int rate_ok;
        if (rate_per_ch > 0) {
            /* Below 48 kHz, SBR has less core bandwidth to extend from, so the
             * ceiling ramps down toward 20000 bps/ch at the HE_MIN_SAMPLE_RATE floor. */
            unsigned int max_he_rate = 0;
            if (hEncoder->sampleRate >= HE_MAX_SAMPLE_RATE) {
                max_he_rate = HE_MAX_BITRATE_PER_CH;
            } else if (hEncoder->sampleRate >= HE_MIN_SAMPLE_RATE) {
                max_he_rate = 20000 + (unsigned int)((hEncoder->sampleRate - HE_MIN_SAMPLE_RATE) *
                              (HE_MAX_BITRATE_PER_CH - 20000) / (HE_MAX_SAMPLE_RATE - HE_MIN_SAMPLE_RATE));
            }
            rate_ok = (rate_per_ch >= HE_MIN_BITRATE_PER_CH && rate_per_ch <= max_he_rate);
        } else {
            rate_ok = (config->quantqual <= HE_VBR_QUANTQUAL_MAX);
        }
        hEncoder->config.aacObjectType =
            (rate_ok && hEncoder->sampleRate >= HE_MIN_SAMPLE_RATE) ? HE_V1 : LOW;
        config->aacObjectType = hEncoder->config.aacObjectType;
    }

    if (hEncoder->config.aacObjectType == HE_V1
        && hEncoder->sampleRate < HE_MIN_SAMPLE_RATE)
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

    /* MaxBitrate() is already per channel, and its frame is FRAME_LEN samples
     * at the core rate -- so the clamp has to follow the HE-AAC resolution
     * above, which halves that rate. */
    if (config->bitRate > MaxBitrate(hEncoder->sampleRate))
        config->bitRate = MaxBitrate(hEncoder->sampleRate);

    /* AUTO keeps the legacy meaning of the two rate fields. */
    if (config->rateControl == RATE_AUTO)
        config->rateControl = config->bitRate ? RATE_ABR : RATE_VBR;
    hEncoder->config.rateControl = config->rateControl;

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);

    if (config->bitRate && !config->bandWidth)
    {
        config->bandWidth = CalcBandwidth(config->bitRate);

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

    /* Only VBR reaches here without a bandwidth: ABR derived its own above. */
    if (!config->bandWidth)
        config->bandWidth = BANDWIDTH_CEILING;

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
              &hEncoder->aacquantCfg,
              hEncoder->sfbOffsetShort,
              hEncoder->sfbOffsetLong);

    {
        const int *sfbOffset[2] = { hEncoder->sfbOffsetLong, hEncoder->sfbOffsetShort };
        const int  sfbn[2]      = { hEncoder->aacquantCfg.max_cbl, hEncoder->aacquantCfg.max_cbs };
        StereoConfigure(&hEncoder->stereoCfg, (JointMode)hEncoder->config.jointmode, hEncoder->sampleRate,
                        hEncoder->config.bandWidth, hEncoder->config.bitRate, sfbOffset, sfbn);
    }

    // reset psymodel
    PsyEnd(hEncoder->psyInfo, hEncoder->numChannels);
    PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
			hEncoder->sampleRate);

	/* load channel_map */
	for( i = 0; i < MAX_CHANNELS; i++ )
		hEncoder->config.channel_map[i] = config->channel_map[i];

    InitElements(hEncoder->elements, &hEncoder->numElements, (int)hEncoder->numChannels, hEncoder->config.useLfe);
    RefreshLfeMap(hEncoder);

    RateControlReset(&hEncoder->rc, hEncoder->numChannels, hEncoder->config.bitRate,
                     hEncoder->sampleRate, hEncoder->config.rateControl == RATE_CBR);

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
    RateControlStatsInit();
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

    /* Identity map; faac_encoder_open() sets every other config field. */
	for( channel = 0; channel < MAX_CHANNELS; channel++ )
		hEncoder->config.channel_map[channel] = channel;

    /* find correct sampling rate depending parameters */
    hEncoder->srInfo = &srInfo[hEncoder->sampleRateIdx];

    for (channel = 0; channel < numChannels; channel++)
	{
        int buf;
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
        RateControlStatsPrint(stderr);
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
    SbrContextProcessFrame(hEncoder->sbrContext, hEncoder->numChannels, hEncoder->isLfeChannel, (int)realPerCh,
                           (int)hEncoder->flushFrame, hEncoder->inputFifo, heHalfRate);
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
    int frameBytes;
    BitStream *bitStream;

    CoderInfo *coderInfo = hEncoder->coderInfo;
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int useTns = hEncoder->config.useTns;
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

    /* SBR's coded-payload ring (frameFIFO) trails the core FIFO by one
     * extra tick, so HE-AAC needs one more flush tick than LC to drain. */
    unsigned int flushBudget = (hEncoder->config.aacObjectType == HE_V1) ?
        SBR_FRAME_FIFO : (LOOKAHEAD_DEPTH + 1);

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

            if (hEncoder->config.aacObjectType == HE_V1 && heHalfRate[channel])
            {
                /* ahead of the flush case: this carries the resampler's tail */
                memcpy(hEncoder->audioFIFO[channel][FIFO_AHEAD2], heHalfRate[channel], FRAME_LEN * sizeof(float));
            }
            else if (realPerCh == 0)
            {
                /* start flushing*/
                memset(hEncoder->audioFIFO[channel][FIFO_AHEAD2], 0, FRAME_LEN * sizeof(float));
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
        PsyCalculate(hEncoder->psyInfo, hEncoder->isLfeChannel, numChannels);

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
            coderInfo[channel].sfb_offset = hEncoder->sfbOffsetShort;
        } else {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbl;
            coderInfo[channel].sfb_offset = hEncoder->sfbOffsetLong;

            coderInfo[channel].groups.n = 1;
            coderInfo[channel].groups.len[0] = 1;
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
        if (!hEncoder->isLfeChannel[channel] && useTns && coderInfo[channel].block_type != ONLY_SHORT_WINDOW) {
            float attack = PsyGetAttack(&hEncoder->psyInfo[channel]);

#ifdef FAAC_STATS
            if (attack > 0.0f && isfinite(attack)) {
                g_faacStats.totalAttack += attack;
                if (attack > g_faacStats.maxAttack) {
                    g_faacStats.maxAttack = attack;
                }
                g_faacStats.attackCount++;
            }
            g_faacStats.longBlocks++;
#endif

            /* No envelope available (HE-AAC skips PsyBufferUpdate) means no
               basis to reject on, so admit and let the LPC gates decide. */
            if (attack > 0.0f && attack < TNS_ATTACK_MIN) {
                coderInfo[channel].tnsInfo.tnsDataPresent = 0;
                continue;
            }

            TnsEncode(&coderInfo[channel], hEncoder->freqBuff[channel]);
        } else {
            coderInfo[channel].tnsInfo.tnsDataPresent = 0;      /* TNS not used for LFE or short blocks */
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
              (float)hEncoder->aacquantCfg.quality/DEFQUAL, &hEncoder->stereoCfg);

    /* AACstereo has already consumed freqBuff in place and BlocQuant
     * accumulates into sf[] while reading book[], so a retry can re-run
     * neither. Snapshot what they produce -- book, sf, and the sfbn the CPE fix
     * below rewrites -- so a retry restarts from identical state. */
    unsigned long long peakBits = 0;
    float baseQuality = hEncoder->aacquantCfg.quality;
    int sfbnSnap[MAX_CHANNELS];
    int attempt;
    /* Every cap below is on the raw_data_block; the ADTS header is transport. */
    int hdrBytes = (hEncoder->config.outputFormat == 1) ? ADTS_HEADER_SIZE : 0;
    int payloadBits = 0;
#ifdef FAAC_STATS
    int resBound = 0;
#endif

    /* ISO/IEC 14496-3 standard frame limit: 6144 bits per channel */
    peakBits = (unsigned long long)numChannels * AAC_MAX_BITS_PER_CH;

    /* If output format is ADTS (outputFormat == 1), respect the 13-bit ADTS
     * container frame length limit (ADTS_MAX_FRAME_SIZE = 8191 bytes), less the header. */
    if (hEncoder->config.outputFormat == 1)
    {
        unsigned long long adtsPeakBits = (unsigned long long)(ADTS_MAX_FRAME_SIZE - ADTS_HEADER_SIZE) * 8;
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

    /* CBR bit reservoir: one more term in the min() of caps, enforced by the
       same retry loop, so a frame that fits costs nothing. */
    {
        int resAvail = RateControlFrameCap(&hEncoder->rc);
        if (resAvail && (unsigned long long)resAvail < peakBits)
        {
            peakBits = resAvail;
#ifdef FAAC_STATS
            resBound = 1;
#endif
        }
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
        payloadBits = (frameBytes - hdrBytes) * 8;

        if (!peakBits || (unsigned long long)payloadBits <= peakBits
            || hEncoder->aacquantCfg.quality <= MINQUAL)
            break;

        /* Aim at the budget rather than stepping down by a fixed factor: rate
         * control can park quality anywhere up to MAXQUAL (5000), and a fixed
         * halving needs far more passes to cross that to MINQUAL than any
         * sane retry budget. Frame bits grow sub-linearly with quality, so
         * scaling by the bit ratio undershoots the budget and converges in a
         * pass or two. */
        float scale = (float)peakBits / (float)payloadBits;
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

    /* The caps are per frame, so the backoff must not outlive the frame: left
     * sticky, one hard frame drags the stream down (2% BD-rate), and with
     * bitRate == 0 nothing below ever claws the quality back. */
    hEncoder->aacquantCfg.quality = baseQuality;

#ifdef FAAC_STATS
    if (attempt > 0)
    {
        g_faacStats.peakRetryFrames++;
        if (resBound) g_faacStats.resBoundRetryFrames++;
    }
    g_faacStats.totalQuality += hEncoder->aacquantCfg.quality;
#endif

    /* Adjust quality to get correct average bitrate */
    if (hEncoder->config.bitRate)
        hEncoder->aacquantCfg.quality = RateControlUpdate(&hEncoder->rc, payloadBits,
                                                          hEncoder->aacquantCfg.quality, maxqual);

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
