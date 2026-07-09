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

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include "stereo.h"
#include "huff2.h"
#include "util.h"
#include "faac_internal.h"

/* Intensity stereo applies only at and above this frequency; below it the ear
 * localizes from waveform detail, so panning the band would be audible. */
#define IS_START_FREQ_HZ 5500
/* Upper bound on the crossover, as a fraction of the sample rate (0.35*Fs =
 * 0.7*Nyquist), so the band stays well inside the coded spectrum. */
#define IS_FREQ_CAP_NUM  7
#define IS_FREQ_CAP_DEN  20
/* Pan, in SF_STEP_ENRG steps, beyond which the quieter channel is inaudible
 * and is dropped to HCB_ZERO rather than intensity-coded. */
#define IS_PAN_LIMIT     30

/* Accumulate channel energies and cross-correlation for a scale factor band.
 * Using three independent accumulators maximizes instruction-level parallelism
 * by avoiding read-after-write dependencies on the FPU pipeline. */
static inline void calculate_energies(const float * restrict sl0, const float * restrict sr0,
                               int start, int len, int wstart, int wend,
                               float * restrict el_out, float * restrict er_out, float * restrict elr_out)
{
    float el = 0, er = 0, elr = 0;
    int win, i;

    for (win = wstart; win < wend; win++) {
        const float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
        const float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
        for (i = 0; i < len; i++) {
            float l = sl[i];
            float r = sr[i];
            el  += l * l;
            er  += r * r;
            elr += l * r;
        }
    }
    *el_out = el;
    *er_out = er;
    *elr_out = elr;
}

/* Fast memory-clearing utility for suppressed channels. */
static inline void apply_mute(float * restrict s0, int start, int len, int wstart, int wend)
{
    int win;
    for (win = wstart; win < wend; win++) {
        float * restrict s = s0 + win * BLOCK_LEN_SHORT + start;
        memset(s, 0, len * sizeof(float));
    }
}

/* When one component (mid or side) dominates, collapse both channels to that
 * component and zero the other — it costs no bits and the signal loss is masked.
 * Factor of 0.5 keeps the coded amplitude on the same scale as L/R. */
static inline void apply_ms(float * restrict sl0, float * restrict sr0,
                            int start, int len, int wstart, int wend, int in_phase)
{
    int win, i;
    if (in_phase) {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = 0.5f * (sl[i] + sr[i]);
                sr[i] = 0.0f;
            }
        }
    } else {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sr[i] = 0.5f * (sl[i] - sr[i]);
                sl[i] = 0.0f;
            }
        }
    }
}

static inline void apply_is(float * restrict sl0, float * restrict sr0,
                            int start, int len, int wstart, int wend, int in_phase, float vfix)
{
    int win, i;
    if (in_phase) {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] + sr[i]) * vfix;
                sr[i] = 0.0f;
            }
        }
    } else {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] - sr[i]) * vfix;
                sr[i] = 0.0f;
            }
        }
    }
}

/* Unified CPE element processing.  Consolidating joint stereo modes into a single
 * pass minimizes cache misses on spectral data and allows the compiler to
 * optimize the mode-specific branches using constant propagation. */
static inline int process_cpe(CoderInfo * restrict cl, CoderInfo * restrict cr,
                               AACElement * restrict element,
                               float * restrict sl0, float * restrict sr0,
                               int * restrict sfcnt, int wstart, int wend,
                               float thrmid, float inv_isthr, float thrside_sq,
                               int is_start_sfb, int mode)
{
    int sfb, sfmin = (cl->block_type == ONLY_SHORT_WINDOW) ? 1 : 8, msused = 0;
    const int * restrict sfb_offset = cl->sfb_offset;

    if (mode == JOINT_IS) {
        *sfcnt += sfmin;
    } else {
        for (sfb = 0; sfb < sfmin; sfb++)
            element->msInfo.ms_used[(*sfcnt)++] = 0;
    }

    for (sfb = sfmin; sfb < cl->sfbn; sfb++) {
        int start = sfb_offset[sfb], len = sfb_offset[sfb+1] - start;
        float el, er, elr;
        calculate_energies(sl0, sr0, start, len, wstart, wend, &el, &er, &elr);

        float es   = el + er + 2.0f*elr;
        float ed   = el + er - 2.0f*elr;
        float etot = el + er;
        if (es < 0) es = 0;
        if (ed < 0) ed = 0;
        if (etot <= 0) {
            if (mode != JOINT_IS) element->msInfo.ms_used[*sfcnt] = 0;
            (*sfcnt)++;
            continue;
        }

        /* Intensity Stereo check.  The threshold formula is expanded from
         * (sqrt(L)+sqrt(R))^2 to (L+R + 2*sqrt(L*R)) to eliminate one square
         * root per band while maintaining identical decision margins. */
        if ((mode == JOINT_IS || (mode == JOINT_MIXED && sfb >= is_start_sfb)) && el > 0 && er > 0) {
            float th = (el + er + 2.0f * sqrtf(el * er)) * inv_isthr;
            int hcb = (es >= th) ? HCB_INTENSITY : (ed >= th ? HCB_INTENSITY2 : HCB_NONE);
            if (hcb != HCB_NONE) {
                float inv_etot = 1.0f / etot;
                int sf  = lrintf(log10f(el * inv_etot) * SF_STEP_ENRG);
                int pan = lrintf(log10f(er * inv_etot) * SF_STEP_ENRG) - sf;
                /* Extreme pan: drop the inaudible channel to HCB_ZERO instead of
                 * intensity-coding it, keeping the band cheap for the quantizer. */
                if (pan > IS_PAN_LIMIT) {
                    cl->book[*sfcnt] = HCB_ZERO;
                    if (mode != JOINT_IS) element->msInfo.ms_used[*sfcnt] = 0;
                    (*sfcnt)++;
                    continue;
                }
                if (pan < -IS_PAN_LIMIT) {
                    cr->book[*sfcnt] = HCB_ZERO;
                    if (mode != JOINT_IS) element->msInfo.ms_used[*sfcnt] = 0;
                    (*sfcnt)++;
                    continue;
                }
                cl->sf[*sfcnt]   = sf;
                cr->sf[*sfcnt]   = -pan;
                cr->book[*sfcnt] = hcb;
                float dom = (hcb == HCB_INTENSITY) ? es : ed;
                apply_is(sl0, sr0, start, len, wstart, wend, hcb == HCB_INTENSITY, sqrtf(etot / dom));
                if (mode != JOINT_IS) element->msInfo.ms_used[*sfcnt] = 0;
                (*sfcnt)++;
                continue;
            }
        }

        /* Mid/Side Stereo and Muting checks */
        int ms = 0;
        if (mode == JOINT_MS || mode == JOINT_MIXED) {
            /* M/S fires when min(L,R) * thrmid ≥ dominant component: the weaker channel
             * contributes enough to justify the transform overhead. 0.25 accounts for halving. */
            float em = 0.25f * es, side = 0.25f * ed;
            if (min(el, er) * thrmid >= max(em, side)) {
                if (em * thrmid * 2.0f >= etot) {
                    ms = 1;
                    apply_ms(sl0, sr0, start, len, wstart, wend, 1);
                } else if (side * thrmid * 2.0f >= etot) {
                    ms = 1;
                    apply_ms(sl0, sr0, start, len, wstart, wend, 0);
                }
            }
            if (ms) {
                msused = 1;
            } else {
                /* Sparsity check: if one channel completely masks the other. */
                if (el <= er * thrside_sq) {
                    apply_mute(sl0, start, len, wstart, wend);
                } else if (er <= el * thrside_sq) {
                    apply_mute(sr0, start, len, wstart, wend);
                }
            }
            element->msInfo.ms_used[*sfcnt] = ms;
        }
        (*sfcnt)++;
    }
    return msused;
}

void AACstereo(CoderInfo *coder, AACElement *elements, int numElements, float *s[MAX_CHANNELS],
               float quality, int mode, int sampleRate)
{
    float inv_quality = 1.0f / quality;
    float thrmid = 1.0f, isthr = 1.0f, thrside = 0.0f;

    switch (mode) {
        case JOINT_MIXED:
            thrmid = (0.09f * 0.85f) * inv_quality;
            if (thrmid > 0.25f) thrmid = 0.25f;
            thrmid += 1.0f;
            isthr = 0.18f * inv_quality + 1.0f;
            if (isthr > M_SQRT2) isthr = M_SQRT2;
            thrside = 0.1f * inv_quality;
            if (thrside > 0.3f) thrside = 0.3f;
            break;
        case JOINT_MS:
            thrmid = (1.09f - 1.0f) * inv_quality;
            if (thrmid > 0.25f) thrmid = 0.25f;
            thrmid += 1.0f;
            thrside = 0.1f * inv_quality;
            if (thrside > 0.3f) thrside = 0.3f;
            break;
        case JOINT_IS:
            isthr = 0.18f * (inv_quality * inv_quality);
            isthr += 1.0f;
            if (isthr > M_SQRT2) isthr = M_SQRT2;
            break;
        default:
            return;
    }
    /* Pre-square thresholds so per-band energy comparisons need no sqrt.
     * Each threshold scales inversely with quality — higher quality encodes
     * apply stereo coding more conservatively, touching the signal less. */
    thrmid *= thrmid;
    float inv_isthr = 1.0f / (isthr * isthr);
    float thrside_sq = thrside * thrside;

    for (int e = 0; e < numElements; e++) {
        AACElement *elem = &elements[e];
        if (elem->type != ID_CPE) continue;

        int lch = elem->channels[0];
        int rch = elem->channels[1];

        elem->common_window = false;
        elem->msInfo.is_present = false;

        if (coder[lch].block_type != coder[rch].block_type || coder[lch].groups.n != coder[rch].groups.n)
            continue;

        int ok = 1;
        for (int g = 0; g < coder[lch].groups.n; g++) {
            if (coder[lch].groups.len[g] != coder[rch].groups.len[g]) {
                ok = 0; break;
            }
        }
        if (!ok) continue;

        elem->common_window  = true;
        elem->msInfo.is_present = (mode == JOINT_MS);

        int start = 0, sfcnt = 0, is_start_sfb = coder[lch].sfbn, msused = 0;
        if (mode == JOINT_MIXED) {
            int mlen  = (coder[lch].block_type == ONLY_SHORT_WINDOW) ? 2*BLOCK_LEN_SHORT : 2*BLOCK_LEN_LONG;
            int ifreq = IS_START_FREQ_HZ, cap = (sampleRate * IS_FREQ_CAP_NUM) / IS_FREQ_CAP_DEN;
            if (ifreq > cap) ifreq = cap;
            for (int sfb = 0; sfb < coder[lch].sfbn; sfb++) {
                if ((coder[lch].sfb_offset[sfb] * sampleRate) / mlen >= ifreq) {
                    is_start_sfb = sfb; break;
                }
            }
        }

        for (int g = 0; g < coder[lch].groups.n; g++) {
            int end = start + coder[lch].groups.len[g];
            msused |= process_cpe(coder+lch, coder+rch, elem, s[lch], s[rch],
                                  &sfcnt, start, end, thrmid, inv_isthr, thrside_sq,
                                  is_start_sfb, mode);
            start = end;
        }
        if (mode == JOINT_MIXED && msused) elem->msInfo.is_present = true;
    }
}
