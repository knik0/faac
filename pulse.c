/*
 *	Pulse Coding
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

#include <math.h>

#include "quant.h"
#include "huffman.h"

#define STARTSFB 16 // approx. 2kHz

void PulseCoder(AACQuantInfo *quantInfo, int *quant)
{
	int i, j, sb, k;
	int pulses = 0;
	pulse pul_arr[4];

#if 1
	for (sb = STARTSFB; sb < quantInfo->nr_of_sfb; sb++)
	{
		for (i = quantInfo->sfb_offset[sb]; i < quantInfo->sfb_offset[sb+1]; i++)
		{
			if ((i != 0)&&(i<quantInfo->sfb_offset[quantInfo->nr_of_sfb]))
			{
				/* This is a peak */
				if ((abs(quant[i]) > abs(quant[i-1])+4)&&(abs(quant[i]) > abs(quant[i+1])+4))
				{
					pulses++;
					if (pulses == 1) {
						j = i;
						pul_arr[pulses-1].offset = i-quantInfo->sfb_offset[sb];
						pul_arr[pulses-1].sfb = sb;
						pul_arr[pulses-1].amp = min(15,(int)(abs((quant[i-1])+abs(quant[i+1]))/2+0.5));
						if (pul_arr[pulses-1].offset >= 32) {
							pulses = 0;
						}
					} else {
						pul_arr[pulses-1].offset = i-j;
						j = i;
						pul_arr[pulses-1].amp = min(15,(int)((abs(quant[i-1])+abs(quant[i+1]))/2+0.5));
						if (pul_arr[pulses-1].offset >= 32) {
							pulses--;
							goto end;
						}
					}
				}
				if (pulses == 4)
					goto end;
			}
		}
	}
#endif

end:
	if (pulses) {
		quantInfo->pulseInfo.pulse_data_present = 1;
		quantInfo->pulseInfo.number_pulse = pulses-1;
		quantInfo->pulseInfo.pulse_start_sfb = pul_arr[0].sfb;
		k = quantInfo->sfb_offset[pul_arr[0].sfb];

		for (i = 0; i < pulses; i++) {
			quantInfo->pulseInfo.pulse_offset[i] = pul_arr[i].offset;
			quantInfo->pulseInfo.pulse_amp[i] = pul_arr[i].amp;

			k += pul_arr[i].offset;
			if (quant[k] > 0)
				quant[k] -= pul_arr[i].amp;
			else
				quant[k] += pul_arr[i].amp;
		}
	} else {
		quantInfo->pulseInfo.pulse_data_present = 0;
	}
}

void PulseDecoder(AACQuantInfo *quantInfo, int *quant)
{
	int i, offset;

	if (quantInfo->pulseInfo.pulse_data_present)
	{
		offset = quantInfo->sfb_offset[quantInfo->pulseInfo.pulse_start_sfb];
		for(i = 0; i < quantInfo->pulseInfo.number_pulse+1; i++) {
			offset += quantInfo->pulseInfo.pulse_offset[i];
			if (quant[offset] > 0)
				quant[offset] -= quantInfo->pulseInfo.pulse_amp[i];
			else
				quant[offset] += quantInfo->pulseInfo.pulse_amp[i];
		}
	}
}
