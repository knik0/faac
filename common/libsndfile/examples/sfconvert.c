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

#define	 BUFFER_LEN      1024


typedef	struct
{	char	*infilename, *outfilename ;
	SF_INFO	infileinfo, outfileinfo ;
} OptionData ;

static
void    copy_data (SNDFILE *outfile, SNDFILE *infile, unsigned int len, double normfactor)
{	static double	data [BUFFER_LEN] ;
	unsigned int	readcount, k ;

	readcount = len ;
	while (readcount == len)
	{	readcount = sf_read_double (infile, data, len, 0) ;
		for (k = 0 ; k < readcount ; k++)
			data [k] *= normfactor ;
		sf_write_double (outfile, data, readcount, 0) ;
		} ;

	return ;
} /* copy_data */

static
int	guess_output_file_type (char *str, unsigned int format)
{	char	buffer [16], *cptr ;
	int		k ;
	
	format &= SF_FORMAT_SUBMASK ;

	if (! (cptr = strrchr (str, '.')))
		return 0 ;

	strncpy (buffer, cptr + 1, 15) ;
	buffer [15] = 0 ;
	
	for (k = 0 ; buffer [k] ; k++)
		buffer [k] = tolower ((buffer [k])) ;
		
	if (! strncmp (buffer, "aif", 3))
		return	(SF_FORMAT_AIFF | format) ;
	if (! strcmp (buffer, "wav"))
		return	(SF_FORMAT_WAV | format) ;
	if (! strcmp (buffer, "au") || ! strcmp (buffer, "snd"))
		return	(SF_FORMAT_AU | format) ;
	return	0 ;
} /* guess_output_file_type */


static
void	print_usage (char *progname)
{	printf ("\nUsage : %s [options] <input file> <output file>\n", progname) ;
	printf ("\n        where [options] may be one of the following:\n") ;
	printf ("            -pcm16     : force the output to 16 bit pcm\n") ;
	printf ("            -pcm24     : force the output to 24 bit pcm\n") ;
	printf ("            -pcm32     : force the output to 32 bit pcm\n") ;
	printf ("\n        with one of the following extra specifiers:\n") ;
	printf ("            -fullscale	: force the output signal to the full bit width\n") ;
	printf ("\n") ;
} /* print_usage */

int     main (int argc, char *argv[])
{	char 		*progname, *infilename, *outfilename ;
	SNDFILE	 	*infile, *outfile ;
	SF_INFO	 	sfinfo ;
	int			k, outfilemajor ;
	int			outfileminor = 0, outfilebits = 0, fullscale = 0 ;
	double		normfactor ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;
		
	if (argc < 3 || argc > 5)
	{	print_usage (progname) ;
		return  1 ;
		} ;
		
	infilename = argv [argc-2] ;
	outfilename = argv [argc-1] ;
		
	if (! strcmp (infilename, outfilename))
	{	printf ("Error : Input and output filenames are the same.\n\n") ;
		print_usage (progname) ;
		return  1 ;
		} ;
		
	if (infilename [0] == '-')
	{	printf ("Error : Input filename (%s) looks like an option.\n\n", infilename) ;
		print_usage (progname) ;
		return  1 ;
		} ;
	
	if (outfilename [0] == '-')
	{	printf ("Error : Output filename (%s) looks like an option.\n\n", outfilename) ;
		print_usage (progname) ;
		return  1 ;
		} ;
		
	for (k = 1 ; k < argc - 2 ; k++)
	{	printf ("%s\n", argv [k]) ;
		if (! strcmp (argv [k], "-pcm16"))
		{	outfileminor = SF_FORMAT_PCM ;
			outfilebits = 16 ;
			continue ;
			} ;
		if (! strcmp (argv [k], "-pcm24"))
		{	outfileminor = SF_FORMAT_PCM ;
			outfilebits = 24 ;
			continue ;
			} ;
		if (! strcmp (argv [k], "-pcm32"))
		{	outfileminor = SF_FORMAT_PCM ;
			outfilebits = 32 ;
			continue ;
			} ;
		if (! strcmp (argv [k], "-fullscale"))
			fullscale = 1 ;
		} ;

	
	if (! (infile = sf_open_read (infilename, &sfinfo)))
	{	printf ("Not able to open input file %s.\n", infilename) ;
		sf_perror (NULL) ;
		return  1 ;
		} ;
		
	if (! (sfinfo.format = guess_output_file_type (outfilename, sfinfo.format)))
	{	printf ("Error : Not able to determine output file type for %s.\n", outfilename) ;
		return 1 ;
		} ;
	
	outfilemajor = sfinfo.format & SF_FORMAT_TYPEMASK ;

	if (outfileminor)
	{	sfinfo.format = outfilemajor | outfileminor ;
		printf ("asdasdasdad\n") ;
		}
	else
		sfinfo.format = outfilemajor | (sfinfo.format & SF_FORMAT_SUBMASK) ;
		
	if (outfilebits)
		sfinfo.pcmbitwidth = outfilebits ;
		
	if (! sf_format_check (&sfinfo))
	{	printf ("Error : output file format is invalid (0x%08X).\n", sfinfo.format) ;
		return 1 ;
		} ;	

	normfactor = sf_signal_max (infile) ;
	if (normfactor < 1.0 && normfactor > 0.0)
		normfactor = fullscale ? 2.0 / normfactor * ((double) (1 << (sfinfo.pcmbitwidth - 2))) : 
								2.0 * ((double) (1 << (sfinfo.pcmbitwidth - 2))) ;
	else
		normfactor = 1.0 ;
	
	printf ("normfactor : %g\n", normfactor) ;
	
	if (! (outfile = sf_open_write (outfilename, &sfinfo)))
	{	printf ("Not able to open output file %s.\n", outfilename) ;
		return  1 ;
		} ;
		
	copy_data (outfile, infile, BUFFER_LEN / sfinfo.channels, normfactor) ;
		
	sf_close (infile) ;
	sf_close (outfile) ;
	
	return 0 ;
} /* main */

