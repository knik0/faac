#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "pulse.h"
#include "interface.h"
#include "tf_main.h"
#include "tns.h"
#include "all.h"
#include "nok_ltp_common.h"

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
int bit_search(int quant[NUM_COEFF],  /* Quantized spectral values */
               AACQuantInfo* quantInfo);       /* Quantization information */

/*********************************************************/
/* noiseless_bit_count                                   */
/*********************************************************/
int noiseless_bit_count(int quant[NUM_COEFF],
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
		int quant[NUM_COEFF],		
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
