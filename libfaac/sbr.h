/*
 * FAAC - Freeware Advanced Audio Coder
 *
 * HE-AAC v1 Spectral Band Replication (SBR) encoder
 * ISO/IEC 14496-3:2009 §4.6.18
 *
 * All SBR patents have expired (≤2021).  This implementation is
 * non-patent-encumbered.
 *
 * Features implemented:
 *   - 32-band cosine-modulated analysis QMF (64-tap prototype)
 *   - FIXFIX frame class, 1 envelope per frame
 *   - Frequency-domain (delta-freq) envelope and noise-floor coding
 *   - Huffman-coded envelope / noise data
 *   - Backward-compatible AudioSpecificConfig (AOT=2 core + 0x2b7 extension)
 *
 * Intentionally omitted (no patent benefit, added complexity):
 *   - Sinusoidal (harmonic) coding  (bs_add_harmonic_flag = 0)
 *   - Parametric Stereo / HE-AACv2
 *   - CRC protection
 */

#ifndef SBR_H
#define SBR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "faac_real.h"
#include "coder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- SBR frame / band constants ---------------------------------- */
#define SBR_NUM_TIME_SLOTS   32   /* time slots per AAC frame (FRAME_LEN/32) */
#define SBR_QMF_BANDS        32   /* 32-band analysis QMF */
#define SBR_QMF_FILTER_LEN   64   /* prototype filter length */
#define SBR_QMF_OVL_LEN      64   /* overlap buffer length (1 filter length) */
#define SBR_MAX_BANDS        48   /* max SBR frequency bands */
#define SBR_NUM_ENVELOPES     1   /* FIXFIX, bs_num_env=0 → 1 envelope/frame */
#define SBR_MAX_NOISE_BANDS   5   /* max noise-floor bands */
#define SBR_HEADER_PERIOD    30   /* re-send sbr_header every N frames */

/* ---- SBR bitstream frame-class code ------------------------------ */
#define SBR_FRAME_CLASS_FIXFIX  0

/* ---- SBRInfo ----------------------------------------------------- */
typedef struct SBRInfo {
    int sbrPresent;
    int headerSent;
    int frameCount;        /* for periodic sbr_header refresh */
    int numChannels;

    int sampleRate;        /* full Fs (encoder input rate) */
    int coreSampleRate;    /* Fs/2 (core AAC-LC rate) */

    /* Frequency band configuration */
    int kx;                /* first SBR QMF band (crossover) */
    int k2;                /* last  SBR QMF band + 1 */
    int numBands;          /* number of SBR master frequency bands */
    int bandEdges[SBR_MAX_BANDS + 1]; /* QMF band-edge indices */
    int numNoiseBands;
    int noiseBandEdges[SBR_MAX_NOISE_BANDS + 1];

    /* bs_header fields */
    int bs_amp_res;        /* 0 = 1.5 dB/step (low-res), default */
    int bs_start_freq;     /* 0..15 → kx offset */
    int bs_stop_freq;      /* 0..13 or 14/15 (formula) */
    int bs_xover_band;     /* always 0 */

    /* Per-channel QMF analysis state */
    faac_real qmfOvl[MAX_CHANNELS][SBR_QMF_OVL_LEN]; /* overlap buf */

    /* Per-channel estimated data (filled by SBRAnalysis) */
    int envData  [MAX_CHANNELS][SBR_NUM_ENVELOPES][SBR_MAX_BANDS];
    int noiseData[MAX_CHANNELS][SBR_MAX_NOISE_BANDS];

    /* Previous envelope (for delta coding across frames) */
    int prevEnv[MAX_CHANNELS][SBR_MAX_BANDS];

    /* Precomputed QMF modulation tables */
    faac_real cos_table[SBR_QMF_BANDS][SBR_QMF_FILTER_LEN];
    faac_real sin_table[SBR_QMF_BANDS][SBR_QMF_FILTER_LEN];
} SBRInfo;

/* ---- API --------------------------------------------------------- */

/**
 * SBRInit - allocate and initialise SBR state.
 * @channels      : number of audio channels
 * @sampleRate    : full input sample rate (Fs)
 * @coreSampleRate: half-rate used by AAC-LC core (Fs/2)
 *
 * Returns NULL on allocation failure.
 */
SBRInfo *SBRInit(int channels, int sampleRate, int coreSampleRate);

/** SBREnd - free SBR state. */
void SBREnd(SBRInfo *sbr);

/**
 * SBRAnalysis - analyse one AAC frame of full-rate PCM.
 * Must be called BEFORE the 2:1 downsampler.
 *
 * @sbr          : SBR state
 * @timeDomain   : per-channel full-rate sample buffers [MAX_CHANNELS][FRAME_LEN]
 * @numChannels  : active channel count
 */
void SBRAnalysis(SBRInfo *sbr,
                 faac_real *timeDomain[MAX_CHANNELS],
                 int numChannels,
                 int numSamples);

/**
 * SBRWriteBitstream - write (or count) the SBR fill element.
 * Conforms to ISO 14496-3 §4.6.18.4 sbr_extension_data().
 *
 * @sbr       : SBR state
 * @bs        : bitstream handle (NULL when writeFlag=0 for count-only)
 * @id_aac    : channel element type (ID_SCE=0 or ID_CPE=1)
 * @writeFlag : 1 = write bits, 0 = count only (dry run)
 *
 * Returns number of bits written / that would be written.
 */
#include "bitstream.h"
int SBRWriteBitstream(SBRInfo *sbr,
                      BitStream *bs,
                      int id_aac,
                      int writeFlag);

#ifdef __cplusplus
}
#endif

#endif /* SBR_H */
