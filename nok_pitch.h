/*
 *	Function prototypes for LTP functions
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
  $Revision: 1.3 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef NOK_PITCH_H_
#define NOK_PITCH_H_


extern double snr_pred (double *mdct_in, double *mdct_pred, int *sfb_flag,
                        int *sfb_offset, enum WINDOW_TYPE block_type, int side_info,
                        int num_of_sfb);

extern void prediction (short *buffer, double *predicted_samples, double *weight,
                        int delay, int flen);

//extern int estimate_delay (double *sb_samples, short *x_buffer, int flen);
extern int estimate_delay (double *sb_samples, short *x_buffer);

extern void pitch (double *sb_samples, double *sb_samples_pred, short *x_buffer,
                   int *ltp_coef, int delay, int flen);


#endif
