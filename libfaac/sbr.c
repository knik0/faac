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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "sbr.h"
#include "sbr_tables.h"
#include "util.h"
#include "sbr_analysis.h"
#include "resample.h"
#include "bitstream.h"
#include "sbr_internal.h"
#include "faac_internal.h"
#include "channels.h"
#include "stats.h"

/* SBR master frequency band table (ISO/IEC 14496-3:2005 §4.6.18.3.2). kx/k2 are
 * spec-mandatory: the decoder reconstructs them from the sample rate alone, so
 * these must match its table exactly or the envelope band count desyncs. The
 * rate here is the full output rate (= 2*core), which is what the decoder uses. */

/* SBR start frequency (kx). Crossover alignment prevents aliasing/gaps. */
static int compute_kx(int sampleRate, int bs_start_freq)
{
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int start_min = ((temp << 7) + (sampleRate >> 1)) / sampleRate;
    int row = (sampleRate <= 16000) ? 0 : (sampleRate <= 22050) ? 1 : (sampleRate <= 24000) ? 2 : (sampleRate <= 32000) ? 3 : (sampleRate <= 64000) ? 4 : 5;
    return clamp_int(start_min + sbr_offset[row][bs_start_freq & 15], 1, 63);
}

static int cmp_int16(const void *a, const void *b) { return (int)(*(const short *)a) - (int)(*(const short *)b); }
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* SBR stop frequency (k2), ISO 14496-3 §4.6.18.3.2.1. Decoders derive it
 * from bs_stop_freq alone, so it can't be adjusted here. */
static int compute_k2(int sampleRate, int bs_stop_freq)
{
    if (bs_stop_freq == 14 || bs_stop_freq == 15) return 64;
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int stop_min = ((temp << 8) + (sampleRate >> 1)) / sampleRate;
    int k2;
    if (bs_stop_freq < 14) {
        short stop_dk[13];
        float prod = (float)stop_min;
        int prev = stop_min;
        float base = powf(64.0f / (float)stop_min, (float)(1.0f / 13.0f));
        for (int i = 0; i < 12; i++) {
            prod *= base;
            int present = (int)lrintf(prod);
            stop_dk[i] = (short)(present - prev);
            prev = present;
        }
        stop_dk[12] = (short)(64 - prev);
        qsort(stop_dk, 13, sizeof(short), cmp_int16);
        k2 = stop_min;
        for (int i = 0; i < bs_stop_freq; i++) k2 += stop_dk[i];
    } else {
        k2 = 64;
    }

    return k2;
}

/* Widest k2 - kx decoders accept (§4.6.18.3.2.1). */
static int max_sbr_span(int sampleRate)
{
    return (sampleRate <= 32000) ? 48 : (sampleRate <= 44100) ? 35 : 32;
}

/* Smallest stop-frequency index reaching targetHz, or the widest one decoders
 * accept. Searched rather than tabulated: the index-to-frequency mapping
 * shifts with sample rate, so a fixed table would overshoot at some rates. */
static int pick_stop_freq(int sampleRate, int kx, int targetHz)
{
    int best = SBR_STOP_FREQ_MIN;
    for (int sf = SBR_STOP_FREQ_MIN; sf <= SBR_STOP_FREQ_MAX; sf++) {
        int k2 = compute_k2(sampleRate, sf);
        if (k2 - kx > max_sbr_span(sampleRate)) break;
        best = sf;
        if ((long)k2 * sampleRate / (2 * SBR_QMF_BANDS_64) >= targetHz) break;
    }
    return best;
}

/* Master table (ISO 14496-3 §4.6.18.3.2.1). bs_freq_scale 0: uniform
 * dk-spacing, residual bands merged into the first/last pairs. 1/2/3:
 * log-spaced with 12/10/8 bands per octave, widths of a geometric series
 * rounded and sorted so the narrow bands sit at the bottom. Only the
 * one-region case exists here: at bs_start_freq 15 kx is at least 30 at
 * every sample rate and k2 at most 64, so k2/kx never reaches the 2.2449
 * split. */
static int build_freq_table(SBRInfo *sbr)
{
    int kx = sbr->kx, k2 = sbr->k2;
    int *edges = sbr->bandEdges;
    int n_master;

    int prev = kx;
    int bands_per_octave = 14 - 2 * sbr->bs_freq_scale; /* 12, 10, 8 for bs_freq_scale 1, 2, 3 */
    n_master = 2 * (int)(bands_per_octave * log2f((float)k2 / (float)kx) / 2.0f + 0.5f);
    n_master = clamp_int(n_master, 1, SBR_MAX_BANDS);
    for (int k = 0; k < n_master; k++) {
        int edge = (int)(kx * powf((float)k2 / (float)kx, (float)(k + 1) / (float)n_master) + 0.5f);
        edges[1 + k] = edge - prev;
        prev = edge;
    }
    qsort(edges + 1, n_master, sizeof(int), cmp_int);
    edges[0] = kx;
    for (int k = 1; k <= n_master; k++) edges[k] += edges[k - 1];
    sbr->numBands = n_master;

    /* Low-res table (ISO 14496-3 §4.6.18.3.2.2): every other high-res edge,
     * parity chosen by n_master. */
    int n_low = (n_master + 1) >> 1;
    sbr->numBandsLow = n_low;
    int odd = n_master & 1;
    sbr->bandEdgesLow[0] = edges[0];
    for (int b = 1; b <= n_low; b++)
        sbr->bandEdgesLow[b] = edges[2 * b - odd];

    return n_master;
}

SBRInfo *SbrInit(int channels, int sampleRate, unsigned long bitRate)
{
    SBRInfo *sbr = (SBRInfo *)AllocMemory(sizeof(SBRInfo));
    if (!sbr) return NULL;
    SetMemory(sbr, 0, sizeof(SBRInfo));
    sbr->sbrPresent = 1;
    sbr->numChannels = channels;
    sbr->sampleRate = sampleRate;

    /* Pre-calculate twiddle factors for the FFT-based QMF analysis.
     * These coefficients rotate the subband indices into the odd-frequency
     * DFT space required by the SBR modulation kernel. */
    for (int m = 0; m < SBR_QMF_BANDS_64; m++) {
        sbr->twidCos[m] = (float)cos(M_PI_DOUBLE * m / 64.0);
        sbr->twidSin[m] = (float)sin(M_PI_DOUBLE * m / 64.0);
        sbr->oddCos[m] = (float)cos(M_PI_DOUBLE * (2 * m + 1) / 128.0);
        sbr->oddSin[m] = (float)sin(M_PI_DOUBLE * (2 * m + 1) / 128.0);
    }
    SbrUpdate(sbr, bitRate);
    return sbr;
}

/* Re-resolve SBR operational parameters (crossover, resolution) when the
 * bitrate or sample rate changes, avoiding handle reallocation. */
void SbrUpdate(SBRInfo *sbr, unsigned long bitRate)
{
    int sampleRate = sbr->sampleRate;
    unsigned long rate_per_ch = bitRate / sbr->numChannels;
    sbr->bs_amp_res = (rate_per_ch < SBR_AMP_RES_BITRATE_BPS) ? 0 : 1;
    /* Target crossover near the core ceiling (~11.6 kHz) maximizes MOS.
     * Higher-order parametric reconstruction below 10 kHz is audible and
     * generally inferior to the bit-starved LC core. */
    sbr->bs_start_freq = 15;
    /* Log-spaced envelope bands, fewer per octave while bits are scarce:
     * what they save, rate control hands to the core. */
    sbr->bs_freq_scale = (rate_per_ch >= SBR_FREQ_SCALE_FINE_BPS) ? 1
                       : (rate_per_ch >= SBR_FREQ_SCALE_COARSE_BPS) ? 3 : 2;
    sbr->bs_alter_scale = 0; /* only warps a two-region table; see build_freq_table */
    sbr->bs_freq_res = 1; /* HIGH resolution */
    sbr->bs_xover_band = 0; /* every master band is an SBR band; no low-res split */
    sbr->kx = compute_kx(sampleRate, sbr->bs_start_freq);

    /* Where the reconstruction stops. Aim at hearing rather than k2's ceiling:
     * bands above the target cost the same envelope bits as the ones below, so
     * there's no reason to stop short of what's audible. */
    sbr->bs_stop_freq = pick_stop_freq(sampleRate, sbr->kx, SBR_STOP_FREQ_TARGET_HZ);
    sbr->k2 = compute_k2(sampleRate, sbr->bs_stop_freq);

    build_freq_table(sbr);
}

void SbrEnd(SBRInfo *sbr)
{
    if (!sbr) return;
    FreeMemory(sbr);
}

/* What analysing a silent frame yields. Needed because zeroed memory is not a
 * legal payload: numEnvelopes == 0 encodes no grid at all. */
static void sbr_frame_silence(SbrFrameData *fd)
{
    SetMemory(fd, 0, sizeof(*fd));
    fd->numEnvelopes = 1;
    fd->eff_amp_res  = 0;
    fd->frameClass   = SBR_FRAME_CLASS_FIXFIX;
    fd->tEnv[0]      = 0;
    fd->tEnv[1]      = SBR_NUM_TIME_SLOTS;
    fd->bsPointer    = 0;
    fd->freqRes      = 1;
}

SBRContext *SbrContextInit(int channels)
{
    SBRContext *sbrCtx = (SBRContext *)AllocMemory(sizeof(SBRContext));
    if (sbrCtx) {
        SetMemory(sbrCtx, 0, sizeof(SBRContext));
        sbrCtx->resampler = ResampleInit(channels);
        if (!sbrCtx->resampler) {
            FreeMemory(sbrCtx);
            return NULL;
        }
        /* The first access units carry the core's silent lead-in, so the ring
         * has to start full of payloads that describe silence. */
        for (int i = 0; i < SBR_FRAME_FIFO; i++)
            sbr_frame_silence(&sbrCtx->frameFIFO[i]);
    }
    return sbrCtx;
}

void SbrContextEnd(SBRContext *sbrCtx)
{
    if (!sbrCtx) return;
    if (sbrCtx->sbrInfo) {
        SbrEnd(sbrCtx->sbrInfo);
    }
    if (sbrCtx->resampler) {
        ResampleEnd(sbrCtx->resampler);
    }
    FreeMemory(sbrCtx);
}

int SbrContextGetASC(SBRContext *sbrCtx, int coreSRIdx, int channels, unsigned char** ppBuffer, unsigned long* pSize)
{
    /* Explicit-hierarchy ASC: AAC-LC core wrapped with an SBR extension
     * (sync 0x2b7, type 5) carrying the full output rate. The core rate is
     * Fs/2 (dual-rate SBR); the extension declares the full output rate.
     *
     * A mono core also carries the PS sync extension with psPresentFlag = 0:
     * without it a decoder may assume parametric stereo is implied and return
     * two channels. */
    const int signalPS = (channels == 1);
    const unsigned long size = signalPS ? 7 : 5;

    unsigned char *buf = (unsigned char *)malloc(size);
    if (buf == NULL) return -3;

    BitStream bs;
    InitBitStream(&bs, buf, (uint32_t)size); /* zeroes the buffer, so the trailing pad bits need no write */

    BitAccumulator a;
    AccumBegin(&a, &bs);
    AccumPutBits(&a, LOW,       5); /* core object type */
    AccumPutBits(&a, coreSRIdx, 4); /* core rate (Fs/2, dual-rate) */
    AccumPutBits(&a, GetChannelConfig(channels), 4);
    AccumPutBits(&a, 0,         3); /* frameLengthFlag, dependsOnCoreCoder, extensionFlag */
    AccumPutBits(&a, 0x2b7,    11); /* syncExtensionType */
    AccumPutBits(&a, HE_V1,     5); /* extObjectType = SBR */
    AccumPutBits(&a, 1,         1); /* sbrPresentFlag */
    AccumPutBits(&a, sbrCtx->fullSampleRateIdx, 4); /* SBR output rate (2*core) */
    if (signalPS) {
        AccumPutBits(&a, 0x548, 11); /* syncExtensionType = PS */
        AccumPutBits(&a, 0,      1); /* psPresentFlag */
    }
    AccumEnd(&a);

    *ppBuffer = buf;
    *pSize = size;
    return 0;
}

unsigned int SbrContextGetXOverBandwidth(SBRContext *sbrCtx)
{
    if (!sbrCtx || !sbrCtx->sbrInfo) return 0;
    /* kx * Fs / (2*64): each QMF band is Fs/(2*SBR_QMF_BANDS_64) Hz wide.
     * Matching core bandwidth to the SBR crossover avoids a gap or overlap. */
    return (unsigned int)((sbrCtx->sbrInfo->kx * sbrCtx->fullSampleRate) /
                           (2 * SBR_QMF_BANDS_64));
}

void SbrContextUpdateConfig(SBRContext *sCtx, int channels, unsigned long bitrate)
{
    if (!sCtx) return;
    if (!sCtx->sbrInfo)
        sCtx->sbrInfo = SbrInit(channels, sCtx->fullSampleRate, bitrate);
    else
        SbrUpdate(sCtx->sbrInfo, bitrate);
}

void SbrContextProcessFrame(SBRContext *sCtx, int numChannels, const bool *isLfe, int realPerCh, int flushTick, float *inputFifo[MAX_CHANNELS], float *heHalfRate[MAX_CHANNELS])
{
    unsigned int channel;
    Resampler *rs = sCtx->resampler;
    float *fullPtrs[MAX_CHANNELS];

    /* SbrEncode quantizes into the new head; SbrWrite (via SbrContextGetBits)
     * emits the oldest slot, which is the payload for this frame's core audio. */
    sCtx->frameHead = (sCtx->frameHead + 1) % SBR_FRAME_FIFO;
    SbrFrameData *fd = &sCtx->frameFIFO[sCtx->frameHead];
    sCtx->sbrInfo->headerDecided = 0;

    /* Tick 1 still has real signal in the QMF overlap and the decimation FIR;
     * by tick 2 both are zero, so the rest of the drain is known silence. */
    if (realPerCh == 0 && flushTick > 1) {
        for (channel = 0; channel < (unsigned int)numChannels; channel++) {
            memset(rs->halfRate[channel], 0, FRAME_LEN * sizeof(float));
            heHalfRate[channel] = rs->halfRate[channel];
            sCtx->signalAnalysis.ch[channel].transientStrength = 0.0f;
        }
        sbr_frame_silence(fd);
    } else {
        for (channel = 0; channel < (unsigned int)numChannels; channel++) {
            float *fullRate = rs->fullRate[channel];
            fullPtrs[channel] = fullRate;
            if (realPerCh)
                memcpy(fullRate, inputFifo[channel], realPerCh * sizeof(float));
            /* Final partial frame: silence-pad the unfilled full-rate tail to
             * prevent the resampler from consuming stale data. */
            if (realPerCh < 2 * FRAME_LEN)
                memset(fullRate + realPerCh, 0, (2 * FRAME_LEN - realPerCh) * sizeof(float));
            heHalfRate[channel] = rs->halfRate[channel];
        }

        /* Always the full padded frame, never [0, realPerCh): the grid unconditionally
         * claims SBR_NUM_TIME_SLOTS, so normalising a short frame over fewer slots
         * would inflate its levels, and the QMF-overlap save below reads the last
         * SBR_QMF_OVL_LEN_64 samples -- behind the buffer for a short frame. */
        SbrAnalyze(&sCtx->signalAnalysis, fullPtrs, numChannels, isLfe, 2 * FRAME_LEN, sCtx->sbrInfo);
        SbrEncode(sCtx->sbrInfo, fullPtrs, numChannels, isLfe, 2 * FRAME_LEN, &sCtx->signalAnalysis, fd);
        /* Dual-rate decimation: produces the halved-rate core signal. */
        Resample(rs, 2 * FRAME_LEN);
    }
}

void SbrContextRestoreRate(SBRContext *sCtx, unsigned long *sampleRate, unsigned int *sampleRateIdx, SR_INFO **srInfoPtr)
{
    if (sCtx && sCtx->fullSampleRate > 0) {
        *sampleRate    = sCtx->fullSampleRate;
        *sampleRateIdx = sCtx->fullSampleRateIdx;
        *srInfoPtr     = &srInfo[*sampleRateIdx];
        sCtx->fullSampleRate = 0;
    }
}

unsigned long SbrContextGetFullRate(SBRContext *sCtx, unsigned long defaultRate)
{
    return (sCtx && sCtx->fullSampleRate) ? sCtx->fullSampleRate : defaultRate;
}

/* Dual-rate SBR: the AAC core encodes at Fs/2 while SBR reconstructs the top
 * octave back to the full rate. Halve the core rate here; the full rate is kept
 * in the context for SBR and the ASC. */
void SbrContextResolveRate(SBRContext *sCtx, unsigned long *sampleRate, unsigned int *sampleRateIdx, SR_INFO **srInfoPtr)
{
    if (sCtx->fullSampleRate == 0) {
        sCtx->fullSampleRate     = *sampleRate;
        sCtx->fullSampleRateIdx  = *sampleRateIdx;
        *sampleRate         = *sampleRate / 2;
        *sampleRateIdx      = GetSRIndex(*sampleRate);
        *srInfoPtr          = &srInfo[*sampleRateIdx];
    }
}

int SbrContextIsPresent(SBRContext *sCtx)
{
    return (sCtx && sCtx->sbrInfo) ? 1 : 0;
}

/* Optimized log2 approximation for energy-to-decibel conversion.
 * Precision is sufficient for the 1.5/3.0 dB envelope quantizer. */
#define FAST_LOG2_A         1.3424f
#define FAST_LOG2_B         0.3427f
#define FAST_LOG2_MANT_NORM (1.0f / (1 << 23))  /* 23-bit mantissa → [0, 1) */
static inline float fast_log2(float x)
{
    union { float f; int32_t i; } vx;
    vx.f = (float)x;
    int32_t exp = (vx.i >> 23) & 0xFF;
    float m = (float)(vx.i & 0x7FFFFF) * FAST_LOG2_MANT_NORM;
    return (float)(exp - 127) + (float)(m * (FAST_LOG2_A - FAST_LOG2_B * m));
}

/* 64-band subband energy analysis using a 64-point complex FFT.
 * Leverages conjugate symmetry to extract two 64-point real-subsequence
 * DFTs from one complex transform, reducing FLOPs by ~50% compared to
 * a standard 128-point implementation. Phase info is discarded as the
 * SBR bitstream only transmits envelope magnitudes. */
void SbrQmfAnalysis(SBRInfo *sbr, const float * restrict ovl_pos, float * restrict energy, int kx, int k2)
{
    float x[128], y[128];
    float * restrict xr = x, * restrict xi = x + 64;
    const float * restrict yr = y, * restrict yi = y + 64;
    const sbrfloat * restrict p0 = qmf_c;
    for (int m = 0; m < 64; m++) {
        int n0 = 2 * m;
        float a = p0[0]   * ovl_pos[639 - n0]
                    + p0[128] * ovl_pos[511 - n0]
                    + p0[256] * ovl_pos[383 - n0]
                    + p0[384] * ovl_pos[255 - n0]
                    + p0[512] * ovl_pos[127 - n0];
        float b = p0[1]   * ovl_pos[638 - n0]
                    + p0[129] * ovl_pos[510 - n0]
                    + p0[257] * ovl_pos[382 - n0]
                    + p0[385] * ovl_pos[254 - n0]
                    + p0[513] * ovl_pos[126 - n0];
        /* c[m] = (a + j*b) * exp(-j*pi*m/64) */
        xr[m] = a * sbr->twidCos[m] - b * sbr->twidSin[m];
        xi[m] = -(a * sbr->twidSin[m] + b * sbr->twidCos[m]);
        p0 += 2;
    }
    fft(x, y, FFT_LOGM_SHORT);
    for (int k = kx; k < k2; k++) {
        int kr = 63 - k;
        /* Separate the two real-subsequence DFTs by conjugate symmetry. */
        float Ar = 0.5f * (yr[k] + yr[kr]);
        float Ai = 0.5f * (yi[kr] - yi[k]);
        float Br = -0.5f * (yi[k] + yi[kr]);
        float Bi = 0.5f * (yr[kr] - yr[k]);
        /* Sr = Ar + w_k_real * Br - w_k_imag * Bi
         * Si = Ai + w_k_real * Bi + w_k_imag * Br */
        float wr = sbr->oddCos[k];
        float wi = sbr->oddSin[k];
        float Sr = Ar + wr * Br - wi * Bi;
        float Si = Ai + wr * Bi + wi * Br;
        energy[k] = Sr * Sr + Si * Si;
    }
}


static void sbr_adopt_envelope_grid(const SBRInfo *sbr, const struct SignalAnalysis *sa, SbrFrameData *fd)
{
    fd->numEnvelopes = sa->numEnvelopes;
    fd->frameClass   = sa->frameClass;
    fd->bsPointer    = sa->bsPointer;
    for (int i = 0; i <= sa->numEnvelopes; i++) fd->tEnv[i] = sa->tEnv[i];
    fd->eff_amp_res = (fd->numEnvelopes == 1) ? 0 : sbr->bs_amp_res;
    fd->freqRes = sbr->bs_freq_res;
}

static void sbr_quantize_envelopes(const SBRInfo *sbr, int nch, const bool *isLfe,
                                   const struct SignalAnalysis *sa, SbrFrameData *fd)
{
    int n_env = fd->numEnvelopes;
    /* Must match write_sbr_envelope's table, or the decoder desyncs. */
    int nb = sbr_env_bands(sbr, fd);
    const int *edges = sbr_env_edges(sbr, fd);

    for (int ch = 0; ch < nch; ch++) {
        if (isLfe[ch]) continue;
        /* Read-only alias; the quantizer never writes back through it. */
        const float (* restrict bandE)[SBR_QMF_BANDS_64] = sa->bandE[ch];
        int dlav = fd->eff_amp_res ? SBR_ENV_DELTA_LIMIT_HIRES : SBR_ENV_DELTA_LIMIT_LORES;
        for (int e = 0; e < n_env; e++) {
            int prevLevel = -1;
            for (int b = 0; b < nb; b++) {
                int k_lo = edges[b], k_hi = edges[b+1];
                /* Weight energy by the number of QMF slots per envelope to
                 * maintain normalized power levels across variable borders. */
                int e_slots = sa->envSampled[e];
                if (e_slots < 1) e_slots = 1;
                float E = 0;
                for (int k = k_lo; k < k_hi; k++) E += bandE[e][k];
                E /= (float)(e_slots * (k_hi - k_lo));
                float factor = fd->eff_amp_res ? 1.0f : 2.0f;
                int level = lrintf(factor * (fast_log2(E + SBR_LOG_ENERGY_FLOOR) - SBR_ENV_LEVEL_LOG2_OFFSET));
                int raw_level = clamp_int(level, 0, 127);
                if (prevLevel < 0) {
                    raw_level = clamp_int(raw_level, 0, fd->eff_amp_res ? 63 : 127);
                    fd->ch[ch].envData[e][b] = raw_level;
                    prevLevel = raw_level;
                } else {
                    int delta = clamp_int(raw_level - prevLevel, -dlav, dlav);
                    fd->ch[ch].envData[e][b] = delta;
                    prevLevel += delta;
                }
            }
        }
    }
}

void SbrEncode(SBRInfo *sbr, float *timeDomain[MAX_CHANNELS], int numChannels, const bool *isLfe, int numSamples, struct SignalAnalysis *sa, SbrFrameData *fd)
{
    for (int ch = 0; ch < numChannels; ch++)
        if (!isLfe[ch])
            memcpy(sbr->ch[ch].qmfOvl64, timeDomain[ch] + numSamples - SBR_QMF_HIST_LEN, SBR_QMF_HIST_LEN * sizeof(float));

    sbr_adopt_envelope_grid(sbr, sa, fd);
    sbr_quantize_envelopes(sbr, numChannels, isLfe, sa, fd);

#ifdef FAAC_STATS
    g_faacStats.sbrFrames++;
    if (fd->frameClass != SBR_FRAME_CLASS_FIXFIX) {
        g_faacStats.sbrTransientFrames++;
    }
    for (int ch = 0; ch < numChannels; ch++) {
        if (isLfe[ch]) continue;
        g_faacStats.sbrInvfSum += SBR_INVF_MODE;
        g_faacStats.sbrInvfCount++;
    }
#endif
}

/* SBR bitstream writer. Emits the SBR fill element payload into the bitstream.
 * Replays the write sequence into a counting sink during rate control to
 * ensure accurate bit budget allocation. */

