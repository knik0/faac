/*
 *	Main definitions
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
  $Revision: 1.10 $
  $Date: 2000/10/05 08:39:03 $ (check in)
  $Author: menno $
  *************************************************************************/

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
#define MAX_TIME_CHANNELS 6

/* max. number of scale factor bands */
#define MAX_SCFAC_BANDS ((NSFB_SHORT+1)*MAX_SHORT_IN_LONG_BLOCK)

void freq2buffer(
  double           p_in_data[],
  double           p_out_data[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
  Window_shape     wfun_select,      
  Window_shape     wfun_select_prev,   
  Mdct_in	   overlap_select
);

void buffer2freq(                    
  double           p_in_data[],      
  double           p_out_mdct[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
  Window_shape     wfun_select,      
  Window_shape     wfun_select_prev,   
  Mdct_in          overlap_select      
);

void specFilter (double p_in[],
		 double p_out[],
		 int  samp_rate,
		 int lowpass_freq,
		 int    specLen
);

#endif	/* #ifndef _TF_MAIN_H_INCLUDED */

