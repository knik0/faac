/*
 * FAAC - Freeware Advanced Audio Coder
 *
 * SBR tables derived from ISO/IEC 14496-3:2009 (Fourth Edition).
 * The prototype filter coefficients and Huffman tables are normative
 * parts of the standard and are reproduced here for implementation
 * purposes.  All SBR patents (Fraunhofer/Dolby) expired no later than
 * 2021 in all major jurisdictions.
 */

#ifndef SBR_TABLES_H
#define SBR_TABLES_H

#include "faac_real.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------
 * 64-tap QMF analysis prototype filter
 * ISO 14496-3:2009 Table C.1 / Table 4.82
 * Used for the 32-band encoder-side analysis QMF.
 * ------------------------------------------------------------------ */
static const faac_real h_sbr_qmf[64] = {
    /* n = 0..31 */
     (faac_real) 0.00000000e+00, (faac_real) 5.36548976e-04,
     (faac_real) 1.49098137e-03, (faac_real) 3.14336202e-03,
     (faac_real) 5.27748948e-03, (faac_real) 7.68975685e-03,
     (faac_real) 1.01109461e-02, (faac_real) 1.20138462e-02,
     (faac_real) 1.28756718e-02, (faac_real) 1.20264939e-02,
     (faac_real) 8.97602006e-03, (faac_real) 2.51825292e-03,
    (faac_real)-7.29687483e-03, (faac_real)-2.08397195e-02,
    (faac_real)-3.62228565e-02, (faac_real)-5.23067017e-02,
    (faac_real)-6.69739674e-02, (faac_real)-7.82829581e-02,
    (faac_real)-8.40448915e-02, (faac_real)-8.21108721e-02,
    (faac_real)-7.07856267e-02, (faac_real)-4.88382545e-02,
    (faac_real)-1.65773682e-02, (faac_real) 2.50142761e-02,
     (faac_real) 7.14696624e-02, (faac_real) 1.24418671e-01,
     (faac_real) 1.80857862e-01, (faac_real) 2.39440231e-01,
     (faac_real) 2.98457860e-01, (faac_real) 3.56453777e-01,
     (faac_real) 4.12082039e-01, (faac_real) 4.64118950e-01,
    /* n = 32..63 (symmetric) */
     (faac_real) 4.64118950e-01, (faac_real) 4.12082039e-01,
     (faac_real) 3.56453777e-01, (faac_real) 2.98457860e-01,
     (faac_real) 2.39440231e-01, (faac_real) 1.80857862e-01,
     (faac_real) 1.24418671e-01, (faac_real) 7.14696624e-02,
     (faac_real) 2.50142761e-02, (faac_real)-1.65773682e-02,
    (faac_real)-4.88382545e-02, (faac_real)-7.07856267e-02,
    (faac_real)-8.21108721e-02, (faac_real)-8.40448915e-02,
    (faac_real)-7.82829581e-02, (faac_real)-6.69739674e-02,
    (faac_real)-5.23067017e-02, (faac_real)-3.62228565e-02,
    (faac_real)-2.08397195e-02, (faac_real)-7.29687483e-03,
     (faac_real) 2.51825292e-03, (faac_real) 8.97602006e-03,
     (faac_real) 1.20264939e-02, (faac_real) 1.28756718e-02,
     (faac_real) 1.20138462e-02, (faac_real) 1.01109461e-02,
     (faac_real) 7.68975685e-03, (faac_real) 5.27748948e-03,
     (faac_real) 3.14336202e-03, (faac_real) 1.49098137e-03,
     (faac_real) 5.36548976e-04, (faac_real) 0.00000000e+00 /* h[62],h[63] */
};

/* ------------------------------------------------------------------
 * SBR start-frequency offset table
 * ISO 14496-3:2009 Table 4.87  (bs_start_freq → k0 offset from start_min)
 * Identical to the FFmpeg/FAAD2 sbr_offset[6][16] table.
 *
 * Row selection by full SBR sample rate:
 *   row 0 : Fs_sbr  = 16000 Hz
 *   row 1 : Fs_sbr  = 22050 Hz
 *   row 2 : Fs_sbr  = 24000 Hz
 *   row 3 : Fs_sbr  = 32000 Hz
 *   row 4 : 44100 ≤ Fs_sbr ≤ 64000 Hz
 *   row 5 : Fs_sbr  > 64000 Hz
 * ------------------------------------------------------------------ */
static const signed char sbr_fs_offset[6][16] = {
    /* 16000   */ {-8,-7,-6,-5,-4,-3,-2,-1, 0, 1, 2, 3, 4, 5, 6, 7},
    /* 22050   */ {-5,-4,-3,-2,-1, 0, 1, 2, 3, 4, 5, 6, 7, 9,11,13},
    /* 24000   */ {-5,-3,-2,-1, 0, 1, 2, 3, 4, 5, 6, 7, 9,11,13,16},
    /* 32000   */ {-6,-4,-2,-1, 0, 1, 2, 3, 4, 5, 6, 7, 9,11,13,16},
    /* 44-64k  */ {-4,-2,-1, 0, 1, 2, 3, 4, 5, 6, 7, 9,11,13,16,20},
    /* >64k    */ {-2,-1, 0, 1, 2, 3, 4, 5, 6, 7, 9,11,13,16,20,24},
};

/* ------------------------------------------------------------------
 * Number of SBR frequency bands per octave (for master frequency
 * band table generation). Used with bs_num_bands (inside header).
 * We hardcode 10 bands/octave – a common midpoint value.
 * ------------------------------------------------------------------ */
#define SBR_NUM_BANDS_PER_OCTAVE 10

/* ------------------------------------------------------------------
 * SBR Huffman encoder tables
 * Derived from ISO 14496-3:2009 Huffman decode trees (FAAD2 / FFmpeg).
 *
 * Format: { code, len }
 *   code : codeword value, MSB at bit (len-1)  — pass directly to PutBit()
 *   len  : codeword length in bits
 *
 * Two tables are defined:
 *   f_huff_env_1_5dB : freq-domain envelope, 1.5 dB resolution (amp_res=0)
 *                      Used for non-coupling channels.
 *                      FIXFIX + 1 envelope forces amp_res=0 per ISO spec.
 *   f_huff_env_3_0dB : freq-domain envelope, 3.0 dB resolution (amp_res=1)
 *                      Also used for freq-domain noise floor coding.
 * ------------------------------------------------------------------ */

/* Encoder entry: { codeword, bit-length } */
typedef struct {
    unsigned int code;
    unsigned char len;
} SBRHuffEntry;

/*
 * f_huff_env_1_5dB — frequency-domain envelope, 1.5 dB steps, non-coupling
 * ISO 14496-3:2009 Table 4.85 (F_huffman_env_1_5dB)
 * Symbols: delta values -60..+60  (121 symbols)
 * Table index = delta + F_HUFF_ENV_1_5DB_OFFSET
 */
#define F_HUFF_ENV_1_5DB_OFFSET  60
#define F_HUFF_ENV_1_5DB_NSYMS   121
static const SBRHuffEntry f_huff_env_1_5dB[F_HUFF_ENV_1_5DB_NSYMS] = {
    /*  -60 */ { 0x0007ffe7u, 19 },
    /*  -59 */ { 0x0007ffe8u, 19 },
    /*  -58 */ { 0x000fffd2u, 20 },
    /*  -57 */ { 0x000fffd3u, 20 },
    /*  -56 */ { 0x000fffd4u, 20 },
    /*  -55 */ { 0x000fffd5u, 20 },
    /*  -54 */ { 0x000fffd6u, 20 },
    /*  -53 */ { 0x000fffd7u, 20 },
    /*  -52 */ { 0x000fffd8u, 20 },
    /*  -51 */ { 0x0007ffdau, 19 },
    /*  -50 */ { 0x000fffd9u, 20 },
    /*  -49 */ { 0x000fffdau, 20 },
    /*  -48 */ { 0x000fffdbu, 20 },
    /*  -47 */ { 0x000fffdcu, 20 },
    /*  -46 */ { 0x0007ffdbu, 19 },
    /*  -45 */ { 0x000fffddu, 20 },
    /*  -44 */ { 0x0007ffdcu, 19 },
    /*  -43 */ { 0x0007ffddu, 19 },
    /*  -42 */ { 0x000fffdeu, 20 },
    /*  -41 */ { 0x0003ffe4u, 18 },
    /*  -40 */ { 0x000fffdfu, 20 },
    /*  -39 */ { 0x000fffe0u, 20 },
    /*  -38 */ { 0x000fffe1u, 20 },
    /*  -37 */ { 0x0007ffdeu, 19 },
    /*  -36 */ { 0x000fffe2u, 20 },
    /*  -35 */ { 0x000fffe3u, 20 },
    /*  -34 */ { 0x000fffe4u, 20 },
    /*  -33 */ { 0x0007ffdfu, 19 },
    /*  -32 */ { 0x000fffe5u, 20 },
    /*  -31 */ { 0x0007ffe0u, 19 },
    /*  -30 */ { 0x0003ffe8u, 18 },
    /*  -29 */ { 0x0007ffe1u, 19 },
    /*  -28 */ { 0x0003ffe0u, 18 },
    /*  -27 */ { 0x0003ffe9u, 18 },
    /*  -26 */ { 0x0001ffefu, 17 },
    /*  -25 */ { 0x0003ffe5u, 18 },
    /*  -24 */ { 0x0001ffecu, 17 },
    /*  -23 */ { 0x0001ffedu, 17 },
    /*  -22 */ { 0x0001ffeeu, 17 },
    /*  -21 */ { 0x0000fff4u, 16 },
    /*  -20 */ { 0x0000fff3u, 16 },
    /*  -19 */ { 0x0000fff0u, 16 },
    /*  -18 */ { 0x00007ff7u, 15 },
    /*  -17 */ { 0x00007ff6u, 15 },
    /*  -16 */ { 0x00003ffau, 14 },
    /*  -15 */ { 0x00001ffau, 13 },
    /*  -14 */ { 0x00001ff9u, 13 },
    /*  -13 */ { 0x00000ffau, 12 },
    /*  -12 */ { 0x00000ff8u, 12 },
    /*  -11 */ { 0x000007f9u, 11 },
    /*  -10 */ { 0x000003fbu, 10 },
    /*   -9 */ { 0x000001fcu,  9 },
    /*   -8 */ { 0x000001fau,  9 },
    /*   -7 */ { 0x000000fbu,  8 },
    /*   -6 */ { 0x0000007cu,  7 },
    /*   -5 */ { 0x0000003cu,  6 },
    /*   -4 */ { 0x0000001cu,  5 },
    /*   -3 */ { 0x0000000cu,  4 },
    /*   -2 */ { 0x00000005u,  3 },
    /*   -1 */ { 0x00000001u,  2 },
    /*    0 */ { 0x00000000u,  2 },
    /*   +1 */ { 0x00000004u,  3 },
    /*   +2 */ { 0x0000000du,  4 },
    /*   +3 */ { 0x0000001du,  5 },
    /*   +4 */ { 0x0000003du,  6 },
    /*   +5 */ { 0x000000fau,  8 },
    /*   +6 */ { 0x000000fcu,  8 },
    /*   +7 */ { 0x000001fbu,  9 },
    /*   +8 */ { 0x000003fau, 10 },
    /*   +9 */ { 0x000007f8u, 11 },
    /*  +10 */ { 0x000007fau, 11 },
    /*  +11 */ { 0x000007fbu, 11 },
    /*  +12 */ { 0x00000ff9u, 12 },
    /*  +13 */ { 0x00000ffbu, 12 },
    /*  +14 */ { 0x00001ff8u, 13 },
    /*  +15 */ { 0x00001ffbu, 13 },
    /*  +16 */ { 0x00003ff8u, 14 },
    /*  +17 */ { 0x00003ff9u, 14 },
    /*  +18 */ { 0x0000fff1u, 16 },
    /*  +19 */ { 0x0000fff2u, 16 },
    /*  +20 */ { 0x0001ffeau, 17 },
    /*  +21 */ { 0x0001ffebu, 17 },
    /*  +22 */ { 0x0003ffe1u, 18 },
    /*  +23 */ { 0x0003ffe2u, 18 },
    /*  +24 */ { 0x0003ffeau, 18 },
    /*  +25 */ { 0x0003ffe3u, 18 },
    /*  +26 */ { 0x0003ffe6u, 18 },
    /*  +27 */ { 0x0003ffe7u, 18 },
    /*  +28 */ { 0x0003ffebu, 18 },
    /*  +29 */ { 0x000fffe6u, 20 },
    /*  +30 */ { 0x0007ffe2u, 19 },
    /*  +31 */ { 0x000fffe7u, 20 },
    /*  +32 */ { 0x000fffe8u, 20 },
    /*  +33 */ { 0x000fffe9u, 20 },
    /*  +34 */ { 0x000fffeau, 20 },
    /*  +35 */ { 0x000fffebu, 20 },
    /*  +36 */ { 0x000fffecu, 20 },
    /*  +37 */ { 0x0007ffe3u, 19 },
    /*  +38 */ { 0x000fffedu, 20 },
    /*  +39 */ { 0x000fffeeu, 20 },
    /*  +40 */ { 0x000fffefu, 20 },
    /*  +41 */ { 0x000ffff0u, 20 },
    /*  +42 */ { 0x0007ffe4u, 19 },
    /*  +43 */ { 0x000ffff1u, 20 },
    /*  +44 */ { 0x0003ffecu, 18 },
    /*  +45 */ { 0x000ffff2u, 20 },
    /*  +46 */ { 0x000ffff3u, 20 },
    /*  +47 */ { 0x0007ffe5u, 19 },
    /*  +48 */ { 0x0007ffe6u, 19 },
    /*  +49 */ { 0x000ffff4u, 20 },
    /*  +50 */ { 0x000ffff5u, 20 },
    /*  +51 */ { 0x000ffff6u, 20 },
    /*  +52 */ { 0x000ffff7u, 20 },
    /*  +53 */ { 0x000ffff8u, 20 },
    /*  +54 */ { 0x000ffff9u, 20 },
    /*  +55 */ { 0x000ffffau, 20 },
    /*  +56 */ { 0x000ffffbu, 20 },
    /*  +57 */ { 0x000ffffcu, 20 },
    /*  +58 */ { 0x000ffffdu, 20 },
    /*  +59 */ { 0x000ffffeu, 20 },
    /*  +60 */ { 0x000fffffu, 20 },
};

/*
 * f_huff_env_3_0dB — frequency-domain envelope, 3.0 dB steps, non-coupling
 * ISO 14496-3:2009 Table 4.87 (F_huffman_env_3_0dB)
 * Also used as the freq-domain noise-floor Huffman table (per ISO spec).
 * Symbols: delta values -31..+31  (63 symbols)
 * Table index = delta + F_HUFF_ENV_3_0DB_OFFSET
 */
#define F_HUFF_ENV_3_0DB_OFFSET  31
#define F_HUFF_ENV_3_0DB_NSYMS   63
static const SBRHuffEntry f_huff_env_3_0dB[F_HUFF_ENV_3_0DB_NSYMS] = {
    /*  -31 */ { 0x000ffff0u, 20 },
    /*  -30 */ { 0x000ffff1u, 20 },
    /*  -29 */ { 0x000ffff2u, 20 },
    /*  -28 */ { 0x000ffff3u, 20 },
    /*  -27 */ { 0x000ffff4u, 20 },
    /*  -26 */ { 0x000ffff5u, 20 },
    /*  -25 */ { 0x000ffff6u, 20 },
    /*  -24 */ { 0x0003fff3u, 18 },
    /*  -23 */ { 0x0007fff5u, 19 },
    /*  -22 */ { 0x0007ffeeu, 19 },
    /*  -21 */ { 0x0007ffefu, 19 },
    /*  -20 */ { 0x0007fff6u, 19 },
    /*  -19 */ { 0x0003fff4u, 18 },
    /*  -18 */ { 0x0003fff2u, 18 },
    /*  -17 */ { 0x000ffff7u, 20 },
    /*  -16 */ { 0x0007fff0u, 19 },
    /*  -15 */ { 0x0001fff5u, 17 },
    /*  -14 */ { 0x0003fff0u, 18 },
    /*  -13 */ { 0x0001fff4u, 17 },
    /*  -12 */ { 0x0000fff7u, 16 },
    /*  -11 */ { 0x0000fff6u, 16 },
    /*  -10 */ { 0x00007ff8u, 15 },
    /*   -9 */ { 0x00003ffbu, 14 },
    /*   -8 */ { 0x00000ffdu, 12 },
    /*   -7 */ { 0x000007fdu, 11 },
    /*   -6 */ { 0x000003fdu, 10 },
    /*   -5 */ { 0x000001fdu,  9 },
    /*   -4 */ { 0x000000fdu,  8 },
    /*   -3 */ { 0x0000003eu,  6 },
    /*   -2 */ { 0x0000000eu,  4 },
    /*   -1 */ { 0x00000002u,  2 },
    /*    0 */ { 0x00000000u,  1 },
    /*   +1 */ { 0x00000006u,  3 },
    /*   +2 */ { 0x0000001eu,  5 },
    /*   +3 */ { 0x000000fcu,  8 },
    /*   +4 */ { 0x000001fcu,  9 },
    /*   +5 */ { 0x000003fcu, 10 },
    /*   +6 */ { 0x000007fcu, 11 },
    /*   +7 */ { 0x00000ffcu, 12 },
    /*   +8 */ { 0x00001ffcu, 13 },
    /*   +9 */ { 0x00003ffau, 14 },
    /*  +10 */ { 0x00007ff9u, 15 },
    /*  +11 */ { 0x00007ffau, 15 },
    /*  +12 */ { 0x0000fff8u, 16 },
    /*  +13 */ { 0x0000fff9u, 16 },
    /*  +14 */ { 0x0001fff6u, 17 },
    /*  +15 */ { 0x0001fff7u, 17 },
    /*  +16 */ { 0x0003fff5u, 18 },
    /*  +17 */ { 0x0003fff6u, 18 },
    /*  +18 */ { 0x0003fff1u, 18 },
    /*  +19 */ { 0x000ffff8u, 20 },
    /*  +20 */ { 0x0007fff1u, 19 },
    /*  +21 */ { 0x0007fff2u, 19 },
    /*  +22 */ { 0x0007fff3u, 19 },
    /*  +23 */ { 0x000ffff9u, 20 },
    /*  +24 */ { 0x0007fff7u, 19 },
    /*  +25 */ { 0x0007fff4u, 19 },
    /*  +26 */ { 0x000ffffau, 20 },
    /*  +27 */ { 0x000ffffbu, 20 },
    /*  +28 */ { 0x000ffffcu, 20 },
    /*  +29 */ { 0x000ffffdu, 20 },
    /*  +30 */ { 0x000ffffeu, 20 },
    /*  +31 */ { 0x000fffffu, 20 },
};

#ifdef __cplusplus
}
#endif

#endif /* SBR_TABLES_H */
