/*
 *	Function prototypes for multichannel configuration
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
  $Date: 2000/11/01 14:05:32 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef MC_ENC
#define MC_ENC

#include "interface.h"

void DetermineChInfo(struct _AACQuantInfo *quantInfo, int numChannels, int lfePresent);


#endif


