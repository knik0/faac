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
#include "channels.h"

void test_GetChannelInfo_Mono() {
    ChannelInfo channels[1];
    /* ISO/IEC 14496-3: Single Channel Element (SCE) configuration */
    GetChannelInfo(channels, 1, 0);
    assert(channels[0].present == 1);
    assert(channels[0].type == ELEMENT_SCE);
}

void test_GetChannelInfo_Stereo() {
    ChannelInfo channels[2];
    /* ISO/IEC 14496-3: Channel Pair Element (CPE) configuration */
    GetChannelInfo(channels, 2, 0);
    assert(channels[0].present == 1);
    assert(channels[0].type == ELEMENT_CPE);
    assert(channels[0].ch_is_left == 1);
    assert(channels[0].paired_ch == 1);

    assert(channels[1].present == 1);
    assert(channels[1].type == ELEMENT_CPE);
    assert(channels[1].ch_is_left == 0);
    assert(channels[1].paired_ch == 0);
}

void test_GetChannelInfo_3_0() {
    /*
     * 3.0 (3 channels): Validating SCE + CPE layout mapping.
     * In accordance with ISO/IEC 14496-3, the mapping assigns:
     * - SCE (Center) as the first element.
     * - CPE (Front Left/Right) as the second element.
     */
    ChannelInfo channels[3];
    GetChannelInfo(channels, 3, 0);

    /* SCE (Center) */
    assert(channels[0].present == 1);
    assert(channels[0].type == ELEMENT_SCE);
    assert(channels[0].tag == 0);

    /* CPE (Left/Right) */
    assert(channels[1].present == 1);
    assert(channels[1].type == ELEMENT_CPE);
    assert(channels[1].ch_is_left == 1);
    assert(channels[1].paired_ch == 2);
    assert(channels[1].tag == 0);

    assert(channels[2].present == 1);
    assert(channels[2].type == ELEMENT_CPE);
    assert(channels[2].ch_is_left == 0);
    assert(channels[2].paired_ch == 1);
}

void test_GetChannelInfo_5_1() {
    /*
     * 5.1 (6 channels): ISO/IEC 14496-3 Channel Configuration 6.
     * The MPEG standard element sequence is SCE -> 2xCPE -> LFE.
     * Mapping for 5.1 Surround:
     * - SCE: Front Center
     * - CPE 1: Front Left/Right
     * - CPE 2: Surround Left/Right
     * - LFE: Low Frequency Effects
     */
    ChannelInfo channels[6];
    GetChannelInfo(channels, 6, 1);

    /* SCE (Center) */
    assert(channels[0].present == 1);
    assert(channels[0].type == ELEMENT_SCE);
    assert(channels[0].tag == 0);

    /* CPE 1 (Front L/R) */
    assert(channels[1].present == 1);
    assert(channels[1].type == ELEMENT_CPE);
    assert(channels[1].ch_is_left == 1);
    assert(channels[1].paired_ch == 2);
    assert(channels[1].tag == 0);

    assert(channels[2].present == 1);
    assert(channels[2].type == ELEMENT_CPE);
    assert(channels[2].ch_is_left == 0);
    assert(channels[2].paired_ch == 1);

    /* CPE 2 (Surround L/R) */
    assert(channels[3].present == 1);
    assert(channels[3].type == ELEMENT_CPE);
    assert(channels[3].ch_is_left == 1);
    assert(channels[3].paired_ch == 4);
    assert(channels[3].tag == 1);

    assert(channels[4].present == 1);
    assert(channels[4].type == ELEMENT_CPE);
    assert(channels[4].ch_is_left == 0);
    assert(channels[4].paired_ch == 3);

    /* LFE */
    assert(channels[5].present == 1);
    assert(channels[5].type == ELEMENT_LFE);
    assert(channels[5].tag == 0);
}

void test_GetChannelInfo_7_1() {
    /*
     * 7.1 (8 channels): ISO/IEC 14496-3 Channel Configuration 7.
     * Standard element sequence: SCE -> 3xCPE -> LFE.
     * Mapping for 7.1 Surround:
     * - SCE: Front Center
     * - CPE 1: Front Left/Right
     * - CPE 2: Side Left/Right
     * - CPE 3: Back Left/Right
     * - LFE: Low Frequency Effects
     */
    ChannelInfo channels[8];
    GetChannelInfo(channels, 8, 1);

    /* SCE (Center) */
    assert(channels[0].present == 1);
    assert(channels[0].type == ELEMENT_SCE);

    /* CPE 1 (Front L/R) */
    assert(channels[1].present == 1);
    assert(channels[1].type == ELEMENT_CPE);
    assert(channels[1].ch_is_left == 1);
    assert(channels[1].paired_ch == 2);
    assert(channels[1].tag == 0);

    assert(channels[2].present == 1);
    assert(channels[2].type == ELEMENT_CPE);
    assert(channels[2].ch_is_left == 0);
    assert(channels[2].paired_ch == 1);

    /* CPE 2 (Side L/R) */
    assert(channels[3].present == 1);
    assert(channels[3].type == ELEMENT_CPE);
    assert(channels[3].ch_is_left == 1);
    assert(channels[3].paired_ch == 4);
    assert(channels[3].tag == 1);

    assert(channels[4].present == 1);
    assert(channels[4].type == ELEMENT_CPE);
    assert(channels[4].ch_is_left == 0);
    assert(channels[4].paired_ch == 3);

    /* CPE 3 (Back L/R) */
    assert(channels[5].present == 1);
    assert(channels[5].type == ELEMENT_CPE);
    assert(channels[5].ch_is_left == 1);
    assert(channels[5].paired_ch == 6);
    assert(channels[5].tag == 2);

    assert(channels[6].present == 1);
    assert(channels[6].type == ELEMENT_CPE);
    assert(channels[6].ch_is_left == 0);
    assert(channels[6].paired_ch == 5);

    /* LFE */
    assert(channels[7].present == 1);
    assert(channels[7].type == ELEMENT_LFE);
}

int main() {
    test_GetChannelInfo_Mono();
    test_GetChannelInfo_Stereo();
    test_GetChannelInfo_3_0();
    test_GetChannelInfo_5_1();
    test_GetChannelInfo_7_1();
    printf("test_channels passed\n");
    return 0;
}
