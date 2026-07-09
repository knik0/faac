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
 */

/*
 * libfaac encoder API.
 *
 * This is the only public surface for the encoder. The legacy faacEnc* API
 * (faacEncOpen/faacEncGetCurrentConfiguration/faacEncSetConfiguration/
 * faacEncEncode/faacEncClose) has been removed; see docs/libfaac.html for a
 * porting guide.
 *
 * Design summary (see docs/libfaac.html for the full narrative):
 *   - Parameters are supplied once, up front, to faac_encoder_open(), so the
 *     encoder never exists in a half-configured state and all derived values
 *     (frame size, output-buffer bound, effective sample rate, resolved object
 *     type) are known and queryable the instant open() returns.
 *   - Every fallible call returns a faac_status; faac_strerror() maps a status
 *     to a human-readable string.
 *   - Fixed-width integer types and width-pinned enums keep the ABI identical
 *     across LP64/LLP64 platforms and regardless of -fshort-enums.
 *   - faac_params grows only additively: new releases append named fields and
 *     grow sizeof(faac_params); callers MUST zero-initialize via
 *     faac_params_init() so struct_size lets the library reconcile versions.
 */

#ifndef FAAC_H
#define FAAC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Export/visibility marker. Shared with <faac.h>; guarded so including both
 * headers is harmless. */
#if !defined(FAACAPI) && defined(__GNUC__) && (__GNUC__ >= 4)
# if defined(_WIN32)
#  define FAACAPI __stdcall __declspec(dllexport)
# else
#  define FAACAPI __attribute__((visibility("default")))
# endif
#endif
#ifndef FAACAPI
#  define FAACAPI
#endif

/* Opaque encoder handle */
typedef struct faac_encoder faac_encoder;

/*
 * Result codes. All values are negative except FAAC_OK so a caller can test
 * `status < 0` for failure. The FAAC_STATUS_MAX sentinel pins the enum to a
 * 32-bit underlying type for a stable ABI under -fshort-enums.
 */
typedef enum faac_status {
    FAAC_OK                   = 0,
    FAAC_ERR_INVALID_ARGUMENT = -1,  /* NULL pointer, bad struct_size, or bad enum/field value */
    FAAC_ERR_UNSUPPORTED      = -2,  /* object type not implemented, or MPEG-2 ASC requested */
    FAAC_ERR_NO_MEMORY        = -3,  /* allocation failed */
    FAAC_ERR_OUTPUT_TOO_SMALL = -4,  /* output buffer smaller than max_output_bytes */
    FAAC_ERR_INPUT_OVERFLOW   = -5,  /* in_samples exceeded the per-call frame capacity */
    FAAC_ERR_INTERNAL         = -6,  /* unexpected failure inside the encoder core */
    FAAC_STATUS_MAX           = 0x7fffffff
} faac_status;

/*
 * AAC object type, numbered per the MPEG-4 Audio Object Type (AOT) registry so
 * a single collision-free namespace covers current and future profiles.
 * FAAC_OBJ_AUTO lets the library pick AAC-LC or HE-AAC by bitrate.
 */
enum faac_object_type {
    FAAC_OBJ_AUTO      = 0,          /* let the library choose LC or HE-AAC by bitrate */
    FAAC_OBJ_LOW       = 2,          /* AAC-LC                                       */
    FAAC_OBJ_HE_AAC_V1 = 5,          /* AAC-LC + SBR                                 */
    FAAC_OBJ_HE_AAC_V2 = 29,         /* AAC-LC + SBR + PS  (reserved, unimplemented) */
    /* AOT 23 (LD), 39 (ELD), 42 (xHE-AAC/USAC) are intentionally NOT reserved
     * here; each would need its own additive configuration surface. */
    FAAC_OBJ_MAX       = 0x7fffffff
};

enum faac_mpeg_version {
    FAAC_MPEG4 = 0,
    FAAC_MPEG2 = 1,
    FAAC_MPEG_MAX = 0x7fffffff
};

enum faac_joint_mode {
    FAAC_JOINT_NONE  = 0,            /* independent (L/R) stereo         */
    FAAC_JOINT_MS,                   /* mid/side                          */
    FAAC_JOINT_IS,                   /* intensity                         */
    FAAC_JOINT_MIXED,                /* per-band mix of M/S and intensity */
    FAAC_JOINT_MAX = 0x7fffffff
};

enum faac_shortctl_mode {
    FAAC_SHORTCTL_NORMAL  = 0,       /* let block switching decide       */
    FAAC_SHORTCTL_NOSHORT,           /* force long blocks only           */
    FAAC_SHORTCTL_NOLONG,            /* force short blocks only           */
    FAAC_SHORTCTL_MAX = 0x7fffffff
};

enum faac_stream_format {
    FAAC_STREAM_RAW  = 0,            /* raw AAC frames (needs out-of-band ASC) */
    FAAC_STREAM_ADTS = 1,            /* self-framing ADTS                       */
    FAAC_STREAM_MAX  = 0x7fffffff
};

/* Interpretation of the interleaved PCM handed to faac_encoder_encode(). */
enum faac_input_format {
    FAAC_INPUT_NULL  = 0,            /* invalid / unset                          */
    FAAC_INPUT_16BIT,                /* native-endian int16                      */
    FAAC_INPUT_24BIT,                /* native-endian int24 in 24 bits (unimpl.) */
    FAAC_INPUT_32BIT,                /* native-endian int24 in 32 bits           */
    FAAC_INPUT_FLOAT,                /* 32-bit float                             */
    FAAC_INPUT_MAX = 0x7fffffff
};

/*
 * Encoder parameters, supplied once to faac_encoder_open().
 *
 * ALWAYS initialize with faac_params_init() before setting fields: it zeroes
 * the whole struct (including padding) and stamps struct_size, which is how the
 * library stays compatible as this struct grows. Construct one from scratch and
 * open() will reject it. The struct only ever grows by appending named fields.
 */
typedef struct faac_params {
    uint32_t                struct_size;   /* set by faac_params_init to sizeof(faac_params) */

    uint32_t                sample_rate;   /* input/output sample rate in Hz (required)      */
    uint32_t                num_channels;  /* channel count, 1..max_channels (required)       */

    enum faac_mpeg_version  mpeg_version;
    enum faac_object_type   object_type;
    enum faac_joint_mode    joint_mode;

    bool                    use_lfe;       /* treat the last channel as LFE (>= 6 ch) */
    bool                    use_tns;       /* temporal noise shaping                   */
    uint8_t                 reserved[2];   /* explicit pad; must remain 0              */

    uint32_t                bit_rate;      /* target bits/sec PER CHANNEL; 0 = use quant_quality */
    uint32_t                bandwidth;     /* cutoff in Hz; 0 = derive from bit_rate             */
    uint32_t                quant_quality; /* quantizer quality; 0 = derive from bit_rate        */

    enum faac_stream_format output_format;
    enum faac_input_format  input_format;
    enum faac_shortctl_mode short_control;

    int32_t                 pns_level;     /* perceptual noise substitution, 0..10 (0 = off) */

    const int32_t          *channel_map;   /* optional reorder table, num_channels entries;
                                            * NULL = identity. Caller-owned; copied by open(). */
    uint32_t                channel_map_count; /* entries in channel_map (0 if NULL) */

    uint32_t                reserved_trailing; /* explicit pad to 8-byte boundary; must be 0 */
} faac_params;

/*
 * Resolved encoder properties, filled by faac_encoder_get_info(). All values are
 * final: the object type is resolved (FAAC_OBJ_AUTO becomes a concrete type),
 * auto-derived rate-control defaults are filled in, and buffer sizes account for
 * the object type (e.g. HE-AAC's 2048-sample frame and full output rate). Set
 * struct_size to sizeof(faac_encoder_info) before the call; the struct grows
 * only by appending fields, so a newer library stays compatible with an older
 * caller's smaller struct.
 */
typedef struct faac_encoder_info {
    uint32_t                struct_size;      /* set by caller to sizeof(faac_encoder_info) */

    uint32_t                frame_samples;    /* samples/channel per full frame (1024 LC, 2048 HE) */
    uint32_t                max_output_bytes; /* upper bound on one encode() call's output          */
    uint32_t                sample_rate;      /* nominal full output rate in Hz (HE: extended rate) */
    enum faac_object_type   object_type;      /* concrete type in effect (AUTO resolved)            */

    uint32_t                bit_rate;         /* resolved bits/sec per channel (0 if quality-driven) */
    uint32_t                bandwidth;        /* resolved cutoff in Hz                               */
    uint32_t                quant_quality;    /* resolved quantizer quality                          */
    int32_t                 pns_level;        /* resolved PNS level, 0..10                           */
} faac_encoder_info;

/*
 * Library-global facts, independent of any encoder instance: the compiled
 * channel ceiling, the version/copyright strings, and the SBR analysis density.
 * These reflect build-time options (max-channels, sbr-decimation) that the
 * header alone cannot report, so they are queried at runtime. Set
 * out->struct_size to sizeof(faac_library_info) before the call; the struct
 * grows only by appending fields, so a newer library stays compatible with an
 * older caller's smaller struct. version/copyright are static and library-owned;
 * do not free them.
 */
typedef struct faac_library_info {
    uint32_t                struct_size;    /* set by caller to sizeof(faac_library_info) */

    uint32_t                max_channels;   /* highest num_channels this build accepts */

    const char             *version;        /* library version string   (library-owned) */
    const char             *copyright;      /* library copyright string (library-owned) */

    uint32_t                sbr_decimation; /* HE-AAC SBR analysis density: every Nth slot
                                             * is analysed (1 = full quality). Build-time. */
} faac_library_info;

FAACAPI faac_status faac_get_library_info(faac_library_info *out);

/* Zero-initialize *p and fill in library defaults and struct_size. Returns
 * FAAC_ERR_INVALID_ARGUMENT if p is NULL. */
FAACAPI faac_status faac_params_init(faac_params *p);

/*
 * Create an encoder from a fully-specified faac_params. On success writes the
 * new handle to *out and returns FAAC_OK. On failure *out is set to NULL and a
 * negative status describes why (e.g. FAAC_ERR_UNSUPPORTED for an object type
 * this build does not implement). The faac_params is consumed by this call and
 * need not outlive it.
 */
FAACAPI faac_status faac_encoder_open(const faac_params *p, faac_encoder **out);

/*
 * Destroy an encoder. Pass the address of your handle; on success the handle is
 * set to NULL to help guard against use-after-free. Passing a pointer to a NULL
 * handle is a no-op that returns FAAC_OK.
 */
FAACAPI faac_status faac_encoder_close(faac_encoder **enc);

/*
 * Query the resolved encoder properties. Set out->struct_size to
 * sizeof(faac_encoder_info) first; the library fills the fields (writing at most
 * out->struct_size bytes, so a newer library is safe against an older caller's
 * struct) and updates struct_size to the number of bytes populated. Returns
 * FAAC_ERR_INVALID_ARGUMENT if enc or out is NULL, or struct_size is too small.
 * Use these values to size the input/output buffers and to report the effective
 * configuration back to the user.
 */
FAACAPI faac_status faac_encoder_get_info(faac_encoder *enc, faac_encoder_info *out);

/*
 * Return the AudioSpecificConfig (a.k.a. decoder-specific info) for this
 * encoder via *buf / *len. The buffer is library-owned and valid until the
 * encoder is closed; do NOT free it. Returns FAAC_ERR_UNSUPPORTED for
 * configurations that have no ASC (e.g. MPEG-2).
 */
FAACAPI faac_status faac_encoder_asc(faac_encoder *enc,
                                     const uint8_t **buf, uint32_t *len);

/*
 * Encode. `in` points to `in_samples` interleaved PCM samples (total across all
 * channels, i.e. samples-per-channel * num_channels), formatted per
 * input_format; it may be any count from 0 to frame_samples*num_channels. The
 * library buffers internally and emits at most one frame per call, writing up
 * to out_cap bytes to `out` and the byte count to *bytes_written (which may be
 * 0 while priming, accumulating, or flushing). Pass in_samples == 0 to flush at
 * end of stream; keep calling until *bytes_written is 0.
 */
FAACAPI faac_status faac_encoder_encode(faac_encoder *enc,
                                        const void *in, uint32_t in_samples,
                                        uint8_t *out, uint32_t out_cap,
                                        uint32_t *bytes_written);

/* Human-readable, static description of a status code. Never returns NULL. */
FAACAPI const char *faac_strerror(faac_status status);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAAC_H */
