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
#include <string.h>
#include "coder.h"
#include "blockswitch.h"

void test_PsyCheckShort() {
    PsyInfo psyInfo;
    psydata_t psydata;
    memset(&psyInfo, 0, sizeof(psyInfo));
    memset(&psydata, 0, sizeof(psydata));
    psyInfo.data = &psydata;

    psydata.lastband = 10;

    float eng_quiet[NSFB_SHORT];
    float eng_loud[NSFB_SHORT];
    for(int i=0; i<NSFB_SHORT; i++) {
        eng_quiet[i] = 1.0f;
        eng_loud[i] = 100.0f;
    }

    for(int i=0; i<8; i++) {
        psydata.engPrev[i] = eng_quiet;
        psydata.eng[i] = eng_quiet;
        psydata.engNext[i] = eng_quiet;
    }

    /* Steady-state signal should favor ONLY_LONG_WINDOW */
    PsyCheckShort(&psyInfo, 1.0);
    assert(psyInfo.block_type == ONLY_LONG_WINDOW);

    /* High volume change ratio (transient) triggers ONLY_SHORT_WINDOW */
    psydata.engNext[0] = eng_loud;
    PsyCheckShort(&psyInfo, 1.0);
    assert(psyInfo.block_type == ONLY_SHORT_WINDOW);
}

int main() {
    test_PsyCheckShort();
    printf("test_blockswitch passed\n");
    return 0;
}
