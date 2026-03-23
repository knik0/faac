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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "coder.h"
#include "bitstream.h"

void test_PutBit_Standard() {
    unsigned char buffer[100];
    memset(buffer, 0, 100);
    BitStream *bs = OpenBitStream(100, buffer);
    assert(bs != NULL);

    /* Verify 8-bit packing */
    PutBit(bs, 0xAA, 8);
    assert(buffer[0] == 0xAA);

    /* Verify partial byte packing (aligned start) */
    PutBit(bs, 0x5, 4);
    assert(buffer[1] == 0x50);

    /* Verify multi-byte boundary crossing */
    PutBit(bs, 0x1F, 5);
    assert(buffer[1] == 0x5F);
    assert(buffer[2] == 0x80);

    CloseBitStream(bs);
}

void test_PutBit_Robustness() {
    unsigned char buffer[100];
    memset(buffer, 0, 100);
    BitStream *bs = OpenBitStream(100, buffer);

    /* Validate zero-length writes (No state change expected) */
    PutBit(bs, 0x1234, 0);
    assert(bs->numBit == 0);

    /* Validate negative bit count resilience (Underflow guard) */
    PutBit(bs, 0xFFFF, -1);
    assert(bs->numBit == 0);

    /* Validate 32-bit word masking and packing */
    PutBit(bs, 0xDEADBEEF, 32);
    assert(bs->numBit == 32);
    assert(buffer[0] == 0xDE);
    assert(buffer[1] == 0xAD);
    assert(buffer[2] == 0xBE);
    assert(buffer[3] == 0xEF);

    /* Validate clamping for bit counts > 32 (Overflow guard) */
    /* Implementation should clamp to 32 and write valid masked data */
    long bitsBefore = bs->numBit;
    PutBit(bs, 0xAAAAAAAA, 64); 

    /* Ensure only 32 bits were actually added to the total */
    assert(bs->numBit == bitsBefore + 32);

    assert(buffer[4] == 0xAA);
    assert(buffer[7] == 0xAA);

    CloseBitStream(bs);
}

void test_WriteADTSHeader() {
    unsigned char buffer[100];
    memset(buffer, 0, 100);
    BitStream *bs = OpenBitStream(100, buffer);

    faacEncStruct encoder;
    memset(&encoder, 0, sizeof(encoder));
    encoder.config.mpegVersion = 0;   /* MPEG-4 */
    encoder.config.aacObjectType = 2; /* LC-AAC */
    encoder.sampleRateIdx = 4;        /* 44100 Hz */
    encoder.numChannels = 2;
    encoder.usedBytes = 100;

    WriteADTSHeader(&encoder, bs, 1);

    /* ADTS Syncword: 0xFFF (12 bits) */
    assert(buffer[0] == 0xFF);
    /* ID, Layer, protection_absent */
    assert(buffer[1] == 0xF1);

    CloseBitStream(bs);
}

int main() {
    test_PutBit_Standard();
    test_PutBit_Robustness();
    test_WriteADTSHeader();
    printf("test_bitstream passed.\n");
    return 0;
}