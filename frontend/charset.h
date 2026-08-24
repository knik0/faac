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

#ifndef CHARSET_H
#define CHARSET_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure string is valid UTF-8, converting from system encoding if needed */
char *utf8_ensure(const char *str);

#ifdef _WIN32
#include <windows.h>
/* Convert UTF-16 wchar_t string to heap-allocated UTF-8 string */
char *win32_utf16_to_utf8(const wchar_t *wstr);

/* Convert UTF-8 string to heap-allocated UTF-16 wchar_t string */
wchar_t *win32_utf8_to_utf16(const char *utf8_str);

/* fopen() on a UTF-8 path: converts to UTF-16 and calls _wfopen(), since the
   narrow CRT's fopen() interprets its argument in the current ANSI code
   page, not UTF-8. */
FILE *win32_fopen_utf8(const char *utf8_path, const char *mode);

/* access() on a UTF-8 path, same rationale as win32_fopen_utf8(). */
int win32_access_utf8(const char *utf8_path, int amode);

/* mtime of a UTF-8 path, same rationale as win32_fopen_utf8(). Returns 0 and
   sets *mtime on success, -1 on failure (path not found, etc). */
int win32_mtime_utf8(const char *utf8_path, time_t *mtime);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CHARSET_H */
