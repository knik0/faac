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
{	char buffer [256] ;
	
	sf_get_lib_version (buffer, 256) ;
	printf ("libsndfile version : %s\n\n", buffer) ;
	printf ("\nUsage : %s <file>\n", progname) ;
	printf ("\n        Prints out information about a sound file.\n") ;
	printf ("\n") ;
} /* print_usage */

int     main (int argc, char *argv[])
{	static	char	strbuffer [BUFFER_LEN] ;
	char 		*progname, *infilename ;
	SNDFILE	 	*infile ;
	SF_INFO	 	sfinfo ;
	int			k ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;
		
	if (argc < 2)
	{	print_usage (progname) ;
		return  1 ;
		} ;
		
	for (k = 1 ; k < argc ; k++)	
	{	infilename = argv [k] ;
		
		infile = sf_open_read (infilename, &sfinfo) ;
	
		sf_get_header_info (infile, strbuffer, BUFFER_LEN, 0) ;
		puts (strbuffer) ;

		printf ("--------------------------------\n") ;
		
		if (infile)
		{	printf ("Sample Rate : %d\n", sfinfo.samplerate) ;
			printf ("Samples     : %d\n", sfinfo.samples) ;
			printf ("Channels    : %d\n", sfinfo.channels) ;
			printf ("Bit Width   : %d\n", sfinfo.pcmbitwidth) ;
			printf ("Format      : 0x%08X\n", sfinfo.format) ;
			printf ("Sections    : %d\n", sfinfo.sections) ;
			printf ("Seekable    : %s\n", (sfinfo.seekable ? "TRUE" : "FALSE")) ;
			printf ("Signal Max  : %g\n", sf_signal_max (infile)) ;
			}
		else
		{	printf ("Error : Not able to open input file %s.\n", infilename) ;
			sf_perror (NULL) ;
			} ;

		sf_close (infile) ;
		} ;
	
	return 0 ;
} /* main */

