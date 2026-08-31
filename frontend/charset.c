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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif defined(HAVE_ICONV)
#include <langinfo.h>
#include <iconv.h>
#include <strings.h>
#endif

#include "charset.h"

/* Structural check only (continuation-byte pattern, NUL-termination): does
   not reject overlong encodings, surrogates, or code points past U+10FFFF.
   A plausibility check, not a strict validator -- good enough to decide
   "does this need repair_to_utf8()", not for security-sensitive callers. */
static bool utf8_is_valid(const char *str)
{
    if (!str)
        return true;

    const unsigned char *bytes = (const unsigned char *)str;
    while (*bytes)
    {
        if (bytes[0] <= 0x7F)
        {
            bytes += 1;
        }
        else if ((bytes[0] & 0xE0) == 0xC0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80) return false;
            bytes += 2;
        }
        else if ((bytes[0] & 0xF0) == 0xE0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80 ||
                bytes[2] == '\0' || (bytes[2] & 0xC0) != 0x80) return false;
            bytes += 3;
        }
        else if ((bytes[0] & 0xF8) == 0xF0)
        {
            if (bytes[1] == '\0' || (bytes[1] & 0xC0) != 0x80 ||
                bytes[2] == '\0' || (bytes[2] & 0xC0) != 0x80 ||
                bytes[3] == '\0' || (bytes[3] & 0xC0) != 0x80) return false;
            bytes += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

#ifdef _WIN32
char *win32_utf16_to_utf8(const wchar_t *wstr)
{
    if (!wstr)
        return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        return NULL;

    char *str = malloc((size_t)len);
    if (str)
    {
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    }
    return str;
}

/* Attempts to repair str, assumed to be in the current Windows ANSI code
   page, into UTF-8. Returns NULL if it can't. */
static char *repair_to_utf8(const char *str)
{
    int wn = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    if (wn <= 0)
        return NULL;

    wchar_t *ws = malloc((size_t)wn * sizeof(wchar_t));
    if (!ws)
        return NULL;

    MultiByteToWideChar(CP_ACP, 0, str, -1, ws, wn);
    char *utf8 = win32_utf16_to_utf8(ws);
    free(ws);
    return utf8;
}

wchar_t *win32_utf8_to_utf16(const char *utf8_str)
{
    if (!utf8_str)
        return NULL;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wlen <= 0)
        return NULL;

    wchar_t *wstr = malloc((size_t)wlen * sizeof(wchar_t));
    if (wstr)
        MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);

    return wstr;
}

FILE *win32_fopen_utf8(const char *utf8_path, const char *mode)
{
    if (!utf8_path || !mode)
        return NULL;

    wchar_t *wpath = win32_utf8_to_utf16(utf8_path);
    if (!wpath)
        return NULL;

    wchar_t *wmode = win32_utf8_to_utf16(mode);
    if (!wmode)
    {
        free(wpath);
        return NULL;
    }

    FILE *f = _wfopen(wpath, wmode);
    free(wpath);
    free(wmode);
    return f;
}

int win32_access_utf8(const char *utf8_path, int amode)
{
    wchar_t *wpath = win32_utf8_to_utf16(utf8_path);
    if (!wpath)
        return -1;

    int ret = _waccess(wpath, amode);
    free(wpath);
    return ret;
}

int win32_mtime_utf8(const char *utf8_path, time_t *mtime)
{
    wchar_t *wpath = win32_utf8_to_utf16(utf8_path);
    if (!wpath)
        return -1;

    struct _stat64 st;
    int ret = _wstat64(wpath, &st);
    free(wpath);
    if (ret == 0)
        *mtime = (time_t)st.st_mtime;
    return ret;
}
#else /* POSIX */
#ifdef HAVE_ICONV
/* Mirrors the Windows path (assume the current code page, convert to UTF-8)
   using POSIX's equivalent: the locale's codeset name plus iconv(). Returns
   NULL if it can't repair str. Requires setlocale(LC_CTYPE, "") to have
   been called at startup -- otherwise nl_langinfo() reports the "C"
   locale's codeset (ASCII) regardless of the environment. */
static char *repair_to_utf8(const char *str)
{
    const char *codeset = nl_langinfo(CODESET);
    if (!codeset || !strcasecmp(codeset, "UTF-8") || !strcasecmp(codeset, "UTF8"))
        return NULL; /* locale already claims UTF-8 (or is unknown): this is
                        corrupt input, not a different encoding */

    iconv_t cd = iconv_open("UTF-8", codeset);
    if (cd == (iconv_t)-1)
        return NULL;

    size_t inbytes = strlen(str);
    size_t outcap = inbytes * 4 + 16; /* worst-case UTF-8 expansion + headroom */
    char *outbuf = malloc(outcap);
    if (!outbuf)
    {
        iconv_close(cd);
        return NULL;
    }

    char *inp = (char *)str;
    char *outp = outbuf;
    size_t inleft = inbytes;
    size_t outleft = outcap - 1; /* leave room for the NUL below */

    size_t rc = iconv(cd, &inp, &inleft, &outp, &outleft);
    /* Flush to the initial shift state: a no-op for simple codesets like
       ISO-8859-*, but stateful ones (e.g. ISO-2022-JP) need a trailing
       escape sequence emitted here, or the converted tag is truncated. */
    if (rc != (size_t)-1 && inleft == 0)
        iconv(cd, NULL, NULL, &outp, &outleft);
    iconv_close(cd);

    if (rc == (size_t)-1 || inleft != 0)
    {
        free(outbuf);
        return NULL;
    }

    *outp = '\0';
    return outbuf;
}
#else
/* No iconv available on this platform: can't attempt repair. */
static char *repair_to_utf8(const char *str)
{
    (void)str;
    return NULL;
}
#endif
#endif

char *utf8_ensure(const char *str)
{
    if (!str)
        return NULL;

    if (utf8_is_valid(str))
        return strdup(str);

    char *fixed = repair_to_utf8(str);
    if (fixed)
        return fixed;

    fprintf(stderr, "warning: tag value is not valid UTF-8, writing as-is\n");
    return strdup(str);
}
