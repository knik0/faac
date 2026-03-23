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
#include "coder.h"
#include "huff2.h"

void test_huffbook_selection() {
    CoderInfo coder;
    memset(&coder, 0, sizeof(coder));

    /* 128 quantized samples, all zero */
    int xi[128];
    memset(xi, 0, sizeof(xi));

    /* Choose book for zeros: should select book 0 */
    huffbook(&coder, xi, 128);
    assert(coder.book[0] == 0);

    /* Verify huffcode return value for zeros */
    int bits = huffcode(xi, 128, 1, NULL);
    assert(bits > 0);
}

void test_huffbook_escape() {
    CoderInfo coder;
    memset(&coder, 0, sizeof(coder));

    /* Large values trigger HCB_ESC (book 11) */
    int xi[4] = {20, 0, 0, 0};
    huffbook(&coder, xi, 4);
    assert(coder.book[0] == 11);
}

int main() {
    test_huffbook_selection();
    test_huffbook_escape();
    printf("test_huffman passed\n");
    return 0;
}
