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

#define	 BUFFER_LEN      400


int     main (int argc, char *argv[])
{	static double	data [BUFFER_LEN] ;
	char 			*progname, *infilename, *outfilename, *cptr ;
	SNDFILE	 		*infile ;
	SF_INFO	 		sfinfo ;
	FILE			*outfile ;
	unsigned int	k, readcount, len, total ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;
		
	if (argc != 3)
	{	printf ("\n Usage : %s <input file> <output file>\n\n", progname) ;
		return  1 ;
		} ;
		
	infilename  = argv [1] ;
	outfilename = argv [2] ;
	
	if (! strcmp (infilename, outfilename))
	{	printf ("Error : Input and output filename are the same.\n") ;
		return 1 ;
		} ;
		
	if (! (infile = sf_open_read (infilename, &sfinfo)))
	{	printf ("Not able to open input file %s.\n", infilename) ;
		sf_perror (NULL) ;
		return  1 ;
		} ;
		
	if (! (outfile = fopen (outfilename, "w")))
	{	printf ("Not able to open output file %s.\n", outfilename) ;
		sf_perror (NULL) ;
		return  1 ;
		} ;
		
	outfilename = strrchr (argv [2], '/') ;
	outfilename = outfilename ? outfilename + 1 : argv [2] ;
	if ((cptr = strchr (outfilename, '.')))
		*cptr = 0 ;
		
	fprintf (outfile, "# name: %s\n", outfilename) ;
	fprintf (outfile, "# type: matrix\n") ;
	fprintf (outfile, "# rows: %d\n", sfinfo.samples) ;
	fprintf (outfile, "# columns: %d", sfinfo.channels) ;
		
	len = BUFFER_LEN - (BUFFER_LEN % sfinfo.channels) ;
	total = 0 ;
	while ((readcount = sf_read_double (infile, data, len, 0)))
	{	
		for (k = 0 ; k < readcount ; k++)
		{	if (! (k % sfinfo.channels))
				fprintf (outfile, "\n") ;
			fprintf (outfile, "%g ", data [k]) ;
			} ;
		memset (data, 0, len * sizeof (double)) ;
		total += readcount ;
		} ;
	if (total != sfinfo.samples * sfinfo.channels)
		printf ("Error : Values read (%d) != samples * channels (%d)\n", total, sfinfo.samples * sfinfo.channels) ;

	fclose (outfile) ;
	sf_close (infile) ;
	
	return 0 ;
} /* main */

