/*
 *	Common definitions for LTP
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

#ifndef _NOK_LTP_COMMON_H
#define _NOK_LTP_COMMON_H

/*
  Macro:	MAX_SHORT_WINDOWS
  Purpose:	Number of short windows in one long window.
  Explanation:	-  */
#ifndef MAX_SHORT_WINDOWS
#define MAX_SHORT_WINDOWS NSHORT
#endif

/*
  Macro:	MAX_SCFAC_BANDS
  Purpose:	Maximum number of scalefactor bands in one frame.
  Explanation:	-  */
#ifndef MAX_SCFAC_BANDS
#define MAX_SCFAC_BANDS MAXBANDS
#endif

/*
  Macro:	NOK_LT_BLEN
  Purpose:	Length of the history buffer.
  Explanation:	Has to hold two long windows of time domain data.  */
#ifndef	NOK_LT_BLEN
#define NOK_LT_BLEN (4 * BLOCK_LEN_LONG)
#endif

/*
  Type:		NOK_LT_PRED_STATUS
  Purpose:	Type of the struct holding the LTP encoding parameters.
  Explanation:	-  */
typedef struct
  {
    short buffer[NOK_LT_BLEN];
    double pred_mdct[2 * BLOCK_LEN_LONG];
    int weight_idx;
    double weight;
    int sbk_prediction_used[MAX_SHORT_WINDOWS];
    int sfb_prediction_used[MAX_SCFAC_BANDS];
    int *delay;
    int global_pred_flag;
    int side_info;
  }
NOK_LT_PRED_STATUS;


#endif /* _NOK_LTP_COMMON_H */
