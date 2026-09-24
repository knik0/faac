/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2026 Nils Schimmelmann
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

#ifndef SBR_INTERNAL_H
#define SBR_INTERNAL_H

#include "sbr.h"
#include "sbr_analysis.h"
#include "resample.h"

/* Per-channel SBR analysis state. Everything indexed [ch] in SBRInfo lives here. */
typedef struct SBRChannel {
    float qmfOvl64[SBR_QMF_HIST_LEN]; /* QMF overlap plus analysis delay (carries across frames) */
} SBRChannel;

/* One frame's coded SBR payload: every field SbrWrite reads that varies per
 * frame. What it reads that is constant for the stream (bs_* header fields,
 * numBands) stays in SBRInfo.
 *
 * Sole home for these values: SbrEncode quantizes into a SBRContext.frameFIFO
 * slot and SbrWrite reads an older one, so the delay costs a ring index. Caching
 * a copy anywhere else reintroduces the skew this ring exists to remove. */
typedef struct SbrFrameData {
    int numEnvelopes;
    int eff_amp_res;
    SbrFrameClass frameClass;
    int tEnv[SBR_MAX_ENVELOPES + 1];
    int bsPointer;
    int freqRes; /* 1 = high-res band table, 0 = low-res (half the bands) */
    /* The noise floor and inverse-filter mode are stream constants
     * (SBR_NOISE_LEVEL_DEFAULT, SBR_INVF_MODE), so only the envelope is carried. */
    struct {
        int envData[SBR_MAX_ENVELOPES][SBR_MAX_BANDS];
    } ch[MAX_CHANNELS];
} SbrFrameData;

struct SBRInfo {
    int sbrPresent;
    int frameCount;        /* access units so far; the header repeats every SBR_HEADER_PERIOD */
    int numChannels;
    int sampleRate;        /* full output rate; the dual-rate core runs at sampleRate/2 */

    /* --- frequency band configuration (set at init, constant per stream) --- */
    int kx;
    int k2;
    int numBands;
    int bandEdges[SBR_MAX_BANDS + 1];
    int numBandsLow; /* low-res band count: every other high-res edge */
    int bandEdgesLow[SBR_MAX_BANDS + 1];

    /* --- bitstream header fields --- */
    int bs_freq_res;       /* envelope frequency resolution: 1 = HIGH (f_master) */
    int bs_start_freq;
    int bs_stop_freq;
    int bs_xover_band;
    int bs_alter_scale;
    int bs_freq_scale;     /* 1..3: log-spaced master table, 12/10/8 bands per octave */

    /* --- per-frame state --- */
    /* The header decision is made once per access unit, on the first write
     * request after analysis: the writer runs once per element and again on
     * every CBR retry, and only access units that are actually written count
     * toward the header period. */
    int headerDecided;
    int sendHeaderThisFrame;

    /* --- per-channel state --- */
    SBRChannel ch[MAX_CHANNELS];

    /* QMF analysis twiddle factors. */
    float twidCos[SBR_QMF_BANDS_64];
    float twidSin[SBR_QMF_BANDS_64];
    float oddCos [SBR_QMF_BANDS_64];
    float oddSin [SBR_QMF_BANDS_64];
};

struct SBRContext {
    unsigned long fullSampleRate;
    unsigned int  fullSampleRateIdx;
    SBRInfo      *sbrInfo;
    struct Resampler *resampler;

    /* Shared signal analysis */
    SignalAnalysis  signalAnalysis;
    /* Coded-payload delay ring; see SBR_FRAME_FIFO. frameHead is the newest
       entry, so its successor (frameHead + 1) % SBR_FRAME_FIFO is the oldest --
       the payload the current access unit emits. */
    SbrFrameData frameFIFO[SBR_FRAME_FIFO];
    int          frameHead;
};

/* The envelope band table this frame codes over. The quantizer and the writer
 * must agree on it, and the decoder picks the same one from bs_freq_res. */
static inline int sbr_env_bands(const SBRInfo *sbr, const SbrFrameData *fd)
{
    return fd->freqRes ? sbr->numBands : sbr->numBandsLow;
}

static inline const int *sbr_env_edges(const SBRInfo *sbr, const SbrFrameData *fd)
{
    return fd->freqRes ? sbr->bandEdges : sbr->bandEdgesLow;
}

SBRInfo *SbrInit(int channels, int sampleRate, unsigned long bitRate);
/* Recompute the bitrate-dependent band config without reallocating; lets
 * SetConfiguration adjust an existing handle. */
void SbrUpdate(SBRInfo *sbr, unsigned long bitRate);
void SbrEnd(SBRInfo *sbr);

void SbrQmfAnalysis(SBRInfo *sbr, const float * restrict ovl_pos, float * restrict energy, int kx, int k2);
/* Quantizes this frame's payload directly into *fd (a delay-line slot). */
void SbrEncode(SBRInfo *sbr, float *timeDomain[MAX_CHANNELS], int numChannels, const bool *isLfe, int numSamples, struct SignalAnalysis *sa, SbrFrameData *fd);

#endif
