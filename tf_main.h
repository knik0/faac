/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
 "This software module was originally developed by 
 Fraunhofer Gesellschaft IIS / University of Erlangen (UER) in the course of 
 development of the MPEG-2 NBC/MPEG-4 Audio standard ISO/IEC 13818-7, 
 14496-1,2 and 3. This software module is an implementation of a part of one or more 
 MPEG-2 NBC/MPEG-4 Audio tools as specified by the MPEG-2 NBC/MPEG-4 
 Audio standard. ISO/IEC  gives users of the MPEG-2 NBC/MPEG-4 Audio 
 standards free license to this software module or modifications thereof for use in 
 hardware or software products claiming conformance to the MPEG-2 NBC/MPEG-4
 Audio  standards. Those intending to use this software module in hardware or 
 software products are advised that this use may infringe existing patents. 
 The original developer of this software module and his/her company, the subsequent 
 editors and their companies, and ISO/IEC have no liability for use of this software 
 module or modifications thereof in an implementation. Copyright is not released for 
 non MPEG-2 NBC/MPEG-4 Audio conforming products.The original developer
 retains full right to use the code for his/her  own purpose, assign or donate the 
 code to a third party and to inhibit third party from using the code for non 
 MPEG-2 NBC/MPEG-4 Audio conforming products. This copyright notice must
 be included in all copies or derivative works." 
 Copyright(c)1996.
 *                                                                           *
 ****************************************************************************/

/* CREATED BY :  Bernhard Grill -- June-96  */

/* 28-Aug-1996  NI: added "NO_SYNCWORD" to enum TRANSPORT_STREAM */
/* 17-Spe-1997  CL: added AAC_PROFILE enum */

#ifndef _TF_MAIN_H_INCLUDED
#define _TF_MAIN_H_INCLUDED

#include "bitstream.h"
#include "block.h"

/* AAC Profile */
enum AAC_PROFILE { MAIN, LOW, SSR };

/* select different pre-/post- processing modules TK */
enum PP_MOD_SELECT { NONE=0x0, AAC_PP=0x1 };

/* select different T/F modules */
enum TF_MOD_SELECT { VM_TF_SOURCE=0x1, MDCT_AAC=0x2, MDCT_UER=0x4, QMF_MDCT_SONY=0x8, LOW_DELAY_UNH=0x10 };

/* select different Q&C modules */
enum QC_MOD_SELECT { VM_QC_SOURCE=0x1, AAC_QC=0x2, MDCT_VALUES_16BIT=0x4, UER_QC=0x8, NTT_VQ=0x10 , AAC_PRED=0x20};

/* name the audio channels */
enum CHANN_ASS { 
  MONO_CHAN=0,
  LEFT_CHAN=0, 
  RIGHT_CHAN=1,
  MAX_CHANNELS
};

/* audio channel configuration coding */
enum CH_CONFIG { CHC_MONO, CHC_DUAL, CHC_JOINT_DUAL, CHC_5CHAN, CHC_MODES };

/* transport layer type */ /* added "NO_SYNCWORD" by NI (28 Aug. 1996) */
enum TRANSPORT_STREAM { NO_TSTREAM, AAC_RAWDATA_TSTREAM, LENINFO_TSTREAM,
		        NO_SYNCWORD};

enum SR_CODING { SR8000, SR11025, SR12000, SR16000, SR22050, SR24000, SR32000, SR44100, SR48000, SR64000, SR88200, SR96000, MAX_SAMPLING_RATES };

enum WINDOW_TYPE { 
  ONLY_LONG_WINDOW, 
  LONG_SHORT_WINDOW, 
  ONLY_SHORT_WINDOW,
  SHORT_LONG_WINDOW,
  SHORT_MEDIUM_WINDOW,
  MEDIUM_LONG_WINDOW,
  LONG_MEDIUM_WINDOW,
  MEDIUM_SHORT_WINDOW,
  ONLY_MEDIUM_WINDOW,
  
  LONG_START_WINDOW,
  EIGHT_SHORT_WINDOW,
  LONG_STOP_WINDOW
};    

enum AAC_WINDOW_SEQUENCE { /* TK */
  ONLY_LONG_SEQUENCE = ONLY_LONG_WINDOW,
  LONG_START_SEQUENCE = LONG_SHORT_WINDOW,
  EIGHT_SHORT_SEQUENCE = ONLY_SHORT_WINDOW,
  LONG_STOP_SEQUENCE = SHORT_LONG_WINDOW
};

enum WIN_SWITCH_MODE {
  STATIC_LONG,
  STATIC_MEDIUM,
  STATIC_SHORT,
  LS_STARTSTOP_SEQUENCE,
  LM_STARTSTOP_SEQUENCE,
  MS_STARTSTOP_SEQUENCE,
  LONG_SHORT_SEQUENCE,
  LONG_MEDIUM_SEQUENCE,
  MEDIUM_SHORT_SEQUENCE,
  LONG_MEDIUM_SHORT_SEQUENCE,
  FFT_PE_WINDOW_SWITCHING
};

#define NSFB_LONG  51
#define NSFB_SHORT 15
#define MAX_SHORT_IN_LONG_BLOCK 8

#define MAX_SHORT_WINDOWS 8

/* if static memory allocation is used, this value tells the max. nr of
   audio channels to be supported */
/*#define MAX_TIME_CHANNELS (MAX_CHANNELS)*/
#define MAX_TIME_CHANNELS 2 //6

/* max. number of scale factor bands */
#define MAX_SCFAC_BANDS ((NSFB_SHORT+1)*MAX_SHORT_IN_LONG_BLOCK)

void freq2buffer(
  double           p_in_data[],
  double           p_out_data[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
//  int              nlong,            /* shift length for long windows   */
  Mdct_in	   overlap_select);    /* select imdct output *TK*	*/

void buffer2freq(                    /* Input: Time signal              */
  double           p_in_data[],      /* Output: MDCT cofficients        */
  double           p_out_mdct[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
//  Window_shape     wfun_select,      /* offers the possibility to select different window functions */
  Mdct_in        overlap_select      /* YT 970615 for Son_PP */
);

void make_MDCT_windows(void);

void specFilter (double p_in[],
				 double p_out[],
				 int  samp_rate,
				 int lowpass_freq,
				 int    specLen
				 );

#endif	/* #ifndef _TF_MAIN_H_INCLUDED */

