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
 * The public faac_* encoder API (see include/faac.h).
 *
 * This is a thin facade over the private encoder core in frame.c (declared in
 * faac_internal.h): it supplies the params-at-open entry point, uniform
 * faac_status error reporting, resolved-property queries, and library-owned
 * ASC. The opaque faac_encoder* is the core's faacEncStruct*.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <faac.h>
#include "frame.h"
#include "bitstream.h"
#include "sbr.h"

/* The public enums are width-pinned to 32 bits by their FAAC_*_MAX sentinels;
 * verify the compiler honored that so the ABI matches the documented layout. */
_Static_assert(sizeof(enum faac_status)        == 4, "faac_status must be 32-bit");
_Static_assert(sizeof(enum faac_object_type)   == 4, "faac_object_type must be 32-bit");
_Static_assert(sizeof(enum faac_mpeg_version)  == 4, "faac_mpeg_version must be 32-bit");
_Static_assert(sizeof(enum faac_joint_mode)    == 4, "faac_joint_mode must be 32-bit");
_Static_assert(sizeof(enum faac_shortctl_mode) == 4, "faac_shortctl_mode must be 32-bit");
_Static_assert(sizeof(enum faac_stream_format) == 4, "faac_stream_format must be 32-bit");
_Static_assert(sizeof(enum faac_input_format)  == 4, "faac_input_format must be 32-bit");

/* The modern enums mirror the legacy numeric values one-for-one, so parameter
 * translation is a plain field copy. Guard that assumption. */
_Static_assert((int)FAAC_MPEG4 == MPEG4 && (int)FAAC_MPEG2 == MPEG2, "mpeg version drift");
_Static_assert((int)FAAC_OBJ_AUTO == AUTO && (int)FAAC_OBJ_LOW == LOW
               && (int)FAAC_OBJ_HE_AAC_V1 == HE_V1, "object type drift");
_Static_assert((int)FAAC_JOINT_NONE == JOINT_NONE && (int)FAAC_JOINT_MIXED == JOINT_MIXED, "joint mode drift");
_Static_assert((int)FAAC_SHORTCTL_NORMAL == SHORTCTL_NORMAL && (int)FAAC_SHORTCTL_NOLONG == SHORTCTL_NOLONG, "shortctl drift");
_Static_assert((int)FAAC_STREAM_RAW == RAW_STREAM && (int)FAAC_STREAM_ADTS == ADTS_STREAM, "stream format drift");
_Static_assert((int)FAAC_INPUT_NULL  == INPUT_NULL  && (int)FAAC_INPUT_16BIT == INPUT_16BIT
            && (int)FAAC_INPUT_24BIT == INPUT_24BIT && (int)FAAC_INPUT_32BIT == INPUT_32BIT
            && (int)FAAC_INPUT_FLOAT == INPUT_FLOAT, "input format drift");

/* faac_encoder* and faacEncHandle are the same underlying object. */
static inline faacEncStruct *unwrap(faac_encoder *enc) { return (faacEncStruct *)enc; }

FAACAPI faac_status faac_get_library_info(faac_library_info *out)
{
    faac_library_info info;
    char *vid = NULL, *vcopy = NULL;
    uint32_t caller_size, n;

    if (!out)
        return FAAC_ERR_INVALID_ARGUMENT;
    caller_size = out->struct_size;
    if (caller_size < sizeof(info.struct_size))
        return FAAC_ERR_INVALID_ARGUMENT;

    faacEncGetVersion(&vid, &vcopy);

    memset(&info, 0, sizeof(info));
    info.max_channels   = (uint32_t)MAX_CHANNELS;
    info.version        = vid;
    info.copyright      = vcopy;
    info.sbr_decimation = (uint32_t)FAAC_SBR_DECIMATION;

    /* Write at most the caller's struct_size so a newer library cannot overrun
     * an older, smaller faac_library_info; report the byte count actually set. */
    n = caller_size < (uint32_t)sizeof(info) ? caller_size : (uint32_t)sizeof(info);
    info.struct_size = n;
    memcpy(out, &info, n);
    return FAAC_OK;
}

FAACAPI faac_status faac_params_init(faac_params *p)
{
    if (!p)
        return FAAC_ERR_INVALID_ARGUMENT;

    memset(p, 0, sizeof(*p));
    p->struct_size   = (uint32_t)sizeof(faac_params);
    p->mpeg_version  = FAAC_MPEG4;
    p->object_type   = FAAC_OBJ_LOW;
    p->joint_mode    = FAAC_JOINT_MIXED;
    p->use_lfe       = false;
    p->use_tns       = false;
    p->bit_rate      = 64000;          /* per channel; 0 would defer to quant_quality */
    p->bandwidth     = 0;              /* derive from bit_rate */
    p->quant_quality = 0;              /* derive from bit_rate */
    p->output_format = FAAC_STREAM_ADTS;
    p->input_format  = FAAC_INPUT_16BIT;
    p->short_control = FAAC_SHORTCTL_NORMAL;
    p->pns_level     = 4;
    return FAAC_OK;
}

/* Validate the enumerated/range fields the caller supplied. Returns FAAC_OK,
 * FAAC_ERR_INVALID_ARGUMENT for out-of-range values, or FAAC_ERR_UNSUPPORTED
 * for values that are valid but not implemented in this build. */
static faac_status validate_params(const faac_params *p)
{
    switch (p->mpeg_version) {
        case FAAC_MPEG4: case FAAC_MPEG2: break;
        default: return FAAC_ERR_INVALID_ARGUMENT;
    }
    switch (p->object_type) {
        case FAAC_OBJ_AUTO: case FAAC_OBJ_LOW: case FAAC_OBJ_HE_AAC_V1:
            break;
        case FAAC_OBJ_HE_AAC_V2:
            return FAAC_ERR_UNSUPPORTED;   /* parametric stereo not implemented */
        default:
            return FAAC_ERR_INVALID_ARGUMENT;
    }
    switch (p->joint_mode) {
        case FAAC_JOINT_NONE: case FAAC_JOINT_MS:
        case FAAC_JOINT_IS:   case FAAC_JOINT_MIXED: break;
        default: return FAAC_ERR_INVALID_ARGUMENT;
    }
    switch (p->short_control) {
        case FAAC_SHORTCTL_NORMAL: case FAAC_SHORTCTL_NOSHORT:
        case FAAC_SHORTCTL_NOLONG: break;
        default: return FAAC_ERR_INVALID_ARGUMENT;
    }
    switch (p->output_format) {
        case FAAC_STREAM_RAW: case FAAC_STREAM_ADTS: break;
        default: return FAAC_ERR_INVALID_ARGUMENT;
    }
    switch (p->input_format) {
        case FAAC_INPUT_16BIT: case FAAC_INPUT_32BIT: case FAAC_INPUT_FLOAT:
            break;
        case FAAC_INPUT_24BIT:
            return FAAC_ERR_UNSUPPORTED;   /* 24-in-24 not implemented */
        default:
            return FAAC_ERR_INVALID_ARGUMENT;
    }
    if (p->sample_rate == 0)
        return FAAC_ERR_INVALID_ARGUMENT;
    if (p->num_channels < 1 || p->num_channels > (uint32_t)MAX_CHANNELS)
        return FAAC_ERR_INVALID_ARGUMENT;
    if (p->pns_level < 0 || p->pns_level > 10)
        return FAAC_ERR_INVALID_ARGUMENT;
    if (p->channel_map) {
        uint32_t i;
        if (p->channel_map_count < p->num_channels)
            return FAAC_ERR_INVALID_ARGUMENT;
        for (i = 0; i < p->num_channels; i++)
            if (p->channel_map[i] < 0 || (uint32_t)p->channel_map[i] >= p->num_channels)
                return FAAC_ERR_INVALID_ARGUMENT;
    }
    /* reserved padding must be zero so future fields can claim it safely */
    if (p->reserved[0] || p->reserved[1] || p->reserved_trailing)
        return FAAC_ERR_INVALID_ARGUMENT;
    return FAAC_OK;
}

FAACAPI faac_status faac_encoder_open(const faac_params *p, faac_encoder **out)
{
    faacEncStruct *h;
    faacEncConfiguration *cfg;
    unsigned long inSamples = 0, maxOut = 0;
    faac_status st;

    if (!out)
        return FAAC_ERR_INVALID_ARGUMENT;
    *out = NULL;
    if (!p)
        return FAAC_ERR_INVALID_ARGUMENT;

    /* struct_size lets the library reconcile a caller compiled against a
     * different header revision. A caller must be at least as new as this
     * build's struct (older/garbage/zero is rejected); newer callers are read
     * only through the fields this build knows. */
    if (p->struct_size < sizeof(faac_params))
        return FAAC_ERR_INVALID_ARGUMENT;

    st = validate_params(p);
    if (st != FAAC_OK)
        return st;

    h = (faacEncStruct *)faacEncOpen(p->sample_rate, p->num_channels,
                                     &inSamples, &maxOut);
    if (!h)
        return FAAC_ERR_NO_MEMORY;

    /* Translate params onto the handle's own configuration and run it through
     * the shared core validation/derivation in faacEncApplyConfig. */
    cfg = &h->config;
    cfg->mpegVersion   = (unsigned int)p->mpeg_version;
    cfg->aacObjectType = (unsigned int)p->object_type;  /* LOW / HE_V1 / AUTO */
    cfg->jointmode     = (unsigned int)p->joint_mode;
    cfg->useLfe        = p->use_lfe ? 1 : 0;
    cfg->useTns        = p->use_tns ? 1 : 0;
    cfg->bitRate       = p->bit_rate;
    cfg->bandWidth     = p->bandwidth;
    cfg->quantqual     = p->quant_quality;
    cfg->outputFormat  = (unsigned int)p->output_format;
    cfg->inputFormat   = (unsigned int)p->input_format;
    cfg->shortctl      = (int)p->short_control;
    cfg->pnslevel      = p->pns_level;
    if (p->channel_map) {
        uint32_t i;
        for (i = 0; i < p->num_channels; i++)
            cfg->channel_map[i] = p->channel_map[i];
    }
    /* else: faacEncOpen already installed the identity map */

    if (!faacEncApplyConfig(h, cfg)) {
        faacEncClose((faacEncHandle)h);
        return FAAC_ERR_INVALID_ARGUMENT;
    }

    *out = (faac_encoder *)h;
    return FAAC_OK;
}

FAACAPI faac_status faac_encoder_close(faac_encoder **enc)
{
    if (!enc)
        return FAAC_ERR_INVALID_ARGUMENT;
    if (*enc) {
        faacEncClose((faacEncHandle)unwrap(*enc));
        *enc = NULL;
    }
    return FAAC_OK;
}

FAACAPI faac_status faac_encoder_get_info(faac_encoder *enc, faac_encoder_info *out)
{
    faac_encoder_info info;
    faacEncStruct *h;
    uint32_t caller_size, n;

    if (!enc || !out)
        return FAAC_ERR_INVALID_ARGUMENT;
    caller_size = out->struct_size;
    if (caller_size < sizeof(info.struct_size))
        return FAAC_ERR_INVALID_ARGUMENT;
    h = unwrap(enc);

    memset(&info, 0, sizeof(info));
    info.frame_samples    = faacFrameSamples(h);
    info.max_output_bytes = (uint32_t)ADTS_FRAMESIZE;
    /* The handle holds the halved core rate for HE-AAC; report the rate the
     * decoder reconstructs. */
    info.sample_rate      = (uint32_t)SbrContextGetFullRate(h->sbrContext, h->sampleRate);
    info.object_type      = (enum faac_object_type)h->config.aacObjectType;
    info.bit_rate         = (uint32_t)h->config.bitRate;
    info.bandwidth        = (uint32_t)h->config.bandWidth;
    info.quant_quality    = (uint32_t)h->config.quantqual;
    info.pns_level        = (int32_t)h->config.pnslevel;

    /* Write at most the caller's struct_size so a newer library cannot overrun
     * an older, smaller faac_encoder_info; report the byte count actually set. */
    n = caller_size < (uint32_t)sizeof(info) ? caller_size : (uint32_t)sizeof(info);
    info.struct_size = n;
    memcpy(out, &info, n);
    return FAAC_OK;
}

FAACAPI faac_status faac_encoder_asc(faac_encoder *enc,
                                     const uint8_t **buf, uint32_t *len)
{
    faacEncStruct *h;

    if (!enc || !buf || !len)
        return FAAC_ERR_INVALID_ARGUMENT;
    h = unwrap(enc);

    if (!h->ascCache) {
        int rc = faacEncGetDecoderSpecificInfo((faacEncHandle)h,
                                               &h->ascCache, &h->ascCacheLen);
        if (rc != 0) {
            h->ascCache = NULL;
            h->ascCacheLen = 0;
            /* -2 is "MPEG-2 has no ASC"; everything else is an allocation or
             * argument failure. */
            return (rc == -2) ? FAAC_ERR_UNSUPPORTED : FAAC_ERR_NO_MEMORY;
        }
    }

    *buf = h->ascCache;
    *len = (uint32_t)h->ascCacheLen;
    return FAAC_OK;
}

FAACAPI faac_status faac_encoder_encode(faac_encoder *enc,
                                        const void *in, uint32_t in_samples,
                                        uint8_t *out, uint32_t out_cap,
                                        uint32_t *bytes_written)
{
    faacEncStruct *h;
    int rc;

    if (!enc || !out || !bytes_written)
        return FAAC_ERR_INVALID_ARGUMENT;
    *bytes_written = 0;
    if (in_samples && !in)
        return FAAC_ERR_INVALID_ARGUMENT;
    h = unwrap(enc);

    if (out_cap < (uint32_t)ADTS_FRAMESIZE)
        return FAAC_ERR_OUTPUT_TOO_SMALL;
    if (in_samples > faacFrameSamples(h) * h->numChannels)
        return FAAC_ERR_INPUT_OVERFLOW;

    /* faacEncEncode reinterprets the input bytes per the configured
     * input_format; the int32_t* parameter is historical and does not imply an
     * int32 layout. */
    rc = faacEncEncode((faacEncHandle)h, (int32_t *)(uintptr_t)in,
                       in_samples, out, out_cap);
    /* A too-small output buffer was already rejected above, so a negative
     * return here is an unexpected core fault, not a buffer-size problem --
     * don't disguise it as one or the caller will grow the buffer and retry. */
    if (rc < 0)
        return FAAC_ERR_INTERNAL;

    *bytes_written = (uint32_t)rc;
    return FAAC_OK;
}

FAACAPI const char *faac_strerror(faac_status status)
{
    switch (status) {
        case FAAC_OK:                   return "success";
        case FAAC_ERR_INVALID_ARGUMENT: return "invalid argument";
        case FAAC_ERR_UNSUPPORTED:      return "unsupported configuration";
        case FAAC_ERR_NO_MEMORY:        return "out of memory";
        case FAAC_ERR_OUTPUT_TOO_SMALL: return "output buffer too small";
        case FAAC_ERR_INPUT_OVERFLOW:   return "input sample count too large";
        case FAAC_ERR_INTERNAL:         return "internal encoder error";
        case FAAC_STATUS_MAX:           break;
    }
    return "unknown error";
}
