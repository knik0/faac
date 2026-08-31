/*
 * FAAC - Freeware Advanced Audio Coder
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

#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Generate default output filename based on input filename and container type */
char *get_output_filename(const char *input_filename, bool container_mp4);

/* Check if filename extension suggests ADTS stream format (.aac, .adts) */
bool is_adts_filename(const char *filename);

/* Check if filename extension suggests MP4 container format (.m4a, .mp4, .m4b) */
bool is_mp4_filename(const char *filename);

/* Auto-detect whether output file should use MP4 container format based on filename */
bool detect_container_mp4(const char *filename);

/* Check image header magic bytes (PNG, JPEG, GIF), used to validate
   --cover-art data before it's embedded as an MP4 covr atom. */
bool check_image_header(const char *buf);

/* Byte size of a regular file, or -1 if it can't be determined (missing,
   or "-" for stdin/stdout, which has no meaningful size). */
long get_file_size(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_H */
