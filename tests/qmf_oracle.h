/* SBR analysis QMF reference — ported from FFmpeg aacsbr_template.c.
 * Self-contained (libm only); naive O(N^2) IMDCT replaces FFmpeg's fast path. */
#ifndef QMF_ORACLE_H
#define QMF_ORACLE_H

/* Processes one slot: 32 new input samples -> 32 complex subband outputs.
 * state[320] must be zero-initialised on the first call and is updated
 * in place.  Output is unscaled (scale factor = 1.0). */
void qmf_ref_slot(const float input[32],
                  float state[320],
                  float W_re[32],
                  float W_im[32]);

#endif
