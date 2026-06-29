/****************************************************************************
    Intensity Stereo

    Copyright (C) 2026 Nils Schimmelmann

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
****************************************************************************/

#define _USE_MATH_DEFINES
#include <math.h>
#include "stereo.h"
#include "huff2.h"

static void zero_channel(faac_real * restrict s0, int start, int len, int wstart, int wend)
{
    faac_real * restrict s = s0 + wstart * BLOCK_LEN_SHORT + start;
    int win, i;
    for (win = wstart; win < wend; win++) {
        for (i = 0; i < len; i++)
            s[i] = 0.0;
        s += BLOCK_LEN_SHORT;
    }
}

/* When one component (mid or side) dominates, collapse both channels to that
 * component and zero the other — it costs no bits and the signal loss is masked.
 * Factor of 0.5 keeps the coded amplitude on the same scale as L/R. */
static void apply_ms(faac_real * restrict sl0, faac_real * restrict sr0,
                     int start, int len, int wstart, int wend, int in_phase)
{
    faac_real * restrict sl = sl0 + wstart * BLOCK_LEN_SHORT + start;
    faac_real * restrict sr = sr0 + wstart * BLOCK_LEN_SHORT + start;
    int win, i;
    for (win = wstart; win < wend; win++) {
        if (in_phase) {
            for (i = 0; i < len; i++) {
                sl[i] = 0.5 * (sl[i] + sr[i]);
                sr[i] = 0.0;
            }
        } else {
            for (i = 0; i < len; i++) {
                sr[i] = 0.5 * (sl[i] - sr[i]);
                sl[i] = 0.0;
            }
        }
        sl += BLOCK_LEN_SHORT;
        sr += BLOCK_LEN_SHORT;
    }
}

static void apply_is(faac_real * restrict sl0, faac_real * restrict sr0,
                     int start, int len, int wstart, int wend, int in_phase, faac_real vfix)
{
    faac_real * restrict sl = sl0 + wstart * BLOCK_LEN_SHORT + start;
    faac_real * restrict sr = sr0 + wstart * BLOCK_LEN_SHORT + start;
    int win, i;
    for (win = wstart; win < wend; win++) {
        if (in_phase) {
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] + sr[i]) * vfix;
                sr[i] = 0.0;
            }
        } else {
            for (i = 0; i < len; i++) {
                sl[i] = (sl[i] - sr[i]) * vfix;
                sr[i] = 0.0;
            }
        }
        sl += BLOCK_LEN_SHORT;
        sr += BLOCK_LEN_SHORT;
    }
}

static void stereo(CoderInfo * restrict cl, CoderInfo * restrict cr,
                   faac_real * restrict sl0, faac_real * restrict sr0,
                   int * restrict sfcnt, int wstart, int wend, faac_real phthr)
{
    int sfb, sfmin = (cl->block_type == ONLY_SHORT_WINDOW) ? 1 : 8;
    *sfcnt += sfmin;
    if (!phthr) return;
    phthr = 1.0 / phthr;

    for (sfb = sfmin; sfb < cl->sfbn; sfb++) {
        int start = cl->sfb_offset[sfb], len = cl->sfb_offset[sfb+1] - start;
        faac_real el = 0, er = 0, elr = 0;
        const faac_real *sl = sl0 + wstart * BLOCK_LEN_SHORT + start;
        const faac_real *sr = sr0 + wstart * BLOCK_LEN_SHORT + start;
        int win, i;

        for (win = wstart; win < wend; win++) {
            for (i = 0; i < len; i++) {
                el  += sl[i] * sl[i];
                er  += sr[i] * sr[i];
                elr += sl[i] * sr[i];
            }
            sl += BLOCK_LEN_SHORT;
            sr += BLOCK_LEN_SHORT;
        }
        faac_real es   = el + er + 2.0*elr;
        faac_real ed   = el + er - 2.0*elr;
        faac_real etot = el + er;
        if (es < 0) es = 0;
        if (ed < 0) ed = 0;
        if (etot <= 0) { (*sfcnt)++; continue; }

        /* IS threshold: (sqrt(el)+sqrt(er))^2 upper-bounds coherent energy; when the
         * in-phase (es) or out-of-phase (ed) sum exceeds this margin, IS coding saves
         * bits over independent quantization.  HCB_INTENSITY = decoder adds the
         * right channel from the left; HCB_INTENSITY2 = decoder subtracts. */
        faac_real th = (FAAC_SQRT(el) + FAAC_SQRT(er));
        th = th * th * phthr;

        int hcb = HCB_NONE;
        faac_real vfix = 1.0;
        if (es >= th) {
            hcb  = HCB_INTENSITY;
            vfix = FAAC_SQRT(etot / es);
        } else if (ed >= th) {
            hcb  = HCB_INTENSITY2;
            vfix = FAAC_SQRT(etot / ed);
        }

        if (hcb != HCB_NONE && el > 0 && er > 0) {
            /* IS pan is in log-scale steps of ~1.5 dB (10/log10(2) per octave →
             * 10/1.50515 ≈ 6.64 steps/dB).  |pan| > 30 means the channel is more
             * than ~4.5 dB below the other — effectively silent; zero it instead. */
            int sf  = FAAC_LRINT(FAAC_LOG10(el / etot) * (10 / 1.50515));
            int pan = FAAC_LRINT(FAAC_LOG10(er / etot) * (10 / 1.50515)) - sf;
            if (pan > 30) {
                cl->book[*sfcnt] = HCB_ZERO;
            } else if (pan < -30) {
                cr->book[*sfcnt] = HCB_ZERO;
            } else {
                cl->sf[*sfcnt]   = sf;
                cr->sf[*sfcnt]   = -pan;
                cr->book[*sfcnt] = hcb;
                apply_is(sl0, sr0, start, len, wstart, wend, hcb == HCB_INTENSITY, vfix);
            }
        }
        (*sfcnt)++;
    }
}

static void midside(CoderInfo * restrict coder, ChannelInfo * restrict channel,
                    faac_real * restrict sl0, faac_real * restrict sr0,
                    int * restrict sfcnt, int wstart, int wend,
                    faac_real thrmid, faac_real thrside)
{
    int sfb, sfmin = (coder->block_type == ONLY_SHORT_WINDOW) ? 1 : 8;
    for (sfb = 0; sfb < sfmin; sfb++)
        channel->msInfo.ms_used[(*sfcnt)++] = 0;
    for (sfb = sfmin; sfb < coder->sfbn; sfb++) {
        int start = coder->sfb_offset[sfb], len = coder->sfb_offset[sfb+1] - start;
        faac_real el = 0, er = 0, elr = 0;
        const faac_real *sl = sl0 + wstart * BLOCK_LEN_SHORT + start;
        const faac_real *sr = sr0 + wstart * BLOCK_LEN_SHORT + start;
        int win, i;
        for (win = wstart; win < wend; win++) {
            for (i = 0; i < len; i++) {
                el  += sl[i] * sl[i];
                er  += sr[i] * sr[i];
                elr += sl[i] * sr[i];
            }
            sl += BLOCK_LEN_SHORT;
            sr += BLOCK_LEN_SHORT;
        }
        /* em = ((L+R)/2)^2, es = ((L-R)/2)^2: energy of the mid and side components
         * after the M/S transform.  The 0.25 factor accounts for the halving in apply_ms. */
        faac_real em = 0.25 * (el + er + 2.0*elr);
        faac_real es = 0.25 * (el + er - 2.0*elr);
        int ms = 0;
        /* M/S fires when min(L,R) * thrmid ≥ dominant component: the weaker channel
         * contributes enough to justify the transform overhead.  When channels are
         * very unequal (thrside test), zeroing the quiet side saves more bits. */
        if (min(el, er) * thrmid >= max(em, es)) {
            if (em * thrmid * 2.0 >= el + er) {
                ms = 1;
                apply_ms(sl0, sr0, start, len, wstart, wend, 1);
            } else if (es * thrmid * 2.0 >= el + er) {
                ms = 1;
                apply_ms(sl0, sr0, start, len, wstart, wend, 0);
            }
        }
        if (!ms && min(el, er) <= thrside * max(el, er)) {
            if (el < er)
                zero_channel(sl0, start, len, wstart, wend);
            else
                zero_channel(sr0, start, len, wstart, wend);
        }
        channel->msInfo.ms_used[(*sfcnt)++] = ms;
    }
}

static int mixed(CoderInfo * restrict cl, CoderInfo * restrict cr,
                 ChannelInfo * restrict channel,
                 faac_real * restrict sl0, faac_real * restrict sr0,
                 int * restrict sfcnt, int wstart, int wend,
                 faac_real thrmid, faac_real thrside, faac_real isthr, int is_start_sfb)
{
    int sfb, sfmin = (cl->block_type == ONLY_SHORT_WINDOW) ? 1 : 8, msused = 0;
    for (sfb = 0; sfb < sfmin; sfb++)
        channel->msInfo.ms_used[(*sfcnt)++] = 0;
    for (sfb = sfmin; sfb < cl->sfbn; sfb++) {
        int start = cl->sfb_offset[sfb], len = cl->sfb_offset[sfb+1] - start;
        faac_real el = 0, er = 0, elr = 0;
        const faac_real *sl = sl0 + wstart * BLOCK_LEN_SHORT + start;
        const faac_real *sr = sr0 + wstart * BLOCK_LEN_SHORT + start;
        int win, i;
        for (win = wstart; win < wend; win++) {
            for (i = 0; i < len; i++) {
                el  += sl[i] * sl[i];
                er  += sr[i] * sr[i];
                elr += sl[i] * sr[i];
            }
            sl += BLOCK_LEN_SHORT;
            sr += BLOCK_LEN_SHORT;
        }
        faac_real em   = el + er + 2.0*elr;
        faac_real ed   = el + er - 2.0*elr;
        faac_real etot = el + er;
        if (em < 0) em = 0;
        if (ed < 0) ed = 0;
        if (etot <= 0) {
            channel->msInfo.ms_used[(*sfcnt)++] = 0;
            continue;
        }

        if (sfb >= is_start_sfb && el > 0 && er > 0) {
            faac_real th = (FAAC_SQRT(el) + FAAC_SQRT(er));
            th = th * th / isthr;
            int hcb = (em >= th) ? HCB_INTENSITY : (ed >= th ? HCB_INTENSITY2 : HCB_NONE);
            if (hcb != HCB_NONE) {
                int sf  = FAAC_LRINT(FAAC_LOG10(el / etot) * (10 / 1.50515));
                int pan = FAAC_LRINT(FAAC_LOG10(er / etot) * (10 / 1.50515)) - sf;
                if (pan > 30) {
                    cl->book[*sfcnt] = HCB_ZERO;
                } else if (pan < -30) {
                    cr->book[*sfcnt] = HCB_ZERO;
                } else {
                    faac_real dom = (hcb == HCB_INTENSITY) ? em : ed;
                    cl->sf[*sfcnt]   = sf;
                    cr->sf[*sfcnt]   = -pan;
                    cr->book[*sfcnt] = hcb;
                    apply_is(sl0, sr0, start, len, wstart, wend,
                             hcb == HCB_INTENSITY, FAAC_SQRT(etot / dom));
                }
                channel->msInfo.ms_used[(*sfcnt)++] = 0;
                continue;
            }
        }
        int ms = 0;
        if (min(el, er) * thrmid >= max(em * 0.25, ed * 0.25)) {
            if (em * 0.25 * thrmid * 2.0 >= el + er) {
                ms = 1;
                apply_ms(sl0, sr0, start, len, wstart, wend, 1);
            } else if (ed * 0.25 * thrmid * 2.0 >= el + er) {
                ms = 1;
                apply_ms(sl0, sr0, start, len, wstart, wend, 0);
            }
        }
        if (ms)
            msused = 1;
        else if (min(el, er) <= thrside * max(el, er)) {
            if (el < er)
                zero_channel(sl0, start, len, wstart, wend);
            else
                zero_channel(sr0, start, len, wstart, wend);
        }
        channel->msInfo.ms_used[(*sfcnt)++] = ms;
    }
    return msused;
}

void AACstereo(CoderInfo *coder, ChannelInfo *channel, faac_real *s[MAX_CHANNELS],
               int maxchan, faac_real quality, int mode, int sampleRate)
{
    int chn;
    faac_real ts = (0.1 / quality > 0.3) ? 0.3 : (0.1 / quality);
    faac_real thrmid = 1.0, thrside = 0.0, isthr = 1.0;

    if (mode == JOINT_MS) {
        thrmid = (1.09 - 1.0) / quality;
        if (thrmid > 0.25) thrmid = 0.25;
        thrside = ts;
        thrmid += 1.0;
    } else if (mode == JOINT_IS) {
        isthr = 0.18 / (quality * quality);
        isthr += 1.0;
        if (isthr > M_SQRT2) isthr = M_SQRT2;
    } else if (mode == JOINT_MIXED) {
        thrmid = (0.09 * 0.85) / quality;
        if (thrmid > 0.25) thrmid = 0.25;
        thrside = ts;
        thrmid += 1.0;
        isthr = 0.18 / quality + 1.0;
        if (isthr > M_SQRT2) isthr = M_SQRT2;
    }
    /* Pre-square all thresholds so per-band energy comparisons need no sqrt.
     * Each threshold scales inversely with quality — higher quality encodes
     * apply stereo coding more conservatively, touching the signal less. */
    thrmid  *= thrmid;
    thrside *= thrside;
    isthr   *= isthr;

    for (chn = 0; chn < maxchan; chn++) {
        if (!channel[chn].present || channel[chn].type != ELEMENT_CPE || !channel[chn].ch_is_left)
            continue;
        int rch = channel[chn].paired_ch;
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
            int ifreq = 5500, cap = (sampleRate * 7) / 20;
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
            if (mode == JOINT_MS)
                midside(coder+chn, channel+chn, s[chn], s[rch], &sfcnt, start, end, thrmid, thrside);
            else if (mode == JOINT_IS)
                stereo(coder+chn, coder+rch, s[chn], s[rch], &sfcnt, start, end, isthr);
            else if (mode == JOINT_MIXED)
                msused |= mixed(coder+chn, coder+rch, channel+chn, s[chn], s[rch],
                                &sfcnt, start, end, thrmid, thrside, isthr, is_start_sfb);
            else
                sfcnt += coder[chn].sfbn;
            start = end;
        }
        if (mode == JOINT_MIXED && msused) {
            channel[chn].msInfo.is_present = 1;
            channel[rch].msInfo.is_present = 1;
        }
    }
}
