/**********************************************************************
MPEG-4 Audio VM



This software module was originally developed by

Fraunhofer Gesellschaft IIS / University of Erlangen (UER

and edited by

in the course of development of the MPEG-2 NBC/MPEG-4 Audio standard
ISO/IEC 13818-7, 14496-1,2 and 3. This software module is an
implementation of a part of one or more MPEG-2 NBC/MPEG-4 Audio tools
as specified by the MPEG-2 NBC/MPEG-4 Audio standard. ISO/IEC gives
users of the MPEG-2 NBC/MPEG-4 Audio standards free license to this
software module or modifications thereof for use in hardware or
software products claiming conformance to the MPEG-2 NBC/ MPEG-4 Audio
standards. Those intending to use this software module in hardware or
software products are advised that this use may infringe existing
patents. The original developer of this software module and his/her
company, the subsequent editors and their companies, and ISO/IEC have
no liability for use of this software module or modifications thereof
in an implementation. Copyright is not released for non MPEG-2
NBC/MPEG-4 Audio conforming products. The original developer retains
full right to use the code for his/her own purpose, assign or donate
the code to a third party and to inhibit third party from using the
code for non MPEG-2 NBC/MPEG-4 Audio conforming products. This
copyright notice must be included in all copies or derivative works.

Copyright (c) 1996.

-----
This software module was modified by

Tadashi Araki (Ricoh Company, ltd.)
Tatsuya Okada (Waseda Univ.)
Itaru Kaneko (Graphics Communication Laboratories)

and edited by

in the course of development of the MPEG-2 NBC/MPEG-4 Audio standard
ISO/IEC 13818-7, 14496-1,2 and 3.

Almost all part of the function EncTf_psycho_acoustic() is made by
T. Araki and T. Okada and its copyright belongs to Ricoh.
The function psy_get_absthr() is made by I. Kaneko
and its copyright belongs to Graphics Communication Laboratories.

Copyright (c) 1997.


Source file:

$Id: psych.c,v 1.17 2000/01/06 10:15:51 menno Exp $
$Id: psych.c,v 1.17 2000/01/06 10:15:51 menno Exp $
$Id: psych.c,v 1.17 2000/01/06 10:15:51 menno Exp $

**********************************************************************/

/* CREATED BY :  Bernhard Grill -- August-96  */
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "tf_main.h"
#include "psych.h"
#include "transfo.h"


SR_INFO sr_info_aac[MAX_SAMPLING_RATES+1] =
{
  { 8000  },
  { 11025 },
  { 12000 },
  { 16000 },

/* added by T. Araki (1997.10.16) */
  { 22050, 47, 15,
     { /*  cb_width_long[NSFB_LONG] */
      4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
      8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
      36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
     },
     { /* cb_width_short[NSFB_SHORT] */
      4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8, 12, 16, 16, 20
     }
  },
/* added by T. Araki (1997.10.16) end */

  { 24000 },
  { 32000 },

/* added by T. Araki (1997.07.09) */
  { 44100, 49, 14,
     { /*  cb_width_long[NSFB_LONG] */
      4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8, 
      12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96 
     }, 
     { /* cb_width_short[NSFB_SHORT] */
      4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16 
     }
/* added by T. Araki (1997.07.09) end */

  },
  { 48000, 49, 14,
     { /*  cb_width_long[NSFB_LONG] */
      4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8, 
      12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
      32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
     }, 
     { /* cb_width_short[NSFB_SHORT] */
      4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16 
     }
  },
  { 96000 },
  { -1 }
};

/* added by T. Araki (1997.07.10) */
PARTITION_TABLE_LONG  part_tbl_long_all[MAX_SAMPLING_RATES+1] =
{
  { 8000  },
  { 11025 },
  { 12000 },
  { 16000 },
 
/* added by T. Araki (1997.10.16) */
  { 22050, 63,
     { /* w_low */
      0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68,
      72, 77, 82, 87, 92, 97, 102, 108, 114, 120, 126, 133, 140, 147, 155,
      163, 172, 181, 191, 201, 212, 224, 237, 251, 266, 282, 299, 318, 338,
      360, 383, 408, 435, 464, 495, 528, 564, 602, 643, 687, 734, 785, 840,
      899, 963
     },
     {/* w_high */
      3, 7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63, 67, 71,
      76, 81, 86, 91, 96, 101, 107, 113, 119, 125, 132, 139, 146, 154, 162,
      171, 180, 190, 200, 211, 223, 236, 250, 265, 281, 298, 317, 337, 359,
      382, 407, 434, 463, 494, 527, 563, 601, 642, 686, 733, 784, 839, 898,
      962, 1023
     },
     { /* width */
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
      6, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17,
      19, 20, 22, 23, 25, 27, 29, 31, 33, 36, 38, 41, 44, 47, 51, 55, 59,
      64, 61
     }
  },
/* added by T. Araki (1997.10.16) end */

  { 24000 },
  { 32000 },
  { 44100, 70,
     { /* w_low */
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36,
      39, 42, 45, 48, 51, 54, 57, 60, 63, 67, 71, 75, 79, 83, 88, 93, 98,
      104, 110, 117, 124, 132, 140, 149, 158, 168, 179, 191, 204, 218, 233,
      249, 266, 284, 304, 325, 348, 372, 398, 426, 456, 489, 525, 564, 607,
      654, 707, 766, 833, 909, 997
     },
     { /* w_high */
      1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 38, 
      41, 44, 47, 50, 53, 56, 59, 62, 66, 70, 74, 78, 82, 87, 92, 97, 103,
      109, 116, 123, 131, 139, 148, 157, 167, 178, 190, 203, 217, 232, 248,
      265, 283, 303, 324, 347, 371, 397, 425, 455, 488, 524, 563, 606, 653,
      706, 765, 832, 908, 996, 1023
     },
     { /* width */
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 
      3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 11,
      12, 13, 14, 15, 16, 17, 18, 20, 21, 23, 24, 26, 28, 30, 33, 36, 39,
      43, 47, 53, 59, 67, 76, 88, 27
     }
  },
  { 48000, 69,
     { /* w_low */
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36,
      38, 41, 44, 47, 50, 53, 56, 59, 62, 66, 70, 74, 78, 82, 87, 92, 97,
      103, 109, 116, 123, 131, 139, 148, 158, 168, 179, 191, 204, 218, 233,
      249, 266, 284, 304, 325, 348, 372, 398, 426, 457, 491, 528, 568, 613,
      663, 719, 782, 854, 938
     },
     { /* w_high */
      1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37,
      40, 43, 46, 49, 52, 55, 58, 61, 65, 69, 73, 77, 81, 86, 91, 96, 102,
      108, 115, 122, 130, 138, 147, 157, 167, 178, 190, 203, 217, 232, 248,
      265, 283, 303, 324, 347, 371, 397, 425, 456, 490, 527, 567, 612, 662,
      718, 781, 853, 937, 1023
     }, 
     { /* width */
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
      3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 11, 12,
      13, 14, 15, 16, 17, 18, 20, 21, 23, 24, 26, 28, 31, 34, 37, 40, 45, 50,
      56, 63, 72, 84, 86
     }
  },
  { 96000 },
  { -1 }
};

PARTITION_TABLE_SHORT  part_tbl_short_all[MAX_SAMPLING_RATES+1] =
{
  { 8000  },
  { 11025 },
  { 12000 },
  { 16000 },

/* added by T. Araki (1997.10.16) */
  { 22050, 46,
     { /* w_low */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 22, 24, 26, 28, 30, 32, 34, 36, 39, 42, 45, 48, 52, 56, 60, 64, 69,
      74, 79, 85, 91, 98, 105, 113, 121
     },
     { /* w_high */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      21, 23, 25, 27, 29, 31, 33, 35, 38, 41, 44, 47, 51, 55, 59, 63, 68, 73,
      78, 84, 90, 97, 104, 112, 120, 127
     },
     { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
      2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 7
     }
  },
/* added by T. Araki (1997.10.16) end */

  { 24000 },
  { 32000 },
  { 44100, 42, 
     { /* w_low */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20,
      22, 24, 26, 28, 30, 32, 35, 38, 41, 44, 48, 52, 56, 60, 65, 70, 76,
      82, 89, 97, 106, 116
     },
     { /* w_high */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21,
      23, 25, 27, 29, 31, 34, 37, 40, 43, 47, 51, 55, 59, 64, 69, 75, 81,
      88, 96, 105, 115, 127
     },
     { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
      2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10, 12
     }
  },
  { 48000, 42,
     { /* w_low */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 21,
      23, 25, 27, 29, 31, 34, 37, 40, 43, 46, 50, 54, 58, 63, 68, 74, 80,
      87, 95, 104, 114, 126
     },
     { /* w_high */
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22,
      24, 26, 28, 30, 33, 36, 39, 42, 45, 49, 53, 57, 62, 67, 73, 79, 86,
      94, 103, 113, 125, 127
     }, 
     { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
      2, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10, 12, 1
     }
  },
  { 96000 },
  { -1 }
};
/* added by T. Araki (1997.07.10) end */

/* added by T. Araki (1997.10.16) */
double          sample[MAX_TIME_CHANNELS+2][BLOCK_LEN_LONG*2];
                               /* sample value */

FFT_TABLE_LONG    fft_tbl_long;  /* table for long fft */
FFT_TABLE_SHORT    fft_tbl_short;  /* table for short fft */
PARTITION_TABLE_LONG    *part_tbl_long;  
PARTITION_TABLE_SHORT    *part_tbl_short;
DYN_PART_TABLE_LONG     dyn_long;  
DYN_PART_TABLE_SHORT    dyn_short;
PSY_STATVARIABLE_LONG    psy_stvar_long[MAX_TIME_CHANNELS+2];
                               /* static variables for long block */
PSY_STATVARIABLE_SHORT    psy_stvar_short[MAX_TIME_CHANNELS+2];
                               /* static variables for short block */
/* added by T. Araki (1997.10.16) end */

void EncTf_psycho_acoustic_init( void )
{
	int chanNum;

	/* added by T. Araki (1997.10.16) */
	psy_fft_table_init(&fft_tbl_long, &fft_tbl_short);
	/* initializing fft table */
	for (chanNum=0;chanNum<MAX_TIME_CHANNELS+2;chanNum++) {
		psy_calc_init(&sample[chanNum], &psy_stvar_long[chanNum], &psy_stvar_short[chanNum]);
		/* initializing static variables */
	}
	/* added by T. Araki (1997.10.16) end */
}

/* added by T. Okada (1997.07.10) */
void psy_fft_table_init(FFT_TABLE_LONG *fft_tbl_long,
			FFT_TABLE_SHORT *fft_tbl_short
			)
{

int i;

/* generating Hann window */
for(i = 0; i < BLOCK_LEN_LONG*2; i++)
	fft_tbl_long->hw[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_LONG*2)));
for(i = 0; i < BLOCK_LEN_SHORT*2; i++)
	fft_tbl_short->hw[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_SHORT*2)));

MakeFFTOrder();
}
/* added by T. Okada (1997.07.10) end */

/* added by T. Araki (1997.07.10) */
void psy_part_table_init( 
			   double sampling_rate,
			   PARTITION_TABLE_LONG **part_tbl_long,
			   PARTITION_TABLE_SHORT **part_tbl_short
			   )
{
    int b,bb; /* Jul 10 */
    double tmp;
	int partition[1024], j, w;

	*part_tbl_long = &part_tbl_long_all[0];

	/* find correct sampling rate depending parameters */
	while( (*part_tbl_long)->sampling_rate != (int)sampling_rate ) {
		(*part_tbl_long)++;
	}

	*part_tbl_short = &part_tbl_short_all[0];

	/* find correct sampling rate depending parameters */
	while( (*part_tbl_short)->sampling_rate != (int)sampling_rate ) {
		(*part_tbl_short)++;
	}

	for (b = 0; b < (*part_tbl_long)->len; b++) {
		for(w = (*part_tbl_long)->w_low[b]; w <= (*part_tbl_long)->w_high[b]; ++w){
			partition[w] = b;
		}
	}

	for(b = 0; b < (*part_tbl_long)->len ; b++) {
		for (j=0;(b != partition[j]);j++);
		{
			double ji = j + ((*part_tbl_long)->width[b]-1)/2.0;
			double freq = (*part_tbl_long)->sampling_rate*ji/2048;
			double bark = 13*atan(0.00076*freq)+3.5*atan((freq/7500)*(freq/7500));
			dyn_long.bval[b] = bark;
		}
	}

	for (b = 0; b < (*part_tbl_short)->len; b++) {
		for(w = (*part_tbl_short)->w_low[b]; w <= (*part_tbl_short)->w_high[b]; ++w){
			partition[w] = b;
		}
	}

	for(b = 0; b < (*part_tbl_short)->len ; b++) {
		for (j=0;(b != partition[j]);j++);
		{
			double ji = j + ((*part_tbl_short)->width[b]-1)/2.0;
			double freq = (*part_tbl_short)->sampling_rate*ji/256;
			double bark = 13*atan(0.00076*freq) + 3.5*atan((freq/7500)*(freq/7500));
			dyn_short.bval[b]=bark;
		}
	}

	// Calculate the spreading function
	{
		double tmpx,tmpy,tmpz,b1,b2;
		int b, bb;

		for( b = 0; b < (*part_tbl_long)->len; b++) {
			b2 = dyn_long.bval[b];
			for( bb = 0; bb < (*part_tbl_long)->len; bb++) {
				b1 = dyn_long.bval[bb];

				//tmpx = (b2 >= b1) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				tmpx = (bb >= b) ? 3.0*(b2-b1) : 1.5*(b2-b1);

				tmpz = 8.0 * psy_min( (tmpx-0.5)*(tmpx-0.5) - 2.0*(tmpx-0.5),0.0 );

				tmpy = 15.811389 + 7.5*(tmpx + 0.474)-17.5 *sqrt(1.0 + (tmpx+0.474)*(tmpx+0.474));

				dyn_long.spreading[b][bb] = ( tmpy < -100.0 ? 0.0 : pow(10.0, (tmpz + tmpy)/10.0) );
			}
		}

		for( b = 0; b < (*part_tbl_short)->len; b++) {
			b2 = dyn_short.bval[b];
			for( bb = 0; bb < (*part_tbl_short)->len; bb++) {
				b1 = dyn_short.bval[bb];

				//tmpx = (b2 >= b1) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				tmpx = (bb >= b) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				
				tmpz = 8.0 * psy_min( (tmpx-0.5)*(tmpx-0.5) - 2.0*(tmpx-0.5),0.0 );

				tmpy = 15.811389 + 7.5*(tmpx + 0.474)-17.5 *sqrt(1.0 + (tmpx+0.474)*(tmpx+0.474));

				dyn_short.spreading[b][bb] = ( tmpy < -100.0 ? 0.0 : pow(10.0, (tmpz + tmpy)/10.0) );
			}
		}
	}

    /* added by T. Okada (1997.07.10) */
    for( b = 0; b < (*part_tbl_long)->len; b++){
		tmp = 0.0;
		for( bb = 0; bb < (*part_tbl_long)->len; bb++)
			//tmp += sprdngf( (*part_tbl_long),(*part_tbl_short), bb, b, 0);
			tmp += dyn_long.spreading[bb][b];
		dyn_long.rnorm[b] = 1.0/tmp;
    }
    /* added by T. Okada (1997.07.10) end */

    /* added by T. Araki (1997.10.16) */
    for( b = 0; b < (*part_tbl_short)->len; b++){
		tmp = 0.0;
		for( bb = 0; bb < (*part_tbl_short)->len; bb++)
			//tmp += sprdngf( (*part_tbl_long), (*part_tbl_short), bb, b, 1);
			tmp += dyn_short.spreading[bb][b];
		dyn_short.rnorm[b] = 1.0/tmp;
    }
    /* added by T. Araki (1997.10.16) end */

	for(b = 0; b < (*part_tbl_long)->len; b++) {
		dyn_long.bmax[b] = pow(10, -3*(0.5+0.5*(M_PI*(min(dyn_long.bval[b], 15.5)/15.5))));
	}
	for(b = 0; b < (*part_tbl_short)->len; b++) {
		dyn_short.bmax[b] = pow(10, -3*(0.5+0.5*(M_PI*(min(dyn_short.bval[b], 15.5)/15.5))));
	}

	(*part_tbl_long)->dyn = &dyn_long;
	(*part_tbl_short)->dyn = &dyn_short;
}


void psy_calc_init( 
		   double sample[][BLOCK_LEN_LONG*2],
		   PSY_STATVARIABLE_LONG *psy_stvar_long,
		   PSY_STATVARIABLE_SHORT *psy_stvar_short
		   )
{
	int ch = 0;
	int i;

	for(i = 0; i < BLOCK_LEN_LONG*2; i++){
		sample[ch][i] = 0.0;
	}

    /*  for(ch = 0; ch < Chans; ++ch){ */
    for(i = 0; i < BLOCK_LEN_LONG*3; i++){
      psy_stvar_long->fft_r[i] = 0.0;
      psy_stvar_long->fft_f[i] = 0.0;
    }
    /*  }*/

  psy_stvar_long->p_fft = 0;

  /*  for(ch = 0; ch < Chans; ++ch){*/
    for(i = 0; i < NPART_LONG*2; i++){
      psy_stvar_long->nb[i] = 90.0;
    }
    /*}*/

  psy_stvar_long->p_nb = NPART_LONG;
/* added by T. Araki (1997.07.10) end */

/* added by T. Araki (1997.10.16) */
  /*  for(ch = 0; ch < Chans; ++ch){*/
  for(i = 0; i < BLOCK_LEN_SHORT; i++) {
      psy_stvar_short->last6_fft_r[i] = 0.0;
      psy_stvar_short->last6_fft_f[i] = 0.0;
      psy_stvar_short->last7_fft_r[i] = 0.0;
      psy_stvar_short->last7_fft_f[i] = 0.0;
    }
    /*  }*/

    /*  for(ch = 0; ch < Chans; ++ch){*/
    for(i = 0; i < NPART_SHORT; i++){
      psy_stvar_short->last7_nb[i] = 90.0;
    }
    /* }*/
/* added by T. Araki (1997.10.16) end */
}

void psy_fill_lookahead(double *p_time_signal[], int no_of_chan)
{
	int i, ch;

	for (ch = 0; ch < no_of_chan; ch++) {
		for(i = 0; i < BLOCK_LEN_LONG; i++){
			sample[ch][i+BLOCK_LEN_LONG] = p_time_signal[ch][i]/32767;
		}
	}
}

/* main */
void EncTf_psycho_acoustic( 
			   /* input */
			   double sampling_rate,
			   int    no_of_chan,         /* no of audio channels */
			   double *p_time_signal[],
			   enum WINDOW_TYPE block_type[],
			   int qcSelect,
			   int frameLength,
			   /* output */
			   CH_PSYCH_OUTPUT_LONG p_chpo_long[],
			   CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS]
			   )
{
	int             ch, i, b;
    SR_INFO         *p_sri;

	/* added by T. Araki (1997.07.10) */

    static int   flag = 0;

	static double save_tb_l[2][70];
	static double save_tb_s[2][8][42];
	static double save_cw_l[70];
	static double save_cw_s[8][42];
	int use_ms_l[NSFB_LONG];
	int use_ms_s[MAX_SHORT_WINDOWS][NSFB_SHORT];

    PSY_VARIABLE_LONG    psy_var_long;  /* variables for long block */
    PSY_VARIABLE_SHORT    psy_var_short;  /* variables for short block */

	memset(&psy_var_long, 0, sizeof(psy_var_long));
	memset(&psy_var_short, 0, sizeof(psy_var_short));
	/* added by T. Araki (1997.07.10) end */

    p_sri = &sr_info_aac[0];
	
	/* find correct sampling rate depending parameters */
	while( p_sri->sampling_rate != (long)sampling_rate ) {
		p_sri++;
	}

	/* added by T. Araki (1997.07.10) */
	if( flag==0 ) {
		psy_part_table_init(sampling_rate, &part_tbl_long, &part_tbl_short);
		/* initializing Table B 2.1.*.a, B 2.1.*.b in N1650 */
		flag = 1;
	}

	{
		ch = 0;
		psy_step1(p_time_signal,sample, no_of_chan);
		psy_step2(&sample[no_of_chan], &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &fft_tbl_long, 
			&fft_tbl_short, ch);
		psy_step3(&psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);
		psy_step4(&psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);

		if (no_of_chan == 0) {
			for (b = 0; b < 70; b++)
				save_cw_l[b] = psy_var_long.cw[b];
			for (i = 0; i < 8; i++)
				for (b = 0; b < 42; b++)
					save_cw_s[i][b] = psy_var_short.cw[i][b];
		}
		if (no_of_chan == 1) {
			for (b = 0; b < 70; b++)
				save_cw_l[b] = min(psy_var_long.cw[b], save_cw_l[b]);
			for (i = 0; i < 8; i++)
				for (b = 0; b < 42; b++)
					save_cw_s[i][b] = min(psy_var_short.cw[i][b], save_cw_s[i][b]);
		}
		if (no_of_chan > 1) {
			for (b = 0; b < 70; b++)
				psy_var_long.cw[b] = save_cw_l[b];
			for (i = 0; i < 8; i++)
				for (b = 0; b < 42; b++)
					psy_var_short.cw[i][b] = save_cw_s[i][b];
		}

		psy_step5(part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], 
			&psy_var_long, &psy_var_short, ch);
		psy_step6(part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan],
			&psy_var_long, &psy_var_short);
		psy_step7(part_tbl_long, part_tbl_short, &psy_var_long, &psy_var_short);

		if (no_of_chan < 2) {
			for (b = 0; b < 70; b++)
				save_tb_l[no_of_chan][b] = psy_var_long.tb[b];
			for (i = 0; i < 8; i++)
				for (b = 0; b < 42; b++)
					save_tb_s[no_of_chan][i][b] = psy_var_short.tb[i][b];
		} else {
			for (b = 0; b < 70; b++)
				psy_var_long.tb[b] = save_tb_l[no_of_chan-2][b];
			for (i = 0; i < 8; i++)
				for (b = 0; b < 42; b++)
					psy_var_short.tb[i][b] = save_tb_s[no_of_chan-2][i][b];
		}

		psy_step8(part_tbl_long, part_tbl_short, &psy_var_long, &psy_var_short);
		psy_step9(part_tbl_long, part_tbl_short, &psy_var_long, &psy_var_short);
		psy_step10(part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], 
			&psy_var_long, &psy_var_short, ch);
		psy_step11andahalf(part_tbl_long, part_tbl_short, psy_stvar_long, psy_stvar_short, no_of_chan);
		psy_step11(part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], ch);
		psy_step12(part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan],
			&psy_var_long, &psy_var_short, ch);
		psy_step13(&psy_var_long, block_type, no_of_chan);
		psy_step14(p_sri, part_tbl_long, part_tbl_short, &psy_stvar_long[no_of_chan], 
			&psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);
		psy_step15(use_ms_l, use_ms_s, p_sri, &psy_stvar_long[0], &psy_stvar_short[0], &psy_var_long, &psy_var_short, no_of_chan);
	}	

	/*  for( ch=0; ch<no_of_chan; ch++ ) { */
	{   /* Now performed for only one channel at a time, CL 97.01.10 */
		int i;

		p_chpo_long[no_of_chan].p_ratio   = psy_stvar_long[no_of_chan].ismr;
		p_chpo_long[no_of_chan].cb_width  = p_sri->cb_width_long;
		p_chpo_long[no_of_chan].no_of_cb = p_sri->num_cb_long;
		if (no_of_chan == 1)
			memcpy(p_chpo_long[no_of_chan].use_ms, use_ms_l, NSFB_LONG*sizeof(int));

		for( i=0; i<MAX_SHORT_WINDOWS; i++ ) {
			p_chpo_short[no_of_chan][i].p_ratio  = psy_stvar_short[no_of_chan].ismr[i];
			p_chpo_short[no_of_chan][i].cb_width = p_sri->cb_width_short;
			p_chpo_short[no_of_chan][i].no_of_cb = p_sri->num_cb_short;
			if (no_of_chan == 1)
				memcpy(p_chpo_short[no_of_chan][i].use_ms, use_ms_s[i], NSFB_SHORT*sizeof(int));
		}

	}
}

void psy_step1(double* p_time_signal[], 
	       double sample[][BLOCK_LEN_LONG*2], 
	       int ch)
{
	int i;

	for(i = 0; i < BLOCK_LEN_LONG; i++){
		sample[ch][i] = sample[ch][i+BLOCK_LEN_LONG];
		sample[ch][i+BLOCK_LEN_LONG] = p_time_signal[0][i]/32767;
	}
}

void psy_step2(double sample[][BLOCK_LEN_LONG*2],
               PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
	       FFT_TABLE_LONG *fft_tbl_long,
	       FFT_TABLE_SHORT *fft_tbl_short,
	       int ch
	       )
{
    int w,i,l,unscambled;
    double t_re,t_im;

    /* FFT for long */
    psy_stvar_long->p_fft += BLOCK_LEN_LONG;

    if(psy_stvar_long->p_fft == BLOCK_LEN_LONG * 3)
		psy_stvar_long->p_fft = 0;

    /* windowing */
    for(i = 0; i < BLOCK_LEN_LONG*2; i++){
		FFTarray[i].re = fft_tbl_long->hw[i] * sample[ch][i];
		FFTarray[i].im = 0.0;
    }

    pfftw_d_2048(FFTarray);

        for(w = 0; w < BLOCK_LEN_LONG; w++){
 		unscambled = unscambled2048[w];
                t_re = FFTarray[unscambled].re;
                t_im = FFTarray[unscambled].im;
		psy_stvar_long->fft_r[w+psy_stvar_long->p_fft]
			= sqrt(t_re*t_re + t_im*t_im);

		if( t_re > 0.0 ){
			if( t_im >= 0.0 )
				psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = atan( t_im / t_re );
			else
				psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = atan( t_im / t_re )+ M_PI * 2.0;
		} else if( t_re < 0.0 ) {
			psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = atan( t_im / t_re ) + M_PI;
		} else {
			if( t_im > 0.0 )
				psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = M_PI * 0.5;
			else if( t_im < 0.0 )
				psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = M_PI * 1.5;
			else
				psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = 0.0; /* tmp */
		}
    }

	/* FFT for short */

	for(l = 0; l < MAX_SHORT_WINDOWS; l++){

        /* windowing */
        for(i = 0; i < BLOCK_LEN_SHORT*2; i++){
			FFTarray[i].re = fft_tbl_short->hw[i] * sample[ch][/*OFFSET_FOR_SHORT +*/ BLOCK_LEN_SHORT * l + i];
			FFTarray[i].im = 0.0;
		}

                pfftw_d_256(FFTarray);

		for(w = 0; w < BLOCK_LEN_SHORT; w++){
         		unscambled = unscambled256[w];
                        t_re = FFTarray[unscambled].re;
                        t_im = FFTarray[unscambled].im;
			psy_stvar_short->fft_r[l][w]
				= sqrt(t_re*t_re + t_im*t_im);

			if( t_re > 0.0 ){
				if( t_im >= 0.0 )
					psy_stvar_short->fft_f[l][w] = atan( t_im / t_re );
				else
					psy_stvar_short->fft_f[l][w] = atan( t_im / t_re )+ M_PI * 2.0;
			} else if( t_re < 0.0 ) {
				psy_stvar_short->fft_f[l][w] = atan( t_im / t_re ) + M_PI;
			} else {
				if( t_im > 0.0 )
					psy_stvar_short->fft_f[l][w] = M_PI * 0.5;
				else if( t_im < 0.0 )
					psy_stvar_short->fft_f[l][w] = M_PI * 1.5;
				else
					psy_stvar_short->fft_f[l][w] = 0.0; /* tmp */
			}
		}
    }
}

void psy_step3(PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
               PSY_VARIABLE_LONG *psy_var_long,
               PSY_VARIABLE_SHORT *psy_var_short,
               int ch
	       )
{
    int w,i;
    int p1_l,p2_l;

	p1_l = psy_stvar_long->p_fft - BLOCK_LEN_LONG;
    if( p1_l < 0 )
		p1_l = BLOCK_LEN_LONG * 2;
    p2_l = p1_l - BLOCK_LEN_LONG;
    if( p2_l < 0 )
		p2_l = BLOCK_LEN_LONG * 2;

    for(w = 0; w < BLOCK_LEN_LONG; w++){
		psy_var_long->r_pred[w] = 2.0 * psy_stvar_long->fft_r[p1_l + w] - psy_stvar_long->fft_r[p2_l + w];
		psy_var_long->f_pred[w] = 2.0 * psy_stvar_long->fft_f[p1_l + w] - psy_stvar_long->fft_f[p2_l + w];
    }

	/* added by T. Araki (1997.10.16) */
    for(w = 0; w < BLOCK_LEN_SHORT; w++){
        psy_var_short->r_pred[0][w] = 2.0 * psy_stvar_short->last7_fft_r[w] - psy_stvar_short->last6_fft_r[w];
        psy_var_short->f_pred[0][w] = 2.0 * psy_stvar_short->last7_fft_f[w] - psy_stvar_short->last6_fft_f[w];
        psy_var_short->r_pred[1][w] = 2.0 * psy_stvar_short->fft_r[0][w] - psy_stvar_short->last7_fft_r[w];
        psy_var_short->f_pred[1][w] = 2.0 * psy_stvar_short->fft_f[0][w] - psy_stvar_short->last7_fft_f[w];
    }

    for(i = 2; i < MAX_SHORT_WINDOWS; i++){
        for(w = 0; w < BLOCK_LEN_SHORT; w++){
			psy_var_short->r_pred[i][w] = 2.0 * psy_stvar_short->fft_r[i - 1][w] - psy_stvar_short->fft_r[i - 2][w];
			psy_var_short->f_pred[i][w] = 2.0 * psy_stvar_short->fft_f[i - 1][w] - psy_stvar_short->fft_f[i - 2][w];
		}
    }

    for(w = 0; w < BLOCK_LEN_SHORT; w++){
        psy_stvar_short->last6_fft_r[w] = psy_stvar_short->fft_r[6][w];
		psy_stvar_short->last6_fft_f[w] = psy_stvar_short->fft_f[6][w];
        psy_stvar_short->last7_fft_r[w] = psy_stvar_short->fft_r[7][w];
		psy_stvar_short->last7_fft_f[w] = psy_stvar_short->fft_f[7][w];
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step4(PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
	       PSY_VARIABLE_LONG *psy_var_long,
	       PSY_VARIABLE_SHORT *psy_var_short,
	       int ch
	       )
{
    int w,i;
    double r,f,rp,fp;

    for(w = 0; w < BLOCK_LEN_LONG; w++){
		r = psy_stvar_long->fft_r[psy_stvar_long->p_fft+w];
		f = psy_stvar_long->fft_f[psy_stvar_long->p_fft+w];
		rp = psy_var_long->r_pred[w];
		fp = psy_var_long->f_pred[w];

		if( r + fabs(rp) != 0.0 )
			psy_var_long->c[w] = sqrt( psy_sqr(r*cos(f) - rp*cos(fp))
				+psy_sqr(r*sin(f) - rp*sin(fp)) )/ ( r + fabs(rp) ) ;
		else
			psy_var_long->c[w] = 0.0; /* tmp */
    }

	/* added by T. Araki (1997.10.16) */
    for(i = 0; i < MAX_SHORT_WINDOWS; i++){
        for(w = 0; w < BLOCK_LEN_SHORT; w++){
			r = psy_stvar_short->fft_r[i][w];
			f = psy_stvar_short->fft_f[i][w];
			rp = psy_var_short->r_pred[i][w];
			fp = psy_var_short->f_pred[i][w];

			if( r + fabs(rp) != 0.0 )
				psy_var_short->c[i][w] = sqrt( psy_sqr(r*cos(f) - rp*cos(fp))
					+psy_sqr(r*sin(f) - rp*sin(fp)) )/ ( r + fabs(rp) ) ;
			else
				psy_var_short->c[i][w] = 0.0; /* tmp */
		}
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step5(PARTITION_TABLE_LONG *part_tbl_long, 
			   PARTITION_TABLE_SHORT *part_tbl_short, 
			   PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   PSY_VARIABLE_LONG *psy_var_long, 
			   PSY_VARIABLE_SHORT *psy_var_short,
			   int ch
			   )
{
    int b,w,i;
    double tmp_cb;

    for(b = 0; b < part_tbl_long->len; b++){
		psy_var_long->e[b] = 0.0;
		tmp_cb = 0.0;

		/* added by T. Araki (1997.10.16) */
		for(w = part_tbl_long->w_low[b]; w <= part_tbl_long->w_high[b]; w++){
			psy_var_long->e[b] += psy_sqr(psy_stvar_long->fft_r[psy_stvar_long->p_fft+w]);
			tmp_cb += psy_sqr(psy_stvar_long->fft_r[psy_stvar_long->p_fft+w]) * psy_var_long->c[w];
		}
		/* added by T. Araki (1997.10.16) end */

		psy_var_long->c[b] = tmp_cb;
    }

	/* added by T. Araki (1997.10.16) */
    for(i = 0; i < MAX_SHORT_WINDOWS; i++){
        for(b = 0; b < part_tbl_short->len; b++){
			psy_var_short->e[i][b] = 0.0;
			tmp_cb = 0.0;

			for(w = part_tbl_short->w_low[b]; w <= part_tbl_short->w_high[b]; w++){
				psy_var_short->e[i][b] += psy_sqr(psy_stvar_short->fft_r[i][w]);
				tmp_cb += psy_sqr(psy_stvar_short->fft_r[i][w]) * psy_var_short->c[i][w]; 
			}

			psy_var_short->c[i][b] = tmp_cb;
		}
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step6(PARTITION_TABLE_LONG *part_tbl_long, 
			   PARTITION_TABLE_SHORT *part_tbl_short, 
			   PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   PSY_VARIABLE_LONG *psy_var_long, 
			   PSY_VARIABLE_SHORT *psy_var_short
			   )
{
    int b,bb,i;
    double ecb,ct;
    double sprd;

    for(b = 0; b < part_tbl_long->len; b++){
		ecb = 0.0;
		ct = 0.0;
		for(bb = 0; bb < part_tbl_long->len; bb++){
			//sprd = sprdngf(part_tbl_long, part_tbl_short, bb, b, 0);
			sprd = part_tbl_long->dyn->spreading[bb][b];
			ecb += psy_var_long->e[bb] * sprd;
			ct += psy_var_long->c[bb] * sprd;
		}
		if (ecb!=0.0) {
			psy_var_long->cb[b] = ct / ecb;
		} else {
			psy_var_long->cb[b] = 0.0;
		}
		psy_stvar_long->en[b] = psy_var_long->en[b] = ecb * part_tbl_long->dyn->rnorm[b];
    }

	/* added by T. Araki (1997.10.16) */
    for(i = 0; i < MAX_SHORT_WINDOWS; i++){ 
        for(b = 0; b < part_tbl_short->len; b++){
			ecb = 0.0;
			ct = 0.0;
			for(bb = 0; bb < part_tbl_short->len; bb++){
				//sprd = sprdngf(part_tbl_long, part_tbl_short, bb, b, 1);
				sprd = part_tbl_short->dyn->spreading[bb][b];
				ecb += psy_var_short->e[i][bb] * sprd;
				ct += psy_var_short->c[i][bb] * sprd;
			}
			if (ecb!=0.0) {	
				psy_var_short->cb[i][b] = ct / ecb;
			} else {
				psy_var_short->cb[i][b] = 0.0;
			}
			psy_stvar_short->en[i][b] = psy_var_short->en[i][b] = ecb * part_tbl_short->dyn->rnorm[b];
		}
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step7(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long, 
	       PSY_VARIABLE_SHORT *psy_var_short)
{
    int b,i;

    for(b = 0; b < part_tbl_long->len; b++){
		if (psy_var_long->cb[b] > 0.0) {
			psy_var_long->tb[b] = -0.299 - 0.43 * log(psy_var_long->cb[b]);
		} else {
			psy_var_long->tb[b] = 1.0;
		}
		if( psy_var_long->tb[b] > 1.0 )
			psy_var_long->tb[b] = 1.0;
		else if( psy_var_long->tb[b] < 0.0 )
			psy_var_long->tb[b] = 0.0;
    }


	/* added by T. Araki (1997.10.16) */
    for(i = 0;  i < MAX_SHORT_WINDOWS; i++){
        for(b = 0; b < part_tbl_short->len; b++){
			if (psy_var_short->cb[i][b]>0.0) {
				psy_var_short->tb[i][b] = -0.299 - 0.43 * log(psy_var_short->cb[i][b]);
			} else {
				psy_var_short->tb[i][b] = 1.0;
			}
			if( psy_var_short->tb[i][b] > 1.0 )
				psy_var_short->tb[i][b] = 1.0;
			else if( psy_var_short->tb[i][b] < 0.0 )
				psy_var_short->tb[i][b] = 0.0;
		}
    }
	/* added by T. Araki (1997.10.16) end */
}


void psy_step8(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long, 
	       PSY_VARIABLE_SHORT *psy_var_short)
{
	int b,i;
	double tmn = 18.0, nmt = 6.0;

	for(b = 0; b < part_tbl_long->len; b++) {
		psy_var_long->snr[b] = psy_var_long->tb[b] * tmn + (1.0 - psy_var_long->tb[b] ) * nmt;
	}

	/* added by T. Araki (1997.10.16) */
	for(i = 0;  i < MAX_SHORT_WINDOWS; i++){
		for(b = 0; b < part_tbl_short->len; b++)
			psy_var_short->snr[i][b] = psy_var_short->tb[i][b] * tmn + (1.0 - psy_var_short->tb[i][b] ) * nmt ;
	}
	/* added by T. Araki (1997.10.16) end */
}    

void psy_step9(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long, 
	       PSY_VARIABLE_SHORT *psy_var_short
	       )
{
    int b,i;
    
    for(b = 0; b < part_tbl_long->len; b++)
		psy_var_long->bc[b] = pow(10.0, -psy_var_long->snr[b]/10.0);

	/* added by T. Araki (1997.10.16) */
    for(i = 0;  i < MAX_SHORT_WINDOWS; i++){
        for(b = 0; b < part_tbl_short->len; b++)
			psy_var_short->bc[i][b] = pow(10.0, -psy_var_short->snr[i][b]/10.0);
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step10(PARTITION_TABLE_LONG *part_tbl_long,
		PARTITION_TABLE_SHORT *part_tbl_short,
		PSY_STATVARIABLE_LONG *psy_stvar_long, 
		PSY_STATVARIABLE_SHORT *psy_stvar_short, 
		PSY_VARIABLE_LONG *psy_var_long, 
		PSY_VARIABLE_SHORT *psy_var_short,
		int ch
		)
{
    int b,i;
    
    psy_stvar_long->p_nb += NPART_LONG;

    if( psy_stvar_long->p_nb == NPART_LONG*2 ) psy_stvar_long->p_nb = 0;

    for(b = 0; b < part_tbl_long->len; b++){
		psy_stvar_long->nb[psy_stvar_long->p_nb + b]
			= psy_var_long->en[b] * psy_var_long->bc[b];
    }

	/* added by T. Araki (1997.10.16) */
    for(i = 0;  i < MAX_SHORT_WINDOWS; i++){
        for(b = 0; b < part_tbl_short->len; b++){
			psy_stvar_short->nb[i][b]
				= psy_var_short->en[i][b] * psy_var_short->bc[i][b];
		}
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_step11(PARTITION_TABLE_LONG *part_tbl_long, 
		PARTITION_TABLE_SHORT *part_tbl_short, 
		PSY_STATVARIABLE_LONG *psy_stvar_long, 
		PSY_STATVARIABLE_SHORT *psy_stvar_short, 
		int ch
		)
{
    int b,i;
    int p1,p2;
	double temp;

    p1 = psy_stvar_long->p_nb;
    if( p1 == 0 ) p2 = NPART_LONG;
    else if( p1 == NPART_LONG ) p2 = 0;

    for(b = 0; b < part_tbl_long->len; b++) {
		temp = psy_min( psy_stvar_long->nb[p1+b],2.0*psy_stvar_long->nb[p2+b]);
		if (temp > 0.01)
			psy_stvar_long->nb[p1+b] = temp;
    }

	/* added by T. Araki (1997.10.16) */
    for(b = 0; b < part_tbl_short->len; b++){
		temp = psy_min( psy_stvar_short->nb[0][b], 1.0*psy_stvar_short->last7_nb[b]);
		if (temp > 0.01)
			psy_stvar_short->nb[0][b] = temp;
    }

	for(b = 0; b < part_tbl_short->len; b++){
		psy_stvar_short->last7_nb[b] = psy_stvar_short->nb[7][b];
	}

	for(i = 1;  i < MAX_SHORT_WINDOWS; i++){
		for(b = 0; b < part_tbl_short->len; b++){
			temp = psy_min( psy_stvar_short->nb[i][b],1.0*psy_stvar_short->nb[i - 1][b]);
			if (temp > 0.01)
				psy_stvar_short->nb[i][b] = temp;
		}
	}
	/* added by T. Araki (1997.10.16) end */
}

void psy_step11andahalf(PARTITION_TABLE_LONG *part_tbl_long, 
						PARTITION_TABLE_SHORT *part_tbl_short, 
						PSY_STATVARIABLE_LONG *psy_stvar_long, 
						PSY_STATVARIABLE_SHORT *psy_stvar_short, 
						int ch)
{

	int b, i,p1,p2;
	double t;
	double tempL, tempR, tempM, tempS;

    p1 = psy_stvar_long->p_nb;
    if( p1 == 0 ) p2 = NPART_LONG;
    else if( p1 == NPART_LONG ) p2 = 0;

	if (ch==3) {
		for(b = 0; b < part_tbl_long->len; b++) {
			if (psy_stvar_long[3].nb[p1+b] != 0.0)
				t = psy_stvar_long[2].nb[p1+b]/psy_stvar_long[3].nb[p1+b];
			else
				t = 0;
			if (t>1)
				t = 1/t;
			tempL = max(psy_stvar_long[0].nb[p1+b]*t, min(psy_stvar_long[0].nb[p1+b], part_tbl_long->dyn->bmax[b]*psy_stvar_long[0].en[b]));
			tempR = max(psy_stvar_long[1].nb[p1+b]*t, min(psy_stvar_long[1].nb[p1+b], part_tbl_long->dyn->bmax[b]*psy_stvar_long[1].en[b]));

			t = min(tempL,tempR);
			tempM = min(t, max(psy_stvar_long[2].nb[p1+b], min(part_tbl_long->dyn->bmax[b]*psy_stvar_long[3].en[b], psy_stvar_long[3].nb[p1+b])));
			tempS = min(t, max(psy_stvar_long[3].nb[p1+b], min(part_tbl_long->dyn->bmax[b]*psy_stvar_long[2].en[b], psy_stvar_long[2].nb[p1+b])));

			if ((psy_stvar_long[0].nb[p1+b] <= 1.58*psy_stvar_long[1].nb[p1+b])&&(psy_stvar_long[1].nb[p1+b] <= 1.58*psy_stvar_long[0].nb[p1+b])) {
				psy_stvar_long[2].nb[p1+b] = tempM;
				psy_stvar_long[3].nb[p1+b] = tempS;
				psy_stvar_long[0].nb[p1+b] = tempL;
				psy_stvar_long[1].nb[p1+b] = tempR;
			}
		}

		for (i = 0; i < MAX_SHORT_WINDOWS; i++) {
			for(b = 0; b < part_tbl_short->len; b++) {
				if (psy_stvar_short[3].nb[i][b] != 0.0)
					t = psy_stvar_short[2].nb[i][b]/psy_stvar_short[3].nb[i][b];
				else
					t = 0;
				if (t>1)
					t = 1/t;
				tempL = max(psy_stvar_short[0].nb[i][b]*t, min(psy_stvar_short[0].nb[i][b], part_tbl_short->dyn->bmax[b]*psy_stvar_short[0].en[i][b]));
				tempR = max(psy_stvar_short[1].nb[i][b]*t, min(psy_stvar_short[1].nb[i][b], part_tbl_short->dyn->bmax[b]*psy_stvar_short[1].en[i][b]));

				t = min(tempL,tempR);
				tempM = min(t, max(psy_stvar_short[2].nb[i][b], min(part_tbl_short->dyn->bmax[b]*psy_stvar_short[3].en[i][b], psy_stvar_short[3].nb[i][b])));
				tempS = min(t, max(psy_stvar_short[3].nb[i][b], min(part_tbl_short->dyn->bmax[b]*psy_stvar_short[2].en[i][b], psy_stvar_short[2].nb[i][b])));

				if ((psy_stvar_short[0].nb[i][b] <= 1.58*psy_stvar_short[1].nb[i][b])&&(psy_stvar_short[1].nb[i][b] <= 1.58*psy_stvar_short[0].nb[i][b])) {
					psy_stvar_short[2].nb[i][b] = tempM;
					psy_stvar_short[3].nb[i][b] = tempS;
					psy_stvar_short[0].nb[i][b] = tempL;
					psy_stvar_short[1].nb[i][b] = tempR;
				}
			}
		}
	}
}


/* added by T. Araki (1997.7.10) */
void psy_step12(PARTITION_TABLE_LONG *part_tbl_long,
				PARTITION_TABLE_SHORT *part_tbl_short,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short,
				PSY_VARIABLE_LONG *psy_var_long,
				PSY_VARIABLE_SHORT *psy_var_short,
				int ch
				)
{
    int b;

    psy_var_long->pe = 0.0;
    for(b = 0; b < part_tbl_long->len; b++){
		double tp = log((psy_stvar_long->nb[psy_stvar_long->p_nb + b] + 0.0001)
			/ (psy_var_long->e[b] + 0.0001)); 

		tp = min(0.0, tp);
		psy_var_long->pe -= part_tbl_long->width[b] * tp;
    }
}

void psy_step13(PSY_VARIABLE_LONG *psy_var_long, 
	        enum WINDOW_TYPE *block_type,
		int ch
		)
{
	static int old_type;
//	if (psy_var_long->pe > 1800)
//	printf("%f\n", psy_var_long->pe);

	if (ch == 0) {
		if(psy_var_long->pe < 1800) {
			old_type = ONLY_LONG_WINDOW;
		} else {
			old_type = ONLY_SHORT_WINDOW;
		}
	} else if(ch == 1) {
		if((psy_var_long->pe < 1800) && (old_type == ONLY_LONG_WINDOW)) {
			*block_type = ONLY_LONG_WINDOW;
		} else {
			*block_type = ONLY_SHORT_WINDOW;
		}
	}
}

void psy_step14(SR_INFO *p_sri,
				PARTITION_TABLE_LONG *part_tbl_long, 
				PARTITION_TABLE_SHORT *part_tbl_short,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short, 
				PSY_VARIABLE_LONG *psy_var_long, 
				PSY_VARIABLE_SHORT *psy_var_short,
				int ch
				)
{
    int b, n, w, i;
    int w_low, w_high;
    double thr, minthr;
    
    w_high = 0;
    for(n = 0; n < p_sri->num_cb_long; n++){
		w_low = w_high;
		w_high += p_sri->cb_width_long[n];

        psy_var_long->epart[n] = 0.0;
		for(w = w_low; w < w_high; w++){
			psy_var_long->epart[n] += psy_sqr(psy_stvar_long->fft_r[psy_stvar_long->p_fft + w]);
		}
    }

    for(b = 0; b < part_tbl_long->len; b++){
        thr = psy_stvar_long->nb[psy_stvar_long->p_nb + b]
			/ part_tbl_long->width[b];
        for(w = part_tbl_long->w_low[b]; w <= part_tbl_long->w_high[b]; w++){
			psy_var_long->thr[w] = thr;
		}
    }

    w_high = 0;
    for(n = 0; n < p_sri->num_cb_long; n++){
        w_low = w_high;
		w_high += p_sri->cb_width_long[n];

        minthr = psy_var_long->thr[w_low];
		for(w = w_low+1; w < w_high; w++){
			if(psy_var_long->thr[w] < minthr){
				minthr = psy_var_long->thr[w];
			}
		}

		psy_var_long->npart[n] = minthr /** (w_high - w_low)*/;
    }

    for(n = 0; n < p_sri->num_cb_long; n++){
		if (psy_var_long->epart[n]!=0.0) {
			psy_stvar_long->ismr[n] = psy_var_long->npart[n] / psy_var_long->epart[n];
		} else {
			psy_stvar_long->ismr[n] = 0.0;
		}
    }

	/* added by T. Araki (1997.10.16) */
    for(i = 0; i < MAX_SHORT_WINDOWS; i++){
        w_high = 0;
		for(n = 0; n < p_sri->num_cb_short; n++){
			w_low = w_high;
			w_high += p_sri->cb_width_short[n];

			psy_var_short->epart[i][n] = 0.0;
			for(w = w_low; w < w_high; w++){
				psy_var_short->epart[i][n] += psy_sqr(psy_stvar_short->fft_r[i][w]);
			}
		}

		for(b = 0; b < part_tbl_short->len; b++){
            thr = psy_stvar_short->nb[i][b] / part_tbl_short->width[b];
			for(w = part_tbl_short->w_low[b]; w <= part_tbl_short->w_high[b]; w++){
				psy_var_short->thr[i][w] = thr;
			}
		}

		w_high = 0;
		for(n = 0; n < p_sri->num_cb_short; n++){
            w_low = w_high;
			w_high += p_sri->cb_width_short[n];

			minthr = psy_var_short->thr[i][w_low];
			for(w = w_low + 1; w < w_high; w++){
				if(psy_var_short->thr[i][w] < minthr){
					minthr = psy_var_short->thr[i][w];
				}
			}

			psy_var_short->npart[i][n] = minthr /** (w_high - w_low)*/;
        }

		for(n = 0; n < p_sri->num_cb_short; n++){
			if (psy_var_short->epart[i][n]!=0.0) {
				psy_stvar_short->ismr[i][n] = psy_var_short->npart[i][n] / psy_var_short->epart[i][n];
			} else {
				psy_stvar_short->ismr[i][n] = 0.0;
			}
		}
    }
	/* added by T. Araki (1997.10.16) end */
}


void psy_step15(int use_ms_l[49], int use_ms_s[8][15],
				SR_INFO *p_sri,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short,
				PSY_VARIABLE_LONG *psy_var_long, PSY_VARIABLE_SHORT *psy_var_short,
				int ch
				)
{
	int b, i;
	double temp, x1, x2, db;

	if (ch == 0) {
		for (b = 0; b < p_sri->num_cb_long; b++)
			psy_stvar_long->save_npart_l[b] = psy_var_long->npart[b];
		for (i = 0; i < 8; i++)
			for (b = 0; b < p_sri->num_cb_short; b++)
				psy_stvar_short->save_npart_s[i][b] = psy_var_short->npart[i][b];
	}

	if (ch == 1) {
		for (b = 0; b < p_sri->num_cb_long; b++) {
			x1 = min(psy_stvar_long->save_npart_l[b],psy_var_long->npart[b]);
			x2 = max(psy_stvar_long->save_npart_l[b],psy_var_long->npart[b]);
			if (x2 >= 1000*x1)
				db=30;
			else
				db = 10*log10(x2/x1);
			temp = 0.35*(db)/5.0;
//			printf("%d\t%f\n", b, temp);
			if (temp < 0.35)
				use_ms_l[b] = 1;
			else
				use_ms_l[b] = 0;
		}
		for (i = 0; i < 8; i++) {
			for (b = 0; b < p_sri->num_cb_short; b++) {
				x1 = min(psy_stvar_short->save_npart_s[i][b],psy_var_short->npart[i][b]);
				x2 = max(psy_stvar_short->save_npart_s[i][b],psy_var_short->npart[i][b]);
				if (x2 >= 1000*x1)
					db=30;
				else
					db = 10*log10(x2/x1);
				temp = 0.35*(db)/5.0;
//				printf("%d\t%f\n", b, temp);
				if (temp < 0.35)
					use_ms_s[i][b] = 1;
				else
					use_ms_s[i][b] = 0;
			}
		}
	}
}


