/*
 *	Function prototypes for MS stereo coding
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
  $Revision: 1.11 $
  $Date: 2000/11/01 14:05:32 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef MS_ENC
#define MS_ENC

#include "quant.h"


void MSPreprocess(double p_ratio_long[][MAX_SCFAC_BANDS],
				  double p_ratio_short[][MAX_SCFAC_BANDS],
				  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
				  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS],
				  enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
				  AACQuantInfo* quantInfo,               /* Quant info */
				  int use_ms,
				  int numberOfChannels
				  );

void MSEnergy(double *spectral_line_vector[MAX_TIME_CHANNELS],
			  double energy[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
			  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS],
			  int sfb_width_table[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			  enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
			  AACQuantInfo* quantInfo,               /* Quant info */
			  int use_ms,
			  int numberOfChannels
			  );
			  
void MSEncodeSwitch(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
	      int sfb_offset_table[][MAX_SCFAC_BANDS+1],
	      AACQuantInfo* quantInfo,
	      int numberOfChannels
              );                 /* Number of channels */

void MSEncode(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
	      int sfb_offset_table[][MAX_SCFAC_BANDS+1],
	      enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
	      AACQuantInfo* quantInfo,
	      int numberOfChannels);                 /* Number of channels */

void MSReconstruct(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
		   int sfb_offset_table[][MAX_SCFAC_BANDS+1],
//		   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
		   AACQuantInfo* quantInfo,               /* Quant info */
		   int numberOfChannels);                 /* Number of channels */

#endif

