/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: huffman.h,v 1.1 2001/01/17 11:21:40 menno Exp $
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "bitstream.h"
#include "coder.h"

/* Huffman tables */
#define MAXINDEX 289
#define NUMINTAB 2
#define FIRSTINTAB 0
#define LASTINTAB 1

#define INTENSITY_HCB 15
#define INTENSITY_HCB2 14


#define ABS(A) ((A) < 0 ? (-A) : (A))

#include "frame.h"

void HuffmanInit(CoderInfo *coderInfo, unsigned int numChannels);
void HuffmanEnd(CoderInfo *coderInfo, unsigned int numChannels);

int BitSearch(CoderInfo *coderInfo,
			  int *quant);

int NoiselessBitCount(CoderInfo *coderInfo,
					  int *quant,
					  int hop,
					  int min_book_choice[112][3]);

static int CalculateEscSequence(int input, int *len_esc_sequence);

int OutputBits(CoderInfo *coderInfo,
			   int book,
			   int *quant,
			   int offset,
			   int length,
			   int write_flag);

int SortBookNumbers(CoderInfo *coderInfo,
					BitStream *bitStream,
					int writeFlag);

int WriteScalefactors(CoderInfo *coderInfo,
					  BitStream *bitStream,
					  int writeFlag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HUFFMAN_H */
