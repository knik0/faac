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
 * ADD   Alberto Duenas, NDS <alberto@ndsuk.com>
 * JB    Jeremy Bennett, NDS <jbennett@ndsuk.com>
 *
 * Changes:
 * 07-jun-97   LY   Initial revision.
 * 14-sep-97   CL   Updated ALPHA, A, and B coefficients.
 * 03-dec-97   ADD & JB   Added prediction reset.
 *
**********************************************************************/

#ifndef _AAC_BACK_H_INCLUDED
#define _AAC_BACK_H_INCLUDED

#include <math.h>
#include "interface.h"
#include "all.h"

#define         FLEN_LONG              2048
#define         SBMAX_L                49
#define         LPC                    2
#define         ALPHA                  PRED_ALPHA
#define         A                      PRED_A
#define         B                      PRED_B
#define         MINVAR                 1.e-10

/* Reset every RESET_FRAME frames. */
#define		RESET_FRAME 8
 
void PredCalcPrediction( double *act_spec, 
			 double *last_spec, 
			 int btype, 
			 int nsfb, 
			 int *isfb_width, 
			 short *pred_global_flag, 
			 int *pred_sfb_flag,
			 int *reset_group,
			 int chanNum); 

void PredInit(void);

#endif

