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
#include	<string.h>
#include	<unistd.h>

#include	<sndfile.h>

extern int	sf_error_number	(int errnum, char *str, size_t maxlen) ;

#define	BUFFER_SIZE		(1<<15)
#define	SHORT_BUFFER	(256)

static	char	strbuffer [BUFFER_SIZE] ;
static	char	noerror   [SHORT_BUFFER] ;

int		main (int argc, char *argv[])
{	int		k ;

	sf_error_number (0, noerror, SHORT_BUFFER) ;
		
	printf ("Testing to see if all internal error numbers have corresponding error messages :\n") ;

	for (k = 1 ; k < 1000 ; k++)
	{	sf_error_number (k, strbuffer, BUFFER_SIZE) ;
		printf ("\t%3d : %s\n", k, strbuffer) ;
		if (! strcmp (strbuffer, noerror))
			break ;
		if (strstr (strbuffer, "This is a bug in libsndfile."))
			return 1 ;
		} ;

	return 0 ;
} /* main */

