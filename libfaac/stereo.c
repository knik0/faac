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
/* Starved rates (bps per channel) from which long windows take real M/S
 * instead of intensity stereo; below them the side channel's bits cost more
 * quality than the image gains. Between the HE tiers only the low band takes
 * it. Short windows take it below MS_SPLIT_HZ only: across a group's shared
 * scalefactors the side channel's noise spreads ahead of the attack. */
#define MS_SPLIT_HE_LOW_BITRATE   20000
#define MS_SPLIT_HE_LOW_HZ        1000
#define MS_SPLIT_HE_HIGH_BITRATE  24000
#define MS_SPLIT_LC_BITRATE       32000
#define MS_SPLIT_HZ               2000

static int band_at(const int *sfbOffset, int sfbn, int hz, int n2, int sampleRate)
{
    int off = (hz * n2 + sampleRate - 1) / sampleRate, sfb = 0;
    while (sfb < sfbn && sfbOffset[sfb] < off) sfb++;
    return sfb;
}

void StereoConfigure(StereoConfig *cfg, JointMode mode, int sampleRate, unsigned int bandWidth,
                     unsigned long bitRatePerCh, int sbr, const int *sfbOffset[2], const int sfbn[2])
{
    int starved = bitRatePerCh && bitRatePerCh < IS_ONLY_BITRATE;

    /* Scale IS crossover with bandwidth (0.35 * bw, 3.5-7 kHz) to save phase bits. */
    int cap = (sampleRate * IS_FREQ_CAP_NUM) / IS_FREQ_CAP_DEN;
    int max_freq = min(IS_START_FREQ_MAX, cap);
    int ifreq = (max_freq < IS_START_FREQ_MIN) ? max_freq : clamp_int((int)((float)bandWidth * IS_BW_RATIO), IS_START_FREQ_MIN, max_freq);

    /* Highest M/S frequency per window type (0 long, 1 short); sampleRate
     * means every band. */
    int msHz[2] = {0, 0};
    if (mode == JOINT_MS) {
        msHz[0] = msHz[1] = sampleRate;
    } else if (mode == JOINT_MIXED) {
        if (!starved)
            msHz[0] = sampleRate;
        else if (sbr ? bitRatePerCh >= MS_SPLIT_HE_HIGH_BITRATE : bitRatePerCh >= MS_SPLIT_LC_BITRATE)
            msHz[0] = sampleRate, msHz[1] = MS_SPLIT_HZ;
        else if (sbr && bitRatePerCh >= MS_SPLIT_HE_LOW_BITRATE)
            msHz[0] = msHz[1] = MS_SPLIT_HE_LOW_HZ;
    }

    /* Each window type takes M/S below msEnd, intensity from isStart and plain
     * L/R between. Without M/S, intensity covers every band past the lowest
     * except on short windows from IS_SHORT_CROSSOVER_BITRATE, which keep L/R
     * below the crossover. */
    cfg->mode = mode;
    for (int w = 0; w < 2; w++) {
        int n = sfbn[w], n2 = 2 * (w ? BLOCK_LEN_SHORT : BLOCK_LEN_LONG);
        int ms = band_at(sfbOffset[w], n, msHz[w], n2, sampleRate);
        int is = ms ? ms : w ? 1 : 8;
        if (!ms && w && mode == JOINT_MIXED && bitRatePerCh >= IS_SHORT_CROSSOVER_BITRATE)
            is = band_at(sfbOffset[w], n, ifreq, n2, sampleRate);
        cfg->msEnd[w] = ms;
        cfg->isStart[w] = is;
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

/* M/S butterfly; the 0.5 keeps mid and side on the L/R scale. */
static inline void apply_ms_full(float * restrict sl0, float * restrict sr0,
                                 int start, int len, int wstart, int wend)
{
    for (int win = wstart; win < wend; win++) {
        float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
        float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
        /* Band widths are multiples of four, so no remainder loop. */
        for (int i = 0; i < len; i += 4) {
            for (int k = i; k < i + 4; k++) {
                float m = 0.5f * (sl[k] + sr[k]);
                float s = 0.5f * (sl[k] - sr[k]);
                sl[k] = m;
                sr[k] = s;
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
            for (i = 0; i < len; i += 4) {
                for (int k = i; k < i + 4; k++) {
                    sl[k] = (sl[k] + sr[k]) * vfix;
                    sr[k] = 0.0f;
                }
            }
        }
    } else {
        for (win = wstart; win < wend; win++) {
            float * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            float * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i += 4) {
                for (int k = i; k < i + 4; k++) {
                    sl[k] = (sl[k] - sr[k]) * vfix;
                    sr[k] = 0.0f;
                }
            }
        }
    }
}

/* One pass over a window group's bands. A band below ms_end takes M/S when it
 * saves bits: mid and side are each coded at half the L/R noise, since the
 * decoder adds both into each channel, so M/S wins when Em*Es < El*Er/4. From
 * is_start a band takes intensity stereo. With M/S in the group, its L/R
 * energy before any transform goes to refTotal[g]. */
static inline int process_cpe(CoderInfo * restrict cl, CoderInfo * restrict cr,
                               AACElement * restrict element,
                               float * restrict sl0, float * restrict sr0,
                               int * restrict sfcnt, int g, int wstart, int wend,
                               float inv_isthr, int is_start, int ms_end)
{
    int sfb, msused = 0;
    const int * restrict sfb_offset = cl->sfb_offset;
    int sfmin = ms_end ? 0 : is_start;
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
            if (es * ed < 4.0f * el * er) {
                apply_ms_full(sl0, sr0, start, len, wstart, wend);
                element->msInfo.ms_used[band] = 1;
                cl->msEl[band] = el;
                cr->msEl[band] = er;
                msused = 1;
#ifdef FAAC_STATS
                g_faacStats.msBands += 2;
#endif
            }
            continue;
        }
        if (sfb >= is_start && el > 0 && er > 0) {
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
    float isthr;

    switch (cfg->mode) {
        case JOINT_MIXED:
            isthr = 0.18f * inv_quality + 1.0f;
            break;
        case JOINT_IS:
            isthr = 0.18f * (inv_quality * inv_quality) + 1.0f;
            break;
        case JOINT_MS:
            isthr = 1.0f;
            break;
        default:
            return;
    }
    /* Pre-square the threshold so per-band energy comparisons need no sqrt.
     * It scales inversely with quality: higher quality encodes apply
     * intensity stereo more conservatively, touching the signal less. */
    if (isthr > M_SQRT2) isthr = M_SQRT2;
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
        int ms_end = cfg->msEnd[shortwin];
        int is_start = min(cfg->isStart[shortwin], coder[lch].sfbn);

        elem->common_window  = true;
        coder[lch].partner = &coder[rch];
        coder[lch].msUsed = elem->msInfo.ms_used;
        coder[lch].msPeer = coder[rch].msEl;
        coder[rch].msPeer = coder[lch].msEl;

        int start = 0, sfcnt = 0, msused = 0;
        for (int g = 0; g < coder[lch].groups.n; g++) {
            int end = start + coder[lch].groups.len[g];
            msused |= process_cpe(coder+lch, coder+rch, elem, s[lch], s[rch],
                                  &sfcnt, g, start, end, inv_isthr, is_start, ms_end);
            start = end;
        }
        elem->msInfo.is_present = msused;
        /* Mid and side levels would skew each channel's allocation; keep the
         * group reference on the L/R energies. */
        coder[lch].useRef = coder[rch].useRef = ms_end > 0;
    }
}
