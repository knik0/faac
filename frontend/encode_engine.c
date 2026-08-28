/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 * Copyright (C) 2002-2017 Krzysztof Nikiel
 * Copyright (C) 2004 Dan Villiom P. Christiansen
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/time.h>
#endif

#include "encode_engine.h"
#include "input.h"
#include "mp4write.h"
#include "charset.h"

void init_encode_options(encode_options_t *opts)
{
    if (!opts)
        return;

    memset(opts, 0, sizeof(*opts));
    opts->mpeg_version = FAAC_MPEG4;
    opts->object_type = FAAC_OBJ_AUTO;
    opts->joint_mode = FAAC_JOINT_MIXED;
    opts->stream_format = FAAC_STREAM_ADTS;
    opts->shortctl = FAAC_SHORTCTL_NORMAL;
    opts->use_tns = false;
    opts->use_lfe = -1;
    opts->pns_level = -1;
    opts->quant_quality = 0;
    opts->bit_rate = DEFAULT_ABR_KBPS * 1000;
    opts->center_channel = 3;
    opts->lfe_channel = 4;
    opts->raw_bits = 16;
    opts->raw_rate = 44100;
    opts->raw_endian = true;
    opts->verbose = 1;
}

bool add_custom_tag_to_options(encode_options_t *opts, const char *name, const char *value)
{
    if (!opts || !name || !value)
        return false;

    if (opts->custom_tag_count >= opts->custom_tag_cap)
    {
        uint16_t new_cap = opts->custom_tag_cap ? (uint16_t)(opts->custom_tag_cap * 2) : 4;
        custom_tag_t *tmp = realloc(opts->custom_tags, (size_t)new_cap * sizeof(custom_tag_t));
        if (!tmp)
            return false;
        opts->custom_tags = tmp;
        opts->custom_tag_cap = new_cap;
    }

    char *dup_name = strdup(name);
    char *dup_val = utf8_ensure(value);
    if (!dup_name || !dup_val)
    {
        free(dup_name);
        free(dup_val);
        return false;
    }

    opts->custom_tags[opts->custom_tag_count].name = dup_name;
    opts->custom_tags[opts->custom_tag_count].value = dup_val;
    opts->custom_tag_count++;
    return true;
}

void free_encode_options(encode_options_t *opts)
{
    if (!opts)
        return;

    if (opts->art_data)
    {
        free((void *)opts->art_data);
        opts->art_data = NULL;
        opts->art_size = 0;
    }

    for (uint32_t i = 0; i < opts->custom_tag_count; i++)
    {
        free(opts->custom_tags[i].name);
        free(opts->custom_tags[i].value);
    }
    free(opts->custom_tags);
    opts->custom_tags = NULL;
    opts->custom_tag_count = 0;
    opts->custom_tag_cap = 0;
}

void parse_quality_or_bitrate(const char *text, bool is_bitrate_mode,
                               encode_options_t *opts)
{
    int val = text ? atoi(text) : 0;

    if (is_bitrate_mode)
    {
        opts->bit_rate = (val > 0) ? (uint32_t)(val * 1000) : DEFAULT_ABR_KBPS * 1000;
        opts->quant_quality = 0;
    }
    else
    {
        opts->quant_quality = (val > 0) ? (uint16_t)val : DEFAULT_QUANT_QUALITY;
        opts->bit_rate = 0;
    }
}

static double calc_speed(uint64_t current_sample, unsigned int sample_rate, double time_used)
{
    if (time_used <= 0.0 || sample_rate == 0)
        return 0.0;

    return ((double)current_sample / (double)sample_rate) / time_used;
}

static double calc_eta(uint64_t current_sample, uint64_t total_samples, double time_used)
{
    if (current_sample == 0 || time_used <= 0.0 || total_samples < current_sample)
        return 0.0;

    return time_used * (double)(total_samples - current_sample) / (double)current_sample;
}

static double get_wall_time_sec(void)
{
#ifdef _WIN32
    return (double)GetTickCount() / 1000.0;
#else
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0)
        return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

static inline bool write_output_bytes(FILE *outfile, const unsigned char *buf, size_t size)
{
    return outfile && (fwrite(buf, 1, size, outfile) == size);
}

static progress_info_t build_progress_info(uint64_t current_input_samples,
                                            uint64_t total_input_samples,
                                            uint32_t sample_rate, uint16_t num_channels,
                                            uint32_t current_frame, uint32_t total_frames,
                                            uint64_t total_bytes_written, double time_used,
                                            bool is_final)
{
    return (progress_info_t){
        .current_input_samples = current_input_samples,
        .total_input_samples = total_input_samples,
        .sample_rate = sample_rate,
        .num_channels = num_channels,
        .current_frame = current_frame,
        .total_frames = total_frames,
        .total_bytes_written = total_bytes_written,
        .time_elapsed_sec = time_used,
        .speed_factor = calc_speed(current_input_samples, sample_rate, time_used),
        .eta_sec = is_final ? 0.0 : calc_eta(current_input_samples, total_input_samples, time_used),
        .is_final = is_final
    };
}

static void finalize_log(log_message_callback_t log_cb, void *user_data,
                          int level, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (log_cb)
        log_cb(level, msg, user_data);
    else
        fputs(msg, stderr);
}

/* Unlike finalize_log(), silent (not stderr) when log_cb is NULL: these
   call sites should only ever log through a caller-supplied callback, and
   run_encoding_session()'s callback-less public wrapper relies on that
   silence. */
static void log_msgf(log_message_callback_t log_cb, void *user_data,
                      int level, const char *fmt, ...)
{
    if (!log_cb)
        return;

    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_cb(level, msg, user_data);
}

/* Output-domain sample counts -> the rate mp4_set_format() declared
   (HE-AAC's container is core rate, half output rate). One conversion,
   used everywhere, so every container-domain number stays consistent. */
typedef struct { uint32_t div; uint64_t rounded_pos; } rate_conv_t;

static uint32_t rc_scalar(rate_conv_t rc, uint64_t v)
{
    return (uint32_t)((v + rc.div / 2) / rc.div);
}

/* Per-frame durations must be derived from the running position, not
   rounded individually, or rounding drift accumulates across frames. */
static uint32_t rc_advance(rate_conv_t *rc, uint64_t new_pos)
{
    uint64_t rounded = rc_scalar(*rc, new_pos);
    uint32_t delta = (uint32_t)(rounded - rc->rounded_pos);
    rc->rounded_pos = rounded;
    return delta;
}

static bool finalize_mp4(faac_encoder *hEncoder, const encode_options_t *opts,
                          log_message_callback_t log_cb, void *user_data)
{
    char *allocated_tags[MP4TAG_COUNT + 1] = { 0 };
    int num_allocated = 0;

    faac_library_info libinfo = { .struct_size = sizeof(libinfo) };
    faac_get_library_info(&libinfo);

    const uint8_t *asc_data = NULL;
    uint32_t asc_size = 0;
    if (hEncoder)
    {
        faac_encoder_asc(hEncoder, &asc_data, &asc_size);
        mp4_set_decoder_config((unsigned char *)asc_data, asc_size);
    }
    uint32_t creation_time = 0;
    if (opts->creation_time_str)
    {
        if (!strcmp(opts->creation_time_str, "auto"))
        {
            if (opts->input_filename && strcmp(opts->input_filename, "-") != 0)
            {
                time_t mtime;
#ifdef _WIN32
                bool ok = win32_mtime_utf8(opts->input_filename, &mtime) == 0;
#else
                struct stat st;
                bool ok = stat(opts->input_filename, &st) == 0;
                mtime = st.st_mtime;
#endif
                if (ok)
                {
                    creation_time = (uint32_t)mtime;
                }
                else if (opts->verbose)
                {
                    finalize_log(log_cb, user_data, 0, "couldn't stat() input file %s, defaulting to 0\n", opts->input_filename);
                }
            }
            else if (opts->verbose)
            {
                finalize_log(log_cb, user_data, 0, "cannot use --creation-time auto with stdin, defaulting to 0\n");
            }
        }
        else if (!strcmp(opts->creation_time_str, "now"))
        {
            creation_time = (uint32_t)time(NULL);
        }
        else
        {
            char *endptr;
            errno = 0;
            creation_time = (uint32_t)strtoul(opts->creation_time_str, &endptr, 10);
            if (errno != 0 || *endptr != '\0')
            {
                if (opts->verbose)
                    finalize_log(log_cb, user_data, 0, "invalid creation time %s, defaulting to 0\n", opts->creation_time_str);
                creation_time = 0;
            }
        }
        mp4_set_creation_time(creation_time);
    }
    else
    {
        const char *sde = getenv("SOURCE_DATE_EPOCH");
        if (sde)
        {
            char *endptr;
            errno = 0;
            creation_time = (uint32_t)strtoul(sde, &endptr, 10);
            if (errno != 0 || *endptr != '\0')
            {
                if (opts->verbose)
                    finalize_log(log_cb, user_data, 0, "invalid SOURCE_DATE_EPOCH %s, ignoring\n", sde);
                creation_time = 0;
            }
        }
        mp4_set_creation_time(creation_time);
    }

    if (opts->art_data && opts->art_size > 0)
    {
        mp4_set_cover(opts->art_data, (int)opts->art_size);
    }

    mp4_metadata_t metadata = opts->metadata;

    if (libinfo.version)
    {
        size_t ver_len = strlen(libinfo.version) + 6;
        char *version_string = malloc(ver_len);
        if (version_string)
        {
            snprintf(version_string, ver_len, "FAAC %s", libinfo.version);
            metadata.encoder = version_string;
            allocated_tags[num_allocated++] = version_string;
        }
    }

#define SETTAG(id, x) \
    do { \
        if (x) { \
            char *utf8_val = utf8_ensure(x); \
            mp4_set_tag(id, utf8_val); \
            if (utf8_val && num_allocated < (int)(sizeof(allocated_tags) / sizeof(allocated_tags[0]))) \
                allocated_tags[num_allocated++] = utf8_val; \
            else if (utf8_val) \
                free(utf8_val); \
        } \
    } while (0)

    SETTAG(MP4TAG_ARTIST, metadata.artist);
    SETTAG(MP4TAG_ARTISTSORT, metadata.artist_sort);
    SETTAG(MP4TAG_COMPOSER, metadata.composer);
    SETTAG(MP4TAG_COMPOSERSORT, metadata.composer_sort);
    SETTAG(MP4TAG_TITLE, metadata.title);
    SETTAG(MP4TAG_ALBUM, metadata.album);
    SETTAG(MP4TAG_ALBUMARTIST, metadata.album_artist);
    SETTAG(MP4TAG_ALBUMARTISTSORT, metadata.album_artist_sort);
    SETTAG(MP4TAG_ALBUMSORT, metadata.album_sort);
    SETTAG(MP4TAG_YEAR, metadata.year);
    SETTAG(MP4TAG_COMMENT, metadata.comment);
#undef SETTAG

    if (metadata.encoder) mp4_set_encoder(metadata.encoder);
    if (metadata.language) mp4_set_language(metadata.language);
    if (metadata.track) mp4_set_track(metadata.track, metadata.ntracks);
    if (metadata.disc) mp4_set_disc(metadata.disc, metadata.ndiscs);
    if (metadata.compilation) mp4_set_compilation(metadata.compilation);
    if (metadata.genre_id) mp4_set_genre(metadata.genre_id);

    for (uint32_t i = 0; i < opts->custom_tag_count; i++)
    {
        if (opts->custom_tags[i].name && opts->custom_tags[i].value)
        {
            char *utf8_val = utf8_ensure(opts->custom_tags[i].value);
            if (utf8_val)
            {
                mp4_add_custom_tag(opts->custom_tags[i].name, utf8_val);
                free(utf8_val);
            }
        }
    }

    bool ok = mp4_finish() == 0;
    if (!ok)
    {
        finalize_log(log_cb, user_data, 1, "mp4_finish() failed: output file may be incomplete\n");
    }

    for (int i = 0; i < num_allocated; i++)
    {
        free(allocated_tags[i]);
    }

    return ok;
}

int run_encoding_session(const encode_options_t *opts,
                          progress_callback_t progress_cb,
                          void *user_data)
{
    encode_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.progress_cb = progress_cb;
    cbs.user_data = user_data;
    return run_encoding_session_ext(opts, &cbs);
}

int run_encoding_session_ext(const encode_options_t *opts,
                              const encode_callbacks_t *callbacks)
{
    if (!opts || !opts->input_filename)
        return 1;

    progress_callback_t progress_cb = callbacks ? callbacks->progress_cb : NULL;
    session_start_callback_t session_start_cb = callbacks ? callbacks->session_start_cb : NULL;
    summary_callback_t summary_cb = callbacks ? (callbacks->summary_cb ? callbacks->summary_cb : callbacks->mp4_summary_cb) : NULL;
    log_message_callback_t log_cb = callbacks ? callbacks->log_cb : NULL;
    void *user_data = callbacks ? callbacks->user_data : NULL;

    pcmfile_t *infile = NULL;
    faac_encoder *hEncoder = NULL;
    FILE *outfile = NULL;

    float *pcmbuf = NULL;
    unsigned char *bitbuf = NULL;
    int *chanmap = NULL;

    int ret = 0;
    bool mp4_is_open = false;

/* Relies on the ret/log_cb/user_data locals above; scoped to this function
   with the #undef at its end. */
#define FAIL(...) \
    do { \
        log_msgf(log_cb, user_data, 1, __VA_ARGS__); \
        ret = 1; \
        goto cleanup; \
    } while (0)

    if (opts->raw_pcm_input)
    {
        infile = wav_open_read(opts->input_filename, 1);
        if (infile)
        {
            infile->bigendian = opts->raw_endian;
            infile->channels = opts->raw_channels > 0 ? opts->raw_channels : 2;
            infile->samplebytes = opts->raw_bits / 8;
            infile->samplerate = opts->raw_rate;
            infile->samples /= (infile->channels * infile->samplebytes);
        }
    }
    else
    {
        infile = wav_open_read(opts->input_filename, 0);
    }

    if (!infile)
        FAIL("Couldn't open input file %s\n", opts->input_filename);

    uint32_t sample_rate = infile->samplerate;
    uint16_t num_channels = (uint16_t)infile->channels;

    faac_library_info libinfo = { .struct_size = sizeof(libinfo) };
    faac_get_library_info(&libinfo);
    if (num_channels > libinfo.max_channels)
        FAIL("Input file %s has %u channels, but this build supports at most %u.\n",
             opts->input_filename, num_channels, libinfo.max_channels);

    faac_params params;
    faac_params_init(&params, sizeof(params));
    params.sample_rate = sample_rate;
    params.num_channels = num_channels;
    params.mpeg_version = opts->mpeg_version;
    params.object_type = opts->object_type;
    params.joint_mode = opts->joint_mode;
    params.use_tns = opts->use_tns;
    params.use_lfe = (opts->use_lfe != -1) ? (opts->use_lfe != 0) : (num_channels >= 6);
    params.short_control = opts->shortctl;
    if (opts->pns_level >= 0)
        params.pns_level = opts->pns_level;

    if (opts->quant_quality > 0 && opts->bit_rate == 0)
    {
        params.quant_quality = opts->quant_quality;
        params.bit_rate = 0;
    }
    else if (opts->bit_rate > 0)
    {
        params.bit_rate = opts->bit_rate / (num_channels ? num_channels : 1);
        params.quant_quality = 0;
    }

    if (opts->max_bit_rate > 0)
        params.max_bit_rate = opts->max_bit_rate;

    if (log_cb)
    {
        if (opts->shortctl == FAAC_SHORTCTL_NOSHORT)
            log_cb(1, "disabling short blocks\n", user_data);
        else if (opts->shortctl == FAAC_SHORTCTL_NOLONG)
            log_cb(1, "disabling long blocks\n", user_data);

        if (opts->pns_level > 0 && opts->mpeg_version == FAAC_MPEG2)
            log_cb(1, "PNS not allowed in MPEG-2 mode, disabling PNS\n", user_data);
    }

    params.bandwidth = opts->bandwidth;
    params.output_format = opts->container_mp4 ? FAAC_STREAM_RAW : opts->stream_format;
    params.input_format = FAAC_INPUT_FLOAT;

    if (faac_encoder_open(&params, &hEncoder) != FAAC_OK)
        FAIL("Couldn't open encoder instance for %s\n", opts->input_filename);

    faac_encoder_info info = { .struct_size = sizeof(info) };
    faac_encoder_get_info(hEncoder, &info);

    unsigned long samples_per_frame = (unsigned long)info.frame_samples * num_channels;
    unsigned long max_output_bytes = info.max_output_bytes;
    uint16_t frame_size = (uint16_t)(samples_per_frame / num_channels);

    /* Implicit SBR signaling expects the container declared at the core
       (pre-SBR) rate, half the reconstructed output rate. */
    rate_conv_t rc = { .div = (info.object_type == FAAC_OBJ_HE_AAC_V1) ? 2 : 1 };

    pcmbuf = malloc(samples_per_frame * sizeof(float));
    bitbuf = malloc(max_output_bytes * sizeof(unsigned char));

    if (!pcmbuf || !bitbuf)
        FAIL("Out of memory!\n");

    chanmap = mk_chan_map(num_channels, opts->center_channel, opts->lfe_channel);
    if (chanmap && log_cb)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Remapping input channels: Center=%u, LFE=%u\n",
                opts->center_channel, opts->lfe_channel);
        log_cb(1, msg, user_data);
    }

    if (opts->container_mp4)
    {
        if (opts->output_filename && !strcmp(opts->output_filename, "-"))
            FAIL("Cannot encode MP4 to stdout\n");

        if (mp4_open(opts->output_filename, opts->overwrite) != 0)
            FAIL("Couldn't create MP4 output file %s\n", opts->output_filename);
        mp4_is_open = true;
        mp4_set_format(rc_scalar(rc, sample_rate), num_channels, infile->samplebytes * 8);
    }
    else if (opts->output_filename)
    {
        if (!strcmp(opts->output_filename, "-"))
        {
            outfile = stdout;
#ifdef _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#endif
        }
        else
        {
#ifdef _WIN32
            outfile = win32_fopen_utf8(opts->output_filename, "wb");
#else
            outfile = fopen(opts->output_filename, "wb");
#endif
            if (!outfile)
                FAIL("Couldn't create output file %s\n", opts->output_filename);
        }
    }

    uint64_t total_input_samples = (infile->samples > 0) ? (uint64_t)infile->samples : 0;
    uint64_t tf_calc = (total_input_samples > 0 && frame_size > 0) ?
        (((total_input_samples + frame_size - 1) / frame_size) + 1) : 0;
    uint32_t total_frames = (tf_calc > UINT32_MAX) ? UINT32_MAX : (uint32_t)tf_calc;

    if (session_start_cb)
    {
        encode_session_info_t sess_info = {
            .input_filename = opts->input_filename,
            .output_filename = opts->output_filename,
            .sample_rate = sample_rate,
            .num_channels = num_channels,
            .total_input_samples = total_input_samples,
            .frame_size = frame_size,

            .container_mp4 = opts->container_mp4,
            .stream_format = opts->stream_format,
            .mpeg_version = opts->mpeg_version,
            .object_type = info.object_type,
            .joint_mode = params.joint_mode,
            .use_tns = params.use_tns,
            .pns_level = (int8_t)info.pns_level,
            .bandwidth = info.bandwidth,
            .quant_quality = (uint16_t)info.quant_quality,
            .bit_rate = info.bit_rate,

            .remapping_channels = (chanmap != NULL),
            .center_channel = opts->center_channel,
            .lfe_channel = opts->lfe_channel,
            .shortctl = opts->shortctl
        };
        session_start_cb(&sess_info, user_data);
    }

    uint32_t current_frame = 0;
    uint64_t total_bytes_written = 0;
    uint64_t current_input_samples = 0;
    uint64_t encoded_samples = 0;
    uint16_t max_frame_bytes = 0;
    int samples_read = 0;

    double start_time = get_wall_time_sec();

    bool input_eof = false;

    for (;;)
    {
        int bytes_written = 0;

        if (!input_eof)
        {
            if (!opts->ignore_wav_length)
            {
                if (current_input_samples < total_input_samples || total_input_samples == 0)
                {
                    samples_read = (int)wav_read_float32(infile, pcmbuf, samples_per_frame, chanmap);
                }
                else
                {
                    samples_read = 0;
                }

                if (total_input_samples > 0 &&
                    current_input_samples + (samples_read / num_channels) > total_input_samples)
                {
                    samples_read = (int)((total_input_samples - current_input_samples) * num_channels);
                }
            }
            else
            {
                samples_read = (int)wav_read_float32(infile, pcmbuf, samples_per_frame, chanmap);
            }

            if (samples_read == 0)
                input_eof = true;
            else
                current_input_samples += (samples_read / num_channels);
        }

        uint32_t nbytes = 0;
        faac_status st = faac_encoder_encode(hEncoder,
                                             input_eof ? NULL : pcmbuf,
                                             input_eof ? 0 : (uint32_t)samples_read,
                                             bitbuf,
                                             (uint32_t)max_output_bytes,
                                             &nbytes);
        bytes_written = (st == FAAC_OK) ? (int)nbytes : -1;

        if (bytes_written > 0)
        {
            current_frame++;
            total_bytes_written += bytes_written;
            if ((uint16_t)bytes_written > max_frame_bytes)
                max_frame_bytes = (uint16_t)bytes_written;
        }

        if (input_eof && bytes_written <= 0)
            break;

        if (bytes_written < 0)
            FAIL("faac_encoder_encode() failed: %s\n", faac_strerror(st));

        if (bytes_written > 0)
        {
            if (opts->container_mp4)
            {
                uint32_t frame_dur = rc_advance(&rc, (uint64_t)current_frame * frame_size);
                if (mp4_write_frame(bitbuf, (uint32_t)bytes_written, frame_dur) != 0)
                    FAIL("mp4_write_frame() failed\n");
            }
            else
            {
                if (!write_output_bytes(outfile, bitbuf, (size_t)bytes_written))
                    FAIL("Output write failed\n");
            }

            encoded_samples += frame_size;
        }

        if (progress_cb)
        {
            double time_used = get_wall_time_sec() - start_time;
            progress_info_t prog = build_progress_info(current_input_samples, total_input_samples,
                                                         sample_rate, num_channels, current_frame,
                                                         total_frames, total_bytes_written, time_used,
                                                         false);

            if (!progress_cb(&prog, user_data))
            {
                ret = ENCODE_CANCELLED;
                goto cleanup;
            }
        }
    }

    /* The in-loop progress_cb above can't know which call is last until
       after it returns, so it can't set is_final itself. */
    if (progress_cb)
    {
        double time_used = get_wall_time_sec() - start_time;
        progress_info_t prog = build_progress_info(current_input_samples, total_input_samples,
                                                     sample_rate, num_channels, current_frame,
                                                     total_frames, total_bytes_written, time_used,
                                                     true);
        progress_cb(&prog, user_data);
    }

    if (opts->container_mp4 && mp4_is_open)
    {
        uint32_t priming = info.encoder_delay;
        uint64_t total_output_samples = (uint64_t)current_frame * frame_size;
        uint64_t padding = 0;
        if (total_output_samples > (uint64_t)priming + current_input_samples)
            padding = total_output_samples - (uint64_t)priming - current_input_samples;

        mp4_set_gapless(rc_scalar(rc, priming), rc_scalar(rc, padding),
                         rc_scalar(rc, current_input_samples));

        if (!finalize_mp4(hEncoder, opts, log_cb, user_data))
        {
            ret = 1;
            goto cleanup;
        }
        if (summary_cb)
        {
            uint32_t max_kbps = (mp4_max_bitrate() + 500) / 1000;
            uint32_t avg_kbps = (mp4_avg_bitrate() + 500) / 1000;
            encode_summary_t summary = {
                .frame_count = mp4_frame_count(),
                .sample_count = mp4_sample_count(),
                .max_bitrate = max_kbps,
                .avg_bitrate = avg_kbps,
                .max_frame_size = mp4_max_frame_size(),
                .is_mp4 = true
            };
            summary_cb(&summary, user_data);
        }
    }
    else if (summary_cb)
    {
        double total_sec = (double)current_input_samples / (double)(sample_rate ? sample_rate : 1);
        uint32_t avg_bitrate = (total_sec > 0.0) ? (uint32_t)(((double)total_bytes_written * 8.0 / 1000.0) / total_sec) : 0;
        encode_summary_t summary = {
            .frame_count = current_frame,
            .sample_count = current_input_samples,
            .max_bitrate = 0,
            .avg_bitrate = avg_bitrate,
            .max_frame_size = max_frame_bytes,
            .is_mp4 = false
        };
        summary_cb(&summary, user_data);
    }

cleanup:
    if (pcmbuf) free(pcmbuf);
    if (bitbuf) free(bitbuf);
    if (chanmap) free(chanmap);

    if (opts->container_mp4 && mp4_is_open)
    {
        mp4_close();
        mp4_is_open = false;
    }

    if (outfile && outfile != stdout)
        fclose(outfile);

    if (hEncoder)
        faac_encoder_close(&hEncoder);

    if (infile)
        wav_close(infile);

#undef FAIL
    return ret;
}
