/*
 *	Function prototypes for LTP
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

#ifndef _NOK_LTP_ENC_H
#define _NOK_LTP_ENC_H

#include "nok_ltp_common.h"

extern void nok_init_lt_pred (NOK_LT_PRED_STATUS * lt_status);

extern int nok_ltp_enc(double *p_spectrum, double *p_time_signal,
		       enum WINDOW_TYPE win_type, Window_shape win_shape,
		       int *sfb_offset, int num_of_sfb,
		       NOK_LT_PRED_STATUS *lt_status);

extern void nok_ltp_reconstruct(double *p_spectrum, enum WINDOW_TYPE win_type, 
                                Window_shape win_shape, 
                                int *sfb_offset, int num_of_sfb,
                                NOK_LT_PRED_STATUS *lt_status);

extern int nok_ltp_encode (BsBitStream *bs, enum WINDOW_TYPE win_type, int num_of_sfb, 
                           NOK_LT_PRED_STATUS *lt_status, int write_flag);

#endif /* not defined _NOK_LTP_ENC_H */

