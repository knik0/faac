/*
 *	Function prototypes for block handling
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
  $Revision: 1.5 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef BLOCK_H
#define BLOCK_H 1

#define IN_DATATYPE  double
#define OUT_DATATYPE double

#define BLOCK_LEN_LONG	   1024
#define BLOCK_LEN_SHORT    128

#define NWINLONG	(BLOCK_LEN_LONG)
#define ALFALONG	4.0
#define NWINSHORT	(BLOCK_LEN_SHORT)
#define ALFASHORT	7.0

#define	NWINFLAT	(NWINLONG)					/* flat params */
#define	NWINADV		(NWINLONG-NWINSHORT)		/* Advanced flat params */
#define NFLAT		((NWINFLAT-NWINSHORT)/2)
#define NADV0		((NWINADV-NWINSHORT)/2)


typedef enum {
    WS_SIN, WS_KBD, N_WINDOW_SHAPES
} 
Window_shape;

/* YT 970615 for Son_PP  */
typedef enum {
	MOVERLAPPED,
	MNON_OVERLAPPED
}
Mdct_in,Imdct_out;

typedef enum {
    WT_LONG, 
    WT_SHORT, 
    WT_FLAT, 
    WT_ADV,			/* Advanced flat window */
    N_WINDOW_TYPES
} 
WINDOW_TYPE_AAC; 

typedef enum {                  /* ADVanced transform types */
    LONG_BLOCK,
    START_BLOCK,
    SHORT_BLOCK,
    STOP_BLOCK,
    START_ADV_BLOCK,
    STOP_ADV_BLOCK,
    START_FLAT_BLOCK,
    STOP_FLAT_BLOCK,
    N_BLOCK_TYPES
} 
BLOCK_TYPE;

typedef enum {  		/* Advanced window sequence (frame) types */
    ONLY_LONG,
    LONG_START, 
    LONG_STOP,
    SHORT_START, 
    SHORT_STOP,
    EIGHT_SHORT, 
    SHORT_EXT_STOP,
    NINE_SHORT,
    OLD_START,
    OLD_STOP,
    N_WINDOW_SEQUENCES
} 
WINDOW_SEQUENCE;


#endif	/*	BLOCK_H	*/

