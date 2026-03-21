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
#include "libfaac/tns.h"

void test_Autocorrelation() {
    faac_real data[10] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    faac_real r[5];

    /* 10 samples, order 4 */
    Autocorrelation(4, 10, data, r);

    /* r[0] is energy: 5.0 */
    assert(r[0] > 4.9 && r[0] < 5.1);
    /* data is periodic with period 2; r[2] should be high */
    assert(r[2] > 3.9);
}

void test_LevinsonDurbin() {
    faac_real k[5];
    /* Constant signal case: r[0]=10, r[1]=9 */
    faac_real data[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    faac_real gain = LevinsonDurbin(4, 10, data, k);

    /* Perfect correlation yields high gain */
    assert(gain > 1.0);
}

void test_StepUp() {
    faac_real k[3] = {1.0, -0.5, 0.2};
    faac_real a[3];

    StepUp(2, k, a);

    /* a[0] is always 1.0 */
    assert(a[0] == 1.0);
}

int main() {
    test_Autocorrelation();
    test_LevinsonDurbin();
    test_StepUp();
    printf("test_tns passed\n");
    return 0;
}
