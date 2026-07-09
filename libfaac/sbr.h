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

#ifndef SBR_H
#define SBR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "coder.h"
#include "fft.h"
#include "sbr_analysis.h"

/* Input sample FIFO slots, each one frame (FRAME_LEN samples) wide, relative to
   the frame currently being coded (FIFO_CURR): one frame behind (FIFO_PAST,
   reused as the MDCT overlap) and two frames ahead. The two ahead slots are
   needed because the block-switch energy analysis works on 2-frame-wide windows
   and keeps one window of lookahead, whose far edge reaches two frames ahead. */
#define LOOKAHEAD_DEPTH 2
#define FIFO_PAST       0
#define FIFO_CURR       1
#define FIFO_AHEAD1     2
#define FIFO_AHEAD2     3

/* Depth of the HE shared-detector decision FIFO. Sized so index 0 lines up with
   the core frame currently being coded: the core lags the freshest SBR analysis
   by LOOKAHEAD_DEPTH frames, so index 0 must be LOOKAHEAD_DEPTH entries behind
   the newest (one extra slot for the newest entry itself). */
#define SBR_DETECT_FIFO (LOOKAHEAD_DEPTH + 1)

#ifdef __cplusplus
extern "C" {
#endif

struct BitStream;

#define SBR_QMF_BANDS_64     64
#define SBR_QMF_OVL_LEN_64   576
#define SBR_MAX_BANDS        64
#define SBR_MAX_ENVELOPES     2
#define SBR_MAX_NOISE_ENVELOPES 2
#define SBR_MAX_NOISE_BANDS   5
#define SBR_HEADER_PERIOD    30

/* SBR frame classes (ISO 14496-3:2009 §4.6.18.3, Table 4.80). */
#define SBR_FRAME_CLASS_FIXFIX  0
#define SBR_FRAME_CLASS_FIXVAR  1
#define SBR_FRAME_CLASS_VARFIX  2
#define SBR_FRAME_CLASS_VARVAR  3

/* Envelope time-slot resolution the decoder uses for an AAC-LC core frame
 * (FFmpeg/FAAD2 pass numTimeSlots=16). All bs_rel_bord/t_env values written in
 * a variable grid live in [0, SBR_NUM_TIME_SLOTS]. */
#define SBR_NUM_TIME_SLOTS   16

/* SBR extension types (ISO 14496-3 §4.6.18). */
#define SBR_EXT_TYPE_SBR     0xd
#define SBR_EXT_TYPE_SBR_CRC 0xe

/* Transient detection threshold (peak-to-mean power ratio). */
#define SBR_TRANSIENT_THRESH_DEFAULT    (4.0f)
/* div-by-zero guard for the peak/mean ratio in silence frames (~-150 dBFS^2). */
#define SBR_ENERGY_FLOOR                (1e-15f)
/* log2(0) guard in envelope quantization: -200 dBFS^2, below all SBR quantizer ranges. */
#define SBR_LOG_ENERGY_FLOOR            (1e-20f)
/* Default noise floor level (ISO 14496-3 §4.6.18.6.4). */
#define SBR_NOISE_LEVEL_DEFAULT         4
/* 6 = log2(64): normalises 64-band QMF energy to per-band level. ISO 14496-3 §4.6.18.6.3. */
#define SBR_ENV_LEVEL_LOG2_OFFSET       (6.0f)
/* Rate-dependent resolution thresholds. */
#define SBR_AMP_RES_BITRATE_BPS         20000u
#define SBR_COARSE_TABLE_BITRATE_BPS    32000u
/* Max delta-coded step for envelope data, per bs_amp_res grid (ISO 14496-3
 * §4.6.18.3.6): the fine (amp_res=1) grid has half the step size of the coarse
 * grid, so its delta range must be roughly double to cover the same dB span. */
#define SBR_ENV_DELTA_LIMIT_HIRES       31
#define SBR_ENV_DELTA_LIMIT_LORES       60

typedef struct SBRInfo SBRInfo;
struct SignalAnalysis;

typedef struct SBRContext SBRContext;

SBRContext *SbrContextInit(int channels);
void SbrContextEnd(SBRContext *sbrCtx);
int SbrContextGetASC(SBRContext *sbrCtx, int coreSRIdx, int channels, unsigned char** ppBuffer, unsigned long* pSize);
unsigned int SbrContextGetXOverBandwidth(SBRContext *sbrCtx);
void SbrContextUpdateConfig(SBRContext *sCtx, int channels, unsigned long bitrate, FFT_Tables *fft_tables);
void SbrContextProcessFrame(SBRContext *sCtx, int numChannels, int realPerCh, float *inputFifo[MAX_CHANNELS], float *heHalfRate[MAX_CHANNELS]);
int SbrContextIsPresent(SBRContext *sCtx);
void SbrContextRestoreRate(SBRContext *sCtx, unsigned long *sampleRate, unsigned int *sampleRateIdx, SR_INFO **srInfo);
unsigned long SbrContextGetFullRate(SBRContext *sCtx, unsigned long defaultRate);
void SbrContextResolveRate(SBRContext *sCtx, unsigned long *sampleRate, unsigned int *sampleRateIdx, SR_INFO **srInfo);
int SbrContextIsAnalysisValid(SBRContext *sCtx);
int SbrContextGetWantShort(SBRContext *sCtx, int channel, int index);

int SbrContextGetBits(SBRContext *sCtx, struct BitStream *bs, int channels, int aacObjectType, int writeFlag);

#ifdef __cplusplus
}
#endif

#endif
