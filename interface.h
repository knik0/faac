/*
 *	Definitions of Bitstream elements
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
  $Revision: 1.4 $
  $Date: 2000/10/06 14:47:27 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef _interface_h_
#define _interface_h_


/* AAC Profile */
enum AAC_PROFILE { MAIN, LOW, SSR };

#define MAX_SAMPLING_RATES 12

enum WINDOW_TYPE { 
  ONLY_LONG_WINDOW, 
  LONG_SHORT_WINDOW, 
  ONLY_SHORT_WINDOW,
  SHORT_LONG_WINDOW,
};    

#define NSFB_LONG  51
#define NSFB_SHORT 15

#define MAX_SHORT_WINDOWS 8

/* if static memory allocation is used, this value tells the max. nr of
   audio channels to be supported */
#define MAX_TIME_CHANNELS 6

/* max. number of scale factor bands */
#define MAX_SCFAC_BANDS ((NSFB_SHORT+1)*MAX_SHORT_WINDOWS)

#define BLOCK_LEN_LONG	   1024
#define BLOCK_LEN_SHORT    128


typedef enum {
    WS_SIN, WS_KBD, N_WINDOW_SHAPES
} Window_shape;

typedef enum {
	MOVERLAPPED,
	MNON_OVERLAPPED
} Mdct_in,Imdct_out;



/* 
 * Raw bitstream constants
 */
#define LEN_SE_ID 3
#define LEN_TAG 4
#define LEN_GLOB_GAIN 8
#define LEN_COM_WIN 1
#define LEN_ICS_RESERV 1
#define LEN_WIN_SEQ 2
#define LEN_WIN_SH 1
#define LEN_MAX_SFBL 6 
#define LEN_MAX_SFBS 4 
#define LEN_CB 4
#define LEN_SCL_PCM 8
#define LEN_PRED_PRES 1
#define LEN_PRED_RST 1
#define LEN_PRED_RSTGRP 5
#define LEN_PRED_ENAB 1
#define LEN_MASK_PRES 2
#define LEN_MASK 1
#define LEN_PULSE_PRES 1

#define LEN_TNS_PRES 1
#define LEN_TNS_NFILTL 2
#define LEN_TNS_NFILTS 1
#define LEN_TNS_COEFF_RES 1
#define LEN_TNS_LENGTHL 6
#define LEN_TNS_LENGTHS 4
#define LEN_TNS_ORDERL 5
#define LEN_TNS_ORDERS 3
#define LEN_TNS_DIRECTION 1
#define LEN_TNS_COMPRESS 1
#define LEN_GAIN_PRES 1

#define LEN_NEC_NPULSE 2 
#define LEN_NEC_ST_SFB 6 
#define LEN_NEC_POFF 5 
#define LEN_NEC_PAMP 4 
#define NUM_NEC_LINES 4 
#define NEC_OFFSET_AMP 4 

#define LEN_NCC 3
#define LEN_IS_CPE 1
#define LEN_CC_LR 1
#define LEN_CC_DOM 1
#define LEN_CC_SGN 1
#define LEN_CCH_GES 2
#define LEN_CCH_CGP 1
#define LEN_D_CNT 4
#define LEN_D_ESC 12
#define LEN_F_CNT 4
#define LEN_F_ESC 8
#define LEN_BYTE 8
#define LEN_PAD_DATA 8

#define LEN_PC_COMM 8 

#define ID_SCE 0
#define ID_CPE 1
#define ID_CCE 2
#define ID_LFE 3
#define ID_DSE 4
#define ID_PCE 5
#define ID_FIL 6
#define ID_END 7

/*
 * program configuration element
 */
#define LEN_PROFILE 2
#define LEN_SAMP_IDX 4
#define LEN_NUM_ELE 4
#define LEN_NUM_LFE 2
#define LEN_NUM_DAT 3
#define LEN_NUM_CCE 4
#define LEN_MIX_PRES 1
#define LEN_ELE_IS_CPE 1
#define LEN_IND_SW_CCE 1
#define LEN_COMMENT_BYTES 8
	
/*
 * audio data interchange format header
 */
#define LEN_ADIF_ID (32/8)
#define LEN_COPYRT_PRES 1
#define LEN_COPYRT_ID (72/8)
#define LEN_ORIG 1
#define LEN_HOME 1
#define LEN_BS_TYPE 1
#define LEN_BIT_RATE 23
#define LEN_NUM_PCE 4
#define LEN_ADIF_BF 20

typedef struct
{
    int is_present;  
    int ms_used[MAX_SCFAC_BANDS];
} MS_Info;

typedef struct
{
    int present;	/* channel present */
    int tag;		/* element tag */
    int cpe;		/* 0 if single channel or lfe, 1 if channel pair */ 
    int lfe;            /* 1 if lfe channel */             
    int	common_window;	/* 1 if common window for cpe */
    int	ch_is_left;	/* 1 if left channel of cpe */
    int	paired_ch;	/* index of paired channel in cpe */
    int widx;		/* window element index for this channel */
    MS_Info ms_info;    /* MS information */
} Ch_Info;

#endif   /* #ifndef _interface_h_ */

