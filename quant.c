/*
 *	AAC quantization
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
  $Date: 2000/10/05 13:04:05 $ (check in)
  $Author: menno $
  *************************************************************************/

#include <math.h>
#include "aacenc.h"
#include "quant.h"
#include "bitstream.h"
#include "pulse.h"
#include "huffman.h"
#include "aac_se_enc.h"



double pow_quant[9000];
double adj_quant[9000];
double adj_quant_asm[9000];
int sign[1024];
int g_Count;
int old_startsf;
int pns_sfb_start = 1000;         /* lower border for Perceptual Noise Substitution
                                      (off by default) */

double ATH[MAX_SCFAC_BANDS];

double ATHformula(double f)
{
	double ath;
	f  = max(0.02, f);
	/* from Painter & Spanias, 1997 */
	/* minimum: (i=77) 3.3kHz = -5db */
	ath=(3.640 * pow(f,-0.8)
		-  6.500 * exp(-0.6*pow(f-3.3,2.0))
		+  0.001 * pow(f,4.0));

	/* convert to energy */
	ath = pow( 10.0, ath/10.0 );
	return ath;
}


void compute_ath(AACQuantInfo *quantInfo, double ATH[MAX_SCFAC_BANDS])
{
	int sfb,i,start=0,end=0;
	double ATH_f;
	double samp_freq = 44.1;
	static int width[] = {0, 4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16};
	if (quantInfo->block_type==ONLY_SHORT_WINDOW) {
		for ( sfb = 0; sfb < 14; sfb++ ) {
			start = start+(width[sfb]*8);
			end   = end+(width[sfb+1]*8);
			ATH[sfb]=1e99;
			for (i=start ; i < end; i++) {
				ATH_f = ATHformula(samp_freq*i/(128)); /* freq in kHz */
				ATH[sfb]=min(ATH[sfb],ATH_f);
			}
		}
	} else {
		for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ ) {
			start = quantInfo->sfb_offset[sfb];
			end   = quantInfo->sfb_offset[sfb+1];
			ATH[sfb]=1e99;
			for (i=start ; i < end; i++) {
				ATH_f = ATHformula(samp_freq*i/(1024)); /* freq in kHz */
				ATH[sfb]=min(ATH[sfb],ATH_f);
			}
		}
	}
}


void tf_init_encode_spectrum_aac( int quality )
{
	int i;

	g_Count = quality;
	old_startsf = 0;

	for (i=0;i<9000;i++){
		pow_quant[i]=pow(i, ((double)4.0/(double)3.0));
	}
	for (i=0;i<8999;i++){
		adj_quant[i] = (i + 1) - pow(0.5 * (pow_quant[i] + pow_quant[i + 1]), 0.75);
	}

	adj_quant_asm[0] = 0.0;
	for (i = 1; i < 9000; i++) {
		adj_quant_asm[i] = i - 0.5 - pow(0.5 * (pow_quant[i - 1] + pow_quant[i]),0.75);
	}
}


#if (defined(__GNUC__) && defined(__i386__))
#define USE_GNUC_ASM
#endif

#ifdef USE_GNUC_ASM
#  define QUANTFAC(rx)  adj_quant_asm[rx]
#  define XRPOW_FTOI(src, dest) \
     asm ("fistpl %0 " : "=m"(dest) : "t"(src) : "st")
#else
#  define QUANTFAC(rx)  adj_quant[rx]
#  define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#endif

/*********************************************************************
 * nonlinear quantization of xr
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 * ASM optimization from
 *    Mathew Hendry <scampi@dial.pipex.com> 11/1999
 *    Acy Stapp <AStapp@austin.rr.com> 11/1999
 *    Takehiro Tominaga <tominaga@isoternet.org> 11/1999
 *********************************************************************/
void quantize(AACQuantInfo *quantInfo,
			  double *pow_spectrum,
			  int *quant)
{
	const double istep = pow(2.0, -0.1875*quantInfo->common_scalefac);

#if ((defined _MSC_VER) || (defined __BORLANDC__))
	{
		/* asm from Acy Stapp <AStapp@austin.rr.com> */
		int rx[4];
		_asm {
			fld qword ptr [istep]
			mov esi, dword ptr [pow_spectrum]
			lea edi, dword ptr [adj_quant_asm]
			mov edx, dword ptr [quant]
			mov ecx, 1024/4
		}
loop1:
		_asm {
			fld qword ptr [esi]         // 0
			fld qword ptr [esi+8]       // 1 0
			fld qword ptr [esi+16]      // 2 1 0
			fld qword ptr [esi+24]      // 3 2 1 0


			fxch st(3)                  // 0 2 1 3
			fmul st(0), st(4)
			fxch st(2)                  // 1 2 0 3
			fmul st(0), st(4)
			fxch st(1)                  // 2 1 0 3
			fmul st(0), st(4)
			fxch st(3)                  // 3 1 0 2
			fmul st(0), st(4)
			add esi, 32
			add edx, 16

			fxch st(2)                  // 0 1 3 2
			fist dword ptr [rx]
			fxch st(1)                  // 1 0 3 2
			fist dword ptr [rx+4]
			fxch st(3)                  // 2 0 3 1
			fist dword ptr [rx+8]
			fxch st(2)                  // 3 0 2 1
			fist dword ptr [rx+12]

			dec ecx

			mov eax, dword ptr [rx]
			mov ebx, dword ptr [rx+4]
			fxch st(1)                  // 0 3 2 1
			fadd qword ptr [edi+eax*8]
			fxch st(3)                  // 1 3 2 0
			fadd qword ptr [edi+ebx*8]

			mov eax, dword ptr [rx+8]
			mov ebx, dword ptr [rx+12]
			fxch st(2)                  // 2 3 1 0
			fadd qword ptr [edi+eax*8]
			fxch st(1)                  // 3 2 1 0
			fadd qword ptr [edi+ebx*8]

			fxch st(3)                  // 0 2 1 3
			fistp dword ptr [edx-16]    // 2 1 3
			fxch st(1)                  // 1 2 3
			fistp dword ptr [edx-12]    // 2 3
			fistp dword ptr [edx-8]     // 3
			fistp dword ptr [edx-4]

			jnz loop1

			mov dword ptr [pow_spectrum], esi
			mov dword ptr [quant], edx
			fstp st(0)
		}
	}
#elif defined (USE_GNUC_ASM)
  {
      int rx[4];
      __asm__ __volatile__(
        "\n\nloop1:\n\t"

        "fldl (%1)\n\t"
        "fldl 8(%1)\n\t"
        "fldl 16(%1)\n\t"
        "fldl 24(%1)\n\t"

        "fxch %%st(3)\n\t"
        "fmul %%st(4)\n\t"
        "fxch %%st(2)\n\t"
        "fmul %%st(4)\n\t"
        "fxch %%st(1)\n\t"
        "fmul %%st(4)\n\t"
        "fxch %%st(3)\n\t"
        "fmul %%st(4)\n\t"

        "addl $32, %1\n\t"
        "addl $16, %3\n\t"

        "fxch %%st(2)\n\t"
        "fistl %5\n\t"
        "fxch %%st(1)\n\t"
        "fistl 4+%5\n\t"
        "fxch %%st(3)\n\t"
        "fistl 8+%5\n\t"
        "fxch %%st(2)\n\t"
        "fistl 12+%5\n\t"

        "dec %4\n\t"

        "movl %5, %%eax\n\t"
        "movl 4+%5, %%ebx\n\t"
        "fxch %%st(1)\n\t"
        "faddl (%2,%%eax,8)\n\t"
        "fxch %%st(3)\n\t"
        "faddl (%2,%%ebx,8)\n\t"

        "movl 8+%5, %%eax\n\t"
        "movl 12+%5, %%ebx\n\t"
        "fxch %%st(2)\n\t"
        "faddl (%2,%%eax,8)\n\t"
        "fxch %%st(1)\n\t"
        "faddl (%2,%%ebx,8)\n\t"

        "fxch %%st(3)\n\t"
        "fistpl -16(%3)\n\t"
        "fxch %%st(1)\n\t"
        "fistpl -12(%3)\n\t"
        "fistpl -8(%3)\n\t"
        "fistpl -4(%3)\n\t"

        "jnz loop1\n\n"
        : /* no outputs */
        : "t" (istep), "r" (pow_spectrum), "r" (adj_quant_asm), "r" (quant), "r" (1024 / 4), "m" (rx)
        : "%eax", "%ebx", "memory", "cc"
      );
  }
#elif 0
	{
		double x;
		int j, rx;
		for (j = 1024 / 4; j > 0; --j) {
			x = *pow_spectrum++ * istep;
			XRPOW_FTOI(x, rx);
			XRPOW_FTOI(x + QUANTFAC(rx), *quant++);

			x = *pow_spectrum++ * istep;
			XRPOW_FTOI(x, rx);
			XRPOW_FTOI(x + QUANTFAC(rx), *quant++);

			x = *pow_spectrum++ * istep;
			XRPOW_FTOI(x, rx);
			XRPOW_FTOI(x + QUANTFAC(rx), *quant++);

			x = *pow_spectrum++ * istep;
			XRPOW_FTOI(x, rx);
			XRPOW_FTOI(x + QUANTFAC(rx), *quant++);
		}
	}
#else
  {/* from Wilfried.Behne@t-online.de.  Reported to be 2x faster than
      the above code (when not using ASM) on PowerPC */
     	int j;

     	for ( j = 1024/8; j > 0; --j)
     	{
			double	x1, x2, x3, x4, x5, x6, x7, x8;
			int rx1, rx2, rx3, rx4, rx5, rx6, rx7, rx8;
			x1 = *pow_spectrum++ * istep;
			x2 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x1, rx1);
			x3 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x2, rx2);
			x4 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x3, rx3);
			x5 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x4, rx4);
			x6 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x5, rx5);
			x7 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x6, rx6);
			x8 = *pow_spectrum++ * istep;
			XRPOW_FTOI(x7, rx7);
			x1 += QUANTFAC(rx1);
			XRPOW_FTOI(x8, rx8);
			x2 += QUANTFAC(rx2);
			XRPOW_FTOI(x1,*quant++);
			x3 += QUANTFAC(rx3);
			XRPOW_FTOI(x2,*quant++);
			x4 += QUANTFAC(rx4);
			XRPOW_FTOI(x3,*quant++);
			x5 += QUANTFAC(rx5);
			XRPOW_FTOI(x4,*quant++);
			x6 += QUANTFAC(rx6);
			XRPOW_FTOI(x5,*quant++);
			x7 += QUANTFAC(rx7);
			XRPOW_FTOI(x6,*quant++);
			x8 += QUANTFAC(rx8);
			XRPOW_FTOI(x7,*quant++);
			XRPOW_FTOI(x8,*quant++);
     	}
  }
#endif
}

int inner_loop(AACQuantInfo *quantInfo,
			   double *pow_spectrum,
			   int quant[BLOCK_LEN_LONG],
			   int max_bits)
{
	int bits;

	quantInfo->common_scalefac -= 1;
	do
	{
		quantInfo->common_scalefac += 1;
		quantize(quantInfo, pow_spectrum, quant);
		bits = count_bits(quantInfo, quant);
	} while ( bits > max_bits );

	return bits;
}

int search_common_scalefac(AACQuantInfo *quantInfo,
						   double *pow_spectrum,
						   int quant[BLOCK_LEN_LONG],
						   int desired_rate)
{
	int flag_GoneOver = 0;
	int CurrentStep = 4;
	int nBits;
	int StepSize = old_startsf;
	int Direction = 0;
	do
	{
		quantInfo->common_scalefac = StepSize;
		quantize(quantInfo, pow_spectrum, quant);
//		nBits = count_bits(quantInfo, quant, quantInfo->book_vector);
		nBits = count_bits(quantInfo, quant);

		if (CurrentStep == 1 ) {
			break; /* nothing to adjust anymore */
		}
		if (flag_GoneOver) {
			CurrentStep /= 2;
		}
		if (nBits > desired_rate) { /* increase Quantize_StepSize */
			if (Direction == -1 && !flag_GoneOver) {
				flag_GoneOver = 1;
				CurrentStep /= 2; /* late adjust */
			}
			Direction = 1;
			StepSize += CurrentStep;
		} else if (nBits < desired_rate) {
			if (Direction == 1 && !flag_GoneOver) {
				flag_GoneOver = 1;
				CurrentStep /= 2; /* late adjust */
			}
			Direction = -1;
			StepSize -= CurrentStep;
		} else break;
    } while (1);

    old_startsf = StepSize;

    return nBits;
}

int calc_noise(AACQuantInfo *quantInfo,
				double *p_spectrum,
				int quant[BLOCK_LEN_LONG],
				double requant[BLOCK_LEN_LONG],
				double error_energy[MAX_SCFAC_BANDS],
				double allowed_dist[MAX_SCFAC_BANDS],
				double *over_noise,
				double *tot_noise,
				double *max_noise
				)
{
	int i, sb, sbw;
	int over = 0, count = 0;
	double invQuantFac;
	double linediff, noise;

	*over_noise = 1;
	*tot_noise = 1;
	*max_noise = 1E-20;

	if (quantInfo->block_type!=ONLY_SHORT_WINDOW)
		PulseDecoder(quantInfo, quant);

	for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {

		double max_sb_noise = 0.0;

		sbw = quantInfo->sfb_offset[sb+1] - quantInfo->sfb_offset[sb];

		invQuantFac = pow(2.0, -0.25*(quantInfo->scale_factor[sb] - quantInfo->common_scalefac));

		error_energy[sb] = 0.0;

		for (i = quantInfo->sfb_offset[sb]; i < quantInfo->sfb_offset[sb+1]; i++){
			requant[i] =  pow_quant[min(ABS(quant[i]),8999)] * invQuantFac; 

			/* measure the distortion in each scalefactor band */
			linediff = (double)(ABS(p_spectrum[i]) - ABS(requant[i]));
			linediff *= linediff;
			error_energy[sb] += linediff;
			max_sb_noise = max(max_sb_noise, linediff);
		}
		error_energy[sb] = error_energy[sb] / sbw;		
		
		noise = error_energy[sb] / allowed_dist[sb];

		/* multiplying here is adding in dB */
		*tot_noise *= max(noise, 1E-20);
		if (noise>1) {
			over++;
			/* multiplying here is adding in dB */
			*over_noise *= noise;
		}
		*max_noise = max(*max_noise,noise);
		error_energy[sb] = noise;
		count++;
  	}

	return over;
}

int quant_compare(int best_over, double best_tot_noise, double best_over_noise,
				  double best_max_noise, int over, double tot_noise, double over_noise,
				  double max_noise)
//int quant_compare(double best_tot_noise, double best_over_noise,
//		  double tot_noise, double over_noise)
{
	/*
	noise is given in decibals (db) relative to masking thesholds.

	over_noise:  sum of quantization noise > masking
	tot_noise:   sum of all quantization noise
	max_noise:   max quantization noise

	*/
	int better;

	better =   over  < best_over ||  ( over == best_over
		&& over_noise < best_over_noise )
		||  ( over == best_over && over_noise==best_over_noise
		&& tot_noise < best_tot_noise);

#if 0
	better = ((over < best_over) ||
		((over==best_over) && (over_noise<best_over_noise)) ) ;
	better = min(better, max_noise < best_max_noise);
	better = min(better, tot_noise < best_tot_noise);
	better = min(better, (tot_noise < best_tot_noise) &&
		(max_noise < best_max_noise + 2));
	better = min(better, ( ( (0>=max_noise) && (best_max_noise>2)) ||
		( (0>=max_noise) && (best_max_noise<0) && ((best_max_noise+2)>max_noise) && (tot_noise<best_tot_noise) ) ||
		( (0>=max_noise) && (best_max_noise>0) && ((best_max_noise+2)>max_noise) && (tot_noise<(best_tot_noise+best_over_noise)) ) ||
		( (0<max_noise) && (best_max_noise>-0.5) && ((best_max_noise+1)>max_noise) && ((tot_noise+over_noise)<(best_tot_noise+best_over_noise)) ) ||
		( (0<max_noise) && (best_max_noise>-1) && ((best_max_noise+1.5)>max_noise) && ((tot_noise+over_noise+over_noise)<(best_tot_noise+best_over_noise+best_over_noise)) ) ));
	better = min(better, (over_noise <  best_over_noise)
		|| ((over_noise == best_over_noise)&&(tot_noise < best_tot_noise)));
	better = min(better, (over_noise < best_over_noise)
		||( (over_noise == best_over_noise)
		&&( (max_noise < best_max_noise)
		||( (max_noise == best_max_noise)
		&&(tot_noise <= best_tot_noise)
		)
		)
		));
#endif

	return better;
}


int count_bits(AACQuantInfo* quantInfo,
			   int quant[BLOCK_LEN_LONG]
			   )
{
	int i, bits = 0;

	if (quantInfo->block_type==ONLY_SHORT_WINDOW)
		quantInfo->pulseInfo.pulse_data_present = 0;
	else
		PulseCoder(quantInfo, quant);

	/* find a good method to section the scalefactor bands into huffman codebook sections */
	bit_search(quant,              /* Quantized spectral values */
		quantInfo);         /* Quantization information */

    /* Set special codebook for bands coded via PNS  */
    if (quantInfo->block_type != ONLY_SHORT_WINDOW) {     /* long blocks only */
		for(i=0;i<quantInfo->nr_of_sfb;i++) {
			if (quantInfo->pns_sfb_flag[i]) {
				quantInfo->book_vector[i] = PNS_HCB;
			}
		}
    }

	/* calculate the amount of bits needed for encoding the huffman codebook numbers */
	bits += sort_book_numbers(quantInfo,             /* Quantization information */
//		output_book_vector,    /* Output codebook vector, formatted for bitstream */
		NULL,          /* Bitstream */
		0);                    /* Write flag: 0 count, 1 write */

	/* calculate the amount of bits needed for the spectral values */
	quantInfo -> spectralCount = 0;
	for(i=0;i< quantInfo -> nr_of_sfb;i++) {  
		bits += output_bits(
			quantInfo,
			quantInfo->book_vector[i],
			quant,
			quantInfo->sfb_offset[i], 
			quantInfo->sfb_offset[i+1]-quantInfo->sfb_offset[i],
			0);
	}

	/* the number of bits for the scalefactors */
	bits += write_scalefactor_bitstream(
		NULL,             /* Bitstream */  
		0,                        /* Write flag */
		quantInfo
		);

	/* the total amount of bits required */
	return bits;
}

int tf_encode_spectrum_aac(
			   double      *p_spectrum[MAX_TIME_CHANNELS],
			   double      *PsySigMaskRatio[MAX_TIME_CHANNELS],
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
//			   int aacAllowScalefacs,
			   AACQuantInfo* quantInfo,      /* AAC quantization information */ 
			   Ch_Info* ch_info
//			   ,int varBitRate
//			   ,int bitRate
                           )
{
	int quant[BLOCK_LEN_LONG];
	int s_quant[BLOCK_LEN_LONG];
	int i;
//	int j=0;
	int k;
	double max_dct_line = 0;
//	int global_gain;
	int store_common_scalefac;
	int best_scale_factor[MAX_SCFAC_BANDS];
	double pow_spectrum[BLOCK_LEN_LONG];
	double requant[BLOCK_LEN_LONG];
	int sb;
	int extra_bits;
//	int max_bits;
//	int output_book_vector[MAX_SCFAC_BANDS*2];
	double SigMaskRatio[MAX_SCFAC_BANDS];
	MS_Info *ms_info;
	int *ptr_book_vector;

	/* Set up local pointers to quantInfo elements for convenience */
	int* sfb_offset = quantInfo -> sfb_offset;
	int* scale_factor = quantInfo -> scale_factor;
	int* common_scalefac = &(quantInfo -> common_scalefac);

	int outer_loop_count, notdone;
	int over, better;
	int best_over = 100;
//	int sfb_overflow;
	int best_common_scalefac;
	double noise_thresh;
	double sfQuantFac;
	double over_noise, tot_noise, max_noise;
	double noise[MAX_SCFAC_BANDS];
	double best_max_noise = 0;
	double best_over_noise = 0;
	double best_tot_noise = 0;
//	static int init = -1;

	/* Set block type in quantization info */
	quantInfo -> block_type = block_type[0];

#if 0
	if (init != quantInfo->block_type) {
		init = quantInfo->block_type;
		compute_ath(quantInfo, ATH);
	}
#endif

	sfQuantFac = pow(2.0, 0.1875);

	/** create the sfb_offset tables **/
	if (quantInfo->block_type == ONLY_SHORT_WINDOW) {

		/* Now compute interleaved sf bands and spectrum */
		sort_for_grouping(
			quantInfo,                       /* ptr to quantization information */
			sfb_width_table[0],      /* Widths of single window */
			p_spectrum,                      /* Spectral values, noninterleaved */
			SigMaskRatio,
			PsySigMaskRatio[0]
			);

		extra_bits = 51;
	} else{
		/* For long windows, band are not actually interleaved */
		if ((quantInfo -> block_type == ONLY_LONG_WINDOW) ||  
			(quantInfo -> block_type == LONG_SHORT_WINDOW) ||
			(quantInfo -> block_type == SHORT_LONG_WINDOW)) {
			quantInfo->nr_of_sfb = quantInfo->max_sfb;

			sfb_offset[0] = 0;
			k=0;
			for( i=0; i< quantInfo -> nr_of_sfb; i++ ){
				sfb_offset[i] = k;
				k +=sfb_width_table[0][i];
				SigMaskRatio[i]=PsySigMaskRatio[0][i];
			}
			sfb_offset[i] = k;
			extra_bits = 100; /* header bits and more ... */

		} 
	}

	extra_bits += 1;

    /* Take into account bits for TNS data */
    extra_bits += WriteTNSData(quantInfo,fixed_stream,0);    /* Count but don't write */

    if(quantInfo->block_type!=ONLY_SHORT_WINDOW)
		/* Take into account bits for LTP data */
		extra_bits += WriteLTP_PredictorData(quantInfo, fixed_stream, 0); /* Count but don't write */

    /* for short windows, compute interleaved energy here */
    if (quantInfo->block_type==ONLY_SHORT_WINDOW) {
		int numWindowGroups = quantInfo->num_window_groups;
		int maxBand = quantInfo->max_sfb;
		int windowOffset=0;
		int sfb_index=0;
		int g;
		for (g=0;g<numWindowGroups;g++) {
			int numWindowsThisGroup = quantInfo->window_group_length[g];
			int b;
			for (b=0;b<maxBand;b++) {
				double sum=0.0;
				int w;
				for (w=0;w<numWindowsThisGroup;w++) {
					int bandNum = (w+windowOffset)*maxBand + b;
					sum += energy[0][bandNum];
				}
				energy[0][sfb_index] = sum/numWindowsThisGroup;
				sfb_index++;
			}
			windowOffset += numWindowsThisGroup;
		}
    } 

	/* initialize the scale_factors that aren't intensity stereo bands */
	for(k=0; k< quantInfo -> nr_of_sfb ;k++) {
		scale_factor[k] = 0;
	}

	/* Mark IS bands by setting book_vector to INTENSITY_HCB */
	ptr_book_vector=quantInfo->book_vector;
	for (k=0;k<quantInfo->nr_of_sfb;k++) {
		ptr_book_vector[k] = 0;
	}

	/* PNS prepare */
	ms_info=&(ch_info->ms_info);
    for(sb=0; sb < quantInfo->nr_of_sfb; sb++ )
		quantInfo->pns_sfb_flag[sb] = 0;

//	if (block_type[0] != ONLY_SHORT_WINDOW) {     /* long blocks only */
		for(sb = pns_sfb_start; sb < quantInfo->nr_of_sfb; sb++ ) {
			/* Calc. pseudo scalefactor */
			if (energy[0][sb] == 0.0) {
				quantInfo->pns_sfb_flag[sb] = 0;
				continue;
			}

			if ((ms_info->is_present)&&(!ms_info->ms_used[sb])) {
				if ((10*log10(energy[0][sb]*sfb_width_table[0][sb]+1e-60)<70)||(SigMaskRatio[sb] > 1.0)) {
					quantInfo->pns_sfb_flag[sb] = 1;
					quantInfo->pns_sfb_nrg[sb] = (int) (2.0 * log(energy[0][sb]*sfb_width_table[0][sb]+1e-60) / log(2.0) + 0.5) + PNS_SF_OFFSET;

					/* Erase spectral lines */
					for( i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++ ) {
						p_spectrum[0][i] = 0.0;
					}
				}
			}
		}
//	}

	/* Compute allowed distortion */
	for(sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
		allowed_dist[0][sb] = energy[0][sb] * SigMaskRatio[sb];
//		if (allowed_dist[0][sb] < ATH[sb]) {
//			printf("%d Yes\n", sb);
//			allowed_dist[0][sb] = ATH[sb];
//		}
//		printf("%d\t\t%.3f\n", sb, SigMaskRatio[sb]);
	}

	/** find the maximum spectral coefficient **/
	/* Bug fix, 3/10/98 CL */
	/* for(i=0; i<BLOCK_LEN_LONG; i++){ */
	for(i=0; i < sfb_offset[quantInfo->nr_of_sfb]; i++){
		pow_spectrum[i] = (pow(ABS(p_spectrum[0][i]), 0.75));
		sign[i] = sgn(p_spectrum[0][i]);
		if ((ABS(p_spectrum[0][i])) > max_dct_line){
			max_dct_line = ABS(p_spectrum[0][i]);
		}
	}

	if (max_dct_line!=0.0) {
		if ((int)(16/3 * (log(ABS(pow(max_dct_line,0.75)/MAX_QUANT)/log(2.0)))) > old_startsf) {
			old_startsf = (int)(16/3 * (log(ABS(pow(max_dct_line,0.75)/MAX_QUANT)/log(2.0))));
		}
		if ((old_startsf > 200) || (old_startsf < 40))
			old_startsf = 40;
	}

	outer_loop_count = 0;

	notdone = 1;
	if (max_dct_line == 0) {
		notdone = 0;
	}
	while (notdone) { // outer iteration loop

		outer_loop_count++;
		over = 0;
//		sfb_overflow = 0;

//		if (max_dct_line == 0.0)
//			sfb_overflow = 1;

		if (outer_loop_count == 1) {
//			max_bits = search_common_scalefac(quantInfo, p_spectrum[0], pow_spectrum,
//				quant, average_block_bits);
			search_common_scalefac(quantInfo, pow_spectrum, quant, average_block_bits);
		}

//		max_bits = inner_loop(quantInfo, p_spectrum[0], pow_spectrum,
//			quant, average_block_bits) + extra_bits;
		inner_loop(quantInfo, pow_spectrum, quant, average_block_bits);

		store_common_scalefac = quantInfo->common_scalefac;

		if (notdone) {
			over = calc_noise(quantInfo, p_spectrum[0], quant, requant, noise, allowed_dist[0],
				&over_noise, &tot_noise, &max_noise);

			better = quant_compare(best_over, best_tot_noise, best_over_noise,
				best_max_noise, over, tot_noise, over_noise, max_noise);
//			better = quant_compare(best_tot_noise, best_over_noise,
//				               tot_noise, over_noise);

			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
				if (scale_factor[sb] > 59) {
//					sfb_overflow = 1;
					better = 0;
				}
			}

			if (outer_loop_count == 1)
				better = 1;

			if (better) {
//				best_over = over;
//				best_max_noise = max_noise;
				best_over_noise = over_noise;
				best_tot_noise = tot_noise;
				best_common_scalefac = store_common_scalefac;

				for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
					best_scale_factor[sb] = scale_factor[sb];
				}
				memcpy(s_quant, quant, sizeof(int)*BLOCK_LEN_LONG);
			}
		}

		if (over == 0) notdone=0;

		if (notdone) {
			notdone = 0;
			noise_thresh = -900;
			for ( sb = 0; sb < quantInfo->nr_of_sfb; sb++ )
				noise_thresh = max(1.05*noise[sb], noise_thresh);
			noise_thresh = min(noise_thresh, 0.0);

			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
				if ((noise[sb] > noise_thresh)&&(quantInfo->book_vector[sb]!=INTENSITY_HCB)&&(quantInfo->book_vector[sb]!=INTENSITY_HCB2)) {

					allowed_dist[0][sb] *= 2;
					scale_factor[sb]++;
					for (i = quantInfo->sfb_offset[sb]; i < quantInfo->sfb_offset[sb+1]; i++){
						pow_spectrum[i] *= sfQuantFac;
					}
					notdone = 1;
				}
			}
			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
				if (scale_factor[sb] > 59)
					notdone = 0;
			}
		}

		if (notdone) {
			notdone = 0;
			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++)
				if (scale_factor[sb] == 0)
					notdone = 1;
		}

	}

	if (max_dct_line > 0) {
		*common_scalefac = best_common_scalefac;
		for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
			scale_factor[sb] = best_scale_factor[sb];
//			printf("%d\t%d\n", sb, scale_factor[sb]);
		}
		for (i = 0; i < 1024; i++)
			quant[i] = s_quant[i]*sign[i];
	} else {
		*common_scalefac = 0;
		for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
			scale_factor[sb] = 0;
		}
		for (i = 0; i < 1024; i++)
			quant[i] = 0;
	}

	calc_noise(quantInfo, p_spectrum[0], quant, requant, noise, allowed_dist[0],
			&over_noise, &tot_noise, &max_noise);
//	count_bits(quantInfo, quant, output_book_vector);
	count_bits(quantInfo, quant);
	if (quantInfo->block_type!=ONLY_SHORT_WINDOW)
		PulseDecoder(quantInfo, quant);

//	for( sb=0; sb< quantInfo -> nr_of_sfb; sb++ ) {
//		printf("%d error: %.4f all.dist.: %.4f energy: %.4f\n", sb,
//			noise[sb], allowed_dist[0][sb], energy[0][sb]);
//	}

	/* offset the differenec of common_scalefac and scalefactors by SF_OFFSET  */
	for (i=0; i<quantInfo->nr_of_sfb; i++){
		if ((ptr_book_vector[i]!=INTENSITY_HCB)&&(ptr_book_vector[i]!=INTENSITY_HCB2)) {
			scale_factor[i] = *common_scalefac - scale_factor[i] + SF_OFFSET;
		}
	}
//	*common_scalefac = global_gain = scale_factor[0];
	*common_scalefac = scale_factor[0];

	/* place the codewords and their respective lengths in arrays data[] and len[] respectively */
	/* there are 'counter' elements in each array, and these are variable length arrays depending on the input */

	quantInfo -> spectralCount = 0;
	for(k=0;k< quantInfo -> nr_of_sfb; k++) {
		output_bits(
			quantInfo,
			quantInfo->book_vector[k],
			quant,
			quantInfo->sfb_offset[k],
			quantInfo->sfb_offset[k+1]-quantInfo->sfb_offset[k],
			1);
//		printf("%d\t%d\n",k,quantInfo->book_vector[k]);
	}

	/* write the reconstructed spectrum to the output for use with prediction */
	{
		int i;
		for (sb=0; sb<quantInfo -> nr_of_sfb; sb++){
			if ((ptr_book_vector[sb]==INTENSITY_HCB)||(ptr_book_vector[sb]==INTENSITY_HCB2)){
				for (i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++){
					p_reconstructed_spectrum[0][i]=673;
				}
			} else {
				for (i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++){
					p_reconstructed_spectrum[0][i] = sgn(p_spectrum[0][i]) * requant[i];
				}
			}
		}
	}

	return FNO_ERROR;
}



int sort_for_grouping(AACQuantInfo* quantInfo,        /* ptr to quantization information */
		      int sfb_width_table[],          /* Widths of single window */
		      double *p_spectrum[],           /* Spectral values, noninterleaved */
		      double *SigMaskRatio,
		      double *PsySigMaskRatio)
{
	int i,j,ii;
	int index;
	double tmp[1024];
//	int book=1;
	int group_offset;
	int k=0;
	int windowOffset;

	/* set up local variables for used quantInfo elements */
	int* sfb_offset = quantInfo -> sfb_offset;
	int* nr_of_sfb = &(quantInfo -> nr_of_sfb);
	int* window_group_length;
	int num_window_groups;
	*nr_of_sfb = quantInfo->max_sfb;              /* Init to max_sfb */
	window_group_length = quantInfo -> window_group_length;
	num_window_groups = quantInfo -> num_window_groups;

	/* calc org sfb_offset just for shortblock */
	sfb_offset[k]=0;
	for (k=0; k < 1024; k++) {
		tmp[k] = 0.0;
	}
	for (k=1 ; k <*nr_of_sfb+1; k++) {
		sfb_offset[k] = sfb_offset[k-1] + sfb_width_table[k-1];
	}

	/* sort the input spectral coefficients */
	index = 0;
	group_offset=0;
	for (i=0; i< num_window_groups; i++) {
		for (k=0; k<*nr_of_sfb; k++) {
			for (j=0; j < window_group_length[i]; j++) {
				for (ii=0;ii< sfb_width_table[k];ii++)
					tmp[index++] = p_spectrum[0][ii+ sfb_offset[k] + 128*j +group_offset];
			}
		}
		group_offset +=  128*window_group_length[i];
	}

	for (k=0; k<1024; k++){
		p_spectrum[0][k] = tmp[k];
	}

	/* now calc the new sfb_offset table for the whole p_spectrum vector*/
	index = 0;
	sfb_offset[index] = 0;
	index++;
	windowOffset = 0;
	for (i=0; i < num_window_groups; i++) {
		for (k=0 ; k <*nr_of_sfb; k++) {
			/* for this window group and this band, find worst case inverse sig-mask-ratio */
			int bandNum=windowOffset*NSFB_SHORT + k;
			double worstISMR = PsySigMaskRatio[bandNum];
			int w;
			for (w=1;w<window_group_length[i];w++) {
				bandNum=(w+windowOffset)*NSFB_SHORT + k;
				if (PsySigMaskRatio[bandNum]<worstISMR) {
					worstISMR += (PsySigMaskRatio[bandNum] > 0)?PsySigMaskRatio[bandNum]:worstISMR;
				}
			}
			worstISMR /= 2.0;
			SigMaskRatio[k+ i* *nr_of_sfb]=worstISMR/window_group_length[i];
			sfb_offset[index] = sfb_offset[index-1] + sfb_width_table[k]*window_group_length[i] ;
			index++;
		}
		windowOffset += window_group_length[i];
	}

	*nr_of_sfb = *nr_of_sfb * num_window_groups;  /* Number interleaved bands. */

	return 0;
}


#if 0 // VBR quantizer not finished yet

#if (defined(__GNUC__) && defined(__i386__))
#define USE_GNUC_ASM
#endif
#ifdef _MSC_VER
#define USE_MSC_ASM
#endif

/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *********************************************************************/
#ifdef USE_GNUC_ASM
#  define ROUNDFAC -0.0946
#elif defined (USE_MSC_ASM)
#  define ROUNDFAC -0.0946
#else
#  define ROUNDFAC 0.4054
#endif



int compute_scalefacs(AACQuantInfo* quantInfo,
					  int sf[MAX_SCFAC_BANDS],
					  int scalefac[MAX_SCFAC_BANDS])
{
	int sfb;
	int maxover;
	int ifqstep = 2;
	

//	if (cod_info->preflag)
//		for ( sfb = 11; sfb < SBPSY_l; sfb++ ) 
//			sf[sfb] += pretab[sfb]*ifqstep;


	maxover = 0;
	for (sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++) {

		if (sf[sfb]<0) {
			/* ifqstep*scalefac >= -sf[sfb], so round UP */
			scalefac[sfb]=-sf[sfb]/ifqstep  + (-sf[sfb] % ifqstep != 0);
			if (scalefac[sfb] > /*max_range[sfb]*/10) scalefac[sfb]=10/*max_range[sfb]*/;
			
			/* sf[sfb] should now be positive: */
			if (  -(sf[sfb] + scalefac[sfb]*ifqstep)  > maxover) {
				maxover = -(sf[sfb] + scalefac[sfb]*ifqstep);
			}
		}
	}

	return maxover;
}
  
  

double calc_sfb_noise_ave(double *p_spectrum, double *pow_spectrum, int bw,int sf)
{
	int j;
	double xfsf=0, xfsf_p1=0, xfsf_m1=0;
	double sfpow34,sfpow34_p1,sfpow34_m1;
	double sfpow,sfpow_p1,sfpow_m1;

	sfpow = pow(2.0,((double)sf)*.25);
	sfpow34  = pow(2.0,-((double)sf)*.1875);

	sfpow_m1 = sfpow*.8408964153;
	sfpow34_m1 = sfpow34*1.13878863476;

	sfpow_p1 = sfpow*1.189207115;  
	sfpow34_p1 = sfpow34*0.878126080187;

	for ( j=0; j < bw ; j++) {
		int ix;
		double temp,temp_p1,temp_m1;

		if (pow_quant[j]*sfpow34_m1 > 8191) return -1;

		temp = pow_quant[j]*sfpow34;
		XRPOW_FTOI(temp, ix);
		XRPOW_FTOI(temp + QUANTFAC(ix), ix);
		temp = fabs(p_spectrum[j])- pow_quant[ix]*sfpow;
		temp *= temp;

		temp_p1 = pow_quant[j]*sfpow34_p1;
		XRPOW_FTOI(temp_p1, ix);
		XRPOW_FTOI(temp_p1 + QUANTFAC(ix), ix);
		temp_p1 = fabs(p_spectrum[j])- pow_quant[ix]*sfpow_p1;
		temp_p1 *= temp_p1;
		
		temp_m1 = pow_quant[j]*sfpow34_m1;
		XRPOW_FTOI(temp_m1, ix);
		XRPOW_FTOI(temp_m1 + QUANTFAC(ix), ix);
		temp_m1 = fabs(p_spectrum[j])- pow_quant[ix]*sfpow_m1;
		temp_m1 *= temp_m1;

		xfsf += temp;
		xfsf_p1 += temp_p1;
		xfsf_m1 += temp_m1;
	}
	if (xfsf_p1>xfsf) xfsf = xfsf_p1;
	if (xfsf_m1>xfsf) xfsf = xfsf_m1;
	return xfsf/bw;
}

int find_scalefac(double *p_spectrum,double *pow_quant,int sfb,
				  double l3_xmin,int bw)
{
	double xfsf;
	int i,sf,sf_ok,delsf;

	/* search will range from sf:  -209 -> 45  */
	sf = -82;
	delsf = 128;

	sf_ok=10000;
	for (i=0; i<7; i++) {
		delsf /= 2;
		xfsf = calc_sfb_noise_ave(p_spectrum,pow_quant,bw,sf);

		if (xfsf < 0) {
			/* scalefactors too small */
			sf += delsf;
		}else{
			if (sf_ok==10000) sf_ok=sf;  
			if (xfsf > l3_xmin)  {
				/* distortion.  try a smaller scalefactor */
				sf -= delsf;
			}else{
				sf_ok = sf;
				sf += delsf;
			}
		}
	} 
//	assert(sf_ok!=10000);

	return sf;
}

int
VBR_quantize_granule(AACQuantInfo* quantInfo,
					 double *pow_spectrum,
					 int quant[1024]
					 )
{
	quantize(quantInfo, pow_spectrum, quant);

	return count_bits(quantInfo, quant);
}

int
VBR_noise_shaping(AACQuantInfo* quantInfo,
				  double p_spectrum[1024],
				  double pow_quant_orig[1024],
				  double allowed_dist[SFB_NUM_MAX],
				  int quant[1024], int minbits, int maxbits,
				  int scalefac[MAX_SCFAC_BANDS])
{
	int start,end,bw,sfb,l, vbrmax;
	int bits_used;
	int vbrsf[MAX_SCFAC_BANDS];
	int save_sf[MAX_SCFAC_BANDS];
	int maxover0,maxover1,maxover,mover;
	int ifqstep;
	double pow_quant[1024];

	
//	for(i=0;i<1024;i++) {
//		double temp=fabs(p_spectrum[i]);
//		pow_quant[i]=sqrt(sqrt(temp)*temp);
//	}
	memcpy(pow_quant, pow_quant_orig, sizeof(pow_quant));

	
	vbrmax=-10000;
	
	for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ )   {
		start = quantInfo->sfb_offset[sfb];
		end   = quantInfo->sfb_offset[sfb+1];
		bw = end - start;
		vbrsf[sfb] = find_scalefac(&p_spectrum[start],&pow_quant[start],sfb,
			allowed_dist[sfb],bw);
	}

#define MAX_SF_DELTA 4

	for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ )   {
		if (sfb>0) 
			vbrsf[sfb] = min(vbrsf[sfb-1]+MAX_SF_DELTA,vbrsf[sfb]);
		if (sfb< quantInfo->nr_of_sfb-1) 
			vbrsf[sfb] = min(vbrsf[sfb+1]+MAX_SF_DELTA,vbrsf[sfb]);
	}

	for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ )   
		if (vbrsf[sfb]>vbrmax) vbrmax = vbrsf[sfb];


	/* save a copy of vbrsf, incase we have to recomptue scalefacs */
	memcpy(&save_sf,&vbrsf,sizeof(int)*MAX_SCFAC_BANDS);


	do {

		memset(scalefac,0,sizeof(int)*MAX_SCFAC_BANDS);

		maxover0=0;
		maxover1=0;
		
		
		for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ ) {
			maxover0 = max(maxover0,(vbrmax - vbrsf[sfb]) - 2*/*max_range[sfb]*/10 );
			maxover1 = max(maxover1,(vbrmax - vbrsf[sfb]) - 4*/*max_range[sfb]*/10 );
		}
		mover = maxover0;


		vbrmax -= mover;
		maxover0 -= mover;
		maxover1 -= mover;

#if 0
		if (maxover0<=0) {
			cod_info->preflag=0;
			vbrmax -= maxover0;
		} else if (maxover0p<=0) {
			cod_info->preflag=1;
			vbrmax -= maxover0p;
		} else if (maxover1==0) {
			cod_info->preflag=0;
		} else if (maxover1p==0) {
			cod_info->preflag=1;
		}
#endif

		
		quantInfo->common_scalefac = vbrmax +210;
//		assert(cod_info->global_gain < 256);
		if (quantInfo->common_scalefac>255) quantInfo->common_scalefac = 255;

		
		for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ )   
			vbrsf[sfb] -= vbrmax;
		
		
		maxover = compute_scalefacs(quantInfo,vbrsf,scalefac);
//		assert(maxover <=0);
		
		
		/* quantize pow_quant[] based on computed scalefactors */
		ifqstep = 2;
		for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ ) {
			int ifac;
			double fac;
			ifac = ifqstep*scalefac[sfb];
//			if (cod_info->preflag)
//				ifac += ifqstep*pretab[sfb];

//			if (ifac+210<330) 
//				fac = 1/pow(2.0,-((double)ifac)*.1875);
//			else
				fac = pow(2.0,.75*ifac/4.0);

			start = quantInfo->sfb_offset[sfb];
			end   = quantInfo->sfb_offset[sfb+1];
			for ( l = start; l < end; l++ ) {
				pow_quant[l]*=fac;
			}
		} 

		bits_used = VBR_quantize_granule(quantInfo, pow_quant, quant);


		if (bits_used < minbits) {
			/* decrease global gain, recompute scale factors */
//			if (digital_silence) break;  
			if (vbrmax+210 ==0 ) break;
			


			--vbrmax;
			memcpy(&vbrsf,&save_sf,sizeof(int)*MAX_SCFAC_BANDS);
			memcpy(pow_quant, pow_quant_orig, sizeof(pow_quant));
		}

	} while ((bits_used < minbits));

	
	while (bits_used > min(maxbits,4095)) {
		/* increase global gain, keep exisiting scale factors */
		++quantInfo->common_scalefac;
		bits_used = VBR_quantize_granule(quantInfo, pow_quant, quant);
	}

	return bits_used;
}

int calc_xmin(AACQuantInfo* quantInfo, double p_spectrum[1024], double ratio[MAX_SCFAC_BANDS],
	       double allowed_dist[MAX_SCFAC_BANDS], double masking_lower)
{
	int start, end, bw,l,ath_over=0;
	int sfb;
	double en0, ener;

	for ( sfb = 0; sfb < quantInfo->nr_of_sfb; sfb++ ){
		start = quantInfo->sfb_offset[sfb];
		end   = quantInfo->sfb_offset[sfb+1];
		bw = end - start;
		
		for (en0 = 0.0, l = start; l < end; l++ ) {
			ener = p_spectrum[l] * p_spectrum[l];
			en0 += ener;
		}
		en0 /= bw;
		
		allowed_dist[sfb] = en0 * ratio[sfb] * masking_lower / en0;
	}
	
	return 0;
}

int tf_encode_spectrum_aac_VBR(
			   double      *p_spectrum[MAX_TIME_CHANNELS],
			   double      *PsySigMaskRatio[MAX_TIME_CHANNELS],
			   double      allowed_dist[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			   double      energy[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS],
			   int         sfb_width_table[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			   int         average_block_bits,
			   BsBitStream *fixed_stream,
			   double      *p_reconstructed_spectrum[MAX_TIME_CHANNELS],
			   AACQuantInfo* quantInfo,      /* AAC quantization information */ 
			   Ch_Info* ch_info
               )
{
	int quant[BLOCK_LEN_LONG];
	int i;
	int k;
	double max_dct_line = 0;
	double pow_spectrum[BLOCK_LEN_LONG];
	double requant[BLOCK_LEN_LONG];
	int sb;
	int extra_bits;
	double SigMaskRatio[MAX_SCFAC_BANDS];
	MS_Info *ms_info;
	int *ptr_book_vector;

	/* Set up local pointers to quantInfo elements for convenience */
	int* sfb_offset = quantInfo->sfb_offset;
	int* scale_factor = quantInfo->scale_factor;
	int* common_scalefac = &(quantInfo -> common_scalefac);

	int outer_loop_count;
	int best_over = 100;
	double sfQuantFac;
	double over_noise, tot_noise, max_noise;
	double noise[MAX_SCFAC_BANDS];
	double best_max_noise = 0;
	double best_over_noise = 0;
	double best_tot_noise = 0;

	int max_bits = 3 * average_block_bits;
	int min_bits = average_block_bits / 1;
	int totbits, bits_ok;
	double masking_lower_db = 0;
	double masking_lower = 0;
	double qadjust = 0;
	int used_bits;

	/* Set block type in quantization info */
	quantInfo -> block_type = block_type[0];

	sfQuantFac = pow(2.0, 0.1875);

	/** create the sfb_offset tables **/
	if (quantInfo->block_type == ONLY_SHORT_WINDOW) {

		/* Now compute interleaved sf bands and spectrum */
		sort_for_grouping(
			quantInfo,                       /* ptr to quantization information */
			sfb_width_table[0],      /* Widths of single window */
			p_spectrum,                      /* Spectral values, noninterleaved */
			SigMaskRatio,
			PsySigMaskRatio[0]
			);

		extra_bits = 51;
	} else{
		/* For long windows, band are not actually interleaved */
		if ((quantInfo -> block_type == ONLY_LONG_WINDOW) ||  
			(quantInfo -> block_type == LONG_SHORT_WINDOW) ||
			(quantInfo -> block_type == SHORT_LONG_WINDOW)) {
			quantInfo->nr_of_sfb = quantInfo->max_sfb;

			sfb_offset[0] = 0;
			k=0;
			for( i=0; i< quantInfo -> nr_of_sfb; i++ ){
				sfb_offset[i] = k;
				k +=sfb_width_table[0][i];
				SigMaskRatio[i]=PsySigMaskRatio[0][i];
			}
			sfb_offset[i] = k;
			extra_bits = 100; /* header bits and more ... */

		} 
	}

	extra_bits += 1;

    /* Take into account bits for TNS data */
    extra_bits += WriteTNSData(quantInfo,fixed_stream,0);    /* Count but don't write */

    if(quantInfo->block_type!=ONLY_SHORT_WINDOW)
		/* Take into account bits for LTP data */
		extra_bits += WriteLTP_PredictorData(quantInfo, fixed_stream, 0); /* Count but don't write */

    /* for short windows, compute interleaved energy here */
    if (quantInfo->block_type==ONLY_SHORT_WINDOW) {
		int numWindowGroups = quantInfo->num_window_groups;
		int maxBand = quantInfo->max_sfb;
		int windowOffset=0;
		int sfb_index=0;
		int g;
		for (g=0;g<numWindowGroups;g++) {
			int numWindowsThisGroup = quantInfo->window_group_length[g];
			int b;
			for (b=0;b<maxBand;b++) {
				double sum=0.0;
				int w;
				for (w=0;w<numWindowsThisGroup;w++) {
					int bandNum = (w+windowOffset)*maxBand + b;
					sum += energy[0][bandNum];
				}
				energy[0][sfb_index] = sum/numWindowsThisGroup;
				sfb_index++;
			}
			windowOffset += numWindowsThisGroup;
		}
    } 

	/* initialize the scale_factors that aren't intensity stereo bands */
	for(k=0; k< quantInfo -> nr_of_sfb ;k++) {
		scale_factor[k] = 0;
	}

	/* Mark IS bands by setting book_vector to INTENSITY_HCB */
	ptr_book_vector=quantInfo->book_vector;
	for (k=0;k<quantInfo->nr_of_sfb;k++) {
		ptr_book_vector[k] = 0;
	}

	/* PNS prepare */
	ms_info=&(ch_info->ms_info);
    for(sb=0; sb < quantInfo->nr_of_sfb; sb++ )
		quantInfo->pns_sfb_flag[sb] = 0;

	for(sb = pns_sfb_start; sb < quantInfo->nr_of_sfb; sb++ ) {
		/* Calc. pseudo scalefactor */
		if (energy[0][sb] == 0.0) {
			quantInfo->pns_sfb_flag[sb] = 0;
			continue;
		}

		if ((ms_info->is_present)&&(!ms_info->ms_used[sb])) {
			if ((10*log10(energy[0][sb]*sfb_width_table[0][sb]+1e-60)<70)||(SigMaskRatio[sb] > 1.0)) {
				quantInfo->pns_sfb_flag[sb] = 1;
				quantInfo->pns_sfb_nrg[sb] = (int) (2.0 * log(energy[0][sb]*sfb_width_table[0][sb]+1e-60) / log(2.0) + 0.5) + PNS_SF_OFFSET;

				/* Erase spectral lines */
				for( i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++ ) {
					p_spectrum[0][i] = 0.0;
				}
			}
		}
	}

	/** find the maximum spectral coefficient **/
	/* Bug fix, 3/10/98 CL */
	/* for(i=0; i<BLOCK_LEN_LONG; i++){ */
	for(i=0; i < sfb_offset[quantInfo->nr_of_sfb]; i++){
		pow_spectrum[i] = (pow(ABS(p_spectrum[0][i]), 0.75));
		sign[i] = sgn(p_spectrum[0][i]);
		if ((ABS(p_spectrum[0][i])) > max_dct_line){
			max_dct_line = ABS(p_spectrum[0][i]);
		}
	}

	if (max_dct_line!=0.0) {
		if ((int)(16/3 * (log(ABS(pow(max_dct_line,0.75)/MAX_QUANT)/log(2.0)))) > old_startsf) {
			old_startsf = (int)(16/3 * (log(ABS(pow(max_dct_line,0.75)/MAX_QUANT)/log(2.0))));
		}
		if ((old_startsf > 200) || (old_startsf < 40))
			old_startsf = 40;
	}

	outer_loop_count = 0;
	used_bits = max_bits + 1;

	calc_xmin(quantInfo, p_spectrum[0], SigMaskRatio, allowed_dist[0],
		masking_lower);

	do {

		int shortblock;
		totbits=0;
				
		/* ENCODE this data first pass, and on future passes unless it uses
		* a very small percentage of the max_frame_bits  */
		if (used_bits > max_bits) {

			shortblock = (quantInfo->block_type == ONLY_SHORT_WINDOW);

			if (qadjust!=0 /*|| shortblock*/) {
				/* Adjust allowed masking based on quality setting */
				masking_lower_db = /*dbQ[0]*/0 + qadjust;

//				if (pe[gr][ch]>750)
//					masking_lower_db -= 4*(pe[gr][ch]-750.)/750.;

				masking_lower = pow(10.0,masking_lower_db/10);
				calc_xmin(quantInfo, p_spectrum[0], SigMaskRatio, allowed_dist[0],
					masking_lower);
			}

			used_bits = VBR_noise_shaping(quantInfo, p_spectrum[0], pow_spectrum,
				allowed_dist[0], quant,	min_bits, max_bits, scale_factor);
		}
		bits_ok=1;
		if (used_bits > max_bits) {
			qadjust += max(.25,(used_bits - max_bits)/300.0);
			min_bits = max(125,min_bits*0.975);
			max_bits = max(min_bits,max_bits*0.975);
			bits_ok=0;
		}
		
	} while (!bits_ok);

	calc_noise(quantInfo, p_spectrum[0], quant, requant, noise, allowed_dist[0],
			&over_noise, &tot_noise, &max_noise);
	count_bits(quantInfo, quant);
	if (quantInfo->block_type!=ONLY_SHORT_WINDOW)
		PulseDecoder(quantInfo, quant);

	/* offset the differenec of common_scalefac and scalefactors by SF_OFFSET  */
	for (i=0; i<quantInfo->nr_of_sfb; i++){
		if ((ptr_book_vector[i]!=INTENSITY_HCB)&&(ptr_book_vector[i]!=INTENSITY_HCB2)) {
			scale_factor[i] = *common_scalefac - scale_factor[i] + SF_OFFSET;
		}
	}
	*common_scalefac = scale_factor[0];

	/* place the codewords and their respective lengths in arrays data[] and len[] respectively */
	/* there are 'counter' elements in each array, and these are variable length arrays depending on the input */

	quantInfo -> spectralCount = 0;
	for(k=0;k< quantInfo -> nr_of_sfb; k++) {
		output_bits(
			quantInfo,
			quantInfo->book_vector[k],
			quant,
			quantInfo->sfb_offset[k],
			quantInfo->sfb_offset[k+1]-quantInfo->sfb_offset[k],
			1);
	}

	/* write the reconstructed spectrum to the output for use with prediction */
	{
		int i;
		for (sb=0; sb<quantInfo -> nr_of_sfb; sb++){
			if ((ptr_book_vector[sb]==INTENSITY_HCB)||(ptr_book_vector[sb]==INTENSITY_HCB2)){
				for (i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++){
					p_reconstructed_spectrum[0][i]=673;
				}
			} else {
				for (i=sfb_offset[sb]; i<sfb_offset[sb+1]; i++){
					p_reconstructed_spectrum[0][i] = sgn(p_spectrum[0][i]) * requant[i];
				}
			}
		}
	}

	return FNO_ERROR;
}
#endif
