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
#include	<ctype.h>

#include	<sndfile.h>

#define	 BUFFER_LEN      4096

static
void	print_usage (char *progname)
{	printf ("\nUsage : %s <file>\n", progname) ;
	printf ("\n") ;
} /* print_usage */

int     main (int argc, char *argv[])
{	static	char	strbuffer [BUFFER_LEN] ;
	unsigned int	linecount ;
	char 		*progname, *infilename ;
	SNDFILE	 	*infile ;
	SF_INFO	 	sfinfo ;
	int			k, start, readcount ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;
		
	if (argc != 2)
	{	print_usage (progname) ;
		return  1 ;
		} ;
		
	infilename = argv [1] ;
		
	if (! (infile = sf_open_read (infilename, &sfinfo)))
	{	printf ("Error : Not able to open input file %s.\n", infilename) ;
		sf_perror (NULL) ;
		sf_get_header_info (NULL, strbuffer, BUFFER_LEN, 0) ;
		printf (strbuffer) ;
		return  1 ;
		} ;
		
	start = 0 ;
	
	linecount = 24 ;
	
	while ((readcount = sf_read_raw (infile, strbuffer, linecount)))
	{	printf ("%08X: ", start) ;
		for (k = 0 ; k < readcount ; k++)
			printf ("%02X ", strbuffer [k] & 0xFF) ;
		for (k = readcount ; k < 16 ; k++)
			printf ("   ") ;
		printf ("\n") ;
		start += readcount ;
		} ;

	sf_close (infile) ;
	
	return 0 ;
} /* main */

