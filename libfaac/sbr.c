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

/* SBR stop frequency (k2). Bark-scale distribution maximizes bit efficiency. */
static int compute_k2(int sampleRate, int kx, int bs_stop_freq)
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

    int max_span = (sampleRate <= 32000) ? 48 : (sampleRate <= 44100) ? 35 : 32;
    return clamp_int(k2, kx + 1, kx + max_span > 64 ? 64 : kx + max_span);
}

/* Distribute QMF bands into SBR master bands using uniform dk-spacing.
 * Residual bands are merged into the first/last pairs to maintain a
 * monotonic frequency grid. */
static int build_freq_table(SBRInfo *sbr)
{
    int kx = sbr->kx, k2 = sbr->k2, dk = sbr->dk;
    int n_master = clamp_int(((k2 - kx + (dk & 2)) >> dk) << 1, 1, SBR_MAX_BANDS);
    int f_master[SBR_MAX_BANDS + 1];
    for (int k = 1; k <= n_master; k++) f_master[k] = dk;
    int k2diff = (k2 - kx) - n_master * dk;
    if (k2diff < 0) {
        f_master[1]--;
        if (k2diff < -1) f_master[2]--;
    } else if (k2diff > 0) f_master[n_master]++;
    f_master[0] = kx;
    for (int k = 1; k <= n_master; k++) f_master[k] += f_master[k - 1];
    sbr->numBands = n_master;
    for (int b = 0; b <= n_master; b++) sbr->bandEdges[b] = f_master[b];
    sbr->numNoiseBands = 1;
    return n_master;
}

SBRInfo *SbrInit(int channels, int sampleRate, unsigned long bitRate, FFT_Tables *fft_tables)
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
    /* Borrow the encoder's shared core FFT tables (same fft() routine, same
     * logm=6 size as the short-block MDCT). The core owns init/terminate; the
     * logm=6 table is built lazily on first use, single-threaded per encoder. */
    sbr->fftTables = fft_tables;

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
    if (rate_per_ch <= SBR_COARSE_TABLE_BITRATE_BPS) {
        sbr->bs_start_freq = 15;
        sbr->bs_alter_scale = 1;
        sbr->dk = 2;
    } else {
        sbr->bs_start_freq = 15;
        sbr->bs_alter_scale = 0;
        sbr->dk = 1;
    }
    /* Stop frequency covers approximately 75% of the upper octave. */
    sbr->bs_stop_freq = 10;
    sbr->bs_freq_res = 1; /* HIGH resolution */
    sbr->bs_xover_band = 0; /* every master band is an SBR band; no low-res split */
    sbr->numEnvelopes = 1;
    sbr->eff_amp_res = (sbr->numEnvelopes == 1) ? 0 : sbr->bs_amp_res;
    sbr->kx = compute_kx(sampleRate, sbr->bs_start_freq);
    sbr->k2 = compute_k2(sampleRate, sbr->kx, sbr->bs_stop_freq);

    build_freq_table(sbr);
}

void SbrEnd(SBRInfo *sbr)
{
    if (!sbr) return;
    /* fftTables is borrowed from the encoder; the core terminates it. */
    FreeMemory(sbr);
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
     * Fs/2 (dual-rate SBR); the extension declares the full output rate. */
    *pSize = 5;
    *ppBuffer = (unsigned char *)malloc(5);
    if (*ppBuffer == NULL) return -3;
    memset(*ppBuffer, 0, 5);
    BitStream *pBitStream = OpenBitStream(5, *ppBuffer);
    PutBit(pBitStream, LOW,                         5);  /* core object type */
    PutBit(pBitStream, coreSRIdx,                   4);  /* core rate (Fs/2, dual-rate) */
    PutBit(pBitStream, channels,                     4);
    PutBit(pBitStream, 0, 1);                            /* frameLengthFlag */
    PutBit(pBitStream, 0, 1);                            /* dependsOnCoreCoder */
    PutBit(pBitStream, 0, 1);                            /* extensionFlag */
    PutBit(pBitStream, 0x2b7,                      11);  /* syncExtensionType */
    PutBit(pBitStream, HE_V1,                       5);  /* extObjectType = SBR */
    PutBit(pBitStream, 1,                           1);  /* sbrPresentFlag */
    PutBit(pBitStream, sbrCtx->fullSampleRateIdx, 4);   /* SBR output rate (2*core) */
    CloseBitStream(pBitStream);
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

void SbrContextUpdateConfig(SBRContext *sCtx, int channels, unsigned long bitrate, FFT_Tables *fft_tables)
{
    if (!sCtx) return;
    if (!sCtx->sbrInfo)
        sCtx->sbrInfo = SbrInit(channels, sCtx->fullSampleRate, bitrate, fft_tables);
    else
        SbrUpdate(sCtx->sbrInfo, bitrate);
}

void SbrContextProcessFrame(SBRContext *sCtx, int numChannels, int realPerCh, float *inputFifo[MAX_CHANNELS], float *heHalfRate[MAX_CHANNELS])
{
    unsigned int channel;
    Resampler *rs = sCtx->resampler;
    float *fullPtrs[MAX_CHANNELS];

    for (channel = 0; channel < (unsigned int)numChannels; channel++) {
        float *fullRate = rs->fullRate[channel];
        fullPtrs[channel] = fullRate;
        memcpy(fullRate, inputFifo[channel], realPerCh * sizeof(float));
        /* Final partial frame: silence-pad the unfilled full-rate tail to
         * prevent the resampler from consuming stale data. SbrEncode reads
         * only [0, realPerCh), so it is unaffected. */
        if (realPerCh < 2 * FRAME_LEN)
            memset(fullRate + realPerCh, 0, (2 * FRAME_LEN - realPerCh) * sizeof(float));
        heHalfRate[channel] = rs->halfRate[channel];
    }

    /* Shared signal analysis. */
    SbrAnalyze(&sCtx->signalAnalysis, fullPtrs, numChannels, realPerCh, sCtx->sbrInfo);

    /* Update the transient FIFO. Shift down by one and push
     * the newest decision at SBR_DETECT_FIFO-1; index 0 stays aligned with the
     * core frame being coded (LOOKAHEAD_DEPTH frames behind this analysis). */
    for (channel = 0; channel < (unsigned int)numChannels; channel++) {
        memmove(&sCtx->transientStrengthFIFO[channel][0], &sCtx->transientStrengthFIFO[channel][1], (SBR_DETECT_FIFO - 1) * sizeof(float));
        sCtx->transientStrengthFIFO[channel][SBR_DETECT_FIFO - 1] = sCtx->signalAnalysis.ch[channel].transientStrength;
        memmove(&sCtx->wantShortFIFO[channel][0], &sCtx->wantShortFIFO[channel][1], (SBR_DETECT_FIFO - 1) * sizeof(int));
        sCtx->wantShortFIFO[channel][SBR_DETECT_FIFO - 1] = sCtx->signalAnalysis.ch[channel].wantShort;
    }

    SbrEncode(sCtx->sbrInfo, fullPtrs, numChannels, realPerCh, &sCtx->signalAnalysis);

    /* Dual-rate decimation: produces the halved-rate core signal. */
    Resample(rs, 2 * FRAME_LEN);
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

int SbrContextIsAnalysisValid(SBRContext *sCtx)
{
    return sCtx ? sCtx->signalAnalysis.valid : 0;
}

int SbrContextGetWantShort(SBRContext *sCtx, int channel, int index)
{
    if (sCtx && channel < MAX_CHANNELS && index < SBR_DETECT_FIFO) {
        return sCtx->wantShortFIFO[channel][index];
    }
    return 0;
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
#if defined(__GNUC__)
__attribute__((hot))
#endif
void SbrQmfAnalysis(SBRInfo *sbr, const float * restrict ovl_pos, float * restrict energy, int kx, int k2)
{
    float xr[64], xi[64];
    const sbrfloat * restrict p0 = qmf_c;
    const sbrfloat * restrict p1 = qmf_c + 1;
    for (int m = 0; m < 64; m++) {
        int n0 = 2 * m;
        float a = p0[0]   * ovl_pos[639 - n0]
                    + p0[128] * ovl_pos[511 - n0]
                    + p0[256] * ovl_pos[383 - n0]
                    + p0[384] * ovl_pos[255 - n0]
                    + p0[512] * ovl_pos[127 - n0];
        float b = p1[0]   * ovl_pos[638 - n0]
                    + p1[128] * ovl_pos[510 - n0]
                    + p1[256] * ovl_pos[382 - n0]
                    + p1[384] * ovl_pos[254 - n0]
                    + p1[512] * ovl_pos[126 - n0];
        /* c[m] = (a + j*b) * exp(-j*pi*m/64) */
        xr[m] = a * sbr->twidCos[m] - b * sbr->twidSin[m];
        xi[m] = -(a * sbr->twidSin[m] + b * sbr->twidCos[m]);
        p0 += 2; p1 += 2;
    }
    fft(sbr->fftTables, xr, xi, 6);
    for (int k = kx; k < k2; k++) {
        int kr = 63 - k;
        /* Separate the two real-subsequence DFTs by conjugate symmetry. */
        float Ar = 0.5f * (xr[k] + xr[kr]);
        float Ai = 0.5f * (xi[kr] - xi[k]);
        float Br = -0.5f * (xi[k] + xi[kr]);
        float Bi = 0.5f * (xr[kr] - xr[k]);
        /* Sr = Ar + w_k_real * Br - w_k_imag * Bi
         * Si = Ai + w_k_real * Bi + w_k_imag * Br */
        float wr = sbr->oddCos[k];
        float wi = sbr->oddSin[k];
        float Sr = Ar + wr * Br - wi * Bi;
        float Si = Ai + wr * Bi + wi * Br;
        energy[k] = Sr * Sr + Si * Si;
    }
}


static void sbr_adopt_envelope_grid(SBRInfo *sbr, struct SignalAnalysis *sa)
{
    sbr->numEnvelopes = sa->numEnvelopes;
    sbr->frameClass   = sa->frameClass;
    sbr->bsPointer    = sa->bsPointer;
    for (int i = 0; i <= sa->numEnvelopes; i++) sbr->tEnv[i] = sa->tEnv[i];
    sbr->eff_amp_res = (sbr->numEnvelopes == 1) ? 0 : sbr->bs_amp_res;
}

static void sbr_quantize_envelopes(SBRInfo *sbr, int nch, int sampled,
                                   struct SignalAnalysis *sa,
                                   float bandHalfE[2][2][SBR_QMF_BANDS_64])
{
    int n_env = sbr->numEnvelopes;

    for (int ch = 0; ch < nch; ch++) {
        int noise_level = SBR_NOISE_LEVEL_DEFAULT;
        sbr->ch[ch].invfMode = 3;

        int dlav = sbr->eff_amp_res ? SBR_ENV_DELTA_LIMIT_HIRES : SBR_ENV_DELTA_LIMIT_LORES;
        for (int e = 0; e < n_env; e++) {
            int prevLevel = -1;
            for (int b = 0; b < sbr->numBands; b++) {
                int k_lo = sbr->bandEdges[b], k_hi = sbr->bandEdges[b+1];
                /* Weight energy by the number of QMF slots per envelope to
                 * maintain normalized power levels across variable borders. */
                int e_slots = (n_env == 1) ? sampled : sa->envSampled[e];
                if (e_slots < 1) e_slots = 1;
                float E = 0;
                if (n_env == 1) {
                    for (int k = k_lo; k < k_hi; k++) E += bandHalfE[ch][0][k] + bandHalfE[ch][1][k];
                } else {
                    for (int k = k_lo; k < k_hi; k++) E += bandHalfE[ch][e][k];
                }
                E /= (float)(e_slots * (k_hi - k_lo));
                float factor = sbr->eff_amp_res ? 1.0f : 2.0f;
                int level = lrintf(factor * (fast_log2(E + SBR_LOG_ENERGY_FLOOR) - SBR_ENV_LEVEL_LOG2_OFFSET));
                int raw_level = clamp_int(level, 0, 127);
                if (prevLevel < 0) {
                    raw_level = clamp_int(raw_level, 0, sbr->eff_amp_res ? 63 : 127);
                    sbr->ch[ch].envData[e][b] = raw_level;
                    prevLevel = raw_level;
                } else {
                    int delta = clamp_int(raw_level - prevLevel, -dlav, dlav);
                    sbr->ch[ch].envData[e][b] = delta;
                    prevLevel += delta;
                }
            }
        }
        int n_q = n_env > 1 ? 2 : 1;
        for (int ne = 0; ne < n_q; ne++) {
            int prevNoise = -1;
            for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
                if (prevNoise < 0) {
                    sbr->ch[ch].noiseData[ne][nb] = noise_level;
                    prevNoise = noise_level;
                } else {
                    int delta = clamp_int(noise_level - prevNoise, -15, 15);
                    sbr->ch[ch].noiseData[ne][nb] = delta; prevNoise += delta;
                }
            }
        }
    }
}

void SbrEncode(SBRInfo *sbr, float *timeDomain[MAX_CHANNELS], int numChannels, int numSamples, struct SignalAnalysis *sa)
{
    int nch = clamp_int(numChannels, 1, 2);
    float bandHalfE[2][2][SBR_QMF_BANDS_64];

    /* New frame: freeze the header-send decision now, before SbrWrite's write
     * pass (later, in the bitstream stage) mutates headerSent/frameCount. */
    sbr->sendHeaderThisFrame = (!sbr->headerSent || (sbr->frameCount % SBR_HEADER_PERIOD == 0));

    for (int ch = 0; ch < nch; ch++) {
        /* Use shared transient strength and accumulated energies from SbrAnalyze. */
        memcpy(bandHalfE[ch][0], sa->ch[ch].bandHalfE[0], SBR_QMF_BANDS_64 * sizeof(float));
        memcpy(bandHalfE[ch][1], sa->ch[ch].bandHalfE[1], SBR_QMF_BANDS_64 * sizeof(float));
        memcpy(sbr->ch[ch].qmfOvl64, timeDomain[ch] + numSamples - SBR_QMF_OVL_LEN_64, SBR_QMF_OVL_LEN_64 * sizeof(float));
    }

    sbr_adopt_envelope_grid(sbr, sa);
    sbr_quantize_envelopes(sbr, nch, sa->sampled, sa, bandHalfE);
}

/* SBR bitstream writer. Emits the SBR fill element payload into the bitstream.
 * Replays the write sequence into a counting sink during rate control to
 * ensure accurate bit budget allocation. */

