/*
** Copyright (C) 1999-2000 Erik de Castro Lopo <erikd@zip.com.au>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include	<stdio.h>
#include	<math.h>

#include	<sndfile.h>

#define		SINE_LENGTH		(4096)

#ifndef M_PI
	#define M_PI 3.14159
#endif

int main (void)
{	SNDFILE	*file ;
	SF_INFO	sfinfo ;
	int		k ;
	double	val ;
	
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	sfinfo.samplerate  = 8000 ;
	sfinfo.samples     = SINE_LENGTH ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.channels	   = 1 ;
	sfinfo.format      = (SF_FORMAT_SVX | SF_FORMAT_PCM) ;

	if (! (file = sf_open_write ("sine.svx", &sfinfo)))
	{	printf ("Error : Not able to open output file.\n") ;
		return 1 ;
		} ;
		
	for (k = 0 ; k < SINE_LENGTH ; k++)
	{	val = 32000 * sin (2.0 * M_PI * ((double) k) / ((double) SINE_LENGTH)) ;
		if (sf_write_double (file, &val, 1, 0) != 1)
			sf_perror (file) ;
		} ;

	sf_close (file) ;
	return	 0 ;
} /* main */
