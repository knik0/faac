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
#include "filtbank.h"

void test_MDCT() {
    FFT_Tables fft_tables;
    fft_initialize(&fft_tables);

    /* 256-sample MDCT input (N=256) */
    faac_real data[256];
    faac_real xr[64], xi[64];
    for(int i=0; i<256; i++) data[i] = (faac_real)sin(2.0 * M_PI * i / 256.0);

    MDCT(&fft_tables, data, 256, xr, xi);

    /* Check transform produces non-zero energy in expected bands */
    faac_real energy = 0;
    for(int i=0; i<64; i++) energy += xr[i]*xr[i] + xi[i]*xi[i];
    assert(energy > 0.0001);

    fft_terminate(&fft_tables);
}

void test_CalculateKBDWindow() {
    faac_real win[16];
    /* N=16, alpha=4.0 */
    CalculateKBDWindow(win, 4.0, 16);

    /* MDCT windows must be symmetric: win[i]^2 + win[i+N/2]^2 = 1.0 is for sine,
       but KBD has its own properties. Basic check: monotonicity. */
    assert(win[0] < win[1]);
    assert(win[7] > win[0]);
}

int main() {
    test_MDCT();
    test_CalculateKBDWindow();
    printf("test_mdct passed\n");
    return 0;
}
