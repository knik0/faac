/*
 *	Internal definitions for LTP
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

#ifndef _NOK_LTP_COMMON_INTERNAL_H
#define _NOK_LTP_COMMON_INTERNAL_H


/*
  Purpose:      Number of LTP coefficients. */
#define LPC 1

/*
  Purpose:      Maximum LTP lag.  */
#define DELAY 2048

/*
  Purpose:	Length of the bitstream element ltp_data_present.  */
#define	LEN_LTP_DATA_PRESENT 1

/*
  Purpose:	Length of the bitstream element ltp_lag.  */
#define	LEN_LTP_LAG 11

/*
  Purpose:	Length of the bitstream element ltp_coef.  */
#define	LEN_LTP_COEF 3

/*
  Purpose:	Length of the bitstream element ltp_short_used.  */
#define	LEN_LTP_SHORT_USED 1

/*
  Purpose:	Length of the bitstream element ltp_short_lag_present.  */
#define	LEN_LTP_SHORT_LAG_PRESENT 1

/*
  Purpose:	Length of the bitstream element ltp_short_lag.  */
#define	LEN_LTP_SHORT_LAG 5

/*
  Purpose:	Offset of the lags written in the bitstream.  */
#define	NOK_LTP_LAG_OFFSET 16

/*
  Purpose:	Length of the bitstream element ltp_long_used.  */
#define	LEN_LTP_LONG_USED 1

/*
  Purpose:	Upper limit for the number of scalefactor bands
   		which can use lt prediction with long windows.
  Explanation:	Bands 0..NOK_MAX_LT_PRED_SFB-1 can use lt prediction.  */
#define	NOK_MAX_LT_PRED_LONG_SFB 40

/*
  Purpose:	Upper limit for the number of scalefactor bands
   		which can use lt prediction with short windows.
  Explanation:	Bands 0..NOK_MAX_LT_PRED_SFB-1 can use lt prediction.  */
#define	NOK_MAX_LT_PRED_SHORT_SFB 13

/*
   Purpose:      Buffer offset to maintain block alignment.
   Explanation:  This is only used for a short window sequence.  */
#define SHORT_SQ_OFFSET (BLOCK_LEN_LONG-(BLOCK_LEN_SHORT*4+BLOCK_LEN_SHORT/2))

/*
  Purpose:	Number of codes for LTP weight. */
#define CODESIZE 8

/*
   Purpose:      Float type for external data
   Explanation:  - */
typedef double float_ext;


#endif /* _NOK_LTP_COMMON_INTERNAL_H */
