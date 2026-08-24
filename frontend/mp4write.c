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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define W_OK 2
#else
#include <unistd.h>
#endif

#include "mp4write.h"
#include "charset.h"

#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap16)
#define MP4_HAVE_BSWAP_BUILTINS 1
#endif
#elif defined(__GNUC__)
#define MP4_HAVE_BSWAP_BUILTINS 1
#endif

#if defined(MP4_HAVE_BSWAP_BUILTINS)
#define BSWAP32 __builtin_bswap32
#define BSWAP16 __builtin_bswap16
#elif defined(_MSC_VER)
#define BSWAP32 _byteswap_ulong
#define BSWAP16 _byteswap_ushort
#else
static inline uint32_t BSWAP32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}
static inline uint16_t BSWAP16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
#endif

enum {
    MP4_EPOCH_OFFSET = 2082844800, /* seconds from 1904-01-01 to 1970-01-01 */

    /* identity transform: required by the spec even though audio-only
       files never use it; fixed-point format differs per field */
    MP4_FP1616_ONE = 0x00010000, /* unity in 16.16 fixed point: rate, matrix a/d */
    MP4_FP0230_ONE = 0x40000000, /* unity in 2.30 fixed point: matrix w */
    MP4_FP0808_ONE = 0x0100,     /* unity in 8.8 fixed point: volume */

    MP4_DESC_HDR = 5, /* descriptor tag byte + 4-byte expandable size, see put_descriptor() */

    /* iTunes 'data' atom type codes */
    ITUNES_DATA_BINARY = 0,
    ITUNES_DATA_TEXT   = 1,
    ITUNES_DATA_UINT8  = 0x15,
    ITUNES_DATA_IMAGE  = 0x0d,

    /* DecoderConfigDescriptor fixed fields (ISO/IEC 14496-1) */
    MP4_OBJECT_TYPE_AUDIO_ISO_14496_3 = 0x40,
    MP4_STREAM_TYPE_AUDIO             = 0x15, /* streamType=5 (audio) << 2 | upStream=0 | reserved=1 */
    MP4_DECODER_BUFFER_SIZE           = 6144, /* bufferSizeDB, arbitrary but generous for one AAC frame */

    MP4_TRACK_ID      = 1, /* single-track file: 'trak' and 'tkhd' both hardcode this */
    MP4_NEXT_TRACK_ID = 2, /* mvhd's hint for the next trak ID a future edit would use */
    MP4_URL_SELF_CONTAINED = 1, /* dref 'url ' flags bit: media data lives in this file, no external ref */

    MP4_IO_BUFSIZE = 65536, /* stdio buffer for the mdat write path, see mp4_open() */

    ISO639_CHAR_OFFSET = 0x60,  /* Offset between ASCII character and 5-bit ISO 639-2 char code */
    ISO639_UND_PACKED  = 0x55C4, /* 15-bit packed representation of undefined language "und" */
};

static struct {
    uint32_t samplerate;
    uint64_t samples;
    uint32_t channels;
    uint32_t bits;
    uint16_t buffersize;

    struct {
        uint32_t max;
        uint32_t avg;
        uint64_t size;
        uint64_t samples;
    } bitrate;

    uint32_t framesamples;

    struct {
        uint32_t *data;
        uint32_t ents;
        uint32_t bufsize;
    } frame;

    struct {
        const uint8_t *data;
        unsigned long size;
    } asc;

    FILE *fout;
    uint32_t mdatofs;
    uint64_t mdatsize;

    uint32_t creation_time;
    const char *encoder;
    const char *language;
    const char *tags[MP4TAG_COUNT];
    bool compilation;
    uint16_t trackno;
    uint16_t ntracks;
    uint16_t discno;
    uint16_t ndiscs;
    uint16_t genre;

    struct {
        const uint8_t *data;
        uint32_t size;
    } cover;

    struct {
        const char *name;
        const char *value;
    } *custom;
    uint32_t customcnt;
    uint32_t customcap;
} g_mp4 = { 0 };

/* Atom trees assembled all at once (ftyp/free in mp4_open, moov in
   mp4_finish) are built here so end_atom() can patch sizes with a memcpy
   instead of an lseek round trip. mdat audio bytes stream straight to
   g_mp4.fout instead, since they arrive incrementally during encoding
   and can be far larger than is worth buffering. */
static uint8_t *g_membuf = NULL;
static size_t g_mempos = 0;
static size_t g_memcap = 0;
/* Set whenever mem_write() can't grow g_membuf to fit a write and drops
   it instead; checked by mp4_finish() so a truncated moov atom is never
   mistaken for a successfully written one. */
static int g_mem_error = 0;

/* Grows g_membuf, if needed, so it can hold at least `extra` more bytes
   past g_mempos, doubling capacity up to a 1GB cap. Returns false (and
   frees g_membuf) if growth can't satisfy the request; the caller decides
   how to surface that failure. */
static inline bool grow_membuf(size_t extra) {
    if (g_mempos + extra <= g_memcap)
        return true;

    size_t max_cap = ((size_t)1 << 30);
    size_t new_cap = g_memcap ? g_memcap * 2 : 1024;
    while (g_mempos + extra > new_cap && new_cap < max_cap) {
        if (new_cap > max_cap / 2) { new_cap = max_cap; break; }
        new_cap *= 2;
    }
    if (g_mempos + extra > new_cap)
        return false;

    void *tmp = realloc(g_membuf, new_cap);
    if (!tmp) {
        free(g_membuf);
        g_membuf = NULL;
        return false;
    }
    g_membuf = (uint8_t *)tmp;
    g_memcap = new_cap;
    return true;
}

static inline void mem_write(const void *data, size_t size) {
    if (g_membuf) {
        if (!grow_membuf(size)) { g_mem_error = 1; return; }
        memcpy(g_membuf + g_mempos, data, size);
        g_mempos += size;
    } else if (g_mp4.fout && !g_mem_error) {
        if (fwrite(data, 1, size, g_mp4.fout) != size)
            g_mem_error = 1;
    }
}


static inline void put_u32(uint32_t val) {
#ifndef WORDS_BIGENDIAN
    val = BSWAP32(val);
#endif
    if (g_membuf && g_mempos + 4 <= g_memcap) {
        memcpy(g_membuf + g_mempos, &val, 4);
        g_mempos += 4;
    } else {
        mem_write(&val, 4);
    }
}

static inline void put_u16(uint16_t val) {
#ifndef WORDS_BIGENDIAN
    val = BSWAP16(val);
#endif
    if (g_membuf && g_mempos + 2 <= g_memcap) {
        memcpy(g_membuf + g_mempos, &val, 2);
        g_mempos += 2;
    } else {
        mem_write(&val, 2);
    }
}

static inline void put_u64(uint64_t val) {
#ifndef WORDS_BIGENDIAN
#if defined(MP4_HAVE_BSWAP_BUILTINS)
    val = __builtin_bswap64(val);
#elif defined(_MSC_VER)
    val = _byteswap_uint64(val);
#else
    val = ((val >> 56) & 0x00000000000000FFULL) |
          ((val >> 40) & 0x000000000000FF00ULL) |
          ((val >> 24) & 0x0000000000FF0000ULL) |
          ((val >> 8)  & 0x00000000FF000000ULL) |
          ((val << 8)  & 0x000000FF00000000ULL) |
          ((val << 24) & 0x0000FF0000000000ULL) |
          ((val << 40) & 0x00FF000000000000ULL) |
          ((val << 56) & 0xFF00000000000000ULL);
#endif
#endif
    if (g_membuf && g_mempos + 8 <= g_memcap) {
        memcpy(g_membuf + g_mempos, &val, 8);
        g_mempos += 8;
    } else {
        mem_write(&val, 8);
    }
}

/* Writes a version-0/version-1 time or duration field: mvhd/tkhd/mdhd
   widen these to 64-bit together, gated on the same use64 flag, once the
   sample count no longer fits a 32-bit duration. */
static inline void put_time(uint64_t val, bool use64) {
    if (use64) put_u64(val); else put_u32((uint32_t)val);
}

static inline void put_u8(uint8_t val) { mem_write(&val, 1); }

static inline void put_data(const void *data, size_t size) { mem_write(data, size); }

/* An atom's size field comes before its contents but isn't known until
   the contents (and any nested atoms) are written, so reserve it as 0
   here and let end_atom() backpatch the real value once it's known. */
static inline long start_atom(const char *name) {
    long pos = g_membuf ? (long)g_mempos : (g_mp4.fout ? ftell(g_mp4.fout) : 0);
    put_u32(0);
    put_data(name, 4);
    return pos;
}

static inline void end_atom(long pos) {
    if (g_membuf) {
        uint32_t size = (uint32_t)(g_mempos - pos);
#ifndef WORDS_BIGENDIAN
        size = BSWAP32(size);
#endif
        memcpy(g_membuf + pos, &size, 4);
    } else if (g_mp4.fout) {
        long curr = ftell(g_mp4.fout);
        fseek(g_mp4.fout, pos, SEEK_SET);
        put_u32((uint32_t)(curr - pos));
        fseek(g_mp4.fout, curr, SEEK_SET);
    }
}

void mp4_set_creation_time(uint32_t t) {
    if (t == 0)
        g_mp4.creation_time = 0;
    else
        g_mp4.creation_time = t + MP4_EPOCH_OFFSET;
}

/* creation/modification time is informational only; the spec (14496-12) never
 * requires a real clock. Emit 0 ("unknown") by default so encodes are
 * byte-reproducible, matching common muxer practice. */
static uint32_t get_mp4_time(void) {
    return g_mp4.creation_time;
}

static uint16_t pack_language(const char *lang) {
    if (!lang || strlen(lang) < 3)
        lang = "und";

    uint8_t c1 = (uint8_t)(lang[0] >= 'A' && lang[0] <= 'Z' ? lang[0] + 32 : lang[0]);
    uint8_t c2 = (uint8_t)(lang[1] >= 'A' && lang[1] <= 'Z' ? lang[1] + 32 : lang[1]);
    uint8_t c3 = (uint8_t)(lang[2] >= 'A' && lang[2] <= 'Z' ? lang[2] + 32 : lang[2]);

    if (c1 < 'a' || c1 > 'z' || c2 < 'a' || c2 > 'z' || c3 < 'a' || c3 > 'z')
        return ISO639_UND_PACKED;

    return (uint16_t)(((c1 - ISO639_CHAR_OFFSET) << 10) |
                      ((c2 - ISO639_CHAR_OFFSET) << 5) |
                      (c3 - ISO639_CHAR_OFFSET));
}

/* MPEG-4 descriptor sizes are a base-128 varint with a continuation bit,
   but always emitted here as the full 4-byte form (continuation bit set
   on all but the last byte) since some parsers assume that fixed width
   rather than the shorter encodings the spec also permits. */
static void put_descriptor(uint8_t tag, uint32_t size) {
    uint8_t buf[5];
    buf[0] = tag;
    buf[1] = ((size >> 21) & 0x7f) | 0x80;
    buf[2] = ((size >> 14) & 0x7f) | 0x80;
    buf[3] = ((size >> 7) & 0x7f) | 0x80;
    buf[4] = (size & 0x7f);
    mem_write(buf, 5);
}

/* Resets per-output-file write state: frame table, mdat bookkeeping, bitrate
   accumulators, the open output handle, and the in-memory atom buffer. Tag
   config (named metadata, custom tags via mp4_add_custom_tag()) is set by
   the caller and may happen before *or* after mp4_open() depending on the
   frontend, so it must survive this reset -- only mp4_close() clears it,
   once the caller is actually done with this muxer session. */
static void reset_write_state(void) {
    if (g_mp4.fout) {
        fclose(g_mp4.fout);
        g_mp4.fout = NULL;
    }
    free(g_mp4.frame.data);
    g_mp4.frame.data = NULL;
    g_mp4.frame.ents = 0;
    g_mp4.frame.bufsize = 0;
    g_mp4.framesamples = 0;
    g_mp4.samples = 0;
    g_mp4.buffersize = 0;
    g_mp4.mdatofs = 0;
    g_mp4.mdatsize = 0;
    memset(&g_mp4.bitrate, 0, sizeof(g_mp4.bitrate));
    free(g_membuf);
    g_membuf = NULL;
}

int mp4_open(const char *path, bool overwrite) {
    reset_write_state(); /* in case of a retry after a failed previous mp4_open() */
    g_mem_error = 0;

#ifdef _WIN32
    if (!overwrite && win32_access_utf8(path, 0) == 0) return 1;
    g_mp4.fout = win32_fopen_utf8(path, "wb");
#else
    if (!overwrite && access(path, 0) == 0) return 1;
    g_mp4.fout = fopen(path, "wb");
#endif
    if (!g_mp4.fout) return 1;
    setvbuf(g_mp4.fout, NULL, _IOFBF, MP4_IO_BUFSIZE);

    g_mp4.frame.bufsize = 1024;
    g_mp4.frame.data = (uint32_t *)malloc(g_mp4.frame.bufsize * sizeof(uint32_t));
    if (!g_mp4.frame.data) return 1;

    g_mempos = 0;
    g_memcap = 1024;
    g_membuf = (uint8_t *)malloc(g_memcap);
    if (!g_membuf)
    {
        free(g_mp4.frame.data);
        g_mp4.frame.data = NULL;
        return 1;
    }

    long ftyp = start_atom("ftyp");
    put_data("M4A \0\0\0\0M4A mp42isom", 20);
    end_atom(ftyp);

    fwrite(g_membuf, 1, g_mempos, g_mp4.fout);
    free(g_membuf);
    g_membuf = NULL;

    /* Emit 8-byte 'wide' atom placeholder between ftyp and mdat */
    put_u32(8);
    put_data("wide", 4);

    /* mdat's size isn't known until every frame has been written, so its
       header goes out now as a placeholder and gets patched in mp4_finish().
       stco also needs mdatofs to point past this header at the first
       audio byte, which is why it's recorded here rather than computed later. */
    g_mp4.mdatofs = (uint32_t)ftell(g_mp4.fout) + 8;
    put_u32(0);
    put_data("mdat", 4);
    return g_mem_error ? 1 : 0;
}

void mp4_set_format(uint32_t samplerate, uint32_t channels, uint32_t bits) {
    g_mp4.samplerate = samplerate;
    g_mp4.channels = channels;
    g_mp4.bits = bits;
}

void mp4_set_decoder_config(const uint8_t *asc, unsigned long size) {
    g_mp4.asc.data = asc;
    g_mp4.asc.size = size;
}

void mp4_set_encoder(const char *value) { g_mp4.encoder = value; }

void mp4_set_tag(mp4_tag_id_t id, const char *value) {
    if (id < MP4TAG_COUNT)
        g_mp4.tags[id] = value;
}

void mp4_set_genre(uint16_t genre) { g_mp4.genre = genre; }

void mp4_set_language(const char *lang) { g_mp4.language = lang; }

void mp4_set_compilation(bool flag) { g_mp4.compilation = flag; }

void mp4_set_track(uint16_t num, uint16_t total) {
    g_mp4.trackno = num;
    g_mp4.ntracks = total;
}

void mp4_set_disc(uint16_t num, uint16_t total) {
    g_mp4.discno = num;
    g_mp4.ndiscs = total;
}

void mp4_set_cover(const uint8_t *data, uint32_t size) {
    g_mp4.cover.data = data;
    g_mp4.cover.size = size;
}

int mp4_add_custom_tag(const char *name, const char *value) {
    if (g_mp4.customcnt >= g_mp4.customcap) {
        uint32_t new_cap = g_mp4.customcap ? g_mp4.customcap * 2 : 8;
        void *tmp = realloc(g_mp4.custom, (size_t)new_cap * sizeof(*g_mp4.custom));
        if (!tmp) return -1;
        g_mp4.custom = tmp;
        g_mp4.customcap = new_cap;
    }
    g_mp4.custom[g_mp4.customcnt].name = name;
    g_mp4.custom[g_mp4.customcnt].value = value;
    g_mp4.customcnt++;
    return 0;
}

int mp4_write_frame(const uint8_t *data, uint32_t size, uint32_t samples) {
    if (!g_mp4.fout) return -1;

    if (fwrite(data, 1, size, g_mp4.fout) != size)
        return -1;
    g_mp4.mdatsize += size;
    g_mp4.samples += samples;

    /* only count frames at the established frame length toward the
       bitrate window, so a shorter trailing frame doesn't skew it */
    if (g_mp4.framesamples <= samples) {
        g_mp4.bitrate.size += size;
        g_mp4.bitrate.samples += samples;
        if (g_mp4.bitrate.samples >= g_mp4.samplerate) {
            uint32_t br = (uint32_t)((uint64_t)8 * g_mp4.bitrate.size * g_mp4.samplerate / g_mp4.bitrate.samples);
            if (g_mp4.bitrate.max < br)
                g_mp4.bitrate.max = br;
            g_mp4.bitrate.size = 0;
            g_mp4.bitrate.samples = 0;
        }
        g_mp4.framesamples = samples;
    }

    if (g_mp4.frame.ents >= g_mp4.frame.bufsize) {
        uint32_t new_cap = g_mp4.frame.bufsize ? g_mp4.frame.bufsize * 2 : 1024;
        /* bound the frame table so an unreasonably long encode can't grow
           this without limit or overflow new_cap * sizeof(uint32_t) */
        if (new_cap > (1U << 28)) return -1;
        uint32_t *tmp = (uint32_t *)realloc(g_mp4.frame.data, (size_t)new_cap * sizeof(uint32_t));
        if (!tmp) return -1;
        g_mp4.frame.data = tmp;
        g_mp4.frame.bufsize = new_cap;
    }
    g_mp4.frame.data[g_mp4.frame.ents++] = size;
    if (g_mp4.buffersize < (uint16_t)size)
        g_mp4.buffersize = (uint16_t)size;
    return 0;
}

static void put_itunes_data_box(const char *name, uint32_t type_code, const void *data, size_t len) {
    if (!name || !data) return;
    long box      = start_atom(name);
    long data_box = start_atom("data");
    put_u32(type_code);
    put_u32(0);
    put_data(data, len);
    end_atom(data_box);
    end_atom(box);
}

static void put_tag(const char *name, const char *data) {
    if (data)
        put_itunes_data_box(name, ITUNES_DATA_TEXT, data, strlen(data));
}

static void put_tag_u8(const char *name, uint8_t val) {
    put_itunes_data_box(name, ITUNES_DATA_UINT8, &val, 1);
}

static void put_tag_genre(uint16_t genre) {
#ifndef WORDS_BIGENDIAN
    uint16_t val = BSWAP16(genre);
#else
    uint16_t val = genre;
#endif
    put_itunes_data_box("gnre", ITUNES_DATA_BINARY, &val, 2);
}

static void put_tag_index(const char *name, uint16_t num, uint16_t total) {
    uint16_t buf[4] = {
        0,
#ifndef WORDS_BIGENDIAN
        BSWAP16(num),
        BSWAP16(total),
#else
        num,
        total,
#endif
        0
    };
    put_itunes_data_box(name, ITUNES_DATA_BINARY, buf, sizeof(buf));
}

static void put_tag_image(const uint8_t *data, uint32_t size) {
    put_itunes_data_box("covr", ITUNES_DATA_IMAGE, data, size);
}

static void put_tag_ext(const char *mean, const char *name, const char *val) {
    if (!mean || !name || !val) return;
    long box      = start_atom("----");
    long mean_box = start_atom("mean");
    put_u32(0);
    put_data(mean, strlen(mean));
    end_atom(mean_box);
    long name_box = start_atom("name");
    put_u32(0);
    put_data(name, strlen(name));
    end_atom(name_box);
    put_itunes_data_box("data", ITUNES_DATA_TEXT, val, strlen(val));
    end_atom(box);
}

/* leading \xa9 marks an atom as iTunes-style "plain text" metadata,
   distinct from the freeform '----' atoms used by mp4_add_custom_tag() */
static const char *tag_atom_names[MP4TAG_COUNT] = {
    [MP4TAG_ARTIST]          = "\xa9" "ART",
    [MP4TAG_ARTISTSORT]      = "soar",
    [MP4TAG_COMPOSER]        = "\xa9" "wrt",
    [MP4TAG_COMPOSERSORT]    = "soco",
    [MP4TAG_TITLE]           = "\xa9" "nam",
    [MP4TAG_ALBUM]           = "\xa9" "alb",
    [MP4TAG_ALBUMARTIST]     = "aART",
    [MP4TAG_ALBUMARTISTSORT] = "soaa",
    [MP4TAG_ALBUMSORT]       = "soal",
    [MP4TAG_YEAR]            = "\xa9" "day",
    [MP4TAG_COMMENT]         = "\xa9" "cmt",
};

/* Returns 0 on success, 1 on failure (mirroring mp4_open()'s convention). */
int mp4_finish(void) {
    if (!g_mp4.fout) return 1;
    g_mem_error = 0;

    /* now that all frames are written, go back and fill in the mdat
       header placeholder left by mp4_open() */
    long pos = ftell(g_mp4.fout);
    if (g_mp4.mdatsize + 8 <= 0xFFFFFFFFULL) {
        /* Standard 32-bit mdat size header */
        fseek(g_mp4.fout, g_mp4.mdatofs - 8, SEEK_SET);
        put_u32((uint32_t)(g_mp4.mdatsize + 8));
    } else {
        /* 64-bit extended mdat header, overwriting the preceding 8-byte 'wide' box */
        fseek(g_mp4.fout, g_mp4.mdatofs - 16, SEEK_SET);
        put_u32(1);
        put_data("mdat", 4);
        put_u64(g_mp4.mdatsize + 16);
    }
    fseek(g_mp4.fout, pos, SEEK_SET);
    if (g_mem_error) return 1;

    g_mp4.bitrate.avg = (uint32_t)((uint64_t)8 * g_mp4.mdatsize * g_mp4.samplerate / (g_mp4.samples ? g_mp4.samples : 1));
    /* a file shorter than one second never crosses the sample-count
       threshold in mp4_write_frame, so bitrate.max would otherwise
       still be 0 here */
    if (!g_mp4.bitrate.max) g_mp4.bitrate.max = g_mp4.bitrate.avg;

    g_mempos = 0;
    g_memcap = 65536 + (size_t)g_mp4.frame.ents * 4;
    g_membuf = (uint8_t *)malloc(g_memcap);
    if (!g_membuf) return 1;

    /* version 0 duration/time fields are 32-bit and saturate past ~24-27h
       of audio (sample count, not mdatsize) well before mdat itself would
       need the 64-bit 'wide' rewrite above; switch mvhd/tkhd/mdhd to
       version 1 (64-bit time fields) once that's reached */
    bool use64_time = (g_mp4.samples > 0xFFFFFFFFULL);

    long moov = start_atom("moov");
    long mvhd = start_atom("mvhd");
    uint32_t now = get_mp4_time();
    put_u32(use64_time ? (1U << 24) : 0);
    put_time(now, use64_time); put_time(now, use64_time);
    put_u32(g_mp4.samplerate); put_time(g_mp4.samples, use64_time);
    put_u32(MP4_FP1616_ONE); put_u16(MP4_FP0808_ONE); put_u16(0); put_u32(0); put_u32(0);
    put_u32(MP4_FP1616_ONE); put_u32(0); put_u32(0);
    put_u32(0); put_u32(MP4_FP1616_ONE); put_u32(0);
    put_u32(0); put_u32(0); put_u32(MP4_FP0230_ONE);
    put_u32(0); put_u32(0); put_u32(0); put_u32(0); put_u32(0); put_u32(0);
    put_u32(MP4_NEXT_TRACK_ID);
    end_atom(mvhd);

    long trak = start_atom("trak");
    long tkhd = start_atom("tkhd");
    put_u32((use64_time ? (1U << 24) : 0) | 1);
    put_time(now, use64_time); put_time(now, use64_time);
    put_u32(MP4_TRACK_ID); put_u32(0);
    put_time(g_mp4.samples, use64_time);
    put_u32(0); put_u32(0);
    put_u16(0); put_u16(0); put_u16(MP4_FP0808_ONE); put_u16(0);
    put_u32(MP4_FP1616_ONE); put_u32(0); put_u32(0);
    put_u32(0); put_u32(MP4_FP1616_ONE); put_u32(0);
    put_u32(0); put_u32(0); put_u32(MP4_FP0230_ONE);
    put_u32(0); put_u32(0);
    end_atom(tkhd);

    uint16_t packed_lang = pack_language(g_mp4.language);
    long mdia = start_atom("mdia");
    long mdhd = start_atom("mdhd");
    put_u32(use64_time ? (1U << 24) : 0);
    put_time(now, use64_time); put_time(now, use64_time);
    put_u32(g_mp4.samplerate); put_time(g_mp4.samples, use64_time);
    put_u16(packed_lang); put_u16(0);
    end_atom(mdhd);

    long hdlr = start_atom("hdlr");
    put_u32(0); put_u32(0); put_data("soun", 4);
    put_u32(0); put_u32(0); put_u32(0); put_u8(0);
    end_atom(hdlr);

    long minf = start_atom("minf");
    long smhd = start_atom("smhd");
    put_u32(0); put_u16(0); put_u16(0);
    end_atom(smhd);

    long dinf = start_atom("dinf");
    long dref = start_atom("dref");
    put_u32(0); put_u32(1);
    long url = start_atom("url ");
    put_u32(MP4_URL_SELF_CONTAINED);
    end_atom(url);
    end_atom(dref);
    end_atom(dinf);

    long stbl = start_atom("stbl");
    long stsd = start_atom("stsd");
    put_u32(0); put_u32(1);
    long mp4a = start_atom("mp4a");
    put_u8(0); put_u8(0); put_u8(0); put_u8(0); put_u8(0); put_u8(0);
    put_u16(1); put_u32(0); put_u32(0);
    put_u16(g_mp4.channels); put_u16(g_mp4.bits);
    put_u16(0); put_u16(0); put_u16(g_mp4.samplerate); put_u16(0);

    long esds = start_atom("esds");
    put_u32(0);
    /* ES descriptor's declared size must cover its own fixed fields plus
       every nested descriptor including their headers (DecoderConfig:
       13 fixed + DecSpecificInfo; SLConfig: 1 fixed byte) */
    put_descriptor(3, 3 + MP4_DESC_HDR + 13 + MP4_DESC_HDR + g_mp4.asc.size + MP4_DESC_HDR + 1);
    put_u16(0); put_u8(0);
    put_descriptor(4, 13 + MP4_DESC_HDR + g_mp4.asc.size);
    put_u8(MP4_OBJECT_TYPE_AUDIO_ISO_14496_3); put_u8(MP4_STREAM_TYPE_AUDIO);
    put_u8((uint8_t)(MP4_DECODER_BUFFER_SIZE >> 16));
    put_u8((uint8_t)(MP4_DECODER_BUFFER_SIZE >> 8));
    put_u8((uint8_t)MP4_DECODER_BUFFER_SIZE);
    put_u32(g_mp4.bitrate.max); put_u32(g_mp4.bitrate.avg);
    put_descriptor(5, g_mp4.asc.size);
    put_data(g_mp4.asc.data, g_mp4.asc.size);
    put_descriptor(6, 1); put_u8(2);
    end_atom(esds);
    end_atom(mp4a);
    end_atom(stsd);

    long stts = start_atom("stts");
    put_u32(0); put_u32(1);
    put_u32(g_mp4.frame.ents); put_u32(g_mp4.framesamples);
    end_atom(stts);

    long stsc = start_atom("stsc");
    put_u32(0); put_u32(1); put_u32(1);
    put_u32(g_mp4.frame.ents); put_u32(1);
    end_atom(stsc);

    long stsz = start_atom("stsz");
    put_u32(0); put_u32(0); put_u32(g_mp4.frame.ents);
    if (g_mp4.frame.ents) {
        /* written by hand instead of looping put_u32() per entry: this
           table has one entry per encoded frame, so for a long file it's
           the hottest loop in mp4_finish() */
        size_t stsz_size = (size_t)g_mp4.frame.ents * 4;
        if (!grow_membuf(stsz_size))
            return 1;
        uint8_t *p = g_membuf + g_mempos;
#ifdef WORDS_BIGENDIAN
        memcpy(p, g_mp4.frame.data, stsz_size);
#else
        for (uint32_t i = 0; i < g_mp4.frame.ents; i++) {
            uint32_t val = BSWAP32(g_mp4.frame.data[i]);
            memcpy(p + i * 4, &val, 4);
        }
#endif
        g_mempos += stsz_size;
    }
    end_atom(stsz);

    if ((uint64_t)g_mp4.mdatofs + g_mp4.mdatsize <= 0xFFFFFFFFULL) {
        long stco = start_atom("stco");
        put_u32(0); put_u32(1); put_u32(g_mp4.mdatofs);
        end_atom(stco);
    } else {
        long co64 = start_atom("co64");
        put_u32(0); put_u32(1); put_u64((uint64_t)g_mp4.mdatofs);
        end_atom(co64);
    }

    end_atom(stbl);
    end_atom(minf);
    end_atom(mdia);
    end_atom(trak);

    long udta = start_atom("udta");
    long meta = start_atom("meta");
    put_u32(0);
    long hdlr2 = start_atom("hdlr");
    put_u32(0); put_u32(0); put_data("mdirappl", 8);
    put_u32(0); put_u32(0); put_u8(0);
    end_atom(hdlr2);

    long ilst = start_atom("ilst");
    put_tag("\xa9" "too", g_mp4.encoder);
    for (int i = 0; i < MP4TAG_COUNT; i++)
        put_tag(tag_atom_names[i], g_mp4.tags[i]);
    if (g_mp4.genre) put_tag_genre(g_mp4.genre);
    if (g_mp4.compilation) put_tag_u8("cpil", 1);
    if (g_mp4.trackno) put_tag_index("trkn", (uint16_t)g_mp4.trackno, (uint16_t)g_mp4.ntracks);
    if (g_mp4.discno) put_tag_index("disk", (uint16_t)g_mp4.discno, (uint16_t)g_mp4.ndiscs);
    if (g_mp4.cover.data) put_tag_image(g_mp4.cover.data, g_mp4.cover.size);
    for (uint32_t i = 0; i < g_mp4.customcnt; i++)
        put_tag_ext("faac", g_mp4.custom[i].name, g_mp4.custom[i].value);
    end_atom(ilst);
    end_atom(meta);
    end_atom(udta);
    end_atom(moov);

    int ok = !g_mem_error && fwrite(g_membuf, 1, g_mempos, g_mp4.fout) == g_mempos;
    free(g_membuf);
    g_membuf = NULL;

    return ok ? 0 : 1;
}

/* Custom tags (mp4_add_custom_tag()) are freed here, not in
   reset_write_state(), so mp4_open() doesn't wipe them if a caller sets
   them before the first open. A future multi-file/batch session reusing
   this muxer across encodes would need to re-add custom tags after each
   mp4_close() -- they don't survive it. */
int mp4_close(void) {
    reset_write_state();
    free(g_mp4.custom);
    g_mp4.custom = NULL;
    g_mp4.customcnt = 0;
    g_mp4.customcap = 0;
    return 0;
}

uint32_t mp4_frame_count(void) { return g_mp4.frame.ents; }
uint64_t mp4_sample_count(void) { return g_mp4.samples; }
uint32_t mp4_max_bitrate(void) { return g_mp4.bitrate.max; }
uint32_t mp4_avg_bitrate(void) { return g_mp4.bitrate.avg; }
uint16_t mp4_max_frame_size(void) { return g_mp4.buffersize; }
