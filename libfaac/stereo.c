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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include "stereo.h"
#include "huff2.h"
#include "util.h"

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
static inline void calculate_energies(const faac_real * restrict sl0, const faac_real * restrict sr0,
                               int start, int len, int wstart, int wend,
                               faac_real * restrict el_out, faac_real * restrict er_out, faac_real * restrict elr_out)
{
    faac_real el = 0, er = 0, elr = 0;
    int win, i;

    for (win = wstart; win < wend; win++) {
        const faac_real * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
        const faac_real * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
        for (i = 0; i < len; i++) {
            faac_real l = sl[i];
            faac_real r = sr[i];
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
static inline void apply_mute(faac_real * restrict s0, int start, int len, int wstart, int wend)
{
    int win;
    for (win = wstart; win < wend; win++) {
        faac_real * restrict s = s0 + win * BLOCK_LEN_SHORT + start;
        memset(s, 0, len * sizeof(faac_real));
    }
}

/* When one component (mid or side) dominates, collapse both channels to that
 * component and zero the other — it costs no bits and the signal loss is masked.
 * Factor of 0.5 keeps the coded amplitude on the same scale as L/R. */
static inline void apply_ms(faac_real * restrict sl0, faac_real * restrict sr0,
                            int start, int len, int wstart, int wend, int in_phase)
{
    int win, i;
    if (in_phase) {
        for (win = wstart; win < wend; win++) {
            faac_real * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            faac_real * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = 0.5 * (sl[i] + sr[i]);
                sr[i] = 0.0;
            }
        }
    } else {
        for (win = wstart; win < wend; win++) {
            faac_real * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            faac_real * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sr[i] = 0.5 * (sl[i] - sr[i]);
                sl[i] = 0.0;
            }
        }
    }
}

static inline void apply_is(faac_real * restrict sl0, faac_real * restrict sr0,
                            int start, int len, int wstart, int wend, int in_phase, faac_real vfix)
{
    int win, i;
    if (in_phase) {
        for (win = wstart; win < wend; win++) {
            faac_real * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            faac_real * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] + sr[i]) * vfix;
                sr[i] = 0.0;
            }
        }
    } else {
        for (win = wstart; win < wend; win++) {
            faac_real * restrict sl = sl0 + win * BLOCK_LEN_SHORT + start;
            faac_real * restrict sr = sr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] - sr[i]) * vfix;
                sr[i] = 0.0;
            }
        }
    }
}


/* Unified CPE element processing.  Consolidating joint stereo modes into a single
 * pass minimizes cache misses on spectral data and allows the compiler to
 * optimize the mode-specific branches using constant propagation. */
static inline int process_cpe(CoderInfo * restrict cl, CoderInfo * restrict cr,
                               ChannelInfo * restrict channel,
                               faac_real * restrict sl0, faac_real * restrict sr0,
                               int * restrict sfcnt, int wstart, int wend,
                               faac_real thrmid, faac_real inv_isthr, faac_real thrside_sq,
                               int is_start_sfb, int mode)
{
    int sfb, sfmin = (cl->block_type == ONLY_SHORT_WINDOW) ? 1 : 8, msused = 0;
    const int * restrict sfb_offset = cl->sfb_offset;

    if (mode == JOINT_IS) {
        *sfcnt += sfmin;
    } else {
        for (sfb = 0; sfb < sfmin; sfb++)
            channel->msInfo.ms_used[(*sfcnt)++] = 0;
    }

    for (sfb = sfmin; sfb < cl->sfbn; sfb++) {
        int start = sfb_offset[sfb], len = sfb_offset[sfb+1] - start;
        faac_real el, er, elr;
        calculate_energies(sl0, sr0, start, len, wstart, wend, &el, &er, &elr);

        faac_real es   = el + er + 2.0*elr;
        faac_real ed   = el + er - 2.0*elr;
        faac_real etot = el + er;
        if (es < 0) es = 0;
        if (ed < 0) ed = 0;
        if (etot <= 0) {
            if (mode != JOINT_IS) channel->msInfo.ms_used[*sfcnt] = 0;
            (*sfcnt)++;
            continue;
        }

        /* Intensity Stereo check.  The threshold formula is expanded from
         * (sqrt(L)+sqrt(R))^2 to (L+R + 2*sqrt(L*R)) to eliminate one square
         * root per band while maintaining identical decision margins. */
        if ((mode == JOINT_IS || (mode == JOINT_MIXED && sfb >= is_start_sfb)) && el > 0 && er > 0) {
            faac_real th = (el + er + 2.0 * FAAC_SQRT(el * er)) * inv_isthr;
            int hcb = (es >= th) ? HCB_INTENSITY : (ed >= th ? HCB_INTENSITY2 : HCB_NONE);
            if (hcb != HCB_NONE) {
                faac_real inv_etot = 1.0 / etot;
                int sf  = FAAC_LRINT(FAAC_LOG10(el * inv_etot) * SF_STEP_ENRG);
                int pan = FAAC_LRINT(FAAC_LOG10(er * inv_etot) * SF_STEP_ENRG) - sf;
                /* Extreme pan: drop the inaudible channel to HCB_ZERO instead of
                 * intensity-coding it, keeping the band cheap for the quantizer. */
                if (pan > IS_PAN_LIMIT) {
                    cl->book[*sfcnt] = HCB_ZERO;
                    if (mode != JOINT_IS) channel->msInfo.ms_used[*sfcnt] = 0;
                    (*sfcnt)++;
                    continue;
                }
                if (pan < -IS_PAN_LIMIT) {
                    cr->book[*sfcnt] = HCB_ZERO;
                    if (mode != JOINT_IS) channel->msInfo.ms_used[*sfcnt] = 0;
                    (*sfcnt)++;
                    continue;
                }
                cl->sf[*sfcnt]   = sf;
                cr->sf[*sfcnt]   = -pan;
                cr->book[*sfcnt] = hcb;
                faac_real dom = (hcb == HCB_INTENSITY) ? es : ed;
                apply_is(sl0, sr0, start, len, wstart, wend, hcb == HCB_INTENSITY, FAAC_SQRT(etot / dom));
                if (mode != JOINT_IS) channel->msInfo.ms_used[*sfcnt] = 0;
                (*sfcnt)++;
                continue;
            }
        }

        /* Mid/Side Stereo and Muting checks */
        int ms = 0;
        if (mode == JOINT_MS || mode == JOINT_MIXED) {
            /* M/S fires when min(L,R) * thrmid ≥ dominant component: the weaker channel
             * contributes enough to justify the transform overhead. 0.25 accounts for halving. */
            faac_real em = 0.25 * es, side = 0.25 * ed;
            if (min(el, er) * thrmid >= max(em, side)) {
                if (em * thrmid * 2.0 >= etot) {
                    ms = 1;
                    apply_ms(sl0, sr0, start, len, wstart, wend, 1);
                } else if (side * thrmid * 2.0 >= etot) {
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
            channel->msInfo.ms_used[*sfcnt] = ms;
        }
        (*sfcnt)++;
    }
    return msused;
}

void AACstereo(CoderInfo *coder, ChannelInfo *channel, faac_real *s[MAX_CHANNELS],
               int maxchan, faac_real quality, int mode, int sampleRate)
{
    int chn;
    faac_real inv_quality = 1.0 / quality;
    faac_real thrmid = 1.0, isthr = 1.0, thrside = 0.0;

    switch (mode) {
        case JOINT_MIXED:
            thrmid = (0.09 * 0.85) * inv_quality;
            if (thrmid > 0.25) thrmid = 0.25;
            thrmid += 1.0;
            isthr = 0.18 * inv_quality + 1.0;
            if (isthr > M_SQRT2) isthr = M_SQRT2;
            thrside = 0.1 * inv_quality;
            if (thrside > 0.3) thrside = 0.3;
            break;
        case JOINT_MS:
            thrmid = (1.09 - 1.0) * inv_quality;
            if (thrmid > 0.25) thrmid = 0.25;
            thrmid += 1.0;
            thrside = 0.1 * inv_quality;
            if (thrside > 0.3) thrside = 0.3;
            break;
        case JOINT_IS:
            isthr = 0.18 * (inv_quality * inv_quality);
            isthr += 1.0;
            if (isthr > M_SQRT2) isthr = M_SQRT2;
            break;
        default:
            return;
    }
    /* Pre-square thresholds so per-band energy comparisons need no sqrt.
     * Each threshold scales inversely with quality — higher quality encodes
     * apply stereo coding more conservatively, touching the signal less. */
    thrmid *= thrmid;
    faac_real inv_isthr = 1.0 / (isthr * isthr);
    faac_real thrside_sq = thrside * thrside;

    for (chn = 0; chn < maxchan; chn++) {
        if (!channel[chn].present || channel[chn].type != ELEMENT_CPE || !channel[chn].ch_is_left)
            continue;
        int rch = channel[chn].paired_ch;
        channel[chn].common_window = 0;
        channel[chn].msInfo.is_present = 0;
        channel[rch].msInfo.is_present = 0;
        if (coder[chn].block_type != coder[rch].block_type || coder[chn].groups.n != coder[rch].groups.n)
            continue;
        int g, ok = 1;
        for (g = 0; g < coder[chn].groups.n; g++) {
            if (coder[chn].groups.len[g] != coder[rch].groups.len[g]) {
                ok = 0;
                break;
            }
        }
        if (!ok) continue;

        channel[chn].common_window  = 1;
        channel[chn].msInfo.is_present = (mode == JOINT_MS);
        channel[rch].msInfo.is_present = (mode == JOINT_MS);

        int start = 0, sfcnt = 0, is_start_sfb = coder[chn].sfbn, msused = 0;
        if (mode == JOINT_MIXED) {
            int sfb;
            int mlen  = (coder[chn].block_type == ONLY_SHORT_WINDOW) ? 2*BLOCK_LEN_SHORT : 2*BLOCK_LEN_LONG;
            int ifreq = IS_START_FREQ_HZ, cap = (sampleRate * IS_FREQ_CAP_NUM) / IS_FREQ_CAP_DEN;
            if (ifreq > cap) ifreq = cap;
            for (sfb = 0; sfb < coder[chn].sfbn; sfb++) {
                if ((coder[chn].sfb_offset[sfb] * sampleRate) / mlen >= ifreq) {
                    is_start_sfb = sfb;
                    break;
                }
            }
        }

        for (g = 0; g < coder[chn].groups.n; g++) {
            int end = start + coder[chn].groups.len[g];
            msused |= process_cpe(coder+chn, coder+rch, channel+chn, s[chn], s[rch],
                                  &sfcnt, start, end, thrmid, inv_isthr, thrside_sq,
                                  is_start_sfb, mode);
            start = end;
        }
        if (mode == JOINT_MIXED && msused) {
            channel[chn].msInfo.is_present = 1;
            channel[rch].msInfo.is_present = 1;
        }
    }
}
