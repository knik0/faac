/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: backpred.h,v 1.1 2001/05/02 05:40:16 menno Exp $
 */

#ifndef _AAC_BACK_H_INCLUDED
#define _AAC_BACK_H_INCLUDED

#define PRED_ALPHA	0.90625
#define PRED_A		0.953125
#define PRED_B		0.953125

#define         FLEN_LONG              2048
#define         SBMAX_L                49
#define         LPC                    2
#define         ALPHA                  PRED_ALPHA
#define         A                      PRED_A
#define         B                      PRED_B
#define         MINVAR                 1.e-10

/* Reset every RESET_FRAME frames. */
#define		RESET_FRAME	       8 
 
int GetMaxPredSfb(int samplingRateIdx);

void PredCalcPrediction( double *act_spec, 
			 double *last_spec, 
			 int btype, 
			 int nsfb, 
			 int *isfb_width,
			 CoderInfo *coderInfo,
			 ChannelInfo *channelInfo,
			 int chanNum); 

void PredInit();

void CopyPredInfo(CoderInfo *right, CoderInfo *left);


#endif
