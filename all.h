/*
 *	General declarations
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
  $Revision: 1.6 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef	_all_h_
#define _all_h_

#include "interface.h"
#include "tns.h"
#include "bitstream.h"

typedef struct
{
    int is_present;  
    int ms_used[MAXBANDS];
} MS_Info;

typedef struct
{
    int present;	/* channel present */
    int tag;		/* element tag */
    int cpe;		/* 0 if single channel or lfe, 1 if channel pair */ 
    int lfe;            /* 1 if lfe channel */             
    int	common_window;	/* 1 if common window for cpe */
    int	ch_is_left;	/* 1 if left channel of cpe */
    int	paired_ch;	/* index of paired channel in cpe */
    int widx;		/* window element index for this channel */
    MS_Info ms_info;    /* MS information */
} Ch_Info;

#endif	/* _all_h_ */

