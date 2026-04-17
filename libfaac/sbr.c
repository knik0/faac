/*
 * FAAC - Freeware Advanced Audio Coder
 *
 * HE-AAC v1 SBR encoder
 * ISO/IEC 14496-3:2009 §4.6.18
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sbr.h"
#include "sbr_tables.h"
#include "bitstream.h"
#include "util.h"
#include "coder.h"

/* ------------------------------------------------------------------ *
 *  Internal helpers
 * ------------------------------------------------------------------ */

/* Clamp integer x to [lo, hi] */
static int clamp_int(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* Write Huffman-coded delta value.
 * table[delta + offset] gives the {code, len} pair.
 * code has the codeword MSB at bit (len-1); PutBit() writes it correctly. */
static int put_huff(BitStream *bs, const SBRHuffEntry *table, int nsyms,
                    int offset, int delta, int writeFlag)
{
    int sym = delta + offset;
    if (sym < 0) sym = 0;
    if (sym >= nsyms) sym = nsyms - 1;
    if (writeFlag)
        PutBit(bs, table[sym].code, table[sym].len);
    return table[sym].len;
}

/* ------------------------------------------------------------------ *
 *  Frequency band table construction
 *  ISO 14496-3:2009 §4.6.18.3.6
 * ------------------------------------------------------------------ */

/*
 * Compute kx (first SBR band) from the FULL sample rate and bs_start_freq.
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master().
 *
 * Verified: 44100 Hz, bs_start_freq=3 → start_min=12, offset=0 → kx=12.
 */
static int compute_kx(int sampleRate, int bs_start_freq)
{
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int start_min = ((temp << 7) + (sampleRate >> 1)) / sampleRate;
    int row = (sampleRate <= 16000) ? 0 : (sampleRate <= 22050) ? 1 :
              (sampleRate <= 24000) ? 2 : (sampleRate <= 32000) ? 3 :
              (sampleRate <= 64000) ? 4 : 5;
    int kx = start_min + sbr_fs_offset[row][bs_start_freq & 15];
    if (kx < 1)  kx = 1;
    if (kx > 63) kx = 63;
    return kx;
}

/* qsort comparison for int16_t (ascending) */
static int cmp_int16(const void *a, const void *b)
{
    return (int)(*(const short *)a) - (int)(*(const short *)b);
}

/*
 * Compute k2 (exclusive stop band) from the FULL sample rate and bs_stop_freq.
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master().
 *
 * For bs_stop_freq < 14: start from stop_min, compute 13 geometric band widths
 * from stop_min to 64, sort ascending, accumulate first bs_stop_freq of them.
 *
 * Verified: 44100 Hz, bs_stop_freq=7 → stop_min=24, k2=39.
 */
static int compute_k2(int sampleRate, int kx, int bs_stop_freq)
{
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int stop_min = ((temp << 8) + (sampleRate >> 1)) / sampleRate;

    int k2;
    if (bs_stop_freq < 14) {
        /* Geometric band widths from stop_min to 64 in 13 steps.
         * Must match FFmpeg make_bands(): lrintf for bands 0..11, exact last band. */
        short stop_dk[13];
        float base = powf(64.0f / (float)stop_min, 1.0f / 13.0f);
        float prod = (float)stop_min;
        int prev = stop_min;
        for (int i = 0; i < 12; i++) {
            prod *= base;
            int present = (int)lrintf(prod);
            stop_dk[i] = (short)(present - prev);
            prev = present;
        }
        stop_dk[12] = (short)(64 - prev);  /* exact last band */
        /* Sort ascending then accumulate first bs_stop_freq widths */
        qsort(stop_dk, 13, sizeof(short), cmp_int16);
        k2 = stop_min;
        for (int i = 0; i < bs_stop_freq; i++)
            k2 += stop_dk[i];
    } else if (bs_stop_freq == 14) {
        k2 = 2 * kx;
    } else {
        k2 = 3 * kx;
    }

    if (k2 > 64)  k2 = 64;
    if (k2 <= kx) k2 = kx + 1;

    /* Decoder per-rate max_qmf_subbands constraints (ISO 14496-3 sp04 p205) */
    int max_span = (sampleRate <= 32000) ? 48 : (sampleRate <= 44100) ? 35 : 32;
    if (k2 - kx > max_span) k2 = kx + max_span;
    return k2;
}

/*
 * Build master frequency band table (bs_freq_scale=0, bs_alter_scale=0).
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master() lines 353-374.
 *
 * dk = bs_alter_scale + 1 = 1
 * n_master = ((k2-kx + (dk&2)) >> dk) << 1 = floor((k2-kx)/2)*2
 * k2diff   = (k2-kx) - n_master*dk
 * band widths start as dk=1 each; last entry gets +1 if k2diff==1,
 * or first/second entry get -1 if k2diff < 0.
 *
 * Noise: one noise floor band spanning the entire SBR region.
 */
static int build_freq_table(SBRInfo *sbr)
{
    int kx = sbr->kx;
    int k2 = sbr->k2;
    int dk = 1;  /* bs_alter_scale=0 → dk=1 */

    int n_master = ((k2 - kx + (dk & 2)) >> dk) << 1;
    if (n_master < 1) n_master = 1;
    if (n_master > SBR_MAX_BANDS) n_master = SBR_MAX_BANDS;

    /* f_master[k] = dk for k=1..n_master, then apply k2diff correction */
    int f_master[SBR_MAX_BANDS + 1];
    for (int k = 1; k <= n_master; k++)
        f_master[k] = dk;

    int k2diff = (k2 - kx) - n_master * dk;
    if (k2diff < 0) {
        f_master[1]--;
        if (k2diff < -1) f_master[2]--;
    } else if (k2diff > 0) {
        f_master[n_master]++;
    }

    f_master[0] = kx;
    for (int k = 1; k <= n_master; k++)
        f_master[k] += f_master[k - 1];

    sbr->numBands = n_master;
    for (int b = 0; b <= n_master; b++)
        sbr->bandEdges[b] = f_master[b];

    /* Single noise floor band */
    sbr->numNoiseBands     = 1;
    sbr->noiseBandEdges[0] = kx;
    sbr->noiseBandEdges[1] = k2;

    return n_master;
}

/* ------------------------------------------------------------------ *
 *  SBRInit / SBREnd
 * ------------------------------------------------------------------ */

SBRInfo *SBRInit(int channels, int sampleRate, int coreSampleRate)
{
    SBRInfo *sbr = (SBRInfo *)calloc(1, sizeof(SBRInfo));
    if (!sbr) return NULL;

    sbr->sbrPresent    = 1;
    sbr->headerSent    = 0;
    sbr->frameCount    = 0;
    sbr->numChannels   = channels;
    sbr->sampleRate    = sampleRate;
    sbr->coreSampleRate = coreSampleRate;

    /* Choose header parameters.
     * bs_stop_freq=7 is safe for all rates: worst-case k2-kx=41 ≤ 48.
     * Higher values (e.g. 8) are better quality but rate-dependent. */
    sbr->bs_amp_res    = 0;   /* 1.5 dB amplitude resolution */
    sbr->bs_start_freq = 15;  /* kx ≈ 31–32 (≈11.6–12 kHz) — just below LC core Nyquist */
    sbr->bs_stop_freq  = 14;  /* k2 = 2*kx — full upper-octave SBR extension */
    sbr->bs_xover_band = 0;

    sbr->kx = compute_kx(sampleRate, sbr->bs_start_freq);
    sbr->k2 = compute_k2(sampleRate, sbr->kx, sbr->bs_stop_freq);
    build_freq_table(sbr);

    return sbr;
}

void SBREnd(SBRInfo *sbr)
{
    free(sbr);
}

/* ------------------------------------------------------------------ *
 *  32-band analysis QMF — ISO 14496-3:2009 §4.6.18.7.1
 *
 *  Per slot (32 new input samples):
 *    1. Shift 32 samples into the 320-sample overlap buffer.
 *    2. z[k] = window[k] * ovl[319-k]               (vector_fmul_reverse)
 *    3. v[n] = Σ_{p=0..4} z[n + 64*p]                (sum64x5, n=0..63)
 *    4. pre-shuffle v[0..63] → t[0..63]
 *    5. IMDCT (N=64) on t → y[0..63]
 *    6. post-shuffle y → (W_re[32], W_im[32])
 * ------------------------------------------------------------------ */

/* ISO 14496-3 Table 4.A.87 — 320-tap QMF analysis prototype. */
static const faac_real sbr_qmf_window_ds320[320] = {
     (faac_real) 0.0000000000f, (faac_real)-0.0005617692f, (faac_real)-0.0004875227f, (faac_real)-0.0005040714f,
     (faac_real)-0.0005466565f, (faac_real)-0.0005870930f, (faac_real)-0.0006312493f, (faac_real)-0.0006777690f,
     (faac_real)-0.0007157736f, (faac_real)-0.0007440941f, (faac_real)-0.0007681371f, (faac_real)-0.0007834332f,
     (faac_real)-0.0007803664f, (faac_real)-0.0007757977f, (faac_real)-0.0007530001f, (faac_real)-0.0007215391f,
     (faac_real)-0.0006650415f, (faac_real)-0.0005946118f, (faac_real)-0.0005145572f, (faac_real)-0.0004095121f,
     (faac_real)-0.0002896981f, (faac_real)-0.0001446380f, (faac_real) 0.0000134949f, (faac_real) 0.0002043017f,
     (faac_real) 0.0004026540f, (faac_real) 0.0006239376f, (faac_real) 0.0008608443f, (faac_real) 0.0011250155f,
     (faac_real) 0.0013902494f, (faac_real) 0.0016868083f, (faac_real) 0.0019841140f, (faac_real) 0.0023017254f,
     (faac_real) 0.0026201758f, (faac_real) 0.0029469447f, (faac_real) 0.0032739613f, (faac_real) 0.0036008268f,
     (faac_real) 0.0039207432f, (faac_real) 0.0042264269f, (faac_real) 0.0045209852f, (faac_real) 0.0047932560f,
     (faac_real) 0.0050393022f, (faac_real) 0.0052461166f, (faac_real) 0.0054196775f, (faac_real) 0.0055475714f,
     (faac_real) 0.0056220643f, (faac_real) 0.0056389199f, (faac_real) 0.0055917128f, (faac_real) 0.0054753783f,
     (faac_real) 0.0052715758f, (faac_real) 0.0049839687f, (faac_real) 0.0046039530f, (faac_real) 0.0041251642f,
     (faac_real) 0.0035401246f, (faac_real) 0.0028446757f, (faac_real) 0.0020274176f, (faac_real) 0.0010902329f,
     (faac_real) 0.0000276045f, (faac_real)-0.0011568135f, (faac_real)-0.0024826723f, (faac_real)-0.0039401124f,
     (faac_real)-0.0055337211f, (faac_real)-0.0072615816f, (faac_real)-0.0091325329f, (faac_real)-0.0111315548f,
     (faac_real) 0.0132718220f, (faac_real) 0.0155405553f, (faac_real) 0.0179433381f, (faac_real) 0.0204531793f,
     (faac_real) 0.0230680169f, (faac_real) 0.0257875847f, (faac_real) 0.0286072173f, (faac_real) 0.0315017608f,
     (faac_real) 0.0344620948f, (faac_real) 0.0374812850f, (faac_real) 0.0405349170f, (faac_real) 0.0436097542f,
     (faac_real) 0.0466843027f, (faac_real) 0.0497385755f, (faac_real) 0.0527630746f, (faac_real) 0.0557173648f,
     (faac_real) 0.0585915683f, (faac_real) 0.0613455171f, (faac_real) 0.0639715898f, (faac_real) 0.0664367512f,
     (faac_real) 0.0687043828f, (faac_real) 0.0707628710f, (faac_real) 0.0725682583f, (faac_real) 0.0741003642f,
     (faac_real) 0.0753137336f, (faac_real) 0.0761992479f, (faac_real) 0.0767093490f, (faac_real) 0.0768230011f,
     (faac_real) 0.0765050718f, (faac_real) 0.0757305756f, (faac_real) 0.0744664394f, (faac_real) 0.0726774642f,
     (faac_real) 0.0703533073f, (faac_real) 0.0674525021f, (faac_real) 0.0639444805f, (faac_real) 0.0598166570f,
     (faac_real) 0.0550460034f, (faac_real) 0.0495978676f, (faac_real) 0.0434768782f, (faac_real) 0.0366418116f,
     (faac_real) 0.0290824006f, (faac_real) 0.0207997072f, (faac_real) 0.0117623832f, (faac_real) 0.0019765601f,
     (faac_real)-0.0085711749f, (faac_real)-0.0198834129f, (faac_real)-0.0319531274f, (faac_real)-0.0447806821f,
     (faac_real)-0.0583705326f, (faac_real)-0.0726943300f, (faac_real)-0.0877547536f, (faac_real)-0.1035329531f,
     (faac_real)-0.1200077984f, (faac_real)-0.1371551761f, (faac_real)-0.1549607071f, (faac_real)-0.1733808172f,
     (faac_real)-0.1923966745f, (faac_real)-0.2119735853f, (faac_real)-0.2320690870f, (faac_real)-0.2526480309f,
     (faac_real)-0.2736634040f, (faac_real)-0.2950716717f, (faac_real)-0.3168278913f, (faac_real)-0.3388722693f,
     (faac_real) 0.3611589903f, (faac_real) 0.3836350013f, (faac_real) 0.4062317676f, (faac_real) 0.4289119920f,
     (faac_real) 0.4515996535f, (faac_real) 0.4742453214f, (faac_real) 0.4967708254f, (faac_real) 0.5191234970f,
     (faac_real) 0.5412553448f, (faac_real) 0.5630789140f, (faac_real) 0.5845403235f, (faac_real) 0.6055783538f,
     (faac_real) 0.6261242695f, (faac_real) 0.6461269695f, (faac_real) 0.6655139880f, (faac_real) 0.6842353293f,
     (faac_real) 0.7022388719f, (faac_real) 0.7194462634f, (faac_real) 0.7358211758f, (faac_real) 0.7513137456f,
     (faac_real) 0.7658674865f, (faac_real) 0.7794287519f, (faac_real) 0.7919735841f, (faac_real) 0.8034485751f,
     (faac_real) 0.8138191270f, (faac_real) 0.8230419890f, (faac_real) 0.8311038457f, (faac_real) 0.8379717337f,
     (faac_real) 0.8436238281f, (faac_real) 0.8480315777f, (faac_real) 0.8511971524f, (faac_real) 0.8531020949f,
     (faac_real) 0.8537385600f, (faac_real) 0.8531020949f, (faac_real) 0.8511971524f, (faac_real) 0.8480315777f,
     (faac_real) 0.8436238281f, (faac_real) 0.8379717337f, (faac_real) 0.8311038457f, (faac_real) 0.8230419890f,
     (faac_real) 0.8138191270f, (faac_real) 0.8034485751f, (faac_real) 0.7919735841f, (faac_real) 0.7794287519f,
     (faac_real) 0.7658674865f, (faac_real) 0.7513137456f, (faac_real) 0.7358211758f, (faac_real) 0.7194462634f,
     (faac_real) 0.7022388719f, (faac_real) 0.6842353293f, (faac_real) 0.6655139880f, (faac_real) 0.6461269695f,
     (faac_real) 0.6261242695f, (faac_real) 0.6055783538f, (faac_real) 0.5845403235f, (faac_real) 0.5630789140f,
     (faac_real) 0.5412553448f, (faac_real) 0.5191234970f, (faac_real) 0.4967708254f, (faac_real) 0.4742453214f,
     (faac_real) 0.4515996535f, (faac_real) 0.4289119920f, (faac_real) 0.4062317676f, (faac_real) 0.3836350013f,
     (faac_real) 0.3611589903f, (faac_real)-0.3388722693f, (faac_real)-0.3168278913f, (faac_real)-0.2950716717f,
     (faac_real)-0.2736634040f, (faac_real)-0.2526480309f, (faac_real)-0.2320690870f, (faac_real)-0.2119735853f,
     (faac_real)-0.1923966745f, (faac_real)-0.1733808172f, (faac_real)-0.1549607071f, (faac_real)-0.1371551761f,
     (faac_real)-0.1200077984f, (faac_real)-0.1035329531f, (faac_real)-0.0877547536f, (faac_real)-0.0726943300f,
     (faac_real)-0.0583705326f, (faac_real)-0.0447806821f, (faac_real)-0.0319531274f, (faac_real)-0.0198834129f,
     (faac_real)-0.0085711749f, (faac_real) 0.0019765601f, (faac_real) 0.0117623832f, (faac_real) 0.0207997072f,
     (faac_real) 0.0290824006f, (faac_real) 0.0366418116f, (faac_real) 0.0434768782f, (faac_real) 0.0495978676f,
     (faac_real) 0.0550460034f, (faac_real) 0.0598166570f, (faac_real) 0.0639444805f, (faac_real) 0.0674525021f,
     (faac_real) 0.0703533073f, (faac_real) 0.0726774642f, (faac_real) 0.0744664394f, (faac_real) 0.0757305756f,
     (faac_real) 0.0765050718f, (faac_real) 0.0768230011f, (faac_real) 0.0767093490f, (faac_real) 0.0761992479f,
     (faac_real) 0.0753137336f, (faac_real) 0.0741003642f, (faac_real) 0.0725682583f, (faac_real) 0.0707628710f,
     (faac_real) 0.0687043828f, (faac_real) 0.0664367512f, (faac_real) 0.0639715898f, (faac_real) 0.0613455171f,
     (faac_real) 0.0585915683f, (faac_real) 0.0557173648f, (faac_real) 0.0527630746f, (faac_real) 0.0497385755f,
     (faac_real) 0.0466843027f, (faac_real) 0.0436097542f, (faac_real) 0.0405349170f, (faac_real) 0.0374812850f,
     (faac_real) 0.0344620948f, (faac_real) 0.0315017608f, (faac_real) 0.0286072173f, (faac_real) 0.0257875847f,
     (faac_real) 0.0230680169f, (faac_real) 0.0204531793f, (faac_real) 0.0179433381f, (faac_real) 0.0155405553f,
     (faac_real) 0.0132718220f, (faac_real)-0.0111315548f, (faac_real)-0.0091325329f, (faac_real)-0.0072615816f,
     (faac_real)-0.0055337211f, (faac_real)-0.0039401124f, (faac_real)-0.0024826723f, (faac_real)-0.0011568135f,
     (faac_real) 0.0000276045f, (faac_real) 0.0010902329f, (faac_real) 0.0020274176f, (faac_real) 0.0028446757f,
     (faac_real) 0.0035401246f, (faac_real) 0.0041251642f, (faac_real) 0.0046039530f, (faac_real) 0.0049839687f,
     (faac_real) 0.0052715758f, (faac_real) 0.0054753783f, (faac_real) 0.0055917128f, (faac_real) 0.0056389199f,
     (faac_real) 0.0056220643f, (faac_real) 0.0055475714f, (faac_real) 0.0054196775f, (faac_real) 0.0052461166f,
     (faac_real) 0.0050393022f, (faac_real) 0.0047932560f, (faac_real) 0.0045209852f, (faac_real) 0.0042264269f,
     (faac_real) 0.0039207432f, (faac_real) 0.0036008268f, (faac_real) 0.0032739613f, (faac_real) 0.0029469447f,
     (faac_real) 0.0026201758f, (faac_real) 0.0023017254f, (faac_real) 0.0019841140f, (faac_real) 0.0016868083f,
     (faac_real) 0.0013902494f, (faac_real) 0.0011250155f, (faac_real) 0.0008608443f, (faac_real) 0.0006239376f,
     (faac_real) 0.0004026540f, (faac_real) 0.0002043017f, (faac_real) 0.0000134949f, (faac_real)-0.0001446380f,
     (faac_real)-0.0002896981f, (faac_real)-0.0004095121f, (faac_real)-0.0005145572f, (faac_real)-0.0005946118f,
     (faac_real)-0.0006650415f, (faac_real)-0.0007215391f, (faac_real)-0.0007530001f, (faac_real)-0.0007757977f,
     (faac_real)-0.0007803664f, (faac_real)-0.0007834332f, (faac_real)-0.0007681371f, (faac_real)-0.0007440941f,
     (faac_real)-0.0007157736f, (faac_real)-0.0006777690f, (faac_real)-0.0006312493f, (faac_real)-0.0005870930f,
     (faac_real)-0.0005466565f, (faac_real)-0.0005040714f, (faac_real)-0.0004875227f, (faac_real)-0.0005617692f,
};

/* Pre-shuffle the 64 polyphase outputs into a 64-point DCT-IV input. */
static void qmf_pre_shuffle(faac_real *buf, const faac_real *v)
{
    buf[0] = v[0];
    buf[1] = v[1];
    for (int k = 1; k < 31; k += 2) {
        buf[2 * k + 0] = -v[64 - k];
        buf[2 * k + 1] =  v[k + 1];
        buf[2 * k + 2] = -v[63 - k];
        buf[2 * k + 3] =  v[k + 2];
    }
    buf[62] = -v[64 - 31];
    buf[63] =  v[32];
}

/* Post-shuffle the 64 IMDCT outputs into the 32 complex subband samples. */
static void qmf_post_shuffle(faac_real *W_re, faac_real *W_im,
                             const faac_real *y)
{
    for (int k = 0; k < 32; k += 2) {
        W_re[k + 0] = -y[63 - k];
        W_im[k + 0] =  y[k + 0];
        W_re[k + 1] = -y[62 - k];
        W_im[k + 1] =  y[k + 1];
    }
}

/* Naive N=64 IMDCT matching FFmpeg AV_TX_FLOAT_MDCT inv=1, scale=1:
 *   out[n] = Σ_{k=0..63} in[k] * cos(π/64 * (n + 0.5 + 32) * (k + 0.5))
 * Only out[0..63] is consumed downstream. */
static void qmf_imdct64(faac_real *out, const faac_real *in)
{
    const double C = M_PI / 64.0;
    for (int n = 0; n < 64; n++) {
        double s = 0.0;
        for (int k = 0; k < 64; k++)
            s += in[k] * cos(C * (n + 0.5 + 32.0) * (k + 0.5));
        out[n] = (faac_real)s;
    }
}

/* Complex slot output: 32 subbands of (re, im).  Shared with the test
 * harness (tests/qmf_test.c) via sbr_internal.h. */
void qmf_analysis_slot_complex(const SBRInfo *sbr,
                               const faac_real *slot,
                               faac_real *ovl,
                               faac_real *W_re,
                               faac_real *W_im)
{
    (void)sbr;
    memmove(ovl, ovl + 32, (SBR_QMF_OVL_LEN - 32) * sizeof(faac_real));
    memcpy(ovl + SBR_QMF_OVL_LEN - 32, slot, 32 * sizeof(faac_real));

    faac_real z[320];
    for (int k = 0; k < SBR_QMF_OVL_LEN; k++)
        z[k] = sbr_qmf_window_ds320[k] * ovl[SBR_QMF_OVL_LEN - 1 - k];

    faac_real v[64];
    for (int n = 0; n < 64; n++)
        v[n] = z[n] + z[n + 64] + z[n + 128] + z[n + 192] + z[n + 256];

    faac_real t[64], y[64];
    qmf_pre_shuffle(t, v);
    qmf_imdct64(y, t);
    qmf_post_shuffle(W_re, W_im, y);
}

static void qmf_analysis_slot(const SBRInfo *sbr,
                               const faac_real *slot,
                               faac_real *ovl,
                               faac_real *energy)
{
    faac_real re[SBR_QMF_BANDS], im[SBR_QMF_BANDS];
    qmf_analysis_slot_complex(sbr, slot, ovl, re, im);
    for (int k = 0; k < SBR_QMF_BANDS; k++)
        energy[k] = re[k] * re[k] + im[k] * im[k];
}

/* ------------------------------------------------------------------ *
 *  SBRAnalysis
 * ------------------------------------------------------------------ */

void SBRAnalysis(SBRInfo *sbr,
                 faac_real *timeDomain[MAX_CHANNELS],
                 int numChannels,
                 int numSamples)
{
    /* Per-band energy accumulator over the whole frame.
     *
     * numSamples = 2*FRAME_LEN = 2048 full-rate samples.
     * The 32-band analysis QMF produces one slot per 32 input samples,
     * so there are numSamples/32 = 64 analysis QMF slots per SBR frame.
     *
     * The SBR temporal grid uses 64-band SYNTHESIS QMF slots (32 per frame).
     * Each synthesis slot = 64 full-rate samples = 2 analysis slots.
     * Therefore the decoder expects the envelope energy normalised by
     * the number of SYNTHESIS slots (32), i.e. summed over all analysis
     * slots and divided by (numSamples / 64) = 32.
     *
     * Combining time normalisation and the QMF filter energy gain
     * (h_sbr_qmf sum-of-squares ≈ 1.5) into one divisor:
     *   norm = (numSamples / 64) * 1.5   =  32 * 1.5 = 48  (for 2048 samples)
     */
    int num_ana_slots = numSamples / SBR_QMF_BANDS;  /* = 2048/32 = 64 */
    faac_real bandEnergy[MAX_CHANNELS][SBR_QMF_BANDS];
    faac_real slotEnergy[SBR_QMF_BANDS];

    for (int ch = 0; ch < numChannels; ch++) {
        memset(bandEnergy[ch], 0, sizeof(bandEnergy[ch]));

        for (int slot = 0; slot < num_ana_slots; slot++) {
            qmf_analysis_slot(sbr,
                              timeDomain[ch] + slot * SBR_QMF_BANDS,
                              sbr->qmfOvl[ch],
                              slotEnergy);
            for (int k = 0; k < SBR_QMF_BANDS; k++)
                bandEnergy[ch][k] += slotEnergy[k];
        }
    }

    /* Normalise to match the decoder's e_curr coordinate system.
     *
     * The decoder (aacsbr.c sbr_dequant) interprets transmitted level L as:
     *   env_facs = 2^(L/2 + 6)    (for amp_res=0)
     * and sbr_gain_calc computes:
     *   gain = sqrt(env_facs / e_curr)
     * where e_curr = mean(X_high^2, per synthesis half-slot per synthesis subband).
     *
     * Our 32-band analysis has:
     *   - num_ana_slots analysis slots = num synthesis half-slots (1:1 in time)
     *   - Each analysis band covers 2 synthesis subbands
     *
     * So: E_per_synth_subband = total_analysis_energy / (num_ana_slots * 2)
     *                         = total / (num_ana_slots * 2)
     *
     * With this normalisation, the correct level formula is:
     *   L = round(2 * (log2(E) - 6))   -- directly matches 2^(L/2+6) dequant
     */
    /* Normalise per synthesis subband only.
     * decoder e_curr = sum_{slots,subbands} |X_synth|^2 / num_synth_subbands
     * Each analysis band covers 2 synthesis subbands; all time-slot sums are
     * kept intact (no per-slot averaging), so norm = 2 (not num_slots*2). */
    float norm = 65536.0f;
    for (int ch = 0; ch < numChannels; ch++)
        for (int k = 0; k < SBR_QMF_BANDS; k++)
            bandEnergy[ch][k] /= norm;

    /* --- Convert normalised subband energies to envelope levels --- */
    /*
     * FIXFIX + 1 envelope forces amp_res=0 (1.5 dB steps) in the decoder.
     *
     * First band: absolute 7-bit level [0..127].
     * Subsequent bands: signed Huffman-coded delta, range [−60..+60].
     *
     * Energy → level (matches decoder's 2^(L/2+6) dequantisation):
     *   level = round(2 * (log2(E) - 6))
     *
     * This is equivalent to 1.5 dB per step: each level unit = 3.01 dB / 2 ≈ 1.505 dB.
     */
    for (int ch = 0; ch < numChannels; ch++) {
        for (int e = 0; e < SBR_NUM_ENVELOPES; e++) {
            /* SBR_NUM_ENVELOPES == 1: single envelope over the full frame. */
            int prevLevel = -1;
            for (int b = 0; b < sbr->numBands; b++) {
                /* Sum energy over analysis QMF subbands for this SBR band.
                 * bandEdges[] are in 64-band synthesis QMF space (0..63).
                 * Our 32-band analysis QMF maps: analysis_band ≈ synth_band/2. */
                faac_real E = 0;
                int q_ana_start = sbr->bandEdges[b] / 2;
                int q_ana_end   = (sbr->bandEdges[b + 1] + 1) / 2;
                if (q_ana_end <= q_ana_start) q_ana_end = q_ana_start + 1;
                for (int qa = q_ana_start; qa < q_ana_end; qa++) {
                    int qi = (qa < SBR_QMF_BANDS) ? qa : SBR_QMF_BANDS - 1;
                    E += bandEnergy[ch][qi];
                }
                /* Convert energy to level: L = round(2*(log2(E)-6)) */
                double log2E = log2((double)E + 1e-20);
                int level = (int)round(2.0 * (log2E - 6.0));

                if (prevLevel < 0) {
                    /* First band: absolute 7-bit level, range [0, 127] (amp_res=0) */
                    level = clamp_int(level, 0, 127);
                    sbr->envData[ch][e][b] = level;
                } else {
                    /* Remaining bands: delta from previous level */
                    int raw_level = clamp_int(level, 0, 127);
                    int delta = clamp_int(raw_level - prevLevel,
                                         -F_HUFF_ENV_1_5DB_OFFSET,
                                          F_HUFF_ENV_1_5DB_OFFSET);
                    sbr->envData[ch][e][b] = delta;
                    level = prevLevel + delta;
                }
                prevLevel = level;
            }
        }

        /* --- Noise floor estimation -------------------------------- */
        /*
         * Use the mean per-synthesis-slot energy in the noise band as a
         * proxy for the noise floor level.  bandEnergy[] is already
         * normalised per synthesis slot by the normalization above.
         */
        int prevNoise = -1;
        for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
            faac_real meanE = 0;
            int count = 0;
            /* Same synthesis→analysis mapping as envelope estimation */
            int q_ana_start = sbr->noiseBandEdges[nb] / 2;
            int q_ana_end   = (sbr->noiseBandEdges[nb + 1] + 1) / 2;
            if (q_ana_end <= q_ana_start) q_ana_end = q_ana_start + 1;
            for (int qa = q_ana_start; qa < q_ana_end; qa++) {
                int qi = (qa < SBR_QMF_BANDS) ? qa : SBR_QMF_BANDS - 1;
                meanE += bandEnergy[ch][qi];
                count++;
            }
            (void)meanE;
            (void)count;
            /* Empirically Q=0 beats absolute-energy and flatness-proxy variants
             * (+0.05 MOS on gate clip). Absolute QMF analysis energy is not a
             * valid noise-level estimator — the decoder interprets this as a ratio. */
            int level = 0;

            if (prevNoise < 0) {
                sbr->noiseData[ch][nb] = level;
            } else {
                int delta = clamp_int(level - prevNoise, -15, 15);
                sbr->noiseData[ch][nb] = delta;
                level = prevNoise + delta;
            }
            prevNoise = level;
        }
    }
}

/* ------------------------------------------------------------------ *
 *  SBR bitstream writing
 *  ISO 14496-3:2009 §4.6.18.4
 * ------------------------------------------------------------------ */

/* Write/count sbr_header() — ISO 14496-3 §4.6.18.4.1
 *
 * We signal bs_header_extra_1=1 to transmit:
 *   bs_freq_scale=0   → linear master frequency table (numBands = k2−kx)
 *   bs_alter_scale=0  → no alternate scale (required for freq_scale=0)
 *   bs_noise_bands=0  → 1 noise floor band
 * This ensures the decoder computes the same band layout as the encoder.
 */
static int write_sbr_header(SBRInfo *sbr, BitStream *bs, int wf)
{
    int bits = 0;
#define WB(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    WB(sbr->bs_amp_res,    1);
    WB(sbr->bs_start_freq, 4);
    WB(sbr->bs_stop_freq,  4);
    WB(sbr->bs_xover_band, 3);
    WB(0, 2);   /* reserved */
    WB(1, 1);   /* bs_header_extra_1 = 1 → transmit freq scale flags */
    /* bs_header_extra_1 contents: */
    WB(0, 2);   /* bs_freq_scale = 0 (linear: numBands = k2−kx) */
    WB(0, 1);   /* bs_alter_scale = 0 */
    WB(0, 2);   /* bs_noise_bands = 0 → numNoiseBands = 1 */
    WB(0, 1);   /* bs_header_extra_2 = 0 */
#undef WB
    return bits;
}

/*
 * Write/count sbr_grid() — FIXFIX, 1 envelope per frame.
 * ISO 14496-3:2009 §4.6.18.4.3
 */
static int write_sbr_grid(BitStream *bs, int wf)
{
    int bits = 0;
#define WB(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    WB(SBR_FRAME_CLASS_FIXFIX, 2); /* bs_frame_class = FIXFIX */
    WB(0, 2);                      /* bs_num_env = 0 → 1 envelope */
    WB(1, 1);                      /* bs_freq_res[0] = 1 (high-res) */
#undef WB
    return bits;
}

/*
 * Write/count sbr_dtdf() — all frequency-domain (no delta-time).
 * ISO 14496-3:2009 §4.6.18.4.4
 */
static int write_sbr_dtdf(BitStream *bs, int wf)
{
    int bits = 0;
    /* bs_df_env[0] = 0 (frequency-domain) */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    /* bs_df_noise[0] = 0 */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    return bits;
}

/*
 * Write/count sbr_envelope() for one channel.
 * ISO 14496-3:2009 §4.6.18.4.6
 *
 * FIXFIX + 1 envelope forces amp_res=0 (1.5 dB) in the decoder.
 *   First band: 7-bit absolute level 0..127 (decoder reads get_bits(gb, 7))
 *   Rest: Huffman-coded delta using f_huff_env_1_5dB (symbols ±60)
 */
static int write_sbr_envelope(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    for (int e = 0; e < SBR_NUM_ENVELOPES; e++) {
        for (int b = 0; b < sbr->numBands; b++) {
            int val = sbr->envData[ch][e][b];
            if (b == 0) {
                /* First band: 7-bit absolute level (amp_res=0 forced by FIXFIX+1env: 0..127) */
                int abs_val = clamp_int(val, 0, 127);
                if (wf) PutBit(bs, abs_val, 7);
                bits += 7;
            } else {
                /* Delta: Huffman-coded with f_huff_env_1_5dB */
                bits += put_huff(bs, f_huff_env_1_5dB,
                                 F_HUFF_ENV_1_5DB_NSYMS,
                                 F_HUFF_ENV_1_5DB_OFFSET,
                                 val, wf);
            }
        }
    }
    return bits;
}

/*
 * Write/count sbr_noise() for one channel.
 * ISO 14496-3:2009 §4.6.18.4.7
 *
 * First noise band: raw 5 bits (absolute 0..30)
 * Additional bands (if any): Huffman-coded delta using f_huff_env_3_0dB
 * (ISO spec uses the envelope 3.0 dB table for noise floor deltas too.)
 * We only ever have 1 noise band, so only the 5-bit absolute is written.
 */
static int write_sbr_noise(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
        int val = sbr->noiseData[ch][nb];
        if (nb == 0) {
            int abs_val = clamp_int(val, 0, 30);
            if (wf) PutBit(bs, abs_val, 5);
            bits += 5;
        } else {
            bits += put_huff(bs, f_huff_env_3_0dB,
                             F_HUFF_ENV_3_0DB_NSYMS,
                             F_HUFF_ENV_3_0DB_OFFSET,
                             val, wf);
        }
    }
    return bits;
}

/*
 * Write/count sbr_invf() — inverse filtering direction.
 * ISO 14496-3:2009 §4.6.18.4.5
 * One 2-bit mode per noise band (NONE=0, LOW=1, MID=2, HIGH=3).
 * We always use NONE (0) since we're doing no explicit inverse filtering.
 */
static int write_sbr_invf(SBRInfo *sbr, BitStream *bs, int wf)
{
    int bits = 0;
    for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
        if (wf) PutBit(bs, 2, 2);  /* bs_invf_mode = MID (partial decorrelation) */
        bits += 2;
    }
    return bits;
}

/*
 * Write/count one channel's per-frame SBR data:
 *   sbr_grid, sbr_dtdf, sbr_invf, sbr_envelope, sbr_noise,
 *   sbr_sinusoidal_coding (bs_add_harmonic_flag=0).
 */
static int write_sbr_channel_data(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    bits += write_sbr_grid(bs, wf);
    bits += write_sbr_dtdf(bs, wf);
    bits += write_sbr_invf(sbr, bs, wf);
    bits += write_sbr_envelope(sbr, bs, ch, wf);
    bits += write_sbr_noise(sbr, bs, ch, wf);
    /* sbr_sinusoidal_coding: bs_add_harmonic_flag = 0 */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    return bits;
}

/*
 * Write/count sbr_data().
 * ISO 14496-3:2009 §4.6.18.4.2
 *
 * SCE: bs_data_extra(1) + sce_data + bs_extended_data(1)
 * CPE (bs_coupling=0): bs_data_extra(1) + bs_coupling(1)
 *   + grid(ch0), grid(ch1)
 *   + dtdf(ch0), dtdf(ch1)
 *   + invf(ch0), invf(ch1)
 *   + envelope(ch0), envelope(ch1)
 *   + noise(ch0), noise(ch1)
 *   + harmonic(ch0), harmonic(ch1)
 *   + bs_extended_data(1)
 *
 * The interleaved CPE order must match FFmpeg read_sbr_channel_pair_element().
 */
static int write_sbr_data(SBRInfo *sbr, BitStream *bs, int id_aac, int wf)
{
#define WBL(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    int bits = 0;
    WBL(0, 1);  /* bs_data_extra = 0 */

    if (id_aac == ID_CPE) {
        WBL(0, 1);  /* bs_coupling = 0 (independent per-channel coding) */
        /* Interleaved order: grid, dtdf, invf, envelope, noise, harmonic
         * must match FFmpeg read_sbr_channel_pair_element() non-coupling path */
        bits += write_sbr_grid(bs, wf);          /* grid ch0 */
        bits += write_sbr_grid(bs, wf);          /* grid ch1 */
        bits += write_sbr_dtdf(bs, wf);          /* dtdf ch0 */
        bits += write_sbr_dtdf(bs, wf);          /* dtdf ch1 */
        bits += write_sbr_invf(sbr, bs, wf);     /* invf ch0 */
        bits += write_sbr_invf(sbr, bs, wf);     /* invf ch1 */
        bits += write_sbr_envelope(sbr, bs, 0, wf); /* envelope ch0 */
        bits += write_sbr_envelope(sbr, bs, 1, wf); /* envelope ch1 */
        bits += write_sbr_noise(sbr, bs, 0, wf);    /* noise ch0 */
        bits += write_sbr_noise(sbr, bs, 1, wf);    /* noise ch1 */
        /* bs_add_harmonic_flag ch0 and ch1 */
        WBL(0, 1);
        WBL(0, 1);
    } else {
        /* ID_SCE or ID_CCE */
        bits += write_sbr_channel_data(sbr, bs, 0, wf);
    }

    WBL(0, 1);  /* bs_extended_data = 0 */
#undef WBL
    return bits;
}

/* ------------------------------------------------------------------ *
 *  SBRWriteBitstream (public)
 *  ISO 14496-3:2009 §4.6.18.4  sbr_extension_data()
 *
 *  Fill element layout:
 *    ID_FIL (3 bits)
 *    count  (4 bits, or 15 + esc_count(8) if >= 15)
 *    -- fill data (count bytes) --
 *    extension_type (4 bits) = EXT_SBR_DATA = 0xd
 *    bs_header_flag (1 bit)
 *    [sbr_header() if bs_header_flag]
 *    sbr_data(id_aac)
 *    bs_fill_bits (pad to byte boundary)
 * ------------------------------------------------------------------ */

int SBRWriteBitstream(SBRInfo *sbr, BitStream *bs, int id_aac, int writeFlag)
{
    if (!sbr || !sbr->sbrPresent) return 0;

    int sendHeader = (!sbr->headerSent ||
                      (sbr->frameCount % SBR_HEADER_PERIOD == 0));

    /* --- Dry-run to count fill-data bits (ext_type onward) -------- */
    int fillContentBits = 4;  /* extension_type */
    fillContentBits += 1;     /* bs_header_flag */
    if (sendHeader)
        fillContentBits += write_sbr_header(sbr, NULL, 0);
    fillContentBits += write_sbr_data(sbr, NULL, id_aac, 0);

    /* Round up to whole bytes; the fill element 'count' is in bytes */
    int fillBytes = (fillContentBits + 7) / 8;
    int padBits   = fillBytes * 8 - fillContentBits;


#define WB(v,n) do { if (writeFlag) PutBit(bs,(v),(n)); totalBits += (n); } while(0)
    int totalBits = 0;

    /* Fill element header */
    WB(6, 3);  /* ID_FIL */
    if (fillBytes < 15) {
        WB(fillBytes, 4);
    } else {
        WB(15, 4);
        WB(fillBytes - 14, 8);  /* esc_count: actual = esc_count + 14 */
    }

    /* Fill data content */
    WB(SBR_EXT_TYPE_SBR, 4);       /* extension_type */
    WB(sendHeader ? 1 : 0, 1);     /* bs_header_flag */
    if (sendHeader)
        totalBits += write_sbr_header(sbr, writeFlag ? bs : NULL, writeFlag);
    totalBits += write_sbr_data(sbr, writeFlag ? bs : NULL, id_aac, writeFlag);

    /* Padding bits to fill declared byte count */
    if (padBits > 0) {
        WB(0, padBits);
    }
#undef WB

    if (writeFlag) {
        sbr->headerSent = 1;
        sbr->frameCount++;
    }

    return totalBits;
}
