/*
 *	Function prototypes for AAC Huffman coding
 *
 *	Copyright (c) 1999 M. Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.6 $
  $Date: 2000/10/06 14:47:27 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "pulse.h"
#include "interface.h"
#include "tns.h"

#ifdef __cplusplus
extern "C" {
#endif


#define PNS_HCB 13                               /* reserved codebook for flagging PNS */
#define PNS_PCM_BITS 9                           /* size of first (PCM) PNS energy */
#define PNS_PCM_OFFSET (1 << (PNS_PCM_BITS-1))   /* corresponding PCM transmission offset */
#define PNS_SF_OFFSET 90                         /* transmission offset for PNS energies */

// Huffman tables
#define MAXINDEX 289
#define NUMINTAB 2
#define FIRSTINTAB 0
#define LASTINTAB 1


void PulseCoder(AACQuantInfo *quantInfo, int *quant);
void PulseDecoder(AACQuantInfo *quantInfo, int *quant);

/*********************************************************/
/* sort_book_numbers                                     */
/*********************************************************/
int sort_book_numbers(AACQuantInfo* quantInfo,     /* Quantization information */
//		  int output_book_vector[],    /* Output codebook vector, formatted for bitstream */
		  BsBitStream* fixed_stream,   /* Bitstream */
		  int write_flag);             /* Write flag: 0 count, 1 write */


/*********************************************************/
/* bit_search                                            */
/*********************************************************/
int bit_search(int quant[BLOCK_LEN_LONG],  /* Quantized spectral values */
               AACQuantInfo* quantInfo);       /* Quantization information */

/*********************************************************/
/* noiseless_bit_count                                   */
/*********************************************************/
int noiseless_bit_count(int quant[BLOCK_LEN_LONG],
			int hop,
			int min_book_choice[112][3],
			AACQuantInfo* quantInfo);         /* Quantization information */

/*********************************************************/
/* output_bits                                           */
/*********************************************************/
#ifndef __BORLANDC__
__inline
#endif
int output_bits(AACQuantInfo* quantInfo,
		/*int huff[13][MAXINDEX][NUMINTAB],*/
                int book,                /* codebook */
		int quant[BLOCK_LEN_LONG],		
		int offset,
		int length,
		int write_flag);


/*********************************************************/
/* find_grouping_bits                                    */
/*********************************************************/
int find_grouping_bits(int window_group_length[],
		       int num_window_groups
		       );

/*********************************************************/
/* write_scalefactor_bitstream                           */
/*********************************************************/
int write_scalefactor_bitstream(BsBitStream* fixed_stream,             /* Bitstream */  
				int write_flag,                        /* Write flag */
				AACQuantInfo* quantInfo);              /* Quantization information */


#ifdef __cplusplus
}
#endif

#endif
