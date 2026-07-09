/*
 * FAAC - Freeware Advanced Audio Coder
 * Huffman codebook tables reproduced from ISO/IEC 14496-3 (non-copyrightable facts)
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

#ifndef HUFFDATA_H
#define HUFFDATA_H

#include "huff2.h"

#include <stdint.h>

typedef struct {
    const uint16_t len;
    const uint16_t data;
} hcode16_t;

typedef struct {
    const uint32_t len  : 8;    /* lengths <= 19        */
    const uint32_t data : 24;   /* codes are <= 19 bits */
} hcode32_t;

extern hcode16_t book01[81];
extern hcode16_t book02[81];
extern hcode16_t book03[81];
extern hcode16_t book04[81];
extern hcode16_t book05[81];
extern hcode16_t book06[81];
extern hcode16_t book07[64];
extern hcode16_t book08[64];
extern hcode16_t book09[169];
extern hcode16_t book10[169];
extern hcode16_t book11[289];
extern hcode32_t book12[2 * SF_DELTA + 1];

#endif /* HUFFDATA_H */
