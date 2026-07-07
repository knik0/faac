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

#ifndef BITSTREAM_H
#define BITSTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * ADTS Constants (ISO/IEC 14496-3)
 */
enum {
    ADTS_MAX_FRAME_SIZE = (1 << 13) - 1,
    ADTS_FRAMESIZE      = 1 << 13, /* Legacy name compatibility */
    ADTS_HEADER_SIZE    = 7        /* 56 bits */
};

/**
 * @struct BitStream
 * @brief Manages a bit-oriented buffer for AAC bitstream generation.
 */
typedef struct BitStream {
    uint8_t *data;       /**< Pointer to the start of the bitstream buffer */
    uint32_t size;       /**< Size of the buffer in bytes */
    uint32_t currentBit; /**< Current write position in bits */
} BitStream;

BitStream *OpenBitStream(uint32_t size, uint8_t *buffer);
int CloseBitStream(BitStream *bs);
int PutBit(BitStream *bs, uint32_t data, int numBits);
int ByteAlign(BitStream *bs);

/* Batches small field writes into a register and flushes whole bytes as
 * they fill, instead of touching the buffer on every write -- for hot
 * loops (e.g. spectral coefficients) where PutBit's per-call overhead
 * dominates. Bracket a self-contained span with no interleaved PutBit
 * calls on the same stream: bs->currentBit is stale until AccumEnd. */
typedef struct BitAccumulator {
    uint64_t bits;   /**< pending bits, left-justified in the register */
    uint8_t *out;    /**< next byte to emit into bs->data */
    uint8_t *limit;  /**< bs->data + bs->size (one-past-end guard) */
    BitStream *bs;   /**< owning stream; currentBit is stale until AccumEnd */
    int fill;        /**< pending bit count (0..7 between calls) */
    int overflow;    /**< sticky: set if a store hit the buffer limit */
} BitAccumulator;

static inline void AccumBegin(BitAccumulator *a, BitStream *bs)
{
    uint32_t bytePos = bs->currentBit >> 3;
    a->bs = bs;
    a->out = bs->data + bytePos;
    a->limit = bs->data + bs->size;
    a->fill = (int)(bs->currentBit & 7);
    a->overflow = 0;
    /* Preload the in-progress byte so its already-written high bits are
     * preserved; its low (8-fill) bits are 0 (the buffer starts zeroed and
     * we only ever move forward), so this seed is exact. */
    a->bits = a->fill ? ((uint64_t)(*a->out) << 56) : 0;
}

static inline void AccumPutBits(BitAccumulator *a, uint32_t value, int numBits)
{
    if (numBits <= 0)
        return;

    if (numBits < 32)
        value &= (1U << numBits) - 1;

    a->bits |= (uint64_t)value << (64 - a->fill - numBits);
    a->fill += numBits;

    while (a->fill >= 8) {
        if (a->out < a->limit)
            *a->out = (uint8_t)(a->bits >> 56);
        else
            a->overflow = 1;
        a->out++;
        a->bits <<= 8;
        a->fill -= 8;
    }
}

static inline int AccumEnd(BitAccumulator *a)
{
    if (a->fill > 0) {
        if (a->out < a->limit)
            *a->out = (uint8_t)(a->bits >> 56);
        else
            a->overflow = 1;
        /* out is not advanced: these bits are unfinished, a later PutBit
         * on the same stream will OR the rest of the byte in. */
    }
    a->bs->currentBit = (uint32_t)((a->out - a->bs->data) * 8 + a->fill);
    return a->overflow ? -1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* BITSTREAM_H */
