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
#include "coder.h"
#include "quantize.h"

void test_CalcBW() {
    unsigned int bw = 16000;
    SR_INFO sr;
    AACQuantCfg cfg;

    sr.num_cb_long = 40;
    sr.num_cb_short = 10;
    for(int i=0; i<40; i++) sr.cb_width_long[i] = 20;
    for(int i=0; i<10; i++) sr.cb_width_short[i] = 4;

    cfg.pnslevel = 0;

    /* 16kHz bandwidth @ 44.1kHz rate */
    CalcBW(&bw, 44100, &sr, &cfg);

    /* max_cbl should be approx bw / (44100/2048) */
    assert(cfg.max_cbl > 0);
    assert(cfg.max_cbs > 0);
}

void test_quantize_scalar() {
    faac_real xr[4] = {1.0, 2.0, -1.0, 0.0};
    int xi[4];

    /* Small fixed scalefactor */
    quantize_scalar(xr, xi, 4, 1.0);

    assert(xi[0] == 1);
    assert(xi[2] == -1);
    assert(xi[3] == 0);
}

int main() {
    test_CalcBW();
    test_quantize_scalar();
    printf("test_quantize passed\n");
    return 0;
}
