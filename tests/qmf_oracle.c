/* SBR analysis QMF reference — straight port of FFmpeg aacsbr_template.c
 * sbr_qmf_analysis() for the single-slot case.
 *
 * Five stages:
 *   1. vector_fmul_reverse:  z[k] = window[k] * state[319-k]
 *   2. sum64x5:              z[n] = Σ_{p=0..4} z[n + 64p]   (n=0..63)
 *   3. pre_shuffle:          z[64..127] = permutation of z[0..63]
 *   4. IMDCT (N=64):         z[0..63] from z[64..127]
 *   5. post_shuffle:         (W_re[k], W_im[k]) read from z[0..63]
 *
 * Scale = 1.0 (FFmpeg uses -2*32768 to cancel 1/32768 sample scaling; we
 * keep it at 1.0 since our harness compares oracle-to-production only). */

#include <math.h>
#include <string.h>

#include "qmf_oracle.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* sbr_qmf_window_ds[320] — FFmpeg aacsbrdata.h, verbatim values, float cast. */
static const float sbr_qmf_window_ds[320] = {
    (float)0.0000000000f, (float)-0.0005617692f, (float)-0.0004875227f, (float)-0.0005040714f,
    (float)-0.0005466565f, (float)-0.0005870930f, (float)-0.0006312493f, (float)-0.0006777690f,
    (float)-0.0007157736f, (float)-0.0007440941f, (float)-0.0007681371f, (float)-0.0007834332f,
    (float)-0.0007803664f, (float)-0.0007757977f, (float)-0.0007530001f, (float)-0.0007215391f,
    (float)-0.0006650415f, (float)-0.0005946118f, (float)-0.0005145572f, (float)-0.0004095121f,
    (float)-0.0002896981f, (float)-0.0001446380f, (float)0.0000134949f, (float)0.0002043017f,
    (float)0.0004026540f, (float)0.0006239376f, (float)0.0008608443f, (float)0.0011250155f,
    (float)0.0013902494f, (float)0.0016868083f, (float)0.0019841140f, (float)0.0023017254f,
    (float)0.0026201758f, (float)0.0029469447f, (float)0.0032739613f, (float)0.0036008268f,
    (float)0.0039207432f, (float)0.0042264269f, (float)0.0045209852f, (float)0.0047932560f,
    (float)0.0050393022f, (float)0.0052461166f, (float)0.0054196775f, (float)0.0055475714f,
    (float)0.0056220643f, (float)0.0056389199f, (float)0.0055917128f, (float)0.0054753783f,
    (float)0.0052715758f, (float)0.0049839687f, (float)0.0046039530f, (float)0.0041251642f,
    (float)0.0035401246f, (float)0.0028446757f, (float)0.0020274176f, (float)0.0010902329f,
    (float)0.0000276045f, (float)-0.0011568135f, (float)-0.0024826723f, (float)-0.0039401124f,
    (float)-0.0055337211f, (float)-0.0072615816f, (float)-0.0091325329f, (float)-0.0111315548f,
    (float)0.0132718220f, (float)0.0155405553f, (float)0.0179433381f, (float)0.0204531793f,
    (float)0.0230680169f, (float)0.0257875847f, (float)0.0286072173f, (float)0.0315017608f,
    (float)0.0344620948f, (float)0.0374812850f, (float)0.0405349170f, (float)0.0436097542f,
    (float)0.0466843027f, (float)0.0497385755f, (float)0.0527630746f, (float)0.0557173648f,
    (float)0.0585915683f, (float)0.0613455171f, (float)0.0639715898f, (float)0.0664367512f,
    (float)0.0687043828f, (float)0.0707628710f, (float)0.0725682583f, (float)0.0741003642f,
    (float)0.0753137336f, (float)0.0761992479f, (float)0.0767093490f, (float)0.0768230011f,
    (float)0.0765050718f, (float)0.0757305756f, (float)0.0744664394f, (float)0.0726774642f,
    (float)0.0703533073f, (float)0.0674525021f, (float)0.0639444805f, (float)0.0598166570f,
    (float)0.0550460034f, (float)0.0495978676f, (float)0.0434768782f, (float)0.0366418116f,
    (float)0.0290824006f, (float)0.0207997072f, (float)0.0117623832f, (float)0.0019765601f,
    (float)-0.0085711749f, (float)-0.0198834129f, (float)-0.0319531274f, (float)-0.0447806821f,
    (float)-0.0583705326f, (float)-0.0726943300f, (float)-0.0877547536f, (float)-0.1035329531f,
    (float)-0.1200077984f, (float)-0.1371551761f, (float)-0.1549607071f, (float)-0.1733808172f,
    (float)-0.1923966745f, (float)-0.2119735853f, (float)-0.2320690870f, (float)-0.2526480309f,
    (float)-0.2736634040f, (float)-0.2950716717f, (float)-0.3168278913f, (float)-0.3388722693f,
    (float)0.3611589903f, (float)0.3836350013f, (float)0.4062317676f, (float)0.4289119920f,
    (float)0.4515996535f, (float)0.4742453214f, (float)0.4967708254f, (float)0.5191234970f,
    (float)0.5412553448f, (float)0.5630789140f, (float)0.5845403235f, (float)0.6055783538f,
    (float)0.6261242695f, (float)0.6461269695f, (float)0.6655139880f, (float)0.6842353293f,
    (float)0.7022388719f, (float)0.7194462634f, (float)0.7358211758f, (float)0.7513137456f,
    (float)0.7658674865f, (float)0.7794287519f, (float)0.7919735841f, (float)0.8034485751f,
    (float)0.8138191270f, (float)0.8230419890f, (float)0.8311038457f, (float)0.8379717337f,
    (float)0.8436238281f, (float)0.8480315777f, (float)0.8511971524f, (float)0.8531020949f,
    (float)0.8537385600f, (float)0.8531020949f, (float)0.8511971524f, (float)0.8480315777f,
    (float)0.8436238281f, (float)0.8379717337f, (float)0.8311038457f, (float)0.8230419890f,
    (float)0.8138191270f, (float)0.8034485751f, (float)0.7919735841f, (float)0.7794287519f,
    (float)0.7658674865f, (float)0.7513137456f, (float)0.7358211758f, (float)0.7194462634f,
    (float)0.7022388719f, (float)0.6842353293f, (float)0.6655139880f, (float)0.6461269695f,
    (float)0.6261242695f, (float)0.6055783538f, (float)0.5845403235f, (float)0.5630789140f,
    (float)0.5412553448f, (float)0.5191234970f, (float)0.4967708254f, (float)0.4742453214f,
    (float)0.4515996535f, (float)0.4289119920f, (float)0.4062317676f, (float)0.3836350013f,
    (float)0.3611589903f, (float)-0.3388722693f, (float)-0.3168278913f, (float)-0.2950716717f,
    (float)-0.2736634040f, (float)-0.2526480309f, (float)-0.2320690870f, (float)-0.2119735853f,
    (float)-0.1923966745f, (float)-0.1733808172f, (float)-0.1549607071f, (float)-0.1371551761f,
    (float)-0.1200077984f, (float)-0.1035329531f, (float)-0.0877547536f, (float)-0.0726943300f,
    (float)-0.0583705326f, (float)-0.0447806821f, (float)-0.0319531274f, (float)-0.0198834129f,
    (float)-0.0085711749f, (float)0.0019765601f, (float)0.0117623832f, (float)0.0207997072f,
    (float)0.0290824006f, (float)0.0366418116f, (float)0.0434768782f, (float)0.0495978676f,
    (float)0.0550460034f, (float)0.0598166570f, (float)0.0639444805f, (float)0.0674525021f,
    (float)0.0703533073f, (float)0.0726774642f, (float)0.0744664394f, (float)0.0757305756f,
    (float)0.0765050718f, (float)0.0768230011f, (float)0.0767093490f, (float)0.0761992479f,
    (float)0.0753137336f, (float)0.0741003642f, (float)0.0725682583f, (float)0.0707628710f,
    (float)0.0687043828f, (float)0.0664367512f, (float)0.0639715898f, (float)0.0613455171f,
    (float)0.0585915683f, (float)0.0557173648f, (float)0.0527630746f, (float)0.0497385755f,
    (float)0.0466843027f, (float)0.0436097542f, (float)0.0405349170f, (float)0.0374812850f,
    (float)0.0344620948f, (float)0.0315017608f, (float)0.0286072173f, (float)0.0257875847f,
    (float)0.0230680169f, (float)0.0204531793f, (float)0.0179433381f, (float)0.0155405553f,
    (float)0.0132718220f, (float)-0.0111315548f, (float)-0.0091325329f, (float)-0.0072615816f,
    (float)-0.0055337211f, (float)-0.0039401124f, (float)-0.0024826723f, (float)-0.0011568135f,
    (float)0.0000276045f, (float)0.0010902329f, (float)0.0020274176f, (float)0.0028446757f,
    (float)0.0035401246f, (float)0.0041251642f, (float)0.0046039530f, (float)0.0049839687f,
    (float)0.0052715758f, (float)0.0054753783f, (float)0.0055917128f, (float)0.0056389199f,
    (float)0.0056220643f, (float)0.0055475714f, (float)0.0054196775f, (float)0.0052461166f,
    (float)0.0050393022f, (float)0.0047932560f, (float)0.0045209852f, (float)0.0042264269f,
    (float)0.0039207432f, (float)0.0036008268f, (float)0.0032739613f, (float)0.0029469447f,
    (float)0.0026201758f, (float)0.0023017254f, (float)0.0019841140f, (float)0.0016868083f,
    (float)0.0013902494f, (float)0.0011250155f, (float)0.0008608443f, (float)0.0006239376f,
    (float)0.0004026540f, (float)0.0002043017f, (float)0.0000134949f, (float)-0.0001446380f,
    (float)-0.0002896981f, (float)-0.0004095121f, (float)-0.0005145572f, (float)-0.0005946118f,
    (float)-0.0006650415f, (float)-0.0007215391f, (float)-0.0007530001f, (float)-0.0007757977f,
    (float)-0.0007803664f, (float)-0.0007834332f, (float)-0.0007681371f, (float)-0.0007440941f,
    (float)-0.0007157736f, (float)-0.0006777690f, (float)-0.0006312493f, (float)-0.0005870930f,
    (float)-0.0005466565f, (float)-0.0005040714f, (float)-0.0004875227f, (float)-0.0005617692f,
};

/* pre_shuffle: 64 in z[0..63] → 64 out in z[64..127].  FFmpeg sbrdsp.c. */
static void pre_shuffle(float *z)
{
    float buf[64];
    buf[0] = z[0];
    buf[1] = z[1];
    for (int k = 1; k < 31; k += 2) {
        buf[2 * k + 0] = -z[64 - k];
        buf[2 * k + 1] =  z[k + 1];
        buf[2 * k + 2] = -z[63 - k];
        buf[2 * k + 3] =  z[k + 2];
    }
    buf[2 * 31 + 0] = -z[64 - 31];
    buf[2 * 31 + 1] =  z[31 + 1];
    memcpy(z + 64, buf, sizeof(buf));
}

/* post_shuffle: reads z[0..63], writes W[0..31][0,1].  FFmpeg sbrdsp.c. */
static void post_shuffle(float W_re[32], float W_im[32], const float *z)
{
    /* Wi interleaved re/im; written as pairs at offsets 2k and 2k+2. */
    float W[64];
    for (int k = 0; k < 32; k += 2) {
        W[2 * k + 0] = -z[63 - k];
        W[2 * k + 1] =  z[k + 0];
        W[2 * k + 2] = -z[62 - k];
        W[2 * k + 3] =  z[k + 1];
    }
    for (int k = 0; k < 32; k++) {
        W_re[k] = W[2 * k + 0];
        W_im[k] = W[2 * k + 1];
    }
}

/* Naive IMDCT, N=64: in[0..63] → out[0..127].
 * Matches FFmpeg AV_TX_FLOAT_MDCT forward=inv=1 definition (scale=1):
 *   out[n] = Σ_{k=0..N-1} in[k] * cos(π/N * (n + 0.5 + N/2) * (k + 0.5))
 * Only out[0..63] is read by post_shuffle; we compute all 128 for clarity. */
static void imdct64(const float *in, float *out)
{
    const int N = 64;
    const double C = M_PI / N;
    for (int n = 0; n < 2 * N; n++) {
        double s = 0.0;
        for (int k = 0; k < N; k++) {
            s += in[k] * cos(C * (n + 0.5 + N / 2.0) * (k + 0.5));
        }
        out[n] = (float)s;
    }
}

void qmf_ref_slot(const float input[32],
                  float state[320],
                  float W_re[32],
                  float W_im[32])
{
    /* Shift state and append new input. */
    memmove(state, state + 32, (320 - 32) * sizeof(float));
    memcpy(state + 288, input, 32 * sizeof(float));

    /* Stage 1: vector_fmul_reverse over the whole 320-tap buffer. */
    float z[128];  /* first 64 used by sum64x5 / MDCT input; last 64 scratch. */
    float zfull[320];
    for (int k = 0; k < 320; k++) {
        zfull[k] = sbr_qmf_window_ds[k] * state[319 - k];
    }

    /* Stage 2: sum64x5. */
    for (int n = 0; n < 64; n++) {
        z[n] = zfull[n] + zfull[n + 64] + zfull[n + 128]
             + zfull[n + 192] + zfull[n + 256];
    }

    /* Stage 3: pre_shuffle (writes into z[64..127]). */
    pre_shuffle(z);

    /* Stage 4: IMDCT N=64. Input z[64..127] (64 vals) → output z[0..127]. */
    float mdct_out[128];
    imdct64(z + 64, mdct_out);

    /* Stage 5: post_shuffle reads first 64 values of mdct output. */
    post_shuffle(W_re, W_im, mdct_out);
}
