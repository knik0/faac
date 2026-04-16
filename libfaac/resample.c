/*
 * FAAC - Freeware Advanced Audio Coder
 *
 * 2:1 FIR downsampler for HE-AAC core signal preparation.
 *
 * Uses a 31-tap linear-phase half-band FIR anti-alias filter.
 * The filter attenuates frequencies above Fs/4 (the Nyquist of the
 * half-rate output), preventing aliasing in the AAC-LC core encoder.
 *
 * Filter design: Kaiser-windowed sinc, beta=5.0, cut-off = 0.5*Fs/2.
 */

#include <stdlib.h>
#include <string.h>

#include "resample.h"
#include "coder.h"   /* MAX_CHANNELS via config.h */

/* 63-tap Kaiser lowpass anti-alias FIR (β=6.0, fc=11.5 kHz at Fs=48 kHz)
 * DC gain ≈ 1.0; passband flat to 11 kHz; stopband >67 dB above 13 kHz.
 * Designed for 2:1 decimation 48 kHz → 24 kHz for HE-AAC core encoder.
 * Symmetric: h[n] = h[62-n]. */
static const faac_real fir_coeffs[RESAMPLE_FILTER_LEN] = {
     (faac_real) 6.7547e-05f,  (faac_real) 2.4177e-04f,
    (faac_real)-1.3024e-04f,  (faac_real)-5.6920e-04f,
     (faac_real) 1.6010e-04f,  (faac_real) 1.0970e-03f,
    (faac_real)-9.5138e-05f,  (faac_real)-1.8736e-03f,
    (faac_real)-1.5518e-04f,  (faac_real) 2.9367e-03f,
     (faac_real) 7.1261e-04f,  (faac_real)-4.3054e-03f,
    (faac_real)-1.7324e-03f,  (faac_real) 5.9735e-03f,
     (faac_real) 3.4076e-03f,  (faac_real)-7.9054e-03f,
    (faac_real)-5.9805e-03f,  (faac_real) 1.0034e-02f,
     (faac_real) 9.7720e-03f,  (faac_real)-1.2263e-02f,
    (faac_real)-1.5255e-02f,  (faac_real) 1.4473e-02f,
     (faac_real) 2.3243e-02f,  (faac_real)-1.6533e-02f,
    (faac_real)-3.5413e-02f,  (faac_real) 1.8307e-02f,
     (faac_real) 5.6117e-02f,  (faac_real)-1.9675e-02f,
    (faac_real)-1.0143e-01f,  (faac_real) 2.0538e-02f,
     (faac_real) 3.1672e-01f,  (faac_real) 4.7917e-01f,
     (faac_real) 3.1672e-01f,  (faac_real) 2.0538e-02f,
    (faac_real)-1.0143e-01f,  (faac_real)-1.9675e-02f,
     (faac_real) 5.6117e-02f,  (faac_real) 1.8307e-02f,
    (faac_real)-3.5413e-02f,  (faac_real)-1.6533e-02f,
     (faac_real) 2.3243e-02f,  (faac_real) 1.4473e-02f,
    (faac_real)-1.5255e-02f,  (faac_real)-1.2263e-02f,
     (faac_real) 9.7720e-03f,  (faac_real) 1.0034e-02f,
    (faac_real)-5.9805e-03f,  (faac_real)-7.9054e-03f,
     (faac_real) 3.4076e-03f,  (faac_real) 5.9735e-03f,
    (faac_real)-1.7324e-03f,  (faac_real)-4.3054e-03f,
     (faac_real) 7.1261e-04f,  (faac_real) 2.9367e-03f,
    (faac_real)-1.5518e-04f,  (faac_real)-1.8736e-03f,
    (faac_real)-9.5138e-05f,  (faac_real) 1.0970e-03f,
     (faac_real) 1.6010e-04f,  (faac_real)-5.6920e-04f,
    (faac_real)-1.3024e-04f,  (faac_real) 2.4177e-04f,
     (faac_real) 6.7547e-05f
};

Resampler *ResampleOpen(int channels)
{
    Resampler *r = (Resampler *)calloc(1, sizeof(Resampler));
    if (r) r->channels = channels;
    return r;
}

void ResampleClose(Resampler *r)
{
    free(r);
}

int Resample2to1(Resampler *r,
                 faac_real *input[MAX_CHANNELS],
                 int input_len,
                 faac_real *output[MAX_CHANNELS])
{
    int output_len = input_len / 2;

    for (int ch = 0; ch < r->channels; ch++) {
        faac_real *in  = input[ch];
        faac_real *out = output[ch];
        faac_real *hist = r->buf[ch];  /* RESAMPLE_FILTER_LEN history samples */

        for (int i = 0; i < output_len; i++) {
            /* Full-rate index of the output sample centre: 2*i */
            faac_real sum = (faac_real)0;
            for (int j = 0; j < RESAMPLE_FILTER_LEN; j++) {
                int idx = 2 * i - j;
                faac_real val;
                if (idx >= 0) {
                    val = in[idx];
                } else {
                    /* Look into history ring (idx is negative).
                     * Map idx == -(RESAMPLE_FILTER_LEN - 1) -> hist[0]
                     * and idx == -1 -> hist[RESAMPLE_FILTER_LEN - 2].
                     */
                    int hidx = RESAMPLE_FILTER_LEN - 1 + idx;
                    val = (hidx >= 0) ? hist[hidx] : (faac_real)0;
                }
                sum += val * fir_coeffs[j];
            }
            out[i] = sum;
        }

        /* Update history: keep last RESAMPLE_FILTER_LEN input samples */
        if (input_len >= RESAMPLE_FILTER_LEN) {
            memcpy(hist, in + input_len - RESAMPLE_FILTER_LEN,
                   RESAMPLE_FILTER_LEN * sizeof(faac_real));
        } else {
            memmove(hist, hist + input_len,
                    (RESAMPLE_FILTER_LEN - input_len) * sizeof(faac_real));
            memcpy(hist + RESAMPLE_FILTER_LEN - input_len, in,
                   input_len * sizeof(faac_real));
        }
    }

    return output_len;
}
