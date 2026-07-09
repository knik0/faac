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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SBR_INTERNAL_H
#define SBR_INTERNAL_H

#include "sbr.h"
#include "sbr_analysis.h"
#include "resample.h"

/* Per-channel SBR state. Everything indexed [ch] in SBRInfo lives here. */
typedef struct SBRChannel {
    faac_real qmfOvl64[SBR_QMF_OVL_LEN_64]; /* QMF overlap state (carries across frames) */
    int envData  [SBR_MAX_ENVELOPES][SBR_MAX_BANDS]; /* quantised envelope indices */
    int noiseData[SBR_MAX_NOISE_ENVELOPES][SBR_MAX_NOISE_BANDS]; /* quantised noise floor indices */
    int invfMode;                                    /* bs_invf_mode (0–3) */
} SBRChannel;

typedef struct SBRInfo {
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
    int numEnvelopes;      /* 1 or 2, set by transient detection in SbrEncode */
    int eff_amp_res;       /* forced to 0 for single-envelope FIXFIX (ISO 14496-3:2009 §4.6.18.3) */

    /* Envelope time grid for the frame (frame-global, shared by both channels of
     * a CPE). frameClass selects FIXFIX or VARFIX; tEnv[0..numEnvelopes] are the
     * envelope borders in SBR time slots ([0, SBR_NUM_TIME_SLOTS]) and are only
     * emitted for the variable classes. bsPointer marks the transient envelope. */
    int frameClass;
    int tEnv[SBR_MAX_ENVELOPES + 1];
    int bsPointer;

    /* Whether SbrWrite should (re)send the sbr_header this frame. Frozen once
     * per frame (in SbrEncode) rather than recomputed in SbrWrite, since
     * headerSent/frameCount only advance on SbrWrite's real write pass, and
     * SbrWrite is called multiple times per frame (BuildFrame's count and
     * write passes, plus frame.c's rate-control bit-accounting call). */
    int sendHeaderThisFrame;

    /* --- per-channel state --- */
    SBRChannel ch[MAX_CHANNELS];

    /* QMF analysis twiddle factors. */
    faac_real twidCos[SBR_QMF_BANDS_64];
    faac_real twidSin[SBR_QMF_BANDS_64];
    faac_real oddCos [SBR_QMF_BANDS_64];
    faac_real oddSin [SBR_QMF_BANDS_64];
    FFT_Tables *fftTables;   /* borrowed: the encoder's shared core FFT tables */
} SBRInfo;

typedef struct SBRContext {
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
    faac_real transientStrengthFIFO[MAX_CHANNELS][SBR_DETECT_FIFO];
    int       wantShortFIFO[MAX_CHANNELS][SBR_DETECT_FIFO];
} SBRContext;

SBRInfo *SbrInit(int channels, int sampleRate, unsigned long bitRate, FFT_Tables *fft_tables);
/* Recompute the bitrate-dependent band config without reallocating; lets
 * SetConfiguration adjust an existing handle. */
void SbrUpdate(SBRInfo *sbr, unsigned long bitRate);
void SbrEnd(SBRInfo *sbr);

void SbrQmfAnalysis(SBRInfo *sbr, const faac_real * restrict ovl_pos, faac_real * restrict energy, int kx, int k2);
void SbrEncode(SBRInfo *sbr, faac_real *timeDomain[MAX_CHANNELS], int numChannels, int numSamples, struct SignalAnalysis *sa);
int SbrWrite(SBRInfo *sbr, struct BitStream *bs, int id_aac, int writeFlag);

#endif
