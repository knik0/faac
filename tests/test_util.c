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
#include <math.h>
#include "libfaac/util.h"

void test_GetSRIndex() {
    /* Nominal sample rate mappings */
    assert(GetSRIndex(96000) == 0);
    assert(GetSRIndex(88200) == 1);
    assert(GetSRIndex(64000) == 2);
    assert(GetSRIndex(48000) == 3);
    assert(GetSRIndex(44100) == 4);
    assert(GetSRIndex(32000) == 5);
    assert(GetSRIndex(24000) == 6);
    assert(GetSRIndex(22050) == 7);
    assert(GetSRIndex(16000) == 8);
    assert(GetSRIndex(12000) == 9);
    assert(GetSRIndex(11025) == 10);
    assert(GetSRIndex(8000) == 11);

    /* Out-of-table rates and boundary conditions */
    assert(GetSRIndex(44200) == 4);
    assert(GetSRIndex(100000) == 0);
    assert(GetSRIndex(4000) == 11);
    assert(GetSRIndex(0) == 11);
}

void test_MinBitrate() {
    assert(MinBitrate() == 8000);
}

void test_MaxBitrate() {
    /* ADTS frame limit: 0x2000 * 8 * rate / FRAME_LEN */
    unsigned int rate = MaxBitrate(44100);
    assert(rate == 2822400);
}

void test_BitAllocation() {
    /* Edge cases for Perceptual Entropy mapping */
    assert(BitAllocation(0.0, 0) == 0);
    assert(BitAllocation(0.0, 1) == 0);

    /* RD-cost saturation point */
    assert(BitAllocation(1000000.0, 0) == 6144);
    assert(BitAllocation(1000000.0, 1) == 6144);

    /* Intermediate PE values */
    assert(BitAllocation(100.0, 0) == 90);
    assert(BitAllocation(100.0, 1) == 300);
}

void test_MaxBitresSize() {
    /* Bit reservoir capacity accounting */
    unsigned int size = MaxBitresSize(128000, 44100);
    assert(size == 3172);

    /* Reservoir depletion (logical safe minimum floor) */
    unsigned int size2 = MaxBitresSize(264600, 44100);
    assert(size2 == 1);

    /* Prevent unsigned underflow on extreme bitrates */
    unsigned int size3 = MaxBitresSize(529200, 44100);
    assert(size3 == 1);
}

int main() {
    test_GetSRIndex();
    test_MinBitrate();
    test_MaxBitrate();
    test_BitAllocation();
    test_MaxBitresSize();
    printf("test_util passed\n");
    return 0;
}
