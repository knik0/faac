

#include "aacenc.h" 
#include "bitstream.h"
#include "tf_main.h"
#include "pulse.h"
#include "aac_qc.h"
#include "aac_se_enc.h"
#include <math.h>

#include "hufftab5.h"


double pow_quant[9000];
double adj_quant[9000];
int sign[1024];
int g_Count;
int old_startsf;

double ATH[SFB_NUM_MAX];

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
 

void compute_ath(AACQuantInfo *quantInfo, double ATH[SFB_NUM_MAX])
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
}

int quantize(AACQuantInfo *quantInfo,
			 double *p_spectrum,
			 double *pow_spectrum,
			 int quant[NUM_COEFF]
			 )
{
	int i, sb;
	double quantFac;
	double x;

	for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {

		quantFac = pow(2.0, 0.1875*(quantInfo->scale_factor[sb] -
			quantInfo->common_scalefac ));

		for (i = quantInfo->sfb_offset[sb]; i < quantInfo->sfb_offset[sb+1]; i++){
			x = pow_spectrum[i] * quantFac;
			if (x > MAX_QUANT) {
				return 1;
			}
			quant[i] = (int)(x + adj_quant[(int)x]);
			quant[i] = sgn(p_spectrum[i]) * quant[i];  /* restore the original sign */
		}
	}

	if (quantInfo->block_type==ONLY_SHORT_WINDOW)
		quantInfo->pulseInfo.pulse_data_present = 0;
	else
		PulseCoder(quantInfo, quant);

	return 0;
}

int calc_noise(AACQuantInfo *quantInfo,
				double *p_spectrum,
				int quant[NUM_COEFF],
				double requant[NUM_COEFF],
				double error_energy[SFB_NUM_MAX],
				double allowed_dist[SFB_NUM_MAX],
				double *over_noise,
				double *tot_noise,
				double *max_noise
				)
{
	int i, sb, sbw;
	int over = 0, count = 0;
	double invQuantFac;
	double linediff;

	*over_noise = 0.0;
	*tot_noise = 0.0;
	*max_noise = -999.0;

	if (quantInfo->block_type!=ONLY_SHORT_WINDOW)
		PulseDecoder(quantInfo, quant);

	for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {

		double max_sb_noise = 0.0;

		sbw = quantInfo->sfb_offset[sb+1] - quantInfo->sfb_offset[sb];

		invQuantFac = pow(2.0,-0.25 * (quantInfo->scale_factor[sb] - quantInfo->common_scalefac ));

		error_energy[sb] = 0.0;

		for (i = quantInfo->sfb_offset[sb]; i < quantInfo->sfb_offset[sb+1]; i++){
			requant[i] =  pow_quant[ABS(quant[i])] * invQuantFac; 

			/* measure the distortion in each scalefactor band */
			linediff = (double)(ABS(p_spectrum[i]) - ABS(requant[i]));
			linediff *= linediff;
			error_energy[sb] += linediff;
			max_sb_noise = max(max_sb_noise, linediff);
		}
		error_energy[sb] = error_energy[sb] / sbw;		
		
		if( (max_sb_noise > 0) && (error_energy[sb] < 1e-7 ) ) {
			double diff = max_sb_noise-error_energy[sb];
			double fac  = pow(diff/max_sb_noise,4);
			error_energy[sb] += diff*fac;
		}
		if (allowed_dist[sb] != 0.0)
			error_energy[sb] = 10*log10(error_energy[sb] / allowed_dist[sb]);
		else error_energy[sb] = 0;
		if (error_energy[sb] > 0) {
			over++;
			*over_noise += error_energy[sb];
		}
		*tot_noise += error_energy[sb];
		*max_noise = max(*max_noise, error_energy[sb]);
		count++;
  	}

	if (count>1) *tot_noise /= count;
	if (over>1) *over_noise /= over;
	return over;
}

int quant_compare(int best_over, double best_tot_noise, double best_over_noise,
				  double best_max_noise, int over, double tot_noise, double over_noise,
				  double max_noise)
{
	/*
	noise is given in decibals (db) relative to masking thesholds.

	over_noise:  sum of quantization noise > masking
	tot_noise:   sum of all quantization noise
	max_noise:   max quantization noise 

	*/
	int better=0;

#if 0
	better = ((over < best_over) ||
			((over==best_over) && (over_noise<best_over_noise)) ) ;
#else
#if 0
	better = max_noise < best_max_noise;
#else
#if 0
	better = tot_noise < best_tot_noise;
#else
#if 0
	better = (tot_noise < best_tot_noise) &&
		(max_noise < best_max_noise + 2);
#else
#if 0
	better = ( ( (0>=max_noise) && (best_max_noise>2)) ||
		( (0>=max_noise) && (best_max_noise<0) && ((best_max_noise+2)>max_noise) && (tot_noise<best_tot_noise) ) ||
		( (0>=max_noise) && (best_max_noise>0) && ((best_max_noise+2)>max_noise) && (tot_noise<(best_tot_noise+best_over_noise)) ) ||
		( (0<max_noise) && (best_max_noise>-0.5) && ((best_max_noise+1)>max_noise) && ((tot_noise+over_noise)<(best_tot_noise+best_over_noise)) ) ||
		( (0<max_noise) && (best_max_noise>-1) && ((best_max_noise+1.5)>max_noise) && ((tot_noise+over_noise+over_noise)<(best_tot_noise+best_over_noise+best_over_noise)) ) );
#else
#if 1
	better =   (over_noise <  best_over_noise)
		|| ((over_noise == best_over_noise)&&(tot_noise < best_tot_noise));
#if 0
	better = (over_noise < best_over_noise)
		||( (over_noise == best_over_noise)
		&&( (max_noise < best_max_noise)
		||( (max_noise == best_max_noise)
		&&(tot_noise <= best_tot_noise)
		)
		) 
		);
#endif
#endif
#endif
#endif
#endif
#endif
#endif

	return better;
}


int count_bits(AACQuantInfo* quantInfo,
			   int quant[NUM_COEFF],
			   int output_book_vector[SFB_NUM_MAX*2])
{
	int i, bits = 0;

	/* find a good method to section the scalefactor bands into huffman codebook sections */
	bit_search(quant,              /* Quantized spectral values */
		quantInfo);         /* Quantization information */

	/* calculate the amount of bits needed for encoding the huffman codebook numbers */
	bits += sort_book_numbers(quantInfo,             /* Quantization information */
		output_book_vector,    /* Output codebook vector, formatted for bitstream */
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
			   int         nr_of_sfb[MAX_TIME_CHANNELS],
			   int         average_block_bits,
			   int         available_bitreservoir_bits,
			   int         padding_limit,
			   BsBitStream *fixed_stream,
			   BsBitStream *var_stream,
			   int         nr_of_chan,
			   double      *p_reconstructed_spectrum[MAX_TIME_CHANNELS],
			   int         useShortWindows,
			   int aacAllowScalefacs,
			   AACQuantInfo* quantInfo,      /* AAC quantization information */ 
			   Ch_Info* ch_info,
			   int varBitRate,
			   int bitRate)
{
	int quant[NUM_COEFF];
	int i=0;
	int j=0;
	int k;
	double max_dct_line = 0;
	int global_gain;
	int store_common_scalefac;
	int best_scale_factor[SFB_NUM_MAX];
	double pow_spectrum[NUM_COEFF];
	double requant[NUM_COEFF];
	int sb;
	int extra_bits;
	int max_bits;
	int output_book_vector[SFB_NUM_MAX*2];
	int start_com_sf;
	double SigMaskRatio[SFB_NUM_MAX];
	IS_Info *is_info;
	int *ptr_book_vector;

	/* Set up local pointers to quantInfo elements for convenience */
	int* sfb_offset = quantInfo -> sfb_offset;
	int* scale_factor = quantInfo -> scale_factor;
	int* common_scalefac = &(quantInfo -> common_scalefac);

	int outer_loop_count;
	int quantizer_change;
	int over = 0, best_over = 100, better;
	int sfb_overflow, amp_over;
	int best_common_scalefac;
	int prev_scfac, prev_is_scfac;
	double noise_thresh;
	double over_noise, tot_noise, max_noise;
	double noise[SFB_NUM_MAX];
	double best_max_noise = 0;
	double best_over_noise = 0;
	double best_tot_noise = 0;
	static int init = -1;

	/* Set block type in quantization info */
	quantInfo -> block_type = block_type[MONO_CHAN];

#if 0
	if (init != quantInfo->block_type) {
		init = quantInfo->block_type;
		compute_ath(quantInfo, ATH);
	}
#endif

	/** create the sfb_offset tables **/
	if (quantInfo -> block_type == ONLY_SHORT_WINDOW) {

		/* Now compute interleaved sf bands and spectrum */
		sort_for_grouping(
			quantInfo,                       /* ptr to quantization information */
			sfb_width_table[MONO_CHAN],      /* Widths of single window */
			p_spectrum,                      /* Spectral values, noninterleaved */
			SigMaskRatio,
			PsySigMaskRatio[MONO_CHAN]
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
				k +=sfb_width_table[MONO_CHAN][i];
				SigMaskRatio[i]=PsySigMaskRatio[MONO_CHAN][i];
			}
			sfb_offset[i] = k;
			extra_bits = 100; /* header bits and more ... */

		} 
	}

	extra_bits += 1;

    /* Take into account bits for TNS data */
    extra_bits += WriteTNSData(quantInfo,fixed_stream,0);    /* Count but don't write */

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
					sum += energy[MONO_CHAN][bandNum];
				}
				energy[MONO_CHAN][sfb_index] = sum;
				sfb_index++;
			}
			windowOffset += numWindowsThisGroup;
		}
    } 

	/* Compute allowed distortion */
	for(sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
		if (10*log10(energy[MONO_CHAN][sb]+1e-15)>70) {
			allowed_dist[MONO_CHAN][sb] = energy[MONO_CHAN][sb] * SigMaskRatio[sb];
//			if (allowed_dist[MONO_CHAN][sb] < ATH[sb]) {
//				printf("%d Yes\n", sb);
//				allowed_dist[MONO_CHAN][sb] = ATH[sb];
//			}
//			printf("%d\t\t%.3f\n", sb, SigMaskRatio[sb]);
		} else {
			allowed_dist[MONO_CHAN][sb] = energy[MONO_CHAN][sb] * 1.1;
		}
	}

	/** find the maximum spectral coefficient **/
	/* Bug fix, 3/10/98 CL */
	/* for(i=0; i<NUM_COEFF; i++){ */
	for(i=0; i < sfb_offset[quantInfo->nr_of_sfb]; i++){ 
		pow_spectrum[i] = (pow(ABS(p_spectrum[0][i]), 0.75));
		sign[i] = sgn(p_spectrum[0][i]);
		if ((ABS(p_spectrum[0][i])) > max_dct_line){
			max_dct_line = ABS(p_spectrum[0][i]);
		}
	}

	if (old_startsf < 40) {
		if (max_dct_line!=0.0) {
			start_com_sf = 30 + (int)(16/3 * (log(ABS(pow(max_dct_line,0.75)/MAX_QUANT)/log(2.0))));
		} else {
			start_com_sf = 40;
		}
	} else {
		start_com_sf = old_startsf;
	}
	if ((start_com_sf>200) || (start_com_sf<40) )
		start_com_sf = 40;

	/* initialize the scale_factors that aren't intensity stereo bands */
	is_info=&(ch_info->is_info);
	for(k=0; k< quantInfo -> nr_of_sfb ;k++) {
		scale_factor[k]=((is_info->is_present)&&(is_info->is_used[k])) ? scale_factor[k] : 0;
	}

	/* Mark IS bands by setting book_vector to INTENSITY_HCB */
	ptr_book_vector=quantInfo->book_vector;
	for (k=0;k<quantInfo->nr_of_sfb;k++) {
		if ((is_info->is_present)&&(is_info->is_used[k])) {
			ptr_book_vector[k] = (is_info->sign[k]) ? INTENSITY_HCB2 : INTENSITY_HCB;
		} else {
			ptr_book_vector[k] = 0;
		}
	}

	outer_loop_count = 0;

	do { // outer iteration loop

		outer_loop_count++;
		over = 0;
		sfb_overflow = 0;

		if (max_dct_line == 0.0)
			sfb_overflow = 1;

		if (outer_loop_count == 1) {
			quantizer_change = 8;
			*common_scalefac = start_com_sf;
		} else {
			quantizer_change = 2;
		}

		do { // inner iteration loop

			if(quantize(quantInfo,	p_spectrum[0], pow_spectrum, quant))
				max_bits = 1000000;
			else
				max_bits = count_bits(quantInfo, quant, output_book_vector) + extra_bits;

			quantizer_change /= 2;
			store_common_scalefac = *common_scalefac;

			if (max_bits > average_block_bits)
				*common_scalefac += quantizer_change;
			else
				*common_scalefac -= quantizer_change;
			if (*common_scalefac > 200)
				return FERROR;

			if ((quantizer_change == 1)&&(max_bits > average_block_bits))
				quantizer_change = 2;

		} while (quantizer_change != 1);

		over = calc_noise(quantInfo, p_spectrum[0], quant, requant, noise, allowed_dist[0],
			&over_noise, &tot_noise, &max_noise);

		better = quant_compare(best_over, best_tot_noise, best_over_noise,
				  best_max_noise, over, tot_noise, over_noise, max_noise);

		prev_is_scfac = 0;
		prev_scfac = store_common_scalefac;
		for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
			if (ch_info->is_info.is_used[sb]) {
				if ((scale_factor[sb] - prev_is_scfac) <= -59)
					sfb_overflow = 1;
				if ((scale_factor[sb] - prev_is_scfac) >= 59)
					sfb_overflow = 1;
				prev_is_scfac = scale_factor[sb];
			} else {
				if ((scale_factor[sb] - prev_scfac) <= -59)
					sfb_overflow = 1;
				if ((scale_factor[sb] - prev_scfac) >= 59)
					sfb_overflow = 1;
				prev_scfac = scale_factor[sb];
			}
			if (sfb_overflow == 1)
				better = 0;
		}

		if (outer_loop_count == 1)
			better = 1;

		if (better) {
			best_over = over;
			best_max_noise = max_noise;
			best_over_noise = over_noise;
			best_tot_noise = tot_noise;
			best_common_scalefac = store_common_scalefac;

			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
				best_scale_factor[sb] = scale_factor[sb];
			}
		}

		amp_over = 0;

#if 1
		noise_thresh = 0.0;
#else
		noise_thresh = -900;
		for ( sb = 0; sb < quantInfo->nr_of_sfb; sb++ )
			noise_thresh = max(1.05*noise[sb], noise_thresh);
		noise_thresh = min(noise_thresh, 0.0);
#endif

		for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
			if ((noise[sb] > noise_thresh)&&(quantInfo->book_vector[sb]!=INTENSITY_HCB)&&(quantInfo->book_vector[sb]!=INTENSITY_HCB2)) {

				amp_over++;

				allowed_dist[0][sb] *= 2;
				scale_factor[sb]++;
			}
		}

		if (sfb_overflow == 0) {
			sfb_overflow = 1;
			for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
				if (scale_factor[sb] == 0)
					sfb_overflow = 0;
			}
		}

		if ((amp_over == 0)||(over == 0)||(amp_over==quantInfo->nr_of_sfb)||(sfb_overflow))
			break;

	} while (1);

	*common_scalefac = best_common_scalefac;
	for (sb = 0; sb < quantInfo->nr_of_sfb; sb++) {
		scale_factor[sb] = best_scale_factor[sb];
	}

	old_startsf = *common_scalefac;

	quantize(quantInfo,	p_spectrum[0], pow_spectrum, quant);
	calc_noise(quantInfo, p_spectrum[0], quant, requant, noise, allowed_dist[0],
			&over_noise, &tot_noise, &max_noise);
	if (quantInfo->block_type==ONLY_SHORT_WINDOW)
		quantInfo->pulseInfo.pulse_data_present = 0;
	else
		PulseCoder(quantInfo, quant);
	count_bits(quantInfo, quant, output_book_vector);

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
	*common_scalefac = global_gain = scale_factor[0];

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
	int index = 0;
	double tmp[1024];
	int book=1;
	int group_offset=0;
	int k=0;
	int windowOffset = 0;

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
					tmp[index++] = p_spectrum[MONO_CHAN][ii+ sfb_offset[k] + 128*j +group_offset];
			}
		}
		group_offset +=  128*window_group_length[i];     
	}

	for (k=0; k<1024; k++){
		p_spectrum[MONO_CHAN][k] = tmp[k];
	}

	/* now calc the new sfb_offset table for the whole p_spectrum vector*/
	index = 0;
	sfb_offset[index++] = 0;	  
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
				//if (PsySigMaskRatio[bandNum]>worstISMR) {
					worstISMR = PsySigMaskRatio[bandNum];
				}
			}
			SigMaskRatio[k+ i* *nr_of_sfb]=worstISMR;
			sfb_offset[index] = sfb_offset[index-1] + sfb_width_table[k]*window_group_length[i] ;
			index++;
		}
		windowOffset += window_group_length[i];
	}

	*nr_of_sfb = *nr_of_sfb * num_window_groups;  /* Number interleaved bands. */

	return 0;
}

sort_book_numbers(AACQuantInfo* quantInfo,     /* Quantization information */
		  int output_book_vector[],    /* Output codebook vector, formatted for bitstream */
		  BsBitStream* fixed_stream,   /* Bitstream */
		  int write_flag)              /* Write flag: 0 count, 1 write */
{
  /*
    This function inputs the vector, 'book_vector[]', which is of length SFB_NUM_MAX,
    and contains the optimal huffman tables of each sfb.  It returns the vector, 'output_book_vector[]', which
    has it's elements formatted for the encoded bit stream.  It's syntax is:
   
    {sect_cb[0], length_segment[0], ... ,sect_cb[num_of_sections], length_segment[num_of_sections]}

    The above syntax is true, unless there is an escape sequence.  An
    escape sequence occurs when a section is longer than 2 ^ (bit_len)
    long in units of scalefactor bands.  Also, the integer returned from
    this function is the number of bits written in the bitstream, 
    'bit_count'.  

    This function supports both long and short blocks.
    */

	int i;
	int repeat_counter = 1;
	int bit_count = 0;
	int previous;
	int max, bit_len/*,sfbs*/;
	int max_sfb,g,band;

	/* Set local pointers to quantInfo elements */
	int* book_vector = quantInfo -> book_vector;
	int nr_of_sfb = quantInfo -> nr_of_sfb;

	if (quantInfo->block_type == ONLY_SHORT_WINDOW){
		max = 7;
		bit_len = 3;
	} else {  /* the block_type is a long,start, or stop window */
		max = 31;
		bit_len = 5;
	}

	/* Compute number of scalefactor bands */
	max_sfb = quantInfo->nr_of_sfb/quantInfo->num_window_groups;


	for (g=0;g<quantInfo->num_window_groups;g++) {
		band=g*max_sfb;

		repeat_counter=1;

		previous = book_vector[band];
		if (write_flag) {   
			BsPutBit(fixed_stream,book_vector[band],4);  
		}
		bit_count += 4;

		for (i=band+1;i<band+max_sfb;i++) {
			if( (book_vector[i] != previous)) {
				if (write_flag) {
					BsPutBit(fixed_stream,repeat_counter,bit_len);  
				}
				bit_count += bit_len;

				if (repeat_counter == max){  /* in case you need to terminate an escape sequence */
					if (write_flag) BsPutBit(fixed_stream,0,bit_len);  
					bit_count += bit_len;
				}
				
				if (write_flag) BsPutBit(fixed_stream,book_vector[i],4);  
				bit_count += 4;
				previous = book_vector[i];
				repeat_counter=1;
				
			}
			/* if the length of the section is longer than the amount of bits available in */
			/* the bitsream, "max", then start up an escape sequence */
			else if ((book_vector[i] == previous) && (repeat_counter == max)) { 
				if (write_flag) {
					BsPutBit(fixed_stream,repeat_counter,bit_len);  
				}
				bit_count += bit_len;
				repeat_counter = 1;
			}
			else {
				repeat_counter++;
			}
		}
		
		if (write_flag) {
			BsPutBit(fixed_stream,repeat_counter,bit_len);  
		}
		bit_count += bit_len;
		if (repeat_counter == max) {  /* special case if the last section length is an */
			/* escape sequence */
			if (write_flag) BsPutBit(fixed_stream,0,bit_len);  
			bit_count += bit_len;
		}
		

	}  /* Bottom of group iteration */

	return(bit_count);
}

int bit_search(int quant[NUM_COEFF],  /* Quantized spectral values */
               AACQuantInfo* quantInfo)        /* Quantization information */
  /*
  This function inputs a vector of quantized spectral data, quant[][], and returns a vector,
  'book_vector[]' that describes how to group together the scalefactor bands into a smaller
  number of sections.  There are SFB_NUM_MAX elements in book_vector (equal to 49 in the 
  case of long blocks and 112 for short blocks), and each element has a huffman codebook 
  number assigned to it.

  For a quick and simple algorithm, this function performs a binary
  search across the sfb's (scale factor bands).  On the first approach, it calculates the 
  needed amount of bits if every sfb were its own section and transmitted its own huffman 
  codebook value side information (equal to 9 bits for a long block, 7 for a short).  The 
  next iteration combines adjacent sfb's, and calculates the bit rate for length two sfb 
  sections.  If any wider two-sfb section requires fewer bits than the sum of the two 
  single-sfb sections (below it in the binary tree), then the wider section will be chosen.
  This process occurs until the sections are split into three uniform parts, each with an
  equal amount of sfb's contained.  

  The binary tree is stored as a two-dimensional array.  Since this tree is not full, (there
  are only 49 nodes, not 2^6 = 64), the numbering is a little complicated.  If the tree were
  full, the top node would be 1.  It's children would be 2 and 3.  But, since this tree
  is not full, the top row of three nodes are numbered {4,5,6}.  The row below it is
  {8,9,10,11,12,13}, and so on.  

  The binary tree is called bit_stats[112][3].  There are 112 total nodes (some are not
  used since it's not full).  bit_stats[x][0] holds the bit totals needed for the sfb sectioning
  strategy represented by the node x in the tree.  bit_stats[x][1] holds the optimal huffman
  codebook table that minimizes the bit rate, given the sectioning boundaries dictated by node x.
*/

{
	int i,j,k,m,n;
	int hop;
	int min_book_choice[112][3];
	int bit_stats[240][3];
	int total_bits;
	int total_bit_count;
	int levels;
	double fraction;

	/* Set local pointer to quantInfo book_vector */
	int* book_vector = quantInfo -> book_vector;

	levels = (int) ((log((double)quantInfo->nr_of_sfb)/log((double)2.0))+1);
	fraction = (pow(2,levels)+quantInfo->nr_of_sfb)/(double)(pow(2,levels)); 

//#define SLOW
#ifdef SLOW
	for(i=0;i<5;i++){
		hop = 1 << i;
#else
		hop = 1;
		i = 0;
#endif
		total_bits = noiseless_bit_count(quant,
			hop,
			min_book_choice,
			quantInfo);         /* Quantization information */

		/* load up the (not-full) binary search tree with the min_book_choice values */
		k=0;
		m=0;
		total_bit_count = 0;

		for (j=(int)(pow(2,levels-i)); j<(int)(fraction*pow(2,levels-i)); j++)
		{
			bit_stats[j][0] = min_book_choice[k][0]; /* the minimum bit cost for this section */
			bit_stats[j][1] = min_book_choice[k][1]; /* used with this huffman book number */

			if (i>0){  /* not on the lowest level, grouping more than one signle scalefactor band per section*/
				if  (bit_stats[j][0] < bit_stats[2*j][0] + bit_stats[2*j+1][0]){

					/* it is cheaper to combine surrounding sfb secionts into one larger huffman book section */
					for(n=k;n<k+hop;n++) { /* write the optimal huffman book value for the new larger section */
						if ( (book_vector[n]!=INTENSITY_HCB)&&(book_vector[n]!=INTENSITY_HCB2) ) { /* Don't merge with IS bands */
							book_vector[n] = bit_stats[j][1];
						}
					}
				} else {  /* it was cheaper to transmit the smaller huffman table sections */
					bit_stats[j][0] = bit_stats[2*j][0] + bit_stats[2*j+1][0];
				}
			} else {  /* during the first stage of the iteration, all sfb's are individual sections */
				if ( (book_vector[k]!=INTENSITY_HCB)&&(book_vector[k]!=INTENSITY_HCB2) ) {
					book_vector[k] = bit_stats[j][1];  /* initially, set all sfb's to their own optimal section table values */
				}
			}
			total_bit_count = total_bit_count +  bit_stats[j][0];
			k=k+hop;
			m++;
		}
#ifdef SLOW
	}
#endif
	/*   book_vector[k] = book_vector[k-1]; */
	return(total_bit_count);
}


int noiseless_bit_count(int quant[NUM_COEFF],
			/*int huff[13][MAXINDEX][NUMINTAB],*/
			int hop,  // hop is now always 1
			int min_book_choice[112][3],
			AACQuantInfo* quantInfo)         /* Quantization information */
{
  int i,j,k;

  /* 
     This function inputs:
     - the quantized spectral data, 'quant[][]';
     - all of the huffman codebooks, 'huff[][]';
     - the size of the sections, in scalefactor bands (SFB's), 'hop';
     - an empty matrix, min_book_choice[][] passed to it; 

     This function outputs:
     - the matrix, min_book_choice.  It is a two dimensional matrix, with its
     rows corresponding to spectral sections.  The 0th column corresponds to 
     the bits needed to code a section with 'hop' scalefactors bands wide, all using 
     the same huffman codebook.  The 1st column contains the huffman codebook number 
     that allows the minimum number of bits to be used.   

     Other notes:
     - Initally, the dynamic range is calculated for each spectral section.  The section
     can only be entropy coded with books that have an equal or greater dynamic range
     than the section's spectral data.  The exception to this is for the 11th ESC codebook.
     If the dynamic range is larger than 16, then an escape code is appended after the
     table 11 codeword which encodes the larger value explicity in a pseudo-non-uniform
     quantization method.
     
     */

	int max_sb_coeff;
	int book_choice[12][2];
	int total_bits_cost = 0;
	int offset, length, end;
	int q;
	int write_flag = 0;

	/* set local pointer to sfb_offset */
	int* sfb_offset = quantInfo->sfb_offset;
	int nr_of_sfb = quantInfo->nr_of_sfb;

	/* each section is 'hop' scalefactor bands wide */
	for (i=0; i < nr_of_sfb; i=i+hop){ 
		if ((i+hop) > nr_of_sfb)
			q = nr_of_sfb;
		else
			q = i+hop;

		{
			
			/* find the maximum absolute value in the current spectral section, to see what tables are available to use */
			max_sb_coeff = 0;
			for (j=sfb_offset[i]; j<sfb_offset[q]; j++){  /* snl */
				if (ABS(quant[j]) > max_sb_coeff)
					max_sb_coeff = ABS(quant[j]);
			}
			
			j = 0;
			offset = sfb_offset[i];
			if ((i+hop) > nr_of_sfb){
				end = sfb_offset[nr_of_sfb];
			}
			else
				end = sfb_offset[q];
			length = end - offset;

			/* all spectral coefficients in this section are zero */
			if (max_sb_coeff == 0) { 
				book_choice[j][0] = output_bits(quantInfo,0,quant,offset,length,write_flag);
				book_choice[j++][1] = 0; 

			}
			else {  /* if the section does have non-zero coefficients */
				/* Changed all the else's to else if's, big speed up. Hardly any loss in coding. */
				if(max_sb_coeff < 2){
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,1,quant,offset,length,write_flag);
					book_choice[j++][1] = 1;
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,2,quant,offset,length,write_flag);
					book_choice[j++][1] = 2;
				}
				/*else*/ if (max_sb_coeff < 3){
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,3,quant,offset,length,write_flag);
					book_choice[j++][1] = 3;
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,4,quant,offset,length,write_flag);
					book_choice[j++][1] = 4;
				}
				/*else*/ if (max_sb_coeff < 5){
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,5,quant,offset,length,write_flag);
					book_choice[j++][1] = 5;
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,6,quant,offset,length,write_flag);
					book_choice[j++][1] = 6;
				}
				/*else*/ if (max_sb_coeff < 8){
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,7,quant,offset,length,write_flag);
					book_choice[j++][1] = 7;
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,8,quant,offset,length,write_flag);
					book_choice[j++][1] = 8;
				}
				/*else*/ if (max_sb_coeff < 13){
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,9,quant,offset,length,write_flag);
					book_choice[j++][1] = 9;
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,10,quant,offset,length,write_flag);
					book_choice[j++][1] = 10;
				}
				/* (max_sb_coeff >= 13), choose table 11 */
				else {
					quantInfo->spectralCount = 0; /* just for debugging : using data and len vectors */
					book_choice[j][0] = output_bits(quantInfo,11,quant,offset,length,write_flag);
					book_choice[j++][1] = 11;
				}
			}

			/* find the minimum bit cost and table number for huffman coding this scalefactor section */
			min_book_choice[i][0] = 100000;  
			for(k=0;k<j;k++){
				if (book_choice[k][0] < min_book_choice[i][0]){
					min_book_choice[i][1] = book_choice[k][1];
					min_book_choice[i][0] = book_choice[k][0];
				}
			}
			total_bits_cost += min_book_choice[i][0];
		}
	}
	return(total_bits_cost);
}



int calculate_esc_sequence(int input,
						   int *len_esc_sequence
						   )
/* 
   This function takes an element that is larger than 16 and generates the base10 value of the 
   equivalent escape sequence.  It returns the escape sequence in the variable, 'output'.  It
   also passed the length of the escape sequence through the parameter, 'len_esc_sequence'.
*/

{
	float x,y;
	int output;
	int N;

	N = -1;
	y = (float)ABS(input);
	x = y / 16;

	while (x >= 1) {
		N++;
		x = x/2;
	}

	*len_esc_sequence = 2*N + 5;  /* the length of the escape sequence in bits */

	output = (int)((pow(2,N) - 1)*pow(2,N+5) + y - pow(2,N+4));
	return(output);
}



int output_bits(AACQuantInfo* quantInfo,
				/*int huff[13][MAXINDEX][NUMINTAB],*/
                int book,
				int quant[NUM_COEFF],
                int offset,
				int length,
				int write_flag)
{
  /* 
     This function inputs 
     - all the huffman codebooks, 'huff[]' 
     - a specific codebook number, 'book'
     - the quantized spectral data, 'quant[][]'
     - the offset into the spectral data to begin scanning, 'offset'
     - the 'length' of the segment to huffman code
     -> therefore, the segment quant[CHANNEL][offset] to quant[CHANNEL][offset+length-1]
     is huffman coded.
     - a flag, 'write_flag' to determine whether the codebooks and lengths need to be written
     to file.  If write_flag=0, then this function is being used only in the quantization
     rate loop, and does not need to spend time writing the codebooks and lengths to file.
     If write_flag=1, then it is being called by the function output_bits(), which is 
     sending the bitsteam out of the encoder.  

     This function outputs 
     - the number of bits required, 'bits'  using the prescribed codebook, book applied to 
     the given segment of spectral data.

     There are three parameters that are passed back and forth into this function.  data[]
     and len[] are one-dimensional arrays that store the codebook values and their respective
     bit lengths.  These are used when packing the data for the bitstream in output_bits().  The
     index into these arrays is 'quantInfo->spectralCount''.  It gets incremented internally in this
     function as counter, then passed to the outside through outside_counter.  The next time
     output_bits() is called, counter starts at the value it left off from the previous call.

   */
 
	int esc_sequence;
	int len_esc;
	int index;
	int bits=0;
	int tmp = 0;
	int codebook,i,j;
	int counter;

	/* Set up local pointers to quantInfo elements data and len */
	int* data= quantInfo -> data;
	int* len=  quantInfo -> len;

	counter = quantInfo->spectralCount;

	/* This case also applies to intensity stereo encoding */
	/*if (book == 0) { */ /* if using the zero codebook, data of zero length is sent */
	if ((book == 0)||(book==INTENSITY_HCB2)||(book==INTENSITY_HCB)) {  /* if using the zero codebook, 
		data of zero length is sent */
		
		if (write_flag) {
			quantInfo->data[counter] = 0;
			quantInfo->len[counter++] = 0;
		}
	}

	if ((book == 1) || (book == 2)) {
		for(i=offset;i<offset+length;i=i+4){
			index = 27*quant[i] + 9*quant[i+1] + 3*quant[i+2] + quant[i+3] + 40;
			if (book == 1) {
				codebook = huff1[index][LASTINTAB];
				tmp = huff1[index][FIRSTINTAB];
			} else {
				codebook = huff2[index][LASTINTAB];
				tmp = huff2[index][FIRSTINTAB];
			}
			bits += tmp;
			if (write_flag) {
				data[counter] = codebook;
				len[counter++] = tmp;
			}
		}
	}

	if ((book == 3) || (book == 4)) {
		for(i=offset;i<offset+length;i=i+4){
			index = 27*ABS(quant[i]) + 9*ABS(quant[i+1]) + 3*ABS(quant[i+2]) + ABS(quant[i+3]);
			if (book == 3) {
				codebook = huff3[index][LASTINTAB];
				tmp = huff3[index][FIRSTINTAB];
			} else {
				codebook = huff4[index][LASTINTAB];
				tmp = huff4[index][FIRSTINTAB];
			}
			bits = bits + tmp;
			for(j=0;j<4;j++){
				if(ABS(quant[i+j]) > 0) bits += 1; /* only for non-zero spectral coefficients */
			}
			if (write_flag) {
				data[counter] = codebook;
				len[counter++] = tmp;
				for(j=0;j<4;j++){
					if(quant[i+j] > 0) {  /* send out '0' if a positive value */
						data[counter] = 0;
						len[counter++] = 1;
					}
					if(quant[i+j] < 0) {  /* send out '1' if a negative value */
						data[counter] = 1;
						len[counter++] = 1;
					}
				}
			}
		}
	}

	if ((book == 5) || (book == 6)) {
		for(i=offset;i<offset+length;i=i+2){
			index = 9*(quant[i]) + (quant[i+1]) + 40;
			if (book == 5) {
				codebook = huff5[index][LASTINTAB];
				tmp = huff5[index][FIRSTINTAB];
			} else {
				codebook = huff6[index][LASTINTAB];
				tmp = huff6[index][FIRSTINTAB];
			}
			bits = bits + tmp;
			if (write_flag) {
				data[counter] = codebook;
				len[counter++] = tmp;
			}
		}
	}

	if ((book == 7) || (book == 8)) {
		for(i=offset;i<offset+length;i=i+2){
			index = 8*ABS(quant[i]) + ABS(quant[i+1]);
			if (book == 7) {
				codebook = huff7[index][LASTINTAB];
				tmp = huff7[index][FIRSTINTAB];
			} else {
				codebook = huff8[index][LASTINTAB];
				tmp = huff8[index][FIRSTINTAB];
			}
			bits = bits + tmp;
			for(j=0;j<2;j++){
				if(ABS(quant[i+j]) > 0) bits += 1; /* only for non-zero spectral coefficients */
			}
			if (write_flag) {
				data[counter] = codebook;
				len[counter++] = tmp;
				for(j=0;j<2;j++){
					if(quant[i+j] > 0) {  /* send out '0' if a positive value */
						data[counter] = 0;
						len[counter++] = 1;
					}
					if(quant[i+j] < 0) {  /* send out '1' if a negative value */
						data[counter] = 1;
						len[counter++] = 1;
					}
				}
			}
		}
	}

	if ((book == 9) || (book == 10)) {
		for(i=offset;i<offset+length;i=i+2){
			index = 13*ABS(quant[i]) + ABS(quant[i+1]);
			if (book == 9) {
				codebook = huff9[index][LASTINTAB];
				tmp = huff9[index][FIRSTINTAB];
			} else {
				codebook = huff10[index][LASTINTAB];
				tmp = huff10[index][FIRSTINTAB];
			}
			bits = bits + tmp;
			for(j=0;j<2;j++){
				if(ABS(quant[i+j]) > 0) bits += 1; /* only for non-zero spectral coefficients */
			}
			if (write_flag) {
				
				data[counter] = codebook;
				len[counter++] = tmp;

				for(j=0;j<2;j++){
					if(quant[i+j] > 0) {  /* send out '0' if a positive value */
						data[counter] = 0;
						len[counter++] = 1;
					}
					if(quant[i+j] < 0) {  /* send out '1' if a negative value */
						data[counter] = 1;
						len[counter++] = 1;
					}
				}
			}
		}
	}

	if ((book == 11)){
		/* First, calculate the indecies into the huffman tables */

		for(i=offset;i<offset+length;i=i+2){
			if ((ABS(quant[i]) >= 16) && (ABS(quant[i+1]) >= 16)) {  /* both codewords were above 16 */
				/* first, code the orignal pair, with the larger value saturated to +/- 16 */
				index = 17*16 + 16;
			}
			else if (ABS(quant[i]) >= 16) {  /* the first codeword was above 16, not the second one */
				/* first, code the orignal pair, with the larger value saturated to +/- 16 */
				index = 17*16 + ABS(quant[i+1]);
			}
			else if (ABS(quant[i+1]) >= 16) { /* the second codeword was above 16, not the first one */
				index = 17*ABS(quant[i]) + 16;
			}
			else {  /* there were no values above 16, so no escape sequences */
				index = 17*ABS(quant[i]) + ABS(quant[i+1]);
			}

			/* write out the codewords */

			tmp = huff11[index][FIRSTINTAB];
			codebook = huff11[index][LASTINTAB];
			bits += tmp;
			if (write_flag) {
				/*	printf("[book %d] {%d %d} \n",book,quant[i],quant[i+1]);*/
				data[counter] = codebook;
				len[counter++] = tmp;
			}
			
			/* Take care of the sign bits */

			for(j=0;j<2;j++){
				if(ABS(quant[i+j]) > 0) bits += 1; /* only for non-zero spectral coefficients */
			}
			if (write_flag) {
				for(j=0;j<2;j++){
					if(quant[i+j] > 0) {  /* send out '0' if a positive value */
						data[counter] = 0;
						len[counter++] = 1;
					}
					if(quant[i+j] < 0) {  /* send out '1' if a negative value */
						data[counter] = 1;
						len[counter++] = 1;
					}
				}
			}

			/* write out the escape sequences */

			if ((ABS(quant[i]) >= 16) && (ABS(quant[i+1]) >= 16)) {  /* both codewords were above 16 */
				/* code and transmit the first escape_sequence */
				esc_sequence = calculate_esc_sequence(quant[i],&len_esc); 
				bits += len_esc;
				if (write_flag) {
					data[counter] = esc_sequence;
					len[counter++] = len_esc;
				}

				/* then code and transmit the second escape_sequence */
				esc_sequence = calculate_esc_sequence(quant[i+1],&len_esc); 
				bits += len_esc;
				if (write_flag) {
					data[counter] = esc_sequence;
					len[counter++] = len_esc;
				}
			}

			else if (ABS(quant[i]) >= 16) {  /* the first codeword was above 16, not the second one */
				/* code and transmit the escape_sequence */
				esc_sequence = calculate_esc_sequence(quant[i],&len_esc); 
				bits += len_esc;
				if (write_flag) {
					data[counter] = esc_sequence;
					len[counter++] = len_esc;
				}
			}

			else if (ABS(quant[i+1]) >= 16) { /* the second codeword was above 16, not the first one */
				/* code and transmit the escape_sequence */
				esc_sequence = calculate_esc_sequence(quant[i+1],&len_esc); 
				bits += len_esc;
				if (write_flag) {
					data[counter] = esc_sequence;
					len[counter++] = len_esc;
				}
			} 
		}
	}

	quantInfo -> spectralCount = counter;  /* send the current count back to the outside world */

	return(bits);
}


int find_grouping_bits(int window_group_length[],
					   int num_window_groups
					   )
{

  /* This function inputs the grouping information and outputs the seven bit 
	'grouping_bits' field that the NBC decoder expects.  */


	int grouping_bits = 0;
	int tmp[8];
	int i,j;
	int index=0;

	for(i=0; i<num_window_groups; i++){
		for (j=0; j<window_group_length[i];j++){
			tmp[index++] = i;
		}
	}

	for(i=1; i<8; i++){
		grouping_bits = grouping_bits << 1;
		if(tmp[i] == tmp[i-1]) {
			grouping_bits++;
		}
	}
	
	return(grouping_bits);
}



int write_scalefactor_bitstream(BsBitStream* fixed_stream,             /* Bitstream */  
				int write_flag,                        /* Write flag */
				AACQuantInfo* quantInfo)               /* Quantization information */
{
	/* this function takes care of counting the number of bits necessary */
	/* to encode the scalefactors.  In addition, if the write_flag == 1, */
	/* then the scalefactors are written out the fixed_stream output bit */
	/* stream.  it returns k, the number of bits written to the bitstream*/

	int i,j,k=0;
	int diff,length,codeword;
	int previous_scale_factor;
	int previous_is_factor;       /* Intensity stereo */
	int index = 0;
	int count = 0;
	int group_offset = 0;
	int nr_of_sfb_per_group=0;

	/* set local pointer to quantInfo elements */
	int* scale_factors = quantInfo->scale_factor;
	
	if (quantInfo->block_type == ONLY_SHORT_WINDOW) { /* short windows */
		nr_of_sfb_per_group = quantInfo->nr_of_sfb/quantInfo->num_window_groups;
	}
	else {
		nr_of_sfb_per_group = quantInfo->nr_of_sfb;
		quantInfo->num_window_groups = 1;
		quantInfo->window_group_length[0] = 1;
	}

	previous_scale_factor = quantInfo->common_scalefac;
	previous_is_factor = 0;
    
	for(j=0; j<quantInfo->num_window_groups; j++){
		for(i=0;i<nr_of_sfb_per_group;i++) {  
			/* test to see if any codebooks in a group are zero */
			if ( (quantInfo->book_vector[index]==INTENSITY_HCB) ||
				(quantInfo->book_vector[index]==INTENSITY_HCB2) ) {
				/* only send scalefactors if using non-zero codebooks */
				diff = scale_factors[index] - previous_is_factor;
				if ((diff < 60)&&(diff >= -60))
					length = huff12[diff+60][FIRSTINTAB];
				else length = 0;
				k+=length;
				previous_is_factor = scale_factors[index];
				if (write_flag == 1 ) {   
					codeword = huff12[diff+60][LASTINTAB];
					BsPutBit(fixed_stream,codeword,length); 
				}
			} else if (quantInfo->book_vector[index]) {
				/* only send scalefactors if using non-zero codebooks */
				diff = scale_factors[index] - previous_scale_factor;
				if ((diff < 60)&&(diff >= -60))
					length = huff12[diff+60][FIRSTINTAB];
				else length = 0;
				k+=length;
				previous_scale_factor = scale_factors[index];
				if (write_flag == 1 ) {   
					codeword = huff12[diff+60][LASTINTAB];
					BsPutBit(fixed_stream,codeword,length); 
				}
			}
			index++;
		}
	}
	return(k);
}

