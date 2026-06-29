/****************************************************************************
    Quantizer core functions

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

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quantize.h"
#include "huff2.h"
#include "cpu_compute.h"

typedef void (*QuantizeFunc)(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix);

#if defined(HAVE_SSE2)
extern void quantize_sse2(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix);
#endif

static void quantize_scalar(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix)
{
    const faac_real magic = MAGIC_NUMBER;
    int i;
    for (i = 0; i < n; i++) {
        faac_real val = xr[i];
        faac_real tmp = FAAC_FABS(val) * sfacfix;
        tmp = FAAC_SQRT(tmp * FAAC_SQRT(tmp));
        int q = (int)(tmp + magic);
        xi[i] = (val < 0) ? -q : q;
    }
}

static QuantizeFunc qfunc = quantize_scalar;
static faac_real sfstep;
static faac_real max_quant_limit;

#define SF_CHAIN_UNSET INT_MIN

void QuantizeInit(void)
{
#if defined(HAVE_SSE2)
    CPUCaps caps = get_cpu_caps();
    if (caps & CPU_CAP_SSE2)
        qfunc = quantize_sse2;
    else
#endif
        qfunc = quantize_scalar;

    /* One AAC scalefactor unit = 2^0.25 in amplitude; sfstep is the
     * log10 multiplier that converts gain to sf units (ISO 14496-3 §8.3.4). */
    sfstep = 1.0 / FAAC_LOG10(FAAC_SQRT(FAAC_SQRT(2.0)));
    max_quant_limit = FAAC_POW((faac_real)MAX_HUFF_ESC_VAL + 1.0 - MAGIC_NUMBER, 4.0/3.0);
}

static faac_real gain_with_overflow_clamp(int *sfac, faac_real band_peak)
{
    faac_real gain = FAAC_POW(10, *sfac / sfstep);
    if (band_peak > 0.0 && gain * band_peak > max_quant_limit) {
        gain = max_quant_limit / band_peak;
        *sfac = (int)FAAC_FLOOR(FAAC_LOG10(gain) * sfstep);
        gain = FAAC_POW(10, *sfac / sfstep);
    }
    return gain;
}

/* Perceptual masking model constants.
 * NOISEFLOOR — bands below this RMS are treated as silence (quantize to zero).
 * NOISETONE/TONEMASK — blend noise-masking vs tonal-peak masking; tonal content
 *   requires tighter quantization than broadband noise of the same power level.
 * SHORT_PENALTY — short blocks represent less energy per block; lower target
 *   prevents over-spending bits on transient frames.
 * *_FLOOR_FACTOR — noise floor preventing log(near-zero/ref) from driving the
 *   quality target to extreme values on near-silent bands. */
#define NOISEFLOOR 0.4
#define NOISETONE     0.2
#define TONEMASK      0.45
#define SHORT_PENALTY 0.45
#define AVGE_FLOOR_FACTOR 0.0010
#define MAXE_FLOOR_FACTOR 0.0050

static void bmask(CoderInfo * __restrict coderInfo, faac_real * __restrict xr0, faac_real * __restrict bandqual,
                  faac_real * __restrict bandenrg, faac_real * __restrict bandmaxe, int gnum, faac_real quality)
{
    int sfb, i, win;
    int *cb_offset = coderInfo->sfb_offset;
    faac_real totenrg = 0.0;
    int gsize = coderInfo->groups.len[gnum];
    int total_len = coderInfo->sfb_offset[coderInfo->sfbn];
    int block_len = (coderInfo->block_type == ONLY_SHORT_WINDOW) ? BLOCK_LEN_SHORT : BLOCK_LEN_LONG;

    for (win = 0; win < gsize; win++) {
        const faac_real *xr = xr0 + win * BLOCK_LEN_SHORT;
        for (i = 0; i < total_len; i++) totenrg += xr[i] * xr[i];
    }

    if (totenrg < ((NOISEFLOOR * NOISEFLOOR) * (faac_real)(gsize * total_len))) {
        for (sfb = 0; sfb < coderInfo->sfbn; sfb++) bandqual[sfb] = bandenrg[sfb] = 0.0;
        return;
    }

    for (sfb = 0; sfb < coderInfo->sfbn; sfb++) {
        int start = cb_offset[sfb], end = cb_offset[sfb+1];
        faac_real avge = 0.0, maxe = 0.0;

        for (win = 0; win < gsize; win++) {
            const faac_real *xr = xr0 + win * BLOCK_LEN_SHORT + start;
            for (i = 0; i < end - start; i++) {
                faac_real e = xr[i] * xr[i];
                avge += e;
                if (maxe < e) maxe = e;
            }
        }
        bandenrg[sfb] = avge;
        bandmaxe[sfb] = FAAC_SQRT(maxe);

        faac_real ref_avge = (totenrg / block_len) * (end - start);
        if (avge < ref_avge * AVGE_FLOOR_FACTOR) avge = ref_avge * AVGE_FLOOR_FACTOR;
        if (maxe < ref_avge * MAXE_FLOOR_FACTOR) maxe = ref_avge * MAXE_FLOOR_FACTOR;

        /* ^0.4 compresses energy ratios into perceptual loudness units (approximates
         * Zwicker); ratio > 1 means louder-than-average, deserving more bits.
         * The 10/(1+f) factor reduces the target at high frequencies, matching
         * reduced masking sensitivity above ~4 kHz. */
        faac_real target = NOISETONE * FAAC_POW(avge/ref_avge, 0.4);
        target += (1.0 - NOISETONE) * TONEMASK * FAAC_POW(maxe/ref_avge, 0.4);
        if (coderInfo->block_type == ONLY_SHORT_WINDOW) target *= SHORT_PENALTY;
        target *= 10.0 / (1.0 + ((faac_real)(start + end) / block_len));
        bandqual[sfb] = target * quality;
    }
}

static void qlevel(CoderInfo * __restrict coderInfo, const faac_real * __restrict xr0,
                   const faac_real * __restrict bandqual, const faac_real * __restrict bandenrg,
                   const faac_real * __restrict bandmaxe, int gnum, int pnslevel, int *p_last_abs)
{
    int sb;
    int gsize = coderInfo->groups.len[gnum];
    faac_real pnsthr = 0.1 * pnslevel;

    for (sb = 0; sb < coderInfo->sfbn && coderInfo->bandcnt < MAX_SCFAC_BANDS; sb++) {
        int band = coderInfo->bandcnt;
        if (coderInfo->book[band] != HCB_NONE) { coderInfo->bandcnt++; continue; }

        int start = coderInfo->sfb_offset[sb], end = coderInfo->sfb_offset[sb+1];
        faac_real etot = bandenrg[sb] / (faac_real)gsize;
        faac_real rmsx = FAAC_SQRT(etot / (end - start));

        if (rmsx < NOISEFLOOR || !bandqual[sb]) {
            coderInfo->book[coderInfo->bandcnt++] = HCB_ZERO;
            continue;
        }

        if (bandqual[sb] < pnsthr) {
            coderInfo->book[band] = HCB_PNS;
            coderInfo->sf[band] += FAAC_LRINT(FAAC_LOG10(etot) * (0.5 * sfstep));
            coderInfo->bandcnt++;
            continue;
        }

        /* sf_rel = SF_OFFSET − sfac: the bitstream encodes each band's scalefactor as
         * a delta from global_gain, with SF_OFFSET as the neutral (no-gain) point. */
        int sfac = FAAC_LRINT(FAAC_LOG10(bandqual[sb] / rmsx) * sfstep);
        int sf_rel = SF_OFFSET - sfac;
        int sf_bias = coderInfo->sf[band];
        int sf_abs = sf_bias + sf_rel;

        /* Three-stage clamp cascade; sfacfix is re-derived after each stage because
         * sfac and gain are coupled — clamping one changes the other.
         * (1) overflow: pull gain down so the peak quantized value stays within the
         *     HCB_ESC ceiling (prevents escape-code overflow).
         * (2) delta: snap sf_abs so the inter-band difference fits in ±SF_DELTA,
         *     the maximum the spec's HCB_DELTA Huffman table can represent.
         * (3) range: clamp absolute scalefactor to the 8-bit bitstream field [0,255]. */
        faac_real sfacfix;
        if (sf_rel < SF_MIN) {
            sfacfix = 0.0;
        } else {
            sfacfix = gain_with_overflow_clamp(&sfac, bandmaxe[sb]);
            sf_rel = SF_OFFSET - sfac;
            sf_abs = sf_bias + sf_rel;

            if (*p_last_abs != SF_CHAIN_UNSET) {
                int diff = clamp_sf_diff(sf_abs - *p_last_abs);
                if (diff != (sf_abs - *p_last_abs)) {
                    sf_abs = *p_last_abs + diff;
                    sf_rel = sf_abs - sf_bias;
                    sfac = SF_OFFSET - sf_rel;
                    sfacfix = gain_with_overflow_clamp(&sfac, bandmaxe[sb]);
                    sf_rel = SF_OFFSET - sfac;
                    sf_abs = sf_bias + sf_rel;
                }
            }

            if (sf_abs < 0 || sf_abs > SF_MAX_ABS) {
                sf_abs = (sf_abs < 0) ? 0 : SF_MAX_ABS;
                sf_rel = sf_abs - sf_bias;
                sfac = SF_OFFSET - sf_rel;
                sfacfix = gain_with_overflow_clamp(&sfac, bandmaxe[sb]);
                sf_rel = SF_OFFSET - sfac;
                sf_abs = sf_bias + sf_rel;
            }
        }

        if (sfacfix <= 0.0) {
            coderInfo->book[band] = HCB_ZERO;
        } else {
            int xitab[FRAME_LEN], win;
            for (win = 0; win < gsize; win++) {
                qfunc(xr0 + win * BLOCK_LEN_SHORT + start, xitab + win * (end - start), end - start, sfacfix);
            }
            huffbook(coderInfo, xitab, gsize * (end - start));
        }
        if (coderInfo->book[band] != HCB_ZERO) *p_last_abs = sf_abs;
        coderInfo->sf[coderInfo->bandcnt++] += sf_rel;
    }
}

void ResetCoderSections(CoderInfo *coder)
{
    int i, n = coder->groups.n * coder->sfbn;
    for (i = 0; i < n; i++) { coder->book[i] = HCB_NONE; coder->sf[i] = 0; }
}

int BlocQuant(CoderInfo * __restrict coder, faac_real * __restrict xr, AACQuantCfg *aacquantCfg)
{
    faac_real bandlvl[MAX_SCFAC_BANDS], bandenrg[MAX_SCFAC_BANDS], bandmaxe[MAX_SCFAC_BANDS];
    int i, lastsf = SF_CHAIN_UNSET;
    faac_real *gxr = xr;

    coder->bandcnt = coder->datacnt = 0;
    for (i = 0; i < coder->groups.n; i++) {
        bmask(coder, gxr, bandlvl, bandenrg, bandmaxe, i, (faac_real)aacquantCfg->quality / DEFQUAL);
        qlevel(coder, gxr, bandlvl, bandenrg, bandmaxe, i, aacquantCfg->pnslevel, &lastsf);
        gxr += coder->groups.len[i] * BLOCK_LEN_SHORT;
    }

    coder->global_gain = 0;
    for (i = 0; i < coder->bandcnt; i++) {
        int b = coder->book[i];
        if (b && b != HCB_INTENSITY && b != HCB_INTENSITY2 && b != HCB_PNS) {
            coder->global_gain = coder->sf[i];
            break;
        }
    }

    int lastis = 0, lastpns = coder->global_gain - SF_PNS_OFFSET;
    for (i = 0; i < coder->bandcnt; i++) {
        int b = coder->book[i];
        if (b == HCB_INTENSITY || b == HCB_INTENSITY2) {
            int diff = clamp_sf_diff(coder->sf[i] - lastis);
            lastis += diff;
            coder->sf[i] = lastis;
        } else if (b == HCB_PNS) {
            int diff = clamp_sf_diff(coder->sf[i] - lastpns);
            lastpns += diff;
            coder->sf[i] = lastpns;
        }
    }
    return 1;
}

void CalcBW(unsigned *bw, int rate, SR_INFO *sr, AACQuantCfg *aacquantCfg)
{
    int i, l = 0, max = *bw * (BLOCK_LEN_SHORT << 1) / rate;
    for (i = 0; i < sr->num_cb_short && l < max; i++) l += sr->cb_width_short[i];
    aacquantCfg->max_cbs = i;
    if (aacquantCfg->pnslevel) *bw = (faac_real)l * rate / (BLOCK_LEN_SHORT << 1);

    l = 0, max = *bw * (BLOCK_LEN_LONG << 1) / rate;
    for (i = 0; i < sr->num_cb_long && l < max; i++) l += sr->cb_width_long[i];
    aacquantCfg->max_cbl = i;
    aacquantCfg->max_l = l;
    *bw = (faac_real)l * rate / (BLOCK_LEN_LONG << 1);
}

void BlocGroup(faac_real *xr, CoderInfo *coderInfo, AACQuantCfg *cfg)
{
    if (coderInfo->block_type != ONLY_SHORT_WINDOW) {
        coderInfo->groups.n = 1;
        coderInfo->groups.len[0] = 1;
        return;
    }

    int maxsfb = cfg->max_cbs, maxl = cfg->max_l / 8;
    int win, sfb, i, win0 = 0, fastmin = ((maxsfb - 2) * 3) >> 2;
    faac_real e[NSFB_SHORT], min[NSFB_SHORT], max[NSFB_SHORT];

    /* manual inline for simplicity and authorship */
    for (win = 0; win < MAX_SHORT_WINDOWS; win++) {
        faac_real *w_xr = xr + win * BLOCK_LEN_SHORT;
        for (i = maxl; i < coderInfo->sfb_offset[maxsfb]; i++) w_xr[i] = 0.0;
        for (sfb = 2; sfb < maxsfb; sfb++) {
            faac_real en = 0.0;
            for (i = coderInfo->sfb_offset[sfb]; i < coderInfo->sfb_offset[sfb+1]; i++) en += w_xr[i] * w_xr[i];
            e[sfb] = en;
        }

        if (win == 0) {
            for (sfb = 2; sfb < maxsfb; sfb++) min[sfb] = max[sfb] = e[sfb];
            coderInfo->groups.n = 0;
            continue;
        }

        int fast = 0;
        for (sfb = 2; sfb < maxsfb; sfb++) {
            if (min[sfb] > e[sfb]) min[sfb] = e[sfb];
            if (max[sfb] < e[sfb]) max[sfb] = e[sfb];
            if (max[sfb] > 3.0 * min[sfb]) fast++;
        }

        /* If more than 3/4 of bands show a 3x energy swing (running max/min),
         * this window marks a transient onset and deserves its own group.
         * Grouping spectally-similar windows reduces scalefactor overhead. */
        if (fast > fastmin) {
            coderInfo->groups.len[coderInfo->groups.n++] = win - win0;
            win0 = win;
            for (sfb = 2; sfb < maxsfb; sfb++) min[sfb] = max[sfb] = e[sfb];
        }
    }
    coderInfo->groups.len[coderInfo->groups.n++] = MAX_SHORT_WINDOWS - win0;
}

void BlocStat(void) {}
