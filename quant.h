/*
 *	Function prototypes for AAC quantization
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
  $Date: 2000/10/31 14:48:41 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef QUANT_H
#define QUANT_H

#include "pulse.h"
#include "interface.h"
#include "tns.h"
#include "ltp_enc.h"
#include "bitstream.h"

#ifdef __cplusplus
extern "C" {
#endif




/* assumptions for the first run of this quantizer */
#define CHANNEL  1
#define MAGIC_NUMBER  0.4054
#define MAX_QUANT 8192
#define SF_OFFSET 100
#define ABS(A) ((A) < 0 ? (-A) : (A))
#define sgn(A) ((A) > 0 ? (1) : (-1))

#define MAXFAC 121   /* maximum scale factor */
#define MIDFAC (MAXFAC-1)/2
#define SF_OFFSET 100   /* global gain must be positive */

#define INTENSITY_HCB 15
#define INTENSITY_HCB2 14


extern int pns_sfb_start;                        /* lower border for PNS */


/*********************************************************/
/* AACQuantInfo, info for AAC quantization and coding.   */
/*********************************************************/
typedef struct {
  int max_sfb;                          /* max_sfb, should = nr_of_sfb/num_window_groups */
  int nr_of_sfb;                        /* Number of scalefactor bands, interleaved */
  int spectralCount;                    /* Number of spectral data coefficients */
  enum WINDOW_TYPE block_type;	        /* Block type */      
  int scale_factor[MAX_SCFAC_BANDS];        /* Scalefactor data array , interleaved */			
  int sfb_offset[250];                  /* Scalefactor spectral offset, interleaved */
  int book_vector[MAX_SCFAC_BANDS];         /* Huffman codebook selected for each sf band */
  int data[5*BLOCK_LEN_LONG];                /* Data of spectral bitstream elements, for each spectral pair, 
                                           5 elements are required: 1*(esc)+2*(sign)+2*(esc value)=5 */
  int len[5*BLOCK_LEN_LONG];                 /* Lengths of spectral bitstream elements */
  int num_window_groups;                /* Number of window groups */
  int window_group_length[MAX_SHORT_WINDOWS]; /* Length (in windows) of each window group */
  int common_scalefac;                  /* Global gain */
  Window_shape window_shape;            /* Window shape parameter */
  Window_shape prev_window_shape;       /* Previous window shape parameter */
  short pred_global_flag;               /* Global prediction enable flag */
  int pred_sfb_flag[MAX_SCFAC_BANDS];       /* Prediction enable flag for each scalefactor band */
  int reset_group_number;               /* Prediction reset group number */
  TNS_INFO* tnsInfo;                    /* Ptr to tns data */
  AACPulseInfo pulseInfo;
  LT_PRED_STATUS *ltpInfo;              /* Ptr to LTP data */
  SR_INFO *sr_info;
  int pns_sfb_nrg[MAX_SCFAC_BANDS];
  int pns_sfb_flag[MAX_SCFAC_BANDS];
  int profile;
  int srate_idx;
} AACQuantInfo;


void quantize(AACQuantInfo *quantInfo,
			  double *pow_spectrum,
			  int quant[BLOCK_LEN_LONG]
			  );
void dequantize(AACQuantInfo *quantInfo,
				double *p_spectrum,
				int quant[BLOCK_LEN_LONG],
				double requant[BLOCK_LEN_LONG],
				double error_energy[MAX_SCFAC_BANDS]
				);
int count_bits(AACQuantInfo* quantInfo,
			   int quant[BLOCK_LEN_LONG]
//			   ,int output_book_vector[MAX_SCFAC_BANDS*2]
                           );


/*********************************************************/
/* sort_for_grouping                                     */
/*********************************************************/
int sort_for_grouping(AACQuantInfo* quantInfo,        /* ptr to quantization information */
		      int sfb_width_table[],          /* Widths of single window */
		      double *p_spectrum[],           /* Spectral values, noninterleaved */
		      double *SigMaskRatio,
		      double *PsySigMaskRatio);

/*********************************************************/
/* tf_init_encode_spectrum_aac                           */
/*********************************************************/
void tf_init_encode_spectrum_aac( int quality );


/*********************************************************/
/* tf_encode_spectrum_aac                                */
/*********************************************************/
int tf_encode_spectrum_aac(
			   double      *p_spectrum[MAX_TIME_CHANNELS],
			   double      *SigMaksRatio[MAX_TIME_CHANNELS],
			   double      allowed_dist[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			   double      energy[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS],
			   int         sfb_width_table[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
//			   int         nr_of_sfb[MAX_TIME_CHANNELS],
			   int         average_block_bits,
//			   int         available_bitreservoir_bits,
//			   int         padding_limit,
			   BsBitStream *fixed_stream,
//			   BsBitStream *var_stream,
//			   int         nr_of_chan,
			   double      *p_reconstructed_spectrum[MAX_TIME_CHANNELS],
//			   int         useShortWindows,
//			   int         aacAllowScalefacs,
			   AACQuantInfo* quantInfo,      /* AAC quantization information */
			   Ch_Info *ch_info
//			   ,int varBitRate
//			   ,int bitRate
                           );



#ifdef __cplusplus
}
#endif

#endif
