/*
 *	Function prototypes for frame encoding
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
  $Revision: 1.7 $
  $Date: 2000/10/05 13:04:05 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef _enc_h_
#define _enc_h_


#include "bitstream.h"		/* bit stream module */

/* ---------- functions ---------- */

#ifdef __cplusplus
extern "C" {
#endif


/* EncTfInit() */
/* Init t/f-based encoder core. */

void EncTfInit (faacAACStream *as);



/* EncTfFrame() */
/* Encode one audio frame into one bit stream frame with */
/* t/f-based encoder core. */

int EncTfFrame (faacAACStream *as, BsBitStream *bitBuf);


/* EncTfFree() */
/* Free memory allocated by t/f-based encoder core. */

void EncTfFree (void);


#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _enc_h_ */

/* end of enc.h */

