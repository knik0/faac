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
#include "stats.h"

/* Intensity stereo crossover scales with core bandwidth (3.5-7 kHz) to save low-band phase bits at low rates. */
#define IS_BW_RATIO              0.35f
#define IS_START_FREQ_MIN        3500
#define IS_START_FREQ_MAX        7000
/* Upper bound on crossover (0.35*Fs) so IS stays well inside coded spectrum. */
#define IS_FREQ_CAP_NUM  7
#define IS_FREQ_CAP_DEN  20
/* Pan, in SF_STEP_ENRG steps, beyond which the quieter channel is inaudible
 * and is dropped to HCB_ZERO rather than intensity-coded. */
#define IS_PAN_LIMIT     30
/* Below this rate (bps per channel) a long window takes intensity stereo
 * like a short one: M/S would pay a mask plus a side channel's scalefactors
 * and sections for spectrum that quantizes to almost nothing. Gated on the
 * configured rate like PSY_SHORT_ONLY_BITRATE; the running quality gates
 * fewer frames and gains less. */
#define IS_ONLY_BITRATE  40000
/* From this rate (bps per channel) a short window keeps L/R below the
 * intensity crossover like a long one; below it the bits that full-band
 * intensity stereo saves still buy more quality than the image they cost. */
#define IS_SHORT_CROSSOVER_BITRATE  80000
/* Starved rates (bps per channel) from which the low band takes real M/S
 * instead of intensity stereo, and the split frequency each gets. Below
 * these the side channel's bits cost more quality than the image gains; the
 * LC core, with no SBR band to hand the treble to, reaches that point later. */
#define MS_SPLIT_HE_LOW_BITRATE   20000
#define MS_SPLIT_HE_LOW_HZ        1000
#define MS_SPLIT_HE_HIGH_BITRATE  24000
#define MS_SPLIT_LC_BITRATE       32000
#define MS_SPLIT_HZ               2000
/* A band takes M/S when its weaker of mid/side is below this share of its
 * weaker channel. */
#define MS_SPLIT_SIDE_RATIO    0.2f

void StereoConfigure(StereoConfig *cfg, JointMode mode, int sampleRate, unsigned int bandWidth,
                     unsigned long bitRatePerCh, int sbr, const int *sfbOffset[2], const int sfbn[2])
{
    /* Grouped short windows share one scalefactor set, so M/S spreads the
     * side channel's quantization noise across the group and ahead of the
     * attack; in mixed mode they never take M/S (see process_cpe). Below
     * IS_ONLY_BITRATE both window types intensity-code every band. */
    int starved = bitRatePerCh && bitRatePerCh < IS_ONLY_BITRATE;
    cfg->mode     = mode;
    cfg->modes[0] = (mode == JOINT_MIXED && starved) ? JOINT_IS : mode;
    cfg->modes[1] = (mode == JOINT_MIXED && (starved || bitRatePerCh < IS_SHORT_CROSSOVER_BITRATE)) ? JOINT_IS : mode;

    /* Scale IS crossover with bandwidth (0.35 * bw, 3.5-7 kHz) to save phase bits. */
    int cap = (sampleRate * IS_FREQ_CAP_NUM) / IS_FREQ_CAP_DEN;
    int max_freq = min(IS_START_FREQ_MAX, cap);
    int ifreq = (max_freq < IS_START_FREQ_MIN) ? max_freq : clamp_int((int)((float)bandWidth * IS_BW_RATIO), IS_START_FREQ_MIN, max_freq);

    int msHz = 0;
    if (mode == JOINT_MIXED && starved) {
        if (sbr)
            msHz = bitRatePerCh >= MS_SPLIT_HE_HIGH_BITRATE ? MS_SPLIT_HZ
                 : bitRatePerCh >= MS_SPLIT_HE_LOW_BITRATE ? MS_SPLIT_HE_LOW_HZ : 0;
        else if (bitRatePerCh >= MS_SPLIT_LC_BITRATE)
            msHz = MS_SPLIT_HZ;
    }

    /* First band at or above the crossover (and below the split) in each
     * window type's sfb table. msHz is always below ifreq, so one forward
     * scan finds both: msEnd first, then isStart from there. */
    for (int w = 0; w < 2; w++) {
        int n2 = 2 * (w ? BLOCK_LEN_SHORT : BLOCK_LEN_LONG);
        int isOff = (ifreq * n2 + sampleRate - 1) / sampleRate, msOff = msHz * n2 / sampleRate;
        int sfb = 0;
        while (sfb < sfbn[w] && sfbOffset[w][sfb] < msOff) sfb++;
        cfg->msEnd[w] = sfb;
        while (sfb < sfbn[w] && sfbOffset[w][sfb] < isOff) sfb++;
        cfg->isStart[w] = sfb;
    }
}

/* Accumulate channel energies and cross-correlation for a scale factor band.
 * Using three independent accumulators maximizes instruction-level parallelism
 * by avoiding read-after-write dependencies on the FPU pipeline. */
static inline void calculate_energies(const float * restrict sl0, const float * restrict sr0,
                               int start, int len, int wstart, int wend,
                               float * restrict el_out, float * restrict er_out, float * restrict elr_out)
{
    float el = 0.0f, er = 0.0f, elr = 0.0f;
    int win, i;

    for (win = wstart; win < wend; win++) {
        const float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
        const float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
        /* Four at a time; band widths are all multiples of four. */
        for (i = 0; i < len; i += 4) {
            float l0 = sl[i],     r0 = sr[i];
            float l1 = sl[i + 1], r1 = sr[i + 1];
            float l2 = sl[i + 2], r2 = sr[i + 2];
            float l3 = sl[i + 3], r3 = sr[i + 3];

            el  += l0 * l0; el  += l1 * l1; el  += l2 * l2; el  += l3 * l3;
            er  += r0 * r0; er  += r1 * r1; er  += r2 * r2; er  += r3 * r3;
            elr += l0 * r0; elr += l1 * r1; elr += l2 * r2; elr += l3 * r3;
        }
    }
    *el_out = el;
    *er_out = er;
    *elr_out = elr;
}

/* Real M/S butterfly: both mid and side are kept. Used only for the
 * starved-rate low-band split, where the side channel is coded rather
 * than dropped. */
static inline void apply_ms_full(float * restrict sl0, float * restrict sr0,
                                 int start, int len, int wstart, int wend)
{
    for (int win = wstart; win < wend; win++) {
        float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
        float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
        for (int i = 0; i < len; i++) {
            float m = 0.5f * (sl[i] + sr[i]);
            float s = 0.5f * (sl[i] - sr[i]);
            sl[i] = m;
            sr[i] = s;
        }
    }
}

/* When one component (mid or side) dominates, collapse both channels to that
 * component and zero the other — it costs no bits and the signal loss is masked.
 * Factor of 0.5 keeps the coded amplitude on the same scale as L/R. */
static inline void apply_ms(float * restrict sl0, float * restrict sr0,
                            int start, int len, int wstart, int wend, int in_phase)
{
    int win, i;
    size_t bytes = (size_t)len * sizeof(float);

    if (in_phase) {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++)
                sl[i] = 0.5f * (sl[i] + sr[i]);
            memset(sr, 0, bytes);
        }
    } else {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++)
                sr[i] = 0.5f * (sl[i] - sr[i]);
            memset(sl, 0, bytes);
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

/* One pass over a window group's bands. With an M/S split (ms_end > 0, starved
 * JOINT_IS), a band below ms_end whose weaker of mid/side is small takes a real
 * M/S butterfly: intensity folds the image, M/S keeps it, and in the low band
 * the side channel is cheap. The group's L/R energy before any transform goes
 * to refTotal[g]. */
static inline int process_cpe(CoderInfo * restrict cl, CoderInfo * restrict cr,
                               AACElement * restrict element,
                               float * restrict sl0, float * restrict sr0,
                               int * restrict sfcnt, int g, int wstart, int wend,
                               float thrmid, float inv_isthr,
                               int is_start_sfb, int ms_end, JointMode mode, int allow_ms)
{
    int sfb, msused = 0;
    const int * restrict sfb_offset = cl->sfb_offset;
    int sfmin = ms_end ? 0 : (cl->block_type == ONLY_SHORT_WINDOW) ? 1 : 8;
    float tl = 0.0f, tr = 0.0f;

    for (sfb = 0; sfb < sfmin; sfb++)
        element->msInfo.ms_used[(*sfcnt)++] = 0;

    for (; sfb < cl->sfbn; sfb++, (*sfcnt)++) {
        int band = *sfcnt;
        int start = sfb_offset[sfb], len = sfb_offset[sfb+1] - start;
        float el, er, elr;
        calculate_energies(sl0, sr0, start, len, wstart, wend, &el, &er, &elr);
        tl += el;
        tr += er;
        element->msInfo.ms_used[band] = 0;

        float es   = el + er + 2.0f*elr;
        float ed   = el + er - 2.0f*elr;
        float etot = el + er;
        if (es < 0) es = 0;
        if (ed < 0) ed = 0;
        if (etot <= 0) continue;

        if (sfb < ms_end) {
            /* es and ed are 4x the mid and side energies. */
            if (0.25f * min(es, ed) < MS_SPLIT_SIDE_RATIO * min(el, er)) {
                apply_ms_full(sl0, sr0, start, len, wstart, wend);
                element->msInfo.ms_used[band] = 1;
                msused = 1;
                /* Decoders skip M/S on a noise band, so these bands can't be PNS. */
                cl->noPns[band] = cr->noPns[band] = 1;
#ifdef FAAC_STATS
                g_faacStats.msBands += 2;
#endif
            }
            continue;
        }
        if ((mode == JOINT_IS || (mode == JOINT_MIXED && sfb >= is_start_sfb)) && el > 0 && er > 0) {
            /* The threshold (sqrt(L)+sqrt(R))^2 is expanded to L+R+2*sqrt(L*R)
             * to save a square root. */
            float th = (el + er + 2.0f * sqrtf(el * er)) * inv_isthr;
            int hcb = (es >= th) ? HCB_INTENSITY : (ed >= th ? HCB_INTENSITY2 : HCB_NONE);
            if (hcb != HCB_NONE) {
                float inv_etot = 1.0f / etot;
                int sf  = lrintf(log10f(el * inv_etot) * SF_STEP_ENRG);
                int pan = lrintf(log10f(er * inv_etot) * SF_STEP_ENRG) - sf;
                /* Extreme pan: drop the inaudible channel to HCB_ZERO instead of
                 * intensity-coding it, keeping the band cheap for the quantizer. */
                if (pan > IS_PAN_LIMIT) {
                    cl->book[band] = HCB_ZERO;
                    continue;
                }
                if (pan < -IS_PAN_LIMIT) {
                    cr->book[band] = HCB_ZERO;
                    continue;
                }
                cl->sf[band]   = sf;
                cr->sf[band]   = -pan;
                cr->book[band] = hcb;
#ifdef FAAC_STATS
                g_faacStats.isBands += 2;
#endif
                float dom = (hcb == HCB_INTENSITY) ? es : ed;
                apply_is(sl0, sr0, start, len, wstart, wend, hcb == HCB_INTENSITY, sqrtf(etot / dom));
                continue;
            }
        }

        if (allow_ms && (mode == JOINT_MS || mode == JOINT_MIXED)) {
            /* M/S fires when min(L,R) * thrmid ≥ dominant component: the weaker channel
             * contributes enough to justify the transform overhead. 0.25 accounts for halving. */
            float em = 0.25f * es, side = 0.25f * ed;
            if (min(el, er) * thrmid >= max(em, side)) {
                int ms = -1;
                if (em * thrmid * 2.0f >= etot)
                    ms = 1;
                else if (side * thrmid * 2.0f >= etot)
                    ms = 0;
                if (ms >= 0) {
                    apply_ms(sl0, sr0, start, len, wstart, wend, ms);
                    element->msInfo.ms_used[band] = 1;
                    msused = 1;
#ifdef FAAC_STATS
                    g_faacStats.msBands += 2;
#endif
                }
            }
        }
    }
    cl->refTotal[g] = tl;
    cr->refTotal[g] = tr;
    return msused;
}

void AACstereo(CoderInfo *coder, AACElement *elements, int numElements, float *s[MAX_CHANNELS],
               float quality, const StereoConfig *cfg)
{
    float inv_quality = 1.0f / quality;
    float thrmid = 1.0f, isthr = 1.0f;

    switch (cfg->mode) {
        case JOINT_MIXED:
            thrmid = (0.09f * 0.85f) * inv_quality;
            if (thrmid > 0.25f) thrmid = 0.25f;
            thrmid += 1.0f;
            isthr = 0.18f * inv_quality + 1.0f;
            if (isthr > M_SQRT2) isthr = M_SQRT2;
            break;
        case JOINT_MS:
            thrmid = (1.09f - 1.0f) * inv_quality;
            if (thrmid > 0.25f) thrmid = 0.25f;
            thrmid += 1.0f;
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

        int shortwin = coder[lch].block_type == ONLY_SHORT_WINDOW;
        JointMode cur_mode = cfg->modes[shortwin];

        elem->common_window  = true;
        coder[lch].partner = &coder[rch];
        elem->msInfo.is_present = (cur_mode == JOINT_MS);

        int start = 0, sfcnt = 0, msused = 0;
        int ms_end = cfg->msEnd[shortwin];
        int is_start_sfb = cfg->isStart[shortwin];
        if (is_start_sfb > coder[lch].sfbn) is_start_sfb = coder[lch].sfbn;

        /* Mixed mode never M/S-codes a short window: the shared scalefactor
         * set would spread the side channel's noise ahead of the attack. */
        int allow_ms = !(shortwin && cfg->mode == JOINT_MIXED);
        for (int g = 0; g < coder[lch].groups.n; g++) {
            int end = start + coder[lch].groups.len[g];
            msused |= process_cpe(coder+lch, coder+rch, elem, s[lch], s[rch],
                                  &sfcnt, g, start, end, thrmid, inv_isthr,
                                  is_start_sfb, ms_end, cur_mode, allow_ms);
            start = end;
        }
        elem->msInfo.is_present |= msused;
        /* Mid and side levels would skew each channel's allocation; keep the
         * split's group reference on the L/R energies. */
        coder[lch].useRef = coder[rch].useRef = ms_end > 0;
    }
}
