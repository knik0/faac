#ifndef QUANT_H
#define QUANT_H

#include "pulse.h"
#include "interface.h"
#include "tf_main.h"
#include "tns.h"
#include "all.h"
#include "nok_ltp_common.h"

#ifdef __cplusplus
extern "C" {
#endif




/* assumptions for the first run of this quantizer */
#define CHANNEL  1
#define NUM_COEFF  BLOCK_LEN_LONG       /* now using BLOCK_LEN_LONG of block.h */
#define MAGIC_NUMBER  0.4054
#define MAX_QUANT 8192
#define SF_OFFSET 100
#define ABS(A) ((A) < 0 ? (-A) : (A))
#define sgn(A) ((A) > 0 ? (1) : (-1))
#define SFB_NUM_MAX MAX_SCFAC_BANDS     /* now using MAX_SCFAC_BANDS of tf_main.h */


extern int pns_sfb_start;                        /* lower border for PNS */


/*********************************************************/
/* AACQuantInfo, info for AAC quantization and coding.   */
/*********************************************************/
typedef struct {
  int max_sfb;                          /* max_sfb, should = nr_of_sfb/num_window_groups */
  int nr_of_sfb;                        /* Number of scalefactor bands, interleaved */
  int spectralCount;                    /* Number of spectral data coefficients */
  enum WINDOW_TYPE block_type;	        /* Block type */      
  int scale_factor[SFB_NUM_MAX];        /* Scalefactor data array , interleaved */			
  int sfb_offset[250];                  /* Scalefactor spectral offset, interleaved */
  int book_vector[SFB_NUM_MAX];         /* Huffman codebook selected for each sf band */
  int data[5*NUM_COEFF];                /* Data of spectral bitstream elements, for each spectral pair, 
                                           5 elements are required: 1*(esc)+2*(sign)+2*(esc value)=5 */
  int len[5*NUM_COEFF];                 /* Lengths of spectral bitstream elements */
  int num_window_groups;                /* Number of window groups */
  int window_group_length
    [MAX_SHORT_IN_LONG_BLOCK];          /* Length (in windows) of each window group */
  int common_scalefac;                  /* Global gain */
  Window_shape window_shape;            /* Window shape parameter */
  Window_shape prev_window_shape;       /* Previous window shape parameter */
  short pred_global_flag;               /* Global prediction enable flag */
  int pred_sfb_flag[SFB_NUM_MAX];       /* Prediction enable flag for each scalefactor band */
  int reset_group_number;               /* Prediction reset group number */
  TNS_INFO* tnsInfo;                    /* Ptr to tns data */
  AACPulseInfo pulseInfo;
  NOK_LT_PRED_STATUS *ltpInfo;          /* Prt to LTP data */
  int pns_sfb_nrg[SFB_NUM_MAX];
  int pns_sfb_flag[SFB_NUM_MAX];
  int profile;
  int srate_idx;
} AACQuantInfo;


void quantize(AACQuantInfo *quantInfo,
			  double *pow_spectrum,
			  int quant[NUM_COEFF]
			  );
void dequantize(AACQuantInfo *quantInfo,
				double *p_spectrum,
				int quant[NUM_COEFF],
				double requant[NUM_COEFF],
				double error_energy[SFB_NUM_MAX]
				);
int count_bits(AACQuantInfo* quantInfo,
			   int quant[NUM_COEFF]
//			   ,int output_book_vector[SFB_NUM_MAX*2]
                           );


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
