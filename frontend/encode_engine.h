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

#ifndef ENCODE_ENGINE_H
#define ENCODE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <faac.h>
#include "progress.h"
#include "mp4write.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Metadata structure for MP4 tags */
typedef struct {
    const char *artist;
    const char *artist_sort;
    const char *title;
    const char *album;
    const char *album_sort;
    const char *album_artist;
    const char *album_artist_sort;
    const char *composer;
    const char *composer_sort;
    const char *year;
    const char *comment;
    const char *encoder;
    const char *language;
    uint16_t genre_id;
    uint16_t track;
    uint16_t ntracks;
    uint16_t disc;
    uint16_t ndiscs;
    bool compilation;
} mp4_metadata_t;

typedef struct {
    char *name;
    char *value;
} custom_tag_t;

typedef struct {
    const char *input_filename;
    const char *output_filename;

    bool container_mp4;
    bool raw_pcm_input;
    uint16_t raw_channels;
    uint8_t raw_bits;
    uint32_t raw_rate;
    bool raw_endian;

    uint16_t center_channel;
    uint16_t lfe_channel;

    enum faac_mpeg_version mpeg_version;
    enum faac_object_type object_type;
    enum faac_joint_mode joint_mode;
    enum faac_stream_format stream_format;
    enum faac_shortctl_mode shortctl;

    bool use_tns;
    int8_t use_lfe; /* -1 for auto (ch >= 6), 0 = false, 1 = true */
    int8_t pns_level; /* -1 to leave default */

    uint16_t quant_quality;
    uint32_t bit_rate; /* total bitrate in bps (whole stream) */
    uint32_t max_bit_rate; /* bps whole stream cap */
    uint32_t bandwidth; /* cutoff frequency in Hz */

    bool ignore_wav_length;
    bool overwrite;

    mp4_metadata_t metadata;
    const char *creation_time_str;
    const uint8_t *art_data;
    uint64_t art_size;

    custom_tag_t *custom_tags;
    uint16_t custom_tag_count;
    uint16_t custom_tag_cap;

    uint8_t verbose;
} encode_options_t;

bool add_custom_tag_to_options(encode_options_t *opts, const char *name, const char *value);
void free_encode_options(encode_options_t *opts);

/* Canonical defaults, shared by both the CLI and GUI frontends. */
#define DEFAULT_QUANT_QUALITY 100
#define DEFAULT_ABR_KBPS      128

void init_encode_options(encode_options_t *opts);

/* Parse a quality-or-bitrate text field (as typed into the CLI's -q/-b or
   the GUI's rate edit box) into opts->quant_quality/opts->bit_rate,
   falling back to DEFAULT_QUANT_QUALITY/DEFAULT_ABR_KBPS on invalid/empty
   input rather than silently producing 0. is_bitrate_mode selects which
   field is being set and whether the value is in kbps (bitrate) or a raw
   quality percentage. */
void parse_quality_or_bitrate(const char *text, bool is_bitrate_mode,
                               encode_options_t *opts);

#define ENCODE_SUCCESS   0
#define ENCODE_ERROR     1
#define ENCODE_CANCELLED 2

typedef struct {
    const char *input_filename;
    const char *output_filename;
    uint32_t sample_rate;
    uint16_t num_channels;
    uint64_t total_input_samples;
    uint16_t frame_size;

    bool container_mp4;
    enum faac_stream_format stream_format;
    enum faac_mpeg_version mpeg_version;
    enum faac_object_type object_type;
    enum faac_joint_mode joint_mode;
    bool use_tns;
    int8_t pns_level;
    uint32_t bandwidth;
    uint16_t quant_quality;
    uint32_t bit_rate; /* bps per channel */

    bool remapping_channels;
    uint16_t center_channel;
    uint16_t lfe_channel;
    enum faac_shortctl_mode shortctl;
} encode_session_info_t;

typedef struct {
    uint32_t frame_count;
    uint64_t sample_count;
    uint32_t max_bitrate;
    uint32_t avg_bitrate;
    uint16_t max_frame_size;
    bool is_mp4;
} encode_summary_t;

typedef encode_summary_t encode_mp4_summary_t;

typedef void (*session_start_callback_t)(const encode_session_info_t *info, void *user_data);
typedef void (*summary_callback_t)(const encode_summary_t *summary, void *user_data);
typedef summary_callback_t mp4_summary_callback_t;
typedef void (*log_message_callback_t)(int level, const char *message, void *user_data);

typedef struct {
    progress_callback_t progress_cb;
    session_start_callback_t session_start_cb;
    summary_callback_t summary_cb;
    mp4_summary_callback_t mp4_summary_cb; /* alias for backward compatibility */
    log_message_callback_t log_cb;
    void *user_data;
} encode_callbacks_t;

int run_encoding_session(const encode_options_t *opts,
                          progress_callback_t progress_cb,
                          void *user_data);

int run_encoding_session_ext(const encode_options_t *opts,
                              const encode_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif

#endif /* ENCODE_ENGINE_H */
