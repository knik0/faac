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

$Id: psych.c,v 1.30 2000/02/04 16:26:03 menno Exp $
$Id: psych.c,v 1.30 2000/02/04 16:26:03 menno Exp $
$Id: psych.c,v 1.30 2000/02/04 16:26:03 menno Exp $

**********************************************************************/

/* CREATED BY :  Bernhard Grill -- August-96  */
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "tf_main.h"
#include "psych.h"
#include "transfo.h"

void complspectrum( double *f, unsigned lg2n );


SR_INFO sr_info_aac[MAX_SAMPLING_RATES+1] =
{
	{ 8000, 40, 15,
		{
			12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16, 
			16, 16, 16, 16, 16, 16, 20, 20, 20, 20, 24, 24, 24, 28, 
			28, 32, 32, 36, 40, 44, 48, 52, 56, 60, 64, 80
		}, {
			4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 12, 16, 20, 20
		}
	}, { 11025, 43, 15,
		{
			8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12, 
			12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24, 
			24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
		}, {
			4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
		}
	}, { 12000, 43, 15,
		{
			8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12, 
			12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24, 
			24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
		}, {
			4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
		}
	}, { 16000, 43, 15,
		{
			8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12, 
			12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24, 
			24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
		}, {
			4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
		}
	}, { 22050, 47, 15,
		{
			4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
			8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
			36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
		}, {
			4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8, 12, 16, 16, 20
		}
	},{ 24000, 47, 15,
		{
			4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
			8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
			36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
		}, {
			4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8, 12, 16, 16, 20
		}
	}, { 32000, 51, 14,
		{
			4,	4,	4,	4,	4,	4,	4,	4,	4,	4,	8,	8,	8,	8,	
			8,	8,	8,	12,	12,	12,	12,	16,	16,	20,	20,	24,	24,	28,	
			28,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,
			32,	32,	32,	32,	32,	32,	32,	32,	32
		},{
			4,	4,	4,	4,	4,	8,	8,	8,	12,	12,	12,	16,	16,	16
		}
	}, { 44100, 49, 14,
		{
			4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8, 
			12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
			32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96 
		}, {
			4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16 
		}
	}, { 48000, 49, 14,
		{
			4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8, 
			12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
			32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
		}, {
			4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16 
		}
	}, {64000, 47, 12,
		{
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
			8, 8, 8, 8, 12, 12, 12, 16, 16, 16, 20, 24, 24, 28,
			36, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
			40, 40, 40, 40, 40
		},{
			4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 32
		}
	}, { 88200, 41, 12,
		{
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
			8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28, 
			36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
		},{
			4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
		}
	}, { 96000, 41, 12,
		{
			4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
			8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28, 
			36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
		},{
			4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
		}
	},
	{ -1 }
};

double          sample[MAX_TIME_CHANNELS+2][BLOCK_LEN_LONG*2];
                               /* sample value */

FFT_TABLE_LONG    fft_tbl_long;  /* table for long fft */
FFT_TABLE_SHORT    fft_tbl_short;  /* table for short fft */
PARTITION_TABLE_LONG    part_tbl_long;  
PARTITION_TABLE_SHORT    part_tbl_short;
DYN_PART_TABLE_LONG     dyn_long;  
DYN_PART_TABLE_SHORT    dyn_short;
PSY_STATVARIABLE_LONG    psy_stvar_long[MAX_TIME_CHANNELS+2];
                               /* variables for long block */
PSY_STATVARIABLE_SHORT    psy_stvar_short[MAX_TIME_CHANNELS+2];
                               /* variables for short block */
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

/*
 * This Function calculates the Frequency in Hertz given a 
 * Bark-value. It uses the Traunmueller-formula for bark>2
 * and a linear inerpolation below. 
 * KAF
 */
double bark2hz (double bark)
{
    double hz;

    if(bark>2.0)
	hz = 1960 * (bark + 0.53) / (26.28 - bark);
    else
	hz = bark * 102.9;

    return (hz);
}

/*
 * This Function calculates the Frequency in Bark given a 
 * Frequency-value in Hertz. It uses the Traunmueller-formula 
 * for frequency>200Hz and a linear inerpolation below. 
 * KAF
 */
double hz2bark (double hz)
{
    double bark;

    if(hz>200.0)
	bark = 26.81 * hz / (1960 + hz) - 0.53; 
    else
	bark = hz / 102.9;

    return (bark);
}

/* added by T. Araki (1997.07.10) */
void psy_part_table_init(double sampling_rate,
						 PARTITION_TABLE_LONG *part_tbl_long,
						 PARTITION_TABLE_SHORT *part_tbl_short
						 )
{
    int b,bb; /* Jul 10 */
    double tmp;
	int partition[1024], j, w;
	int cbands, prev_cbound, crit_bands, cbound;

	cbands = (int)hz2bark(sampling_rate/2.0) + 1;
	cbands *= 3;
	part_tbl_long->sampling_rate = (int)sampling_rate;
	part_tbl_long->w_low[0] = 0;
	part_tbl_long->w_high[0] = 0;
	part_tbl_long->width[0] = 1;
    prev_cbound = 0;
	crit_bands = 0;
	for(j = 1; j <= cbands; j++)
	{
		cbound = (int)(bark2hz((double)j/3) * (double)BLOCK_LEN_LONG * 2.0 / sampling_rate + 0.5);
		if(cbound > prev_cbound) {
			crit_bands++;
			part_tbl_long->w_low[crit_bands] = min(prev_cbound,BLOCK_LEN_LONG-1);
			part_tbl_long->w_high[crit_bands] = min(cbound-1,BLOCK_LEN_LONG-1);
			part_tbl_long->width[crit_bands] = 
				part_tbl_long->w_high[crit_bands] - part_tbl_long->w_low[crit_bands] + 1;
			prev_cbound = cbound;
			if (part_tbl_long->w_high[crit_bands] == (BLOCK_LEN_LONG-1))
				break;
		}
	}
	part_tbl_long->len = crit_bands+1;
//	printf("%d %d\t",part_tbl_long->len, part_tbl_long->w_high[crit_bands]);

	cbound /= 3;
	part_tbl_short->sampling_rate = (int)sampling_rate;
	part_tbl_short->w_low[0] = 0;
	part_tbl_short->w_high[0] = 0;
	part_tbl_short->width[0] = 1;
    prev_cbound = 0;
	crit_bands = 0;
	for(j = 1; j <= cbands; j++)
	{
		cbound = (int)(bark2hz((double)j/*/3*/) * (double)BLOCK_LEN_SHORT * 2.0 / sampling_rate +0.5);
		if(cbound > prev_cbound) {
			crit_bands++;
			part_tbl_short->w_low[crit_bands] = min(prev_cbound,BLOCK_LEN_SHORT-1);
			part_tbl_short->w_high[crit_bands] = min(cbound-1,BLOCK_LEN_SHORT-1);
			part_tbl_short->width[crit_bands] = 
				part_tbl_short->w_high[crit_bands] - part_tbl_short->w_low[crit_bands] + 1;
			prev_cbound = cbound;
			if (part_tbl_short->w_high[crit_bands] == (BLOCK_LEN_SHORT-1))
				break;
		}
	}
	part_tbl_short->len = crit_bands+1;
//	printf("%d %d\n",part_tbl_short->len, part_tbl_short->w_high[crit_bands]);

	for (b = 0; b < part_tbl_long->len; b++) {
		for(w = part_tbl_long->w_low[b]; w <= part_tbl_long->w_high[b]; ++w){
			partition[w] = b;
		}
	}

	for(b = 0; b < part_tbl_long->len ; b++) {
		for (j=0;(b != partition[j]);j++);
		{
			double ji = j + (part_tbl_long->width[b]-1)/2.0;
			double freq = part_tbl_long->sampling_rate*ji/2048;
			double bark = 13*atan(0.00076*freq)+3.5*atan((freq/7500)*(freq/7500));
			dyn_long.bval[b] = bark;
		}
	}

	for (b = 0; b < part_tbl_short->len; b++) {
		for(w = part_tbl_short->w_low[b]; w <= part_tbl_short->w_high[b]; ++w){
			partition[w] = b;
		}
	}

	for(b = 0; b < part_tbl_short->len ; b++) {
		for (j=0;(b != partition[j]);j++);
		{
			double ji = j + (part_tbl_short->width[b]-1)/2.0;
			double freq = part_tbl_short->sampling_rate*ji/256;
			double bark = 13*atan(0.00076*freq) + 3.5*atan((freq/7500)*(freq/7500));
			dyn_short.bval[b]=bark;
		}
	}

	// Calculate the spreading function
	{
		double tmpx,tmpy,tmpz,b1,b2;
		int b, bb;

		for( b = 0; b < part_tbl_long->len; b++) {
			b2 = dyn_long.bval[b];
			for( bb = 0; bb < part_tbl_long->len; bb++) {
				b1 = dyn_long.bval[bb];

				//tmpx = (b2 >= b1) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				tmpx = (b >= bb) ? 3.0*(b2-b1) : 1.5*(b2-b1);

				tmpz = 8.0 * psy_min( (tmpx-0.5)*(tmpx-0.5) - 2.0*(tmpx-0.5),0.0 );

				tmpy = 15.811389 + 7.5*(tmpx + 0.474)-17.5 *sqrt(1.0 + (tmpx+0.474)*(tmpx+0.474));

				dyn_long.spreading[b][bb] = ( tmpy < -100.0 ? 0.0 : pow(10.0, (tmpz + tmpy)/10.0) );
			}
		}

		for( b = 0; b < part_tbl_short->len; b++) {
			b2 = dyn_short.bval[b];
			for( bb = 0; bb < part_tbl_short->len; bb++) {
				b1 = dyn_short.bval[bb];

				//tmpx = (b2 >= b1) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				tmpx = (b >= bb) ? 3.0*(b2-b1) : 1.5*(b2-b1);
				
				tmpz = 8.0 * psy_min( (tmpx-0.5)*(tmpx-0.5) - 2.0*(tmpx-0.5),0.0 );

				tmpy = 15.811389 + 7.5*(tmpx + 0.474)-17.5 *sqrt(1.0 + (tmpx+0.474)*(tmpx+0.474));

				dyn_short.spreading[b][bb] = ( tmpy < -100.0 ? 0.0 : pow(10.0, (tmpz + tmpy)/10.0) );
			}
		}
	}

    /* added by T. Okada (1997.07.10) */
    for( b = 0; b < part_tbl_long->len; b++){
		tmp = 0.0;
		for( bb = 0; bb < part_tbl_long->len; bb++)
			//tmp += sprdngf( (*part_tbl_long),(*part_tbl_short), bb, b, 0);
			tmp += dyn_long.spreading[bb][b];
		dyn_long.rnorm[b] = 1.0/tmp;
    }
    /* added by T. Okada (1997.07.10) end */

    /* added by T. Araki (1997.10.16) */
    for( b = 0; b < part_tbl_short->len; b++){
		tmp = 0.0;
		for( bb = 0; bb < part_tbl_short->len; bb++)
			//tmp += sprdngf( (*part_tbl_long), (*part_tbl_short), bb, b, 1);
			tmp += dyn_short.spreading[bb][b];
		dyn_short.rnorm[b] = 1.0/tmp;
    }
    /* added by T. Araki (1997.10.16) end */

	for(b = 0; b < part_tbl_long->len; b++) {
		dyn_long.bmax[b] = pow(10, -3*(0.5+0.5*(M_PI*(min(dyn_long.bval[b], 15.5)/15.5))));
	}
	for(b = 0; b < part_tbl_short->len; b++) {
		dyn_short.bmax[b] = pow(10, -3*(0.5+0.5*(M_PI*(min(dyn_short.bval[b], 15.5)/15.5))));
	}

	part_tbl_long->dyn = &dyn_long;
	part_tbl_short->dyn = &dyn_short;
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

    for(i = 0; i < BLOCK_LEN_LONG*3; i++){
		psy_stvar_long->fft_r[i] = 0.0;
		psy_stvar_long->fft_f[i] = 0.0;
    }

	psy_stvar_long->p_fft = 0;

    for(i = 0; i < NPART_LONG*2; i++){
		psy_stvar_long->nb[i] = 90.0;
    }

	psy_stvar_long->p_nb = NPART_LONG;
	/* added by T. Araki (1997.07.10) end */

	/* added by T. Araki (1997.10.16) */
	for(i = 0; i < BLOCK_LEN_SHORT; i++) {
		psy_stvar_short->last6_fft_r[i] = 0.0;
		psy_stvar_short->last6_fft_f[i] = 0.0;
		psy_stvar_short->last7_fft_r[i] = 0.0;
		psy_stvar_short->last7_fft_f[i] = 0.0;
    }

    for(i = 0; i < NPART_SHORT; i++){
		psy_stvar_short->last7_nb[i] = 90.0;
    }
	/* added by T. Araki (1997.10.16) end */
}

void psy_fill_lookahead(double *p_time_signal[], int no_of_chan)
{
	int i, ch;

	for (ch = 0; ch < no_of_chan; ch++) {
		for(i = 0; i < BLOCK_LEN_LONG; i++){
			sample[ch][i+BLOCK_LEN_LONG] = p_time_signal[ch][i];
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
		if (no_of_chan < 2) {
			psy_step1(p_time_signal,sample, no_of_chan);
			psy_step2(&sample[no_of_chan], &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &fft_tbl_long, 
				&fft_tbl_short, ch);
			psy_step3(&psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);
			psy_step4(&psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);
		} else if (no_of_chan == 3) {
			int w, l;
			for (w = 0; w < BLOCK_LEN_LONG; w++) {
				psy_stvar_long[2].fft_r[w+psy_stvar_long->p_fft] = (psy_stvar_long[0].fft_r[w+psy_stvar_long->p_fft]+psy_stvar_long[1].fft_r[w+psy_stvar_long->p_fft])*0.5;
				psy_stvar_long[3].fft_r[w+psy_stvar_long->p_fft] = (psy_stvar_long[0].fft_r[w+psy_stvar_long->p_fft]-psy_stvar_long[1].fft_r[w+psy_stvar_long->p_fft])*0.5;
			}
			for (l = 0; l < MAX_SHORT_WINDOWS; l++) {
				for (w = 0; w < BLOCK_LEN_SHORT; w++) {
					psy_stvar_short[2].fft_r[l][w] = (psy_stvar_short[0].fft_r[l][w]+psy_stvar_short[1].fft_r[l][w])*0.5;
					psy_stvar_short[3].fft_r[l][w] = (psy_stvar_short[0].fft_r[l][w]-psy_stvar_short[1].fft_r[l][w])*0.5;
				}
			}
		}

		if (no_of_chan == 0) {
			for (b = 0; b < NPART_LONG; b++)
				psy_stvar_long[no_of_chan].save_cw[b] = psy_var_long.cw[b];
			for (i = 0; i < MAX_SHORT_WINDOWS; i++)
				for (b = 0; b < NPART_SHORT; b++)
					psy_stvar_short[no_of_chan].save_cw[i][b] = psy_var_short.cw[i][b];
		}
		if (no_of_chan == 1) {
			for (b = 0; b < NPART_LONG; b++)
				psy_stvar_long[no_of_chan].save_cw[b] = min(psy_var_long.cw[b], psy_stvar_long[0].save_cw[b]);
			for (i = 0; i < MAX_SHORT_WINDOWS; i++)
				for (b = 0; b < NPART_SHORT; b++)
					psy_stvar_short[no_of_chan].save_cw[i][b] = min(psy_var_short.cw[i][b], psy_stvar_short[0].save_cw[i][b]);
		}
		if (no_of_chan > 1) {
			for (b = 0; b < NPART_LONG; b++)
				psy_var_long.cw[b] = psy_stvar_long[1].save_cw[b];
			for (i = 0; i < MAX_SHORT_WINDOWS; i++)
				for (b = 0; b < NPART_SHORT; b++)
					psy_var_short.cw[i][b] = psy_stvar_short[1].save_cw[i][b];
		}

		psy_step5(&part_tbl_long, &part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], 
			&psy_var_long, &psy_var_short, ch);
		psy_step6(&part_tbl_long, &part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan],
			&psy_var_long, &psy_var_short);
		psy_step7(&part_tbl_long, &part_tbl_short, &psy_var_long, &psy_var_short);

		if (no_of_chan < 2) {
			for (b = 0; b < NPART_LONG; b++)
				psy_stvar_long[no_of_chan].save_tb[b] = psy_var_long.tb[b];
			for (i = 0; i < MAX_SHORT_WINDOWS; i++)
				for (b = 0; b < NPART_SHORT; b++)
					psy_stvar_short[no_of_chan].save_tb[i][b] = psy_var_short.tb[i][b];
		} else {
			for (b = 0; b < NPART_LONG; b++)
				psy_var_long.tb[b] = psy_stvar_long[no_of_chan-2].save_tb[b];
			for (i = 0; i < MAX_SHORT_WINDOWS; i++)
				for (b = 0; b < NPART_SHORT; b++)
					psy_var_short.tb[i][b] = psy_stvar_short[no_of_chan-2].save_tb[i][b];
		}

		psy_step8(&part_tbl_long, &part_tbl_short, &psy_var_long, &psy_var_short);
		psy_step9(&part_tbl_long, &part_tbl_short, &psy_var_long, &psy_var_short);
		psy_step10(&part_tbl_long, &part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], 
			&psy_var_long, &psy_var_short, ch);
		psy_step11andahalf(&part_tbl_long, &part_tbl_short, psy_stvar_long, psy_stvar_short, no_of_chan);
		psy_step11(&part_tbl_long, &part_tbl_short, &psy_stvar_long[no_of_chan], &psy_stvar_short[no_of_chan], ch);
		psy_step14(p_sri, &part_tbl_long, &part_tbl_short, &psy_stvar_long[no_of_chan],
			&psy_stvar_short[no_of_chan], &psy_var_long, &psy_var_short, ch);
		psy_step15(psy_stvar_long[no_of_chan].use_ms, psy_stvar_short[no_of_chan].use_ms, p_sri, &psy_stvar_long[0], &psy_stvar_short[0], &psy_var_long, &psy_var_short, no_of_chan);
	}	

	/*  for( ch=0; ch<no_of_chan; ch++ ) { */
	{   /* Now performed for only one channel at a time, CL 97.01.10 */
		int i;

		p_chpo_long[no_of_chan].p_ratio   = psy_stvar_long[no_of_chan].ismr;
		p_chpo_long[no_of_chan].cb_width  = p_sri->cb_width_long;
		p_chpo_long[no_of_chan].no_of_cb = p_sri->num_cb_long;
		if (no_of_chan == 1)
			memcpy(p_chpo_long[no_of_chan].use_ms, psy_stvar_long[no_of_chan].use_ms, NSFB_LONG*sizeof(int));

		for( i=0; i<MAX_SHORT_WINDOWS; i++ ) {
			p_chpo_short[no_of_chan][i].p_ratio  = psy_stvar_short[no_of_chan].ismr[i];
			p_chpo_short[no_of_chan][i].cb_width = p_sri->cb_width_short;
			p_chpo_short[no_of_chan][i].no_of_cb = p_sri->num_cb_short;
			if (no_of_chan == 1)
				memcpy(p_chpo_short[no_of_chan][i].use_ms, psy_stvar_short[no_of_chan].use_ms[i], NSFB_SHORT*sizeof(int));
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
		sample[ch][i+BLOCK_LEN_LONG] = p_time_signal[0][i];
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
    int w,l;
    double sqrtN;
	double data[2048];

    /* FFT for long */
    psy_stvar_long->p_fft += BLOCK_LEN_LONG;

    if(psy_stvar_long->p_fft == BLOCK_LEN_LONG * 3)
		psy_stvar_long->p_fft = 0;

	sqrtN = 1/sqrt(2048);

    /* windowing */
    for(w = 0; w < BLOCK_LEN_LONG*2; w++){
		data[w] = fft_tbl_long->hw[w] * sample[ch][w];
    }

	complspectrum(data, 11);

	for(w = 0; w < BLOCK_LEN_LONG; w++){
		psy_stvar_long->fft_r[w+psy_stvar_long->p_fft] = data[w] * sqrtN;
		if (w < 420)
			psy_stvar_long->fft_f[w+psy_stvar_long->p_fft] = data[w+BLOCK_LEN_LONG];
    }

	/* FFT for short */
	sqrtN = 1/sqrt(256);

	for(l = 0; l < MAX_SHORT_WINDOWS; l++){

        /* windowing */
        for(w = 0; w < BLOCK_LEN_SHORT*2; w++){
			data[w] = fft_tbl_short->hw[w] * sample[ch][OFFSET_FOR_SHORT + (BLOCK_LEN_SHORT * l) + w];
		}

		complspectrum(data, 8);

		for(w = 0; w < BLOCK_LEN_SHORT; w++){
			psy_stvar_short->fft_r[l][w] = data[w] * sqrtN;
			if (w < 60)
				psy_stvar_short->fft_f[l][w] = data[w+BLOCK_LEN_SHORT];
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

    for(w = 0; w < 420; w++){
		psy_var_long->r_pred[w] = 2.0 * psy_stvar_long->fft_r[p1_l + w] - psy_stvar_long->fft_r[p2_l + w];
		psy_var_long->f_pred[w] = 2.0 * psy_stvar_long->fft_f[p1_l + w] - psy_stvar_long->fft_f[p2_l + w];
    }

	/* added by T. Araki (1997.10.16) */
    for(w = 0; w < 60; w++){
        psy_var_short->r_pred[0][w] = 2.0 * psy_stvar_short->last7_fft_r[w] - psy_stvar_short->last6_fft_r[w];
        psy_var_short->f_pred[0][w] = 2.0 * psy_stvar_short->last7_fft_f[w] - psy_stvar_short->last6_fft_f[w];
        psy_var_short->r_pred[1][w] = 2.0 * psy_stvar_short->fft_r[0][w] - psy_stvar_short->last7_fft_r[w];
        psy_var_short->f_pred[1][w] = 2.0 * psy_stvar_short->fft_f[0][w] - psy_stvar_short->last7_fft_f[w];
    }

    for(i = 2; i < MAX_SHORT_WINDOWS; i++){
        for(w = 0; w < 60; w++){
			psy_var_short->r_pred[i][w] = 2.0 * psy_stvar_short->fft_r[i - 1][w] - psy_stvar_short->fft_r[i - 2][w];
			psy_var_short->f_pred[i][w] = 2.0 * psy_stvar_short->fft_f[i - 1][w] - psy_stvar_short->fft_f[i - 2][w];
		}
    }

    for(w = 0; w < 60; w++){
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

    for(w = 0; w < 420; w++){
		r = psy_stvar_long->fft_r[psy_stvar_long->p_fft+w];
		f = psy_stvar_long->fft_f[psy_stvar_long->p_fft+w];
		rp = psy_var_long->r_pred[w];
		fp = psy_var_long->f_pred[w];

		if( fabs(r) + fabs(rp) != 0.0 )
			psy_var_long->c[w] = sqrt( psy_sqr(r*cos(f) - rp*cos(fp))
				+psy_sqr(r*sin(f) - rp*sin(fp)) )/ ( r + fabs(rp) );
		else
			psy_var_long->c[w] = 0.0; /* tmp */
    }
    for(w = 420; w < BLOCK_LEN_LONG; w++){
		psy_var_long->c[w] = 0.4;
	}

	/* added by T. Araki (1997.10.16) */
    for(i = 0; i < MAX_SHORT_WINDOWS; i++){
        for(w = 0; w < 60; w++){
			r = psy_stvar_short->fft_r[i][w];
			f = psy_stvar_short->fft_f[i][w];
			rp = psy_var_short->r_pred[i][w];
			fp = psy_var_short->f_pred[i][w];

			if( fabs(r) + fabs(rp) != 0.0 )
				psy_var_short->c[i][w] = sqrt( psy_sqr(r*cos(f) - rp*cos(fp))
					+psy_sqr(r*sin(f) - rp*sin(fp)) )/ ( r + fabs(rp) ) ;
			else
				psy_var_short->c[i][w] = 0.0; /* tmp */
		}
    }
	for(i = 0; i < MAX_SHORT_WINDOWS; i++){
		for(w = 60; w < BLOCK_LEN_SHORT; w++){
			psy_var_short->c[i][w] = 0.4;
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
//		if ((psy_var_long->tb[b]<1.0)&&(psy_var_long->tb[b]>0.5))
//			printf("%d\t%.2f\n", b, psy_var_long->tb[b]);
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
//			if ((psy_var_short->tb[i][b]<1.0)&&(psy_var_short->tb[i][b]>0.5))
//				printf("%d\t%.2f\n", b, psy_var_short->tb[i][b]);
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
		temp = psy_min( psy_stvar_short->nb[0][b], 2.0*psy_stvar_short->last7_nb[b]);
		if (temp > 0.01)
			psy_stvar_short->nb[0][b] = temp;
    }

	for(b = 0; b < part_tbl_short->len; b++){
		psy_stvar_short->last7_nb[b] = psy_stvar_short->nb[7][b];
	}

	for(i = 1;  i < MAX_SHORT_WINDOWS; i++){
		for(b = 0; b < part_tbl_short->len; b++){
			temp = psy_min( psy_stvar_short->nb[i][b],2.0*psy_stvar_short->nb[i - 1][b]);
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

	int b, i,p1;
	double t;
	double tempL, tempR, tempM, tempS;

    p1 = psy_stvar_long->p_nb;

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

//			if ((psy_stvar_long[0].nb[p1+b] <= 1.58*psy_stvar_long[1].nb[p1+b])&&(psy_stvar_long[1].nb[p1+b] <= 1.58*psy_stvar_long[0].nb[p1+b])) {
				psy_stvar_long[2].nb[p1+b] = tempM;
				psy_stvar_long[3].nb[p1+b] = tempS;
				psy_stvar_long[0].nb[p1+b] = tempL;
				psy_stvar_long[1].nb[p1+b] = tempR;
//			}
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

//				if ((psy_stvar_short[0].nb[i][b] <= 1.58*psy_stvar_short[1].nb[i][b])&&(psy_stvar_short[1].nb[i][b] <= 1.58*psy_stvar_short[0].nb[i][b])) {
					psy_stvar_short[2].nb[i][b] = tempM;
					psy_stvar_short[3].nb[i][b] = tempS;
					psy_stvar_short[0].nb[i][b] = tempL;
					psy_stvar_short[1].nb[i][b] = tempR;
//				}
			}
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

		psy_var_long->npart[n] = minthr * (w_high - w_low);
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

			psy_var_short->npart[i][n] = minthr * (w_high - w_low);
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


void psy_step15(int use_ms_l[NSFB_LONG], int use_ms_s[MAX_SHORT_WINDOWS][NSFB_SHORT],
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


