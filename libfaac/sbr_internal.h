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
    float qmfOvl64[SBR_QMF_OVL_LEN_64]; /* QMF overlap state (carries across frames) */
} SBRChannel;

/* One frame's coded SBR payload: every field SbrWrite reads that varies per
 * frame. What it reads that is constant for the stream (bs_* header fields,
 * numBands, numNoiseBands) stays in SBRInfo.
 *
 * Sole home for these values: SbrEncode quantizes into a SBRContext.frameFIFO
 * slot and SbrWrite reads an older one, so the delay costs a ring index. Caching
 * a copy anywhere else reintroduces the skew this ring exists to remove. */
typedef struct SbrFrameData {
    int numEnvelopes;
    int eff_amp_res;
    int frameClass;
    int tEnv[SBR_MAX_ENVELOPES + 1];
    int bsPointer;
    struct {
        int envData  [SBR_MAX_ENVELOPES][SBR_MAX_BANDS];
        int noiseData[SBR_MAX_NOISE_ENVELOPES][SBR_MAX_NOISE_BANDS];
        int invfMode;
    } ch[SBR_MAX_CODED_CHANNELS];
} SbrFrameData;

struct SBRInfo {
    int sbrPresent;
    int headerSent;
    int frameCount;
    int numChannels;
    int sampleRate;        /* full output rate; the dual-rate core runs at sampleRate/2 */

    /* --- frequency band configuration (set at init, constant per stream) --- */
    int kx;
    int k2;
    int dk;                /* master frequency table step (1 or 2 QMF bands) */
    int numBands;
    int bandEdges[SBR_MAX_BANDS + 1];
    int numNoiseBands;

    /* --- bitstream header fields --- */
    int bs_amp_res;
    int bs_freq_res;       /* envelope frequency resolution: 1 = HIGH (f_master) */
    int bs_start_freq;
    int bs_stop_freq;
    int bs_xover_band;
    int bs_alter_scale;

    /* --- per-frame state --- */
    /* Whether SbrWrite should (re)send the sbr_header this frame. Frozen once
     * per frame (in SbrEncode) rather than recomputed in SbrWrite, since
     * headerSent/frameCount only advance on SbrWrite's real write pass, and
     * SbrWrite is called multiple times per frame (BuildFrame's count and
     * write passes, plus frame.c's rate-control bit-accounting call). */
    int sendHeaderThisFrame;

    /* --- per-channel state --- */
    SBRChannel ch[MAX_CHANNELS];

    /* QMF analysis twiddle factors. */
    float twidCos[SBR_QMF_BANDS_64];
    float twidSin[SBR_QMF_BANDS_64];
    float oddCos [SBR_QMF_BANDS_64];
    float oddSin [SBR_QMF_BANDS_64];
    FFT_Tables *fftTables;   /* borrowed: the encoder's shared core FFT tables */
};

struct SBRContext {
    unsigned long fullSampleRate;
    unsigned int  fullSampleRateIdx;
    SBRInfo      *sbrInfo;
    struct Resampler *resampler;

    /* Shared signal analysis */
    SignalAnalysis  signalAnalysis;
    /* Shared-detector FIFO: holds the HE block-switch decision for the last
       SBR_DETECT_FIFO analyzed frames. Index 0 is the decision aligned to the
       core frame being coded now, which lags the freshest analysis by the core
       lookahead (LOOKAHEAD_DEPTH frames); newest sits at SBR_DETECT_FIFO-1. */
    float transientStrengthFIFO[MAX_CHANNELS][SBR_DETECT_FIFO];
    int       wantShortFIFO[MAX_CHANNELS][SBR_DETECT_FIFO];

    /* Coded-payload delay ring; see SBR_FRAME_FIFO. frameHead is the newest
       entry, so its successor (frameHead + 1) % SBR_FRAME_FIFO is the oldest --
       the payload the current access unit emits. */
    SbrFrameData frameFIFO[SBR_FRAME_FIFO];
    int          frameHead;
};

SBRInfo *SbrInit(int channels, int sampleRate, unsigned long bitRate, FFT_Tables *fft_tables);
/* Recompute the bitrate-dependent band config without reallocating; lets
 * SetConfiguration adjust an existing handle. */
void SbrUpdate(SBRInfo *sbr, unsigned long bitRate);
void SbrEnd(SBRInfo *sbr);

void SbrQmfAnalysis(SBRInfo *sbr, const float * restrict ovl_pos, float * restrict energy, int kx, int k2);
/* Quantizes this frame's payload directly into *fd (a delay-line slot). */
void SbrEncode(SBRInfo *sbr, float *timeDomain[MAX_CHANNELS], int numChannels, int numSamples, struct SignalAnalysis *sa, SbrFrameData *fd);
/* Emits the payload in *fd, which is a delayed slot, not the newest one. */
int SbrWrite(SBRInfo *sbr, const SbrFrameData *fd, struct BitStream *bs, int id_aac, int writeFlag);

#endif
