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
#include "fft.h"

void test_fft_tables() {
    FFT_Tables tables;
    fft_initialize(&tables);

    /* Initialization confirms allocation for standard sizes */
    assert(tables.costbl != NULL);
    assert(tables.negsintbl != NULL);
    assert(tables.reordertbl != NULL);

    fft_terminate(&tables);
}

void test_fft_exec() {
    FFT_Tables tables;
    fft_initialize(&tables);

    faac_real xr[8] = {1, 0, -1, 0, 1, 0, -1, 0};
    faac_real xi[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    /* 8-point FFT (log2(8)=3) */
    fft(&tables, xr, xi, 3);

    /* Input is a complex exponential; result should have peak at k=2 */
    assert(xr[2] > 3.0);

    fft_terminate(&tables);
}

int main() {
    test_fft_tables();
    test_fft_exec();
    printf("test_fft passed\n");
    return 0;
}
