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

#ifndef ENDIAN_H
#define ENDIAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__has_builtin)
# if __has_builtin(__builtin_bswap16) && __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap64)
#  define FAAC_HAVE_BSWAP_BUILTINS 1
# endif
#elif defined(__GNUC__)
# define FAAC_HAVE_BSWAP_BUILTINS 1
#endif

#if defined(FAAC_HAVE_BSWAP_BUILTINS)
# define bswap16(x) __builtin_bswap16((uint16_t)(x))
# define bswap32(x) __builtin_bswap32((uint32_t)(x))
# define bswap64(x) __builtin_bswap64((uint64_t)(x))
#elif defined(_MSC_VER)
# include <stdlib.h>
# define bswap16(x) _byteswap_ushort((unsigned short)(x))
# define bswap32(x) _byteswap_ulong((unsigned long)(x))
# define bswap64(x) _byteswap_uint64((unsigned __int64)(x))
#else
static inline uint16_t bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}

static inline uint32_t bswap32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

static inline uint64_t bswap64(uint64_t x) {
    return ((x >> 56) & 0x00000000000000FFULL) |
           ((x >> 40) & 0x000000000000FF00ULL) |
           ((x >> 24) & 0x0000000000FF0000ULL) |
           ((x >> 8)  & 0x00000000FF000000ULL) |
           ((x << 8)  & 0x000000FF00000000ULL) |
           ((x << 24) & 0x0000FF0000000000ULL) |
           ((x << 40) & 0x00FF000000000000ULL) |
           ((x << 56) & 0xFF00000000000000ULL);
}
#endif

#if WORDS_BIGENDIAN
# define htobe16(x) ((uint16_t)(x))
# define htobe32(x) ((uint32_t)(x))
# define htobe64(x) ((uint64_t)(x))
# define htole16(x) bswap16(x)
# define htole32(x) bswap32(x)
# define htole64(x) bswap64(x)
#else
# define htobe16(x) bswap16(x)
# define htobe32(x) bswap32(x)
# define htobe64(x) bswap64(x)
# define htole16(x) ((uint16_t)(x))
# define htole32(x) ((uint32_t)(x))
# define htole64(x) ((uint64_t)(x))
#endif

#define be16toh(x) htobe16(x)
#define be32toh(x) htobe32(x)
#define be64toh(x) htobe64(x)
#define le16toh(x) htole16(x)
#define le32toh(x) htole32(x)
#define le64toh(x) htole64(x)

static inline int16_t read_pcm16_le(const int16_t *src) { return (int16_t)le16toh(*src); }
static inline int16_t read_pcm16_be(const int16_t *src) { return (int16_t)be16toh(*src); }
static inline int16_t read_pcm16(const int16_t *src, bool bigendian) {
    return bigendian ? read_pcm16_be(src) : read_pcm16_le(src);
}

static inline int32_t read_pcm24_le(const uint8_t *src) {
    int32_t s = (int32_t)src[0] | ((int32_t)src[1] << 8) | ((int32_t)src[2] << 16);
    if (s & 0x800000) s |= (int32_t)0xff000000;
    return s;
}

static inline int32_t read_pcm24_be(const uint8_t *src) {
    int32_t s = ((int32_t)src[0] << 16) | ((int32_t)src[1] << 8) | (int32_t)src[2];
    if (s & 0x800000) s |= (int32_t)0xff000000;
    return s;
}

static inline int32_t read_pcm24(const uint8_t *src, bool bigendian) {
    return bigendian ? read_pcm24_be(src) : read_pcm24_le(src);
}

static inline int32_t read_pcm32_le(const int32_t *src) { return (int32_t)le32toh(*src); }
static inline int32_t read_pcm32_be(const int32_t *src) { return (int32_t)be32toh(*src); }
static inline int32_t read_pcm32(const int32_t *src, bool bigendian) {
    return bigendian ? read_pcm32_be(src) : read_pcm32_le(src);
}

#ifdef __cplusplus
}
#endif

#endif /* ENDIAN_H */
