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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "output.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#include "charset.h"
#else
#include <strings.h>
#endif

static const char *find_extension(const char *filename)
{
    if (!filename)
        return NULL;

    const char *last_slash = strrchr(filename, '/');
    const char *last_bslash = strrchr(filename, '\\');
    if (last_bslash && (!last_slash || last_bslash > last_slash))
        last_slash = last_bslash;

    const char *start = last_slash ? last_slash + 1 : filename;
    return strrchr(start, '.');
}

char *get_output_filename(const char *input_filename, bool container_mp4)
{
    if (!input_filename)
        return NULL;

    const char *ext = container_mp4 ? ".m4a" : ".aac";
    size_t ext_len = strlen(ext);
    const char *dot = find_extension(input_filename);
    size_t len = dot ? (size_t)(dot - input_filename) : strlen(input_filename);

    char *aac_file_name = malloc(len + ext_len + 1);
    if (aac_file_name)
    {
        memcpy(aac_file_name, input_filename, len);
        memcpy(aac_file_name + len, ext, ext_len + 1);
    }
    return aac_file_name;
}

bool is_adts_filename(const char *filename)
{
    if (!filename)
        return false;

    const char *ext = find_extension(filename);
    if (ext)
    {
        if (!strcasecmp(ext, ".aac") || !strcasecmp(ext, ".adts"))
            return true;
    }
    return false;
}

bool is_mp4_filename(const char *filename)
{
    if (!filename)
        return false;

    const char *ext = find_extension(filename);
    if (ext)
    {
        if (!strcasecmp(ext, ".m4a") || !strcasecmp(ext, ".mp4") || !strcasecmp(ext, ".m4b"))
            return true;
    }
    return false;
}

bool detect_container_mp4(const char *filename)
{
    if (!filename)
        return true;

    if (!strcmp(filename, "-") || is_adts_filename(filename))
        return false;

    if (is_mp4_filename(filename))
        return true;

    return true;
}

bool check_image_header(const char *buf)
{
    if (!buf)
        return false;

    if (!strncmp(buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8))
        return true;               /* PNG */
    else if (!strncmp(buf, "\xFF\xD8\xFF\xE0", 4) ||
             !strncmp(buf, "\xFF\xD8\xFF\xE1", 4))
        return true;               /* JPEG */
    else if (!strncmp(buf, "GIF87a", 6) || !strncmp(buf, "GIF89a", 6))
        return true;               /* GIF */

    return false;
}

long get_file_size(const char *filename)
{
    if (!filename || !strcmp(filename, "-"))
        return -1;

#ifdef _WIN32
    FILE *f = win32_fopen_utf8(filename, "rb");
#else
    FILE *f = fopen(filename, "rb");
#endif
    if (!f)
        return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}
