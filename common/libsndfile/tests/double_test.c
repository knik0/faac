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
#include	<math.h>


#include	<sndfile.h>

#define	SAMPLE_RATE			11025
#define	BUFFER_SIZE			(1<<14)

static	void	scaled_test		(char *str, char *filename, int filetype, int bitwidth, double tolerance) ;

static	int		error_function (double data, double orig, double margin) ;
static	void	gen_signal (double *data, unsigned int datalen) ;

static	double	orig_data [BUFFER_SIZE] ;
static	double	test_data [BUFFER_SIZE] ;

int		main (int argc, char *argv[])
{	
	scaled_test	("pcm8" , "test.wav", SF_FORMAT_WAV | SF_FORMAT_PCM, 8, 0.001) ;
	scaled_test	("pcm16", "test.wav", SF_FORMAT_WAV | SF_FORMAT_PCM, 16, 0.001) ;
	scaled_test	("pcm24", "test.wav", SF_FORMAT_WAV | SF_FORMAT_PCM, 24, 0.001) ;
	scaled_test	("pcm32", "test.wav", SF_FORMAT_WAV | SF_FORMAT_PCM, 32, 0.001) ;
	
	scaled_test	("ulaw", "test.au", SF_FORMAT_AU | SF_FORMAT_ULAW, 16, 0.05) ;
	scaled_test	("alaw", "test.au", SF_FORMAT_AU | SF_FORMAT_ALAW, 16, 0.05) ;
	
	scaled_test	("imaadpcm", "test.wav", SF_FORMAT_WAV | SF_FORMAT_IMA_ADPCM, 16, 0.21) ;
	scaled_test	("msadpcm" , "test.wav", SF_FORMAT_WAV | SF_FORMAT_MS_ADPCM, 16, 0.7) ;
	scaled_test	("gsm610"  , "test.wav", SF_FORMAT_WAV | SF_FORMAT_GSM610, 16, 3.0) ;

	scaled_test	("g721_32", "test.au", SF_FORMAT_AU | SF_FORMAT_G721_32, 16, 1.1) ;
	scaled_test	("g723_24", "test.au", SF_FORMAT_AU | SF_FORMAT_G723_24, 16, 1.1) ;

	return 0;
} /* main */

/*============================================================================================
 *	Here are the test functions.
 */ 

static	
void	scaled_test (char *str, char *filename, int filetype, int bitwidth, double tolerance)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	unsigned int	k ;
	double			scale ;

	printf ("    scaled_test : %s ", str) ;
	for (k = strlen (str) ; k < 10 ; k++)
		putchar ('.') ;
	putchar (' ') ;

	gen_signal (orig_data, BUFFER_SIZE) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = BUFFER_SIZE ;
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = bitwidth ;
	sfinfo.format 	   = filetype ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sf_write_double (file, orig_data, BUFFER_SIZE, 1) != BUFFER_SIZE)
	{	printf ("sf_write_int failed with error : ") ;
		sf_perror (file) ;
		exit (1) ;
		} ;
		
	sf_close (file) ;
	
	memset (test_data, 0, sizeof (test_data)) ;

	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != filetype)
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", filetype, sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < BUFFER_SIZE)
	{	printf ("Incorrect number of samples in file (too short). (%d should be %d)\n", sfinfo.samples, BUFFER_SIZE) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != bitwidth)
	{	printf ("Incorrect bit width (%d => %d).\n", bitwidth, sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_double (file, test_data, BUFFER_SIZE, 1)) < 0.99 * BUFFER_SIZE)
	{	printf ("short read (%d).\n", k) ;
		exit (1) ;
		} ;

	sf_close (file) ;

	scale = bitwidth > 8 ? 32000.0 : 120.0 ;
	for (k = 0 ; k < BUFFER_SIZE ; k++)
		if (error_function (scale * test_data [k], scale * orig_data [k], tolerance))
		{	printf ("Incorrect sample (#%d : %f should be %f).\n", k, test_data [k], orig_data [k]) ;
			exit (1) ;
			} ;

	unlink (filename) ;
			
	printf ("ok\n") ;
} /* scaled_test */

/*========================================================================================
**	Auxiliary functions
*/

static
void	gen_signal (double *data, unsigned int datalen)
{	unsigned int k, ramplen ;
	double	amp = 0.0 ;
	
	ramplen = datalen / 20 ;

	for (k = 0 ; k < datalen ; k++)
	{	if (k <= ramplen)
			amp = k / ((double) ramplen) ;
		else if (k > datalen - ramplen)
			amp = (datalen - k) / ((double) ramplen) ;
		data [k] = amp * (0.4 * sin (33.3 * 2.0 * M_PI * ((double) (k+1)) / ((double) SAMPLE_RATE))
							+ 0.3 * cos (201.1 * 2.0 * M_PI * ((double) (k+1)) / ((double) SAMPLE_RATE))) ;
		} ;
	return ;
} /* gen_signal */

static
int error_function (double data, double orig, double margin)
{	double error ;

	if (fabs (orig) <= 500.0)
		error = fabs (fabs (data) - fabs(orig)) / 2000.0 ;
	else if (fabs (orig) <= 1000.0)
		error = fabs (data - orig) / 3000.0 ;
	else
		error = fabs (data - orig) / fabs (orig) ;
		
	if (error > margin)
	{	printf ("\n\n*******************\nError : %f\n", error) ;
		return 1 ;
		} ;
	return 0 ;
} /* error_function */

