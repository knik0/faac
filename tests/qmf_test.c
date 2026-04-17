/* QMF analysis harness: compares production qmf_analysis_slot_complex
 * against the standalone oracle (tests/qmf_oracle.c).
 *
 * Pass criterion per the plan: per-band |X[k]|² matches oracle within
 * −40 dBFS relative error.  Until Phase C rewrites production, this test
 * is EXPECTED to fail; running it prints the worst-case gap so progress
 * is measurable. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfaac/sbr.h"
#include "libfaac/sbr_internal.h"
#include "qmf_oracle.h"

#define SLOT   32
#define BANDS  32
#define FS     48000
#define NSLOTS 100

static SBRInfo *make_sbr(void)
{
    /* coreSampleRate arg is unused by the QMF itself; pick Fs/2. */
    SBRInfo *s = SBRInit(1, FS, FS / 2);
    if (!s) { fprintf(stderr, "SBRInit failed\n"); exit(2); }
    return s;
}

/* Drive N_slots of input through both oracle and production; return max
 * per-band relative energy error in dB (−∞ = perfect match). */
static double run_case(const char *name,
                       const faac_real *input,
                       int n_slots)
{
    SBRInfo *sbr = make_sbr();
    float ostate[320] = {0};
    double band_ref_acc[BANDS] = {0};
    double band_err_acc[BANDS] = {0};

    for (int s = 0; s < n_slots; s++) {
        float ore[BANDS], oim[BANDS];
        float oin[SLOT];
        for (int i = 0; i < SLOT; i++) oin[i] = (float)input[s * SLOT + i];
        qmf_ref_slot(oin, ostate, ore, oim);

        faac_real pre[BANDS], pim[BANDS];
        qmf_analysis_slot_complex(sbr,
                                  input + s * SLOT,
                                  sbr->qmfOvl[0],
                                  pre, pim);

        for (int k = 0; k < BANDS; k++) {
            double Eo = (double)ore[k] * ore[k] + (double)oim[k] * oim[k];
            double Ep = (double)pre[k] * pre[k] + (double)pim[k] * pim[k];
            band_ref_acc[k] += Eo;
            double d = Ep - Eo;
            band_err_acc[k] += d * d;
        }
    }

    double worst_db = -1e9;
    int worst_k = -1;
    double total_ref = 0;
    for (int k = 0; k < BANDS; k++) total_ref += band_ref_acc[k];
    double ref_mean = total_ref / BANDS + 1e-30;

    for (int k = 0; k < BANDS; k++) {
        double rms_err = sqrt(band_err_acc[k] / n_slots);
        double db = 20.0 * log10(rms_err / sqrt(ref_mean) + 1e-30);
        if (db > worst_db) { worst_db = db; worst_k = k; }
    }
    printf("  [%s] worst-band err = %+7.2f dB (band %d)\n",
           name, worst_db, worst_k);

    SBREnd(sbr);
    return worst_db;
}

static faac_real *impulse_buf(int n_slots)
{
    faac_real *b = calloc(n_slots * SLOT, sizeof(faac_real));
    b[0] = 1.0f;
    return b;
}

static faac_real *tone_buf(int n_slots, double cycles_per_sample)
{
    int N = n_slots * SLOT;
    faac_real *b = malloc(N * sizeof(faac_real));
    for (int n = 0; n < N; n++)
        b[n] = (faac_real)sin(2.0 * M_PI * cycles_per_sample * n);
    return b;
}

static faac_real *noise_buf(int n_slots)
{
    int N = n_slots * SLOT;
    faac_real *b = malloc(N * sizeof(faac_real));
    unsigned s = 0x12345u;
    for (int n = 0; n < N; n++) {
        s = s * 1103515245u + 12345u;
        double u1 = ((s >> 8) & 0xffff) / 65536.0 + 1e-9;
        s = s * 1103515245u + 12345u;
        double u2 = ((s >> 8) & 0xffff) / 65536.0;
        b[n] = (faac_real)(sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2));
    }
    return b;
}

int main(void)
{
    double threshold_db = -40.0;
    int failed = 0;

    printf("QMF oracle-vs-production harness (threshold %.1f dB)\n",
           threshold_db);

    {
        faac_real *b = impulse_buf(4);
        double w = run_case("impulse", b, 4);
        if (w > threshold_db) failed++;
        free(b);
    }
    {
        /* Band centre k lies at normalised freq (k + 0.5)/64. */
        int ks[] = { 5, 15, 25 };
        for (size_t i = 0; i < sizeof(ks)/sizeof(ks[0]); i++) {
            int k = ks[i];
            char name[32]; snprintf(name, sizeof(name), "tone k=%d", k);
            faac_real *b = tone_buf(NSLOTS, (k + 0.5) / 64.0);
            double w = run_case(name, b, NSLOTS);
            if (w > threshold_db) failed++;
            free(b);
        }
    }
    {
        faac_real *b = noise_buf(NSLOTS);
        double w = run_case("white noise", b, NSLOTS);
        if (w > threshold_db) failed++;
        free(b);
    }

    if (failed) {
        printf("FAIL: %d case(s) above threshold\n", failed);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
