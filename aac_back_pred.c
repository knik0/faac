/**********************************************************************

This software module was originally developed by
and edited by Nokia in the course of 
development of the MPEG-2 NBC/MPEG-4 Audio standard
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

Copyright (c) 1997.
**********************************************************************/
/*********************************************************************
 *
 * aac_back_pred.c  -  AAC backward prediction subroutine
 *
 * Authors:
 * LY    Lin Yin, Nokia Research Center
 * CL    Chuck Lueck, TI <lueck@ti.com>
 * ADD   Alberto Duenas, NDS <alberto@ndsuk.com)
 * JB    Jeremy Bennett, NDS <jbennett@ndsuk.com>
 *
 * Changes:
 * 07-jun-97   LY   Initial revision.
 * 14-sep-97   CL   Made predicted samples array, sb_samples_pred, static.
 *                  Modified line 98 to add these predicted values to
 *                  the last spectrum prior to analysis.
 * 03-dec-97   ADD & JB   Added prediction reset.
 *
**********************************************************************/

/**********************************************************************

INPUT:
act_spec[]: the current frame's spectral components to be encoded.
last_spec[]: the previous frame's quantized spectral error components 
btype: the current frame's window type
nsfb: the number of scalefactor bands
isfb_width[]: scalefactor bandwidth

OUTPUT:
act_spec[]: the current frame's spectral error components to be encoded
pred_global_flag: global prediction enable/disable flag
pred_sbf_glag[]: enable/disable flag for each scalefactor band
reset_group: number for the reset group => if -1, then no reset
***********************************************************************/

#include "aac_back_pred.h"


int psy_init_mc[MAX_TIME_CHANNELS];
double dr_mc[MAX_TIME_CHANNELS][LPC+1][FLEN_LONG/2],e_mc[MAX_TIME_CHANNELS][LPC+1+1][FLEN_LONG/2];
double K_mc[MAX_TIME_CHANNELS][LPC+1][FLEN_LONG/2], R_mc[MAX_TIME_CHANNELS][LPC+1][FLEN_LONG/2];
double VAR_mc[MAX_TIME_CHANNELS][LPC+1][FLEN_LONG/2], KOR_mc[MAX_TIME_CHANNELS][LPC+1][FLEN_LONG/2];
double sb_samples_pred_mc[MAX_TIME_CHANNELS][FLEN_LONG/2];
int thisLineNeedsResetting_mc[MAX_TIME_CHANNELS][FLEN_LONG/2];
int reset_count_mc[MAX_TIME_CHANNELS];

void PredInit(void)
{
	int i;
	for (i=0;i<MAX_TIME_CHANNELS;i++) {
		psy_init_mc[i] = 0;
		reset_count_mc[i] = 0;
	}
}
 
void PredCalcPrediction( double *act_spec, double *last_spec, int btype, 
						int nsfb, int *isfb_width, short *pred_global_flag, 
						int *pred_sfb_flag, int *reset_group ,
						int chanNum) 
{
	int i, k, j, cb_long;
	double num_bit, snr[SBMAX_L];
	double energy[FLEN_LONG/2], snr_p[FLEN_LONG/2], temp1, temp2;

	/* Set pointers for specified channel number */
	/* int psy_init; */
	int *psy_init;
	double (*dr)[FLEN_LONG/2],(*e)[FLEN_LONG/2];
	double (*K)[FLEN_LONG/2], (*R)[FLEN_LONG/2];
	double (*VAR)[FLEN_LONG/2], (*KOR)[FLEN_LONG/2];
	double *sb_samples_pred;
	int *thisLineNeedsResetting;
	/* int reset_count; */
	int *reset_count;

	/* psy_init = psy_init_mc[chanNum]; */
	psy_init = &psy_init_mc[chanNum];
	dr = &dr_mc[chanNum][0];
	e = &e_mc[chanNum][0];
	K = &K_mc[chanNum][0]; 
	R = &R_mc[chanNum][0];
	VAR = &VAR_mc[chanNum][0];
	KOR = &KOR_mc[chanNum][0];
	sb_samples_pred = &sb_samples_pred_mc[chanNum][0];
	thisLineNeedsResetting = &thisLineNeedsResetting_mc[chanNum][0];
	reset_count = &reset_count_mc[chanNum];

	*psy_init = (*psy_init && (btype!=2));

	if((*psy_init) == 0) {
		for (j=0; j<FLEN_LONG/2; j++) {
			thisLineNeedsResetting[j]=1;
		}
		*psy_init = 1;
	}

	if (btype==2) {
		pred_global_flag[0]=0;

		/* As SHORT WINDOWS reset all the co-efficients, the count
		* must also be reset.
		*/
		*reset_count = ((*reset_count) / RESET_FRAME) * RESET_FRAME;
		return;
	}



	/**************************************************/
	/*  Compute state using last_spec                 */
	/**************************************************/
	for (i=0;i<FLEN_LONG/2;i++) 
    {
		/* e[0][i]=last_spec[i]; */ 
		e[0][i]=last_spec[i]+sb_samples_pred[i];
		
		for(j=1;j<=LPC;j++)
			e[j][i] = e[j-1][i]-K[j][i]*R[j-1][i];
		
		for(j=1;j<LPC;j++) 
			dr[j][i] = K[j][i]*e[j-1][i];
		
		for(j=1;j<=LPC;j++) {
			VAR[j][i] = ALPHA*VAR[j][i]+.5*(R[j-1][i]*R[j-1][i]+e[j-1][i]*e[j-1][i]);
			KOR[j][i] = ALPHA*KOR[j][i]+R[j-1][i]*e[j-1][i];
		}

		for(j=LPC-1;j>=1;j--) 
			R[j][i] = A*(R[j-1][i]-dr[j][i]);
		R[0][i] = A*e[0][i];
    }


	/**************************************************/
	/* Reset state here if resets were sent           */
	/**************************************************/
	for (i=0;i<FLEN_LONG/2;i++) {
		if (thisLineNeedsResetting[i]) {
			for (j = 0; j <= LPC; j++)
			{
				K[j][i] = 0.0;
				dr[j][i] = 0.0;
				e[j][i] = 0.0;
				R[j][i] = 0.0;
				VAR[j][i] = 1.0;
				KOR[j][i] = 0.0;
			}
		}
	}


	/**************************************************/
	/* Compute predictor coefficients, predicted data */
	/**************************************************/
	for (i=0;i<FLEN_LONG/2;i++) 
    {
		for(j=1;j<=LPC;j++) {
			if(VAR[j][i]>MINVAR)
				K[j][i] = KOR[j][i]/VAR[j][i]*B;
			else
				K[j][i] = 0;
		}
    }


	for (k=0; k<FLEN_LONG/2; k++)
    {
		sb_samples_pred[k]=0.0;
		for (i=1; i<=LPC; i++)
			sb_samples_pred[k]+=K[i][k]*R[i-1][k];
    }



	/**************************************************/
	/* Determine whether to enable/disable prediction */
	/**************************************************/

	for (k=0; k<FLEN_LONG/2; k++)
    {
		energy[k]=act_spec[k]*act_spec[k];
		snr_p[k]=(act_spec[k]-sb_samples_pred[k])*(act_spec[k]-sb_samples_pred[k]);
	}
	
	cb_long=0;
	for (i=0; i<nsfb; i++)
    {
		pred_sfb_flag[i]=1;
		temp1=0.0;
		temp2=0.0;
		for (j=cb_long; j<cb_long+isfb_width[i]; j++)
		{
			temp1+=energy[j];
			temp2+=snr_p[j];
		}
		if(temp2<1.e-20)
			temp2=1.e-20;
		if(temp1!=0.0) {
			snr[i]=-10.*log10((double ) temp2/temp1);
		} else
			snr[i]=0.0;
		if(snr[i]<=0.0) {
			pred_sfb_flag[i]=0; 
			for (j=cb_long; j<cb_long+isfb_width[i]; j++)
				sb_samples_pred[j]=0.0;
		}
		cb_long+=isfb_width[i];
    }

	/* Disable prediction for bands nsfb through SBMAX_L */ 
	for (i=cb_long;i<FLEN_LONG/2;i++) {
		sb_samples_pred[i]=0.0;
	}
	for (i=nsfb;i<SBMAX_L;i++) {
		pred_sfb_flag[i]=0;
	}

	num_bit=0.0;
	for (i=0; i<nsfb; i++)
		if(snr[i]>0.0)
			num_bit+=snr[i]/6.*isfb_width[i];	

	pred_global_flag[0]=1;
	if(num_bit < 50) {
		pred_global_flag[0]=0; num_bit=0.0; 
		for (j=0; j<FLEN_LONG/2; j++)
			sb_samples_pred[j]=0.0; 
	}
	for (j=0; j<FLEN_LONG/2; j++)
		act_spec[j]-=sb_samples_pred[j];

	/* Code segment added by JB */

	/* Determine whether a prediction reset is required - if so, then
	* set reset flag for the appropriate group.
	*/
	(*reset_count)++;
	/* Reset the frame counter */
	for (i=0;i<FLEN_LONG/2;i++) {
		thisLineNeedsResetting[i]=0;
	}
	if (*reset_count >= 31 * RESET_FRAME)
		*reset_count = RESET_FRAME;
	if (*reset_count % RESET_FRAME == 0)
	{ /* Send a reset in this frame */
		*reset_group = *reset_count / 8;

		for (i = *reset_group - 1; i < FLEN_LONG / 2; i += 30)
		{
			thisLineNeedsResetting[i]=1;
		}
	}
	else
		*reset_group = -1;
	/* End of code segment */


	/* Code segment added by JB */

	/* Ensure that prediction data is sent when there is a prediction
	* reset.
	*/
	if (*reset_group != -1 && pred_global_flag[0] == 0)
	{
		pred_global_flag[0] = 1;
		for (i = 0; i < nsfb; i++)
			pred_sfb_flag[i] = 0;
	}
	/* End of code segment */
}
