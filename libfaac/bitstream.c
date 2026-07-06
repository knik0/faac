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

#include "bitstream.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

BitStream *OpenBitStream(uint32_t size, uint8_t *buffer)
{
    BitStream *bs = (BitStream *)AllocMemory(sizeof(BitStream));
    if (!bs) return NULL;

    InitBitStream(bs, buffer, size);

    return bs;
}

int CloseBitStream(BitStream *bs)
{
    if (!bs) return 0;
    int bytes = (int)((bs->currentBit + 7) >> 3);
    FreeMemory(bs);
    return bytes;
}

void InitBitStream(BitStream *bs, uint8_t *buffer, uint32_t size)
{
    bs->data = buffer;
    bs->size = size;
    bs->currentBit = 0;

    if (buffer) memset(buffer, 0, size);
}

/* Packs the low `numBits` bits of `data` into `bs`, MSB-first. Returns 0,
 * or -1 if the write would run past the buffer. Most fields fit in a
 * single byte, so that case short-circuits the general fill-from-the-end
 * loop below rather than paying its per-iteration bookkeeping. */
int PutBit(BitStream *bs, uint32_t data, int numBits)
{
    if (numBits <= 0)
        return 0;

    uint32_t start = bs->currentBit;
    uint32_t end = start + (uint32_t)numBits;

    if (end > bs->size * 8)
        return -1;

    bs->currentBit = end;

    if (numBits < 32)
        data &= (1U << numBits) - 1;

    if ((start >> 3) == ((end - 1) >> 3)) {
        uint32_t bitsAvailable = ((end - 1) & 7) + 1;
        uint32_t shift = 8 - bitsAvailable;
        /* PutBit ORs bits into place; it never zeroes them first, so every
         * caller-supplied buffer must already be zeroed (OpenBitStream does
         * this). A stale/reused buffer that skips OpenBitStream will corrupt
         * silently in release builds -- catch it here in debug builds. */
        assert((bs->data[start >> 3] & (((1U << (uint32_t)numBits) - 1) << shift)) == 0);
        bs->data[start >> 3] |= (uint8_t)(data << shift);
        return 0;
    }

    uint32_t bitPos = end;
    while (numBits > 0) {
        uint32_t byteIndex = (bitPos - 1) >> 3;
        uint32_t bitsAvailable = ((bitPos - 1) & 7) + 1;
        uint32_t take = (uint32_t)numBits < bitsAvailable ? (uint32_t)numBits : bitsAvailable;
        uint32_t shift = 8 - bitsAvailable;

        assert((bs->data[byteIndex] & (((1U << take) - 1) << shift)) == 0);
        bs->data[byteIndex] |= (uint8_t)((data & ((1U << take) - 1)) << shift);

        data >>= take;
        numBits -= (int)take;
        bitPos -= take;
    }

    return 0;
}

int ByteAlign(BitStream *bs)
{
    int bits = (8 - (int)(bs->currentBit & 7)) & 7;
    if (bits > 0) {
        PutBit(bs, 0, bits);
    }
    return bits;
}
