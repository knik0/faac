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

#ifndef MP4WRITE_H
#define MP4WRITE_H

#include <stdint.h>

typedef enum {
    MP4TAG_ARTIST,
    MP4TAG_ARTISTSORT,
    MP4TAG_COMPOSER,
    MP4TAG_COMPOSERSORT,
    MP4TAG_TITLE,
    MP4TAG_ALBUM,
    MP4TAG_ALBUMARTIST,
    MP4TAG_ALBUMARTISTSORT,
    MP4TAG_ALBUMSORT,
    MP4TAG_YEAR,
    MP4TAG_COMMENT,
    MP4TAG_COUNT
} mp4_tag_id_t;

int mp4_open(const char *path, int overwrite);
void mp4_set_creation_time(uint32_t t);
void mp4_set_format(uint32_t samplerate, uint32_t channels, uint32_t bits);
void mp4_set_decoder_config(const uint8_t *asc, unsigned long size);
void mp4_set_encoder(const char *value);
void mp4_set_tag(mp4_tag_id_t id, const char *value);
void mp4_set_genre(int genre);
void mp4_set_compilation(int flag);
void mp4_set_track(uint32_t num, uint32_t total);
void mp4_set_disc(uint32_t num, uint32_t total);
void mp4_set_cover(const uint8_t *data, int size);
int mp4_add_custom_tag(const char *name, const char *value);
int mp4_write_frame(const uint8_t *data, uint32_t size, uint32_t samples);
int mp4_finish(void);
int mp4_close(void);

uint32_t mp4_frame_count(void);
uint32_t mp4_sample_count(void);
uint32_t mp4_max_bitrate(void);
uint32_t mp4_avg_bitrate(void);
uint32_t mp4_max_frame_size(void);

#endif
