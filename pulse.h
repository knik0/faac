/*
 *	Function prototypes for pulse coding
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
  $Date: 2000/10/05 08:39:03 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef PULSE_H_
#define PULSE_H_

#include "interface.h"
//#define LEN_NEC_NPULSE	2
//#define LEN_NEC_ST_SFB	6
//#define LEN_NEC_POFF	5
//#define LEN_NEC_PAMP	4
//#define NUM_NEC_LINES	4
//#define NEC_OFFSET_AMP	4

typedef struct
{
    int pulse_data_present;
    int number_pulse;
    int pulse_start_sfb;
    int pulse_position[NUM_NEC_LINES];
    int pulse_offset[NUM_NEC_LINES];
    int pulse_amp[NUM_NEC_LINES];
} AACPulseInfo;

typedef struct {
	int offset;
	int sfb;
	int amp;
} pulse;


#endif
