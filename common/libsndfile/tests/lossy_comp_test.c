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

#ifndef M_PI
	#define M_PI 3.14159
#endif

#define	BUFFER_SIZE		(1<<14)
#define	SAMPLE_RATE		11025

static	void	lcomp_test_short	(char *str, char *filename, int typemajor, int typeminor, double margin) ;
static	void	lcomp_test_int		(char *str, char *filename, int typemajor, int typeminor, double margin) ;
static	void	lcomp_test_double	(char *str, char *filename, int typemajor, int typeminor, double margin) ;

static	void	sdlcomp_test_short	(char *str, char *filename, int typemajor, int typeminor, double margin) ;
static	void	sdlcomp_test_int	(char *str, char *filename, int typemajor, int typeminor, double margin) ;
static	void	sdlcomp_test_double	(char *str, char *filename, int typemajor, int typeminor, double margin) ;

static	int		error_function (double data, double orig, double margin) ;
static	int		decay_response (int k) ;
static	void	gen_signal (double *data, unsigned int datalen) ;
static	void	smoothed_diff_short (short *data, unsigned int datalen) ;
static	void	smoothed_diff_int (int *data, unsigned int datalen) ;
static	void	smoothed_diff_double (double *data, unsigned int datalen) ;

/* Force the start of these buffers to be double aligned. Sparc-solaris will
** choke if they are not.
*/
static	double	data_buffer [BUFFER_SIZE + 1] ;
static	double	orig_buffer [BUFFER_SIZE + 1] ;
static	double	smooth_buffer [BUFFER_SIZE + 1] ;

int		main (int argc, char *argv[])
{	char	*filename ;
	int		bDoAll = 0 ;
	int		nTests = 0 ;

	if (argc != 2)
	{	printf ("Usage : %s <test>\n", argv [0]) ;
		printf ("    Where <test> is one of the following:\n") ;
		printf ("           wav_ima     - test IMA ADPCM WAV file functions\n") ;
		printf ("           wav_msadpcm - test MS ADPCM WAV file functions\n") ;
		printf ("           wav_gsm610  - test GSM 6.10 WAV file functions\n") ;
		printf ("           wav_ulaw    - test u-law WAV file functions\n") ;
		printf ("           wav_alaw    - test A-law WAV file functions\n") ;
		printf ("           wav_pcm     - test PCM WAV file functions\n") ;
		printf ("           all         - perform all tests\n") ;
		exit (1) ;
		} ;
		
	bDoAll = !strcmp (argv [1], "all") ;

	if (bDoAll || ! strcmp (argv [1], "wav_pcm"))
	{	filename = "test.wav" ;
		lcomp_test_short	("wav_pcm", filename, SF_FORMAT_WAV, SF_FORMAT_PCM, 0.00001) ;
		lcomp_test_int		("wav_pcm", filename, SF_FORMAT_WAV, SF_FORMAT_PCM, 0.00001) ;
		lcomp_test_double	("wav_pcm", filename, SF_FORMAT_WAV, SF_FORMAT_PCM, 0.005) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "wav_ima"))
	{	filename = "test.wav" ;
		lcomp_test_short	("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;
		lcomp_test_int		("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;
		lcomp_test_double	("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;

		sdlcomp_test_short	("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;
		sdlcomp_test_int	("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;
		sdlcomp_test_double	("wav_ima", filename, SF_FORMAT_WAV, SF_FORMAT_IMA_ADPCM, 0.17) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "wav_msadpcm"))
	{	filename = "test.wav" ;
		lcomp_test_short	("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;
		lcomp_test_int		("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;
		lcomp_test_double	("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;

		sdlcomp_test_short	("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;
		sdlcomp_test_int	("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;
		sdlcomp_test_double	("wav_msadpcm", filename, SF_FORMAT_WAV, SF_FORMAT_MS_ADPCM, 0.36) ;

		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "wav_ulaw"))
	{	filename = "test.wav" ;
		lcomp_test_short	("wav_ulaw", filename, SF_FORMAT_WAV, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_int		("wav_ulaw", filename, SF_FORMAT_WAV, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_double	("wav_ulaw", filename, SF_FORMAT_WAV, SF_FORMAT_ULAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "wav_alaw"))
	{	filename = "test.wav" ;
		lcomp_test_short	("wav_alaw", filename, SF_FORMAT_WAV, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_int		("wav_alaw", filename, SF_FORMAT_WAV, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_double	("wav_alaw", filename, SF_FORMAT_WAV, SF_FORMAT_ALAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "wav_gsm610"))
	{	filename = "test.wav" ;
		sdlcomp_test_short	("wav_gsm610", filename, SF_FORMAT_WAV, SF_FORMAT_GSM610, 0.2) ;
		sdlcomp_test_int	("wav_gsm610", filename, SF_FORMAT_WAV, SF_FORMAT_GSM610, 0.2) ;
		sdlcomp_test_double	("wav_gsm610", filename, SF_FORMAT_WAV, SF_FORMAT_GSM610, 0.2) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "au_ulaw"))
	{	filename = "test.au" ;
		lcomp_test_short	("au_ulaw", filename, SF_FORMAT_AU, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_int		("au_ulaw", filename, SF_FORMAT_AU, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_double	("au_ulaw", filename, SF_FORMAT_AU, SF_FORMAT_ULAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "au_alaw"))
	{	filename = "test.au" ;
		lcomp_test_short	("au_alaw", filename, SF_FORMAT_AU, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_int		("au_alaw", filename, SF_FORMAT_AU, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_double	("au_alaw", filename, SF_FORMAT_AU, SF_FORMAT_ALAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "aule_ulaw"))
	{	filename = "test.au" ;
		lcomp_test_short	("aule_ulaw", filename, SF_FORMAT_AULE, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_int		("aule_ulaw", filename, SF_FORMAT_AULE, SF_FORMAT_ULAW, 0.04) ;
		lcomp_test_double	("aule_ulaw", filename, SF_FORMAT_AULE, SF_FORMAT_ULAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "aule_alaw"))
	{	filename = "test.au" ;
		lcomp_test_short	("aule_alaw", filename, SF_FORMAT_AULE, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_int		("aule_alaw", filename, SF_FORMAT_AULE, SF_FORMAT_ALAW, 0.04) ;
		lcomp_test_double	("aule_alaw", filename, SF_FORMAT_AULE, SF_FORMAT_ALAW, 0.04) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "au_g721"))
	{	filename = "test.au" ;
		lcomp_test_short	("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;
		lcomp_test_int		("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;
		lcomp_test_double	("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;

		sdlcomp_test_short	("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;
		sdlcomp_test_int	("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;
		sdlcomp_test_double	("au_g721", filename, SF_FORMAT_AU, SF_FORMAT_G721_32, 0.05) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "aule_g721"))
	{	filename = "test.au" ;
		lcomp_test_short	("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		lcomp_test_int		("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		lcomp_test_double	("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		
		sdlcomp_test_short	("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		sdlcomp_test_int	("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		sdlcomp_test_double	("aule_g721", filename, SF_FORMAT_AULE, SF_FORMAT_G721_32, 0.05) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "au_g723"))
	{	filename = "test.au" ;
		lcomp_test_short	("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		lcomp_test_int		("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		lcomp_test_double	("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		
		sdlcomp_test_short	("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		sdlcomp_test_int	("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		sdlcomp_test_double	("au_g723", filename, SF_FORMAT_AU, SF_FORMAT_G723_24, 0.15) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (bDoAll || ! strcmp (argv [1], "aule_g723"))
	{	filename = "test.au" ;
		lcomp_test_short	("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		lcomp_test_int		("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		lcomp_test_double	("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		
		sdlcomp_test_short	("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		sdlcomp_test_int	("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		sdlcomp_test_double	("aule_g723", filename, SF_FORMAT_AULE, SF_FORMAT_G723_24, 0.15) ;
		unlink (filename) ;
		nTests++ ;
		} ;

	if (nTests == 0)
	{	printf ("************************************\n") ;
		printf ("*  No '%s' test defined.\n", argv [1]) ;
		printf ("************************************\n") ;
		return 1 ;
		} ;

	return 0;
} /* main */

/*============================================================================================
**	Here are the test functions.
*/ 
 
static	
void	lcomp_test_short (char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	short			*orig, *data ;

	printf ("    lcomp_test_short    : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	orig = (short*) orig_buffer ;
	data = (short*) data_buffer ;
	gen_signal (orig_buffer, datalen) ;
	for (k = 0 ; k < datalen ; k++)
		orig [k] = (short) (orig_buffer [k]) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_short (file, orig, datalen)) != datalen)
	{	printf ("sf_write_short failed with short write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (short)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + datalen/2))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", sfinfo.samples, datalen) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_short (file, data, datalen)) < 0.99 * datalen)
	{	printf ("short read (%d).\n", k) ;
		exit (1) ;
		} ;

	for (k = 0 ; k < datalen ; k++)
	{	if (error_function ((double) data [k], (double) orig [k], margin))
		{	printf ("Incorrect sample A (#%d : %d should be %d).\n", k, data [k], orig [k]) ;
			exit (1) ;
			} ;
		} ;

	if ((k = sf_read_short (file, data, datalen)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", sfinfo.samples - datalen, k) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs (data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, data [k], decay_response (k)) ;
				exit (1) ;
				} ;

	if (! sfinfo.seekable)
	{	printf ("ok\n") ;
		return ;
		} ;

	/* Now test sf_seek function. */
	
	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
	{	printf ("Seek to start of file failed (%d).\n", k) ;
		exit (1) ;
		} ;

	for (m = 0 ; m < 3 ; m++)
	{	if ((k = sf_read_short (file, data, datalen/7)) != datalen / 7)
		{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
			exit (1) ;
			} ;

		for (k = 0 ; k < datalen/7 ; k++)
			if (error_function ((double) data [k], (double) orig [k + m * (datalen / 7)], margin))
			{	printf ("Incorrect sample C (#%d : %d => %d).\n", k + m * (datalen / 7), orig [k + m * (datalen / 7)], data [k]) ;
				for (m = 0 ; m < 10 ; m++)
					printf ("%d ", data [k]) ;
				printf ("\n") ;
				exit (1) ;
				} ;
		} ;

	seekpos = BUFFER_SIZE / 10 ;
	
	/* Check seek from start of file. */
	if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
	{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
		exit (1) ;
		} ;
	if ((k = sf_read_short (file, data, 1)) != 1)
	{	printf ("sf_read_short (file, data, 1) returned %d.\n", k) ;
		exit (1) ;
		} ;
	
	if (error_function ((double) data [0], (double) orig [seekpos], margin))
	{	printf ("sf_seek (SEEK_SET) followed by sf_read_short failed (%d, %d).\n", orig [1], data [0]) ;
		exit (1) ;
		} ;
	
	if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
	{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
		exit (1) ;
		} ;

	seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
	k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
	sf_read_short (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_short failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos + 1) ;
		exit (1) ;
		} ;
	
	seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
	/* Check seek backward from current position. */
	k = sf_seek (file, -20, SEEK_CUR) ;
	sf_read_short (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_short failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos) ;
		exit (1) ;
		} ;
	
	/* Check that read past end of file returns number of items. */
	sf_seek (file, (int) datalen, SEEK_SET) ;

 	if ((k = sf_read_short (file, data, datalen)) != sfinfo.samples - datalen)
 	{	printf ("Return value from sf_read_short past end of file incorrect (%d).\n", k) ;
 		exit (1) ;
 		} ;
	
	/* Check seek backward from end. */
	if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
	{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
		exit (1) ;
		} ;

	sf_read_short (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [5], margin))
	{	printf ("sf_seek (SEEK_END) followed by sf_read_short failed (%d should be %d).\n", data [0], orig [5]) ;
		exit (1) ;
		} ;

	sf_close (file) ;

	printf ("ok\n") ;
} /* lcomp_test_short */

/*--------------------------------------------------------------------------------------------
*/ 
 
static	
void	lcomp_test_int (char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	int				*orig, *data ;

	printf ("    lcomp_test_int      : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	data = (int*) data_buffer ;
	orig = (int*) orig_buffer ;
	gen_signal (orig_buffer, datalen) ;
	for (k = 0 ; k < datalen ; k++)
		orig [k] = (int) (orig_buffer [k]) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_int (file, orig, datalen)) != datalen)
	{	printf ("sf_write_int failed with short write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (short)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + datalen/2))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_int (file, data, datalen)) != datalen)
	{	printf ("short read (%d).\n", k) ;
		exit (1) ;
		} ;

	for (k = 0 ; k < datalen ; k++)
	{	if (error_function ((double) data [k], (double) orig [k], margin))
		{	printf ("Incorrect sample A (#%d : %d should be %d).\n", k, data [k], orig [k]) ;
			exit (1) ;
			} ;
		} ;

	if ((k = sf_read_int (file, data, datalen)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", sfinfo.samples - datalen, k) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs (data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, data [k], decay_response (k)) ;
				exit (1) ;
				} ;

	if (! sfinfo.seekable)
	{	printf ("ok\n") ;
		return ;
		} ;

	/* Now test sf_seek function. */
	
	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
	{	printf ("Seek to start of file failed (%d).\n", k) ;
		exit (1) ;
		} ;

	for (m = 0 ; m < 3 ; m++)
	{	if ((k = sf_read_int (file, data, datalen/7)) != datalen / 7)
		{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
			exit (1) ;
			} ;

		for (k = 0 ; k < datalen/7 ; k++)
			if (error_function ((double) data [k], (double) orig [k + m * (datalen / 7)], margin))
			{	printf ("Incorrect sample C (#%d : %d => %d).\n", k + m * (datalen / 7), orig [k + m * (datalen / 7)], data [k]) ;
				for (m = 0 ; m < 10 ; m++)
					printf ("%d ", data [k]) ;
				printf ("\n") ;
				exit (1) ;
				} ;
		} ;

	seekpos = BUFFER_SIZE / 10 ;
	
	/* Check seek from start of file. */
	if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
	{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
		exit (1) ;
		} ;
	if ((k = sf_read_int (file, data, 1)) != 1)
	{	printf ("sf_read_int (file, data, 1) returned %d.\n", k) ;
		exit (1) ;
		} ;
	
	if (error_function ((double) data [0], (double) orig [seekpos], margin))
	{	printf ("sf_seek (SEEK_SET) followed by sf_read_int failed (%d, %d).\n", orig [1], data [0]) ;
		exit (1) ;
		} ;
	
	if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
	{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
		exit (1) ;
		} ;

	seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
	k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
	sf_read_int (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_int failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos + 1) ;
		exit (1) ;
		} ;
	
	seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
	/* Check seek backward from current position. */
	k = sf_seek (file, -20, SEEK_CUR) ;
	sf_read_int (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_int failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos) ;
		exit (1) ;
		} ;
	
	/* Check that read past end of file returns number of items. */
	sf_seek (file, (int) datalen, SEEK_SET) ;

 	if ((k = sf_read_int (file, data, datalen)) != sfinfo.samples - datalen)
 	{	printf ("Return value from sf_read_int past end of file incorrect (%d).\n", k) ;
 		exit (1) ;
 		} ;
	
	/* Check seek backward from end. */
	if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
	{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
		exit (1) ;
		} ;

	sf_read_int (file, data, 1) ;
	if (error_function ((double) data [0], (double) orig [5], margin))
	{	printf ("sf_seek (SEEK_END) followed by sf_read_short failed (%d should be %d).\n", data [0], orig [5]) ;
		exit (1) ;
		} ;

	sf_close (file) ;

	printf ("ok\n") ;
} /* lcomp_test_int */

/*--------------------------------------------------------------------------------------------
*/ 

static	
void	lcomp_test_double (char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	double			*orig, *data ;

	printf ("    lcomp_test_double   : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	orig = (double*) orig_buffer ;
	data = (double*) data_buffer ;
	gen_signal (orig_buffer, datalen) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_double (file, orig, datalen, 0)) != datalen)
	{	printf ("sf_write_double failed with double write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (double)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + datalen/2))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_double (file, data, datalen, 0)) != datalen)
	{	printf ("double read (%d).\n", k) ;
		exit (1) ;
		} ;

	for (k = 0 ; k < datalen ; k++)
		if (error_function (data [k], orig [k], margin))
		{	printf ("Incorrect sample A (#%d : %f should be %f).\n", k, data [k], orig [k]) ;
			exit (1) ;
			} ;

	if ((k = sf_read_double (file, data, datalen, 0)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", sfinfo.samples - datalen, k) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs ((int) data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, (int) data [k], decay_response (k)) ;
				exit (1) ;
				} ;

	if (! sfinfo.seekable)
	{	printf ("ok\n") ;
		return ;
		} ;

	/* Now test sf_seek function. */
	
	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
	{	printf ("Seek to start of file failed (%d).\n", k) ;
		exit (1) ;
		} ;

	for (m = 0 ; m < 3 ; m++)
	{	if ((k = sf_read_double (file, data, datalen/7, 0)) != datalen / 7)
		{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
			exit (1) ;
			} ;

		for (k = 0 ; k < datalen/7 ; k++)
			if (error_function (data [k], orig [k + m * (datalen / 7)], margin))
			{	printf ("Incorrect sample C (#%d : %d => %d).\n", k + m * (datalen / 7), (int) orig [k + m * (datalen / 7)], (int) data [k]) ;
				for (m = 0 ; m < 10 ; m++)
					printf ("%d ", (int) data [k]) ;
				printf ("\n") ;
				exit (1) ;
				} ;
		} ;

	seekpos = BUFFER_SIZE / 10 ;
	
	/* Check seek from start of file. */
	if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
	{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
		exit (1) ;
		} ;
	if ((k = sf_read_double (file, data, 1, 0)) != 1)
	{	printf ("sf_read_double (file, data, 1, 0) returned %d.\n", k) ;
		exit (1) ;
		} ;
	
	if (error_function ((double) data [0], (double) orig [seekpos], margin))
	{	printf ("sf_seek (SEEK_SET) followed by sf_read_double failed (%d, %d).\n", (int) orig [1], (int) data [0]) ;
		exit (1) ;
		} ;
	
	if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
	{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
		exit (1) ;
		} ;

	seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
	k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
	sf_read_double (file, data, 1, 0) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_double failed (%d, %d) (%d, %d).\n", (int) data [0], (int) orig [seekpos], k, seekpos + 1) ;
		exit (1) ;
		} ;
	
	seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
	/* Check seek backward from current position. */
	k = sf_seek (file, -20, SEEK_CUR) ;
	sf_read_double (file, data, 1, 0) ;
	if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
	{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_double failed (%d, %d) (%d, %d).\n", (int) data [0], (int) orig [seekpos], k, seekpos) ;
		exit (1) ;
		} ;
	
	/* Check that read past end of file returns number of items. */
	sf_seek (file, (int) datalen, SEEK_SET) ;

 	if ((k = sf_read_double (file, data, datalen, 0)) != sfinfo.samples - datalen)
 	{	printf ("Return value from sf_read_double past end of file incorrect (%d).\n", k) ;
 		exit (1) ;
 		} ;
	
	/* Check seek backward from end. */
	if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
	{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
		exit (1) ;
		} ;

	sf_read_double (file, data, 1, 0) ;
	if (error_function (data [0], orig [5], margin))
	{	printf ("sf_seek (SEEK_END) followed by sf_read_double failed (%d, %d).\n", (int) data [0], (int) orig [5]) ;
		exit (1) ;
		} ;

	sf_close (file) ;

	printf ("ok\n") ;
} /* lcomp_test_double */

/*========================================================================================
**	Smoothed differential loss compression tests.
*/

static	
void	sdlcomp_test_short	(char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	short			*orig, *data, *smooth ;

	printf ("    sdlcomp_test_short  : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	orig = (short*) orig_buffer ;
	data = (short*) data_buffer ;
	smooth = (short*) smooth_buffer ;

	gen_signal (orig_buffer, datalen) ;
	for (k = 0 ; k < datalen ; k++)
		orig [k] = (short) (orig_buffer [k]) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_short (file, orig, datalen)) != datalen)
	{	printf ("sf_write_short failed with short write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (short)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + 400))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", sfinfo.samples, datalen) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_short (file, data, datalen)) != datalen)
	{	printf ("short read (%d).\n", k) ;
		exit (1) ;
		} ;

	memcpy (smooth, orig, datalen * sizeof (short)) ;
	smoothed_diff_short (data, datalen) ;
	smoothed_diff_short (smooth, datalen) ;

	for (k = 1 ; k < datalen ; k++)
	{	if (error_function ((double) data [k], (double) smooth [k], margin))
		{	printf ("Incorrect sample A (#%d : %d should be %d).\n", k, data [k], smooth [k]) ;
			exit (1) ;
			} ;
		} ;

	if ((k = sf_read_short (file, data, datalen)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", k, sfinfo.samples - datalen) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM &&
		(sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_GSM610)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs (data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, data [k], decay_response (k)) ;
				exit (1) ;
				} ;

	/* Now test sf_seek function. */
	if (sfinfo.seekable)
	{	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
		{	printf ("Seek to start of file failed (%d).\n", k) ;
			exit (1) ;
			} ;
	
		for (m = 0 ; m < 3 ; m++)
		{	if ((k = sf_read_short (file, data, datalen/7)) != datalen / 7)
			{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
				exit (1) ;
				} ;
	
			smoothed_diff_short (data, datalen/7) ;
			memcpy (smooth, orig + m * datalen/7, datalen/7 * sizeof (short)) ;
			smoothed_diff_short (smooth, datalen/7) ;
			
			for (k = 0 ; k < datalen/7 ; k++)
				if (error_function ((double) data [k], (double) smooth [k], margin))
				{	printf ("Incorrect sample C (#%d (%d) : %d => %d).\n", k, k + m * (datalen / 7), smooth [k], data [k]) ;
					for (m = 0 ; m < 10 ; m++)
						printf ("%d ", data [k]) ;
					printf ("\n") ;
					exit (1) ;
					} ;
			} ; /* for (m = 0 ; m < 3 ; m++) */
	
		seekpos = BUFFER_SIZE / 10 ;
		
		/* Check seek from start of file. */
		if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
		{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
			exit (1) ;
			} ;
		if ((k = sf_read_short (file, data, 1)) != 1)
		{	printf ("sf_read_short (file, data, 1) returned %d.\n", k) ;
			exit (1) ;
			} ;
		
		if (error_function ((double) data [0], (double) orig [seekpos], margin))
		{	printf ("sf_seek (SEEK_SET) followed by sf_read_short failed (%d, %d).\n", orig [1], data [0]) ;
			exit (1) ;
			} ;
		
		if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
		{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
			exit (1) ;
			} ;
	
		seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
		k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
		sf_read_short (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_short failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos + 1) ;
			exit (1) ;
			} ;
		
		seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
		/* Check seek backward from current position. */
		k = sf_seek (file, -20, SEEK_CUR) ;
		sf_read_short (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_short failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos) ;
			exit (1) ;
			} ;
		
		/* Check that read past end of file returns number of items. */
		sf_seek (file, (int) datalen, SEEK_SET) ;
	
	 	if ((k = sf_read_short (file, data, datalen)) != sfinfo.samples - datalen)
	 	{	printf ("Return value from sf_read_short past end of file incorrect (%d).\n", k) ;
	 		exit (1) ;
	 		} ;
		
		/* Check seek backward from end. */
		
		if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
		{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
			exit (1) ;
			} ;
	
		sf_read_short (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [5], margin))
		{	printf ("sf_seek (SEEK_END) followed by sf_read_short failed (%d should be %d).\n", data [0], orig [5]) ;
			exit (1) ;
			} ;
		} /* if (sfinfo.seekable) */

	sf_close (file) ;

	printf ("ok\n") ;
} /* sdlcomp_test_short */

static	
void	sdlcomp_test_int	(char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	int			*orig, *data, *smooth ;

	printf ("    sdlcomp_test_int    : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	orig = (int*) orig_buffer ;
	data = (int*) data_buffer ;
	smooth = (int*) smooth_buffer ;

	gen_signal (orig_buffer, datalen) ;
	for (k = 0 ; k < datalen ; k++)
		orig [k] = (int) (orig_buffer [k]) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_int (file, orig, datalen)) != datalen)
	{	printf ("sf_write_int failed with int write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (int)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + 400))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", sfinfo.samples, datalen) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_int (file, data, datalen)) != datalen)
	{	printf ("int read (%d).\n", k) ;
		exit (1) ;
		} ;

	memcpy (smooth, orig, datalen * sizeof (int)) ;
	smoothed_diff_int (data, datalen) ;
	smoothed_diff_int (smooth, datalen) ;

	for (k = 1 ; k < datalen ; k++)
	{	if (error_function ((double) data [k], (double) smooth [k], margin))
		{	printf ("Incorrect sample A (#%d : %d should be %d).\n", k, data [k], smooth [k]) ;
			exit (1) ;
			} ;
		} ;

	if ((k = sf_read_int (file, data, datalen)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", k, sfinfo.samples - datalen) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM &&
		(sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_GSM610)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs (data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, data [k], decay_response (k)) ;
				exit (1) ;
				} ;

	/* Now test sf_seek function. */
	if (sfinfo.seekable)
	{	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
		{	printf ("Seek to start of file failed (%d).\n", k) ;
			exit (1) ;
			} ;
	
		for (m = 0 ; m < 3 ; m++)
		{	if ((k = sf_read_int (file, data, datalen/7)) != datalen / 7)
			{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
				exit (1) ;
				} ;
	
			smoothed_diff_int (data, datalen/7) ;
			memcpy (smooth, orig + m * datalen/7, datalen/7 * sizeof (int)) ;
			smoothed_diff_int (smooth, datalen/7) ;
			
			for (k = 0 ; k < datalen/7 ; k++)
				if (error_function ((double) data [k], (double) smooth [k], margin))
				{	printf ("Incorrect sample C (#%d (%d) : %d => %d).\n", k, k + m * (datalen / 7), smooth [k], data [k]) ;
					for (m = 0 ; m < 10 ; m++)
						printf ("%d ", data [k]) ;
					printf ("\n") ;
					exit (1) ;
					} ;
			} ; /* for (m = 0 ; m < 3 ; m++) */
	
		seekpos = BUFFER_SIZE / 10 ;
		
		/* Check seek from start of file. */
		if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
		{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
			exit (1) ;
			} ;
		if ((k = sf_read_int (file, data, 1)) != 1)
		{	printf ("sf_read_int (file, data, 1) returned %d.\n", k) ;
			exit (1) ;
			} ;
		
		if (error_function ((double) data [0], (double) orig [seekpos], margin))
		{	printf ("sf_seek (SEEK_SET) followed by sf_read_int failed (%d, %d).\n", orig [1], data [0]) ;
			exit (1) ;
			} ;
		
		if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
		{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
			exit (1) ;
			} ;
	
		seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
		k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
		sf_read_int (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_int failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos + 1) ;
			exit (1) ;
			} ;
		
		seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
		/* Check seek backward from current position. */
		k = sf_seek (file, -20, SEEK_CUR) ;
		sf_read_int (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_int failed (%d, %d) (%d, %d).\n", data [0], orig [seekpos], k, seekpos) ;
			exit (1) ;
			} ;
		
		/* Check that read past end of file returns number of items. */
		sf_seek (file, (int) datalen, SEEK_SET) ;
	
	 	if ((k = sf_read_int (file, data, datalen)) != sfinfo.samples - datalen)
	 	{	printf ("Return value from sf_read_int past end of file incorrect (%d).\n", k) ;
	 		exit (1) ;
	 		} ;
		
		/* Check seek backward from end. */
		
		if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
		{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
			exit (1) ;
			} ;
	
		sf_read_int (file, data, 1) ;
		if (error_function ((double) data [0], (double) orig [5], margin))
		{	printf ("sf_seek (SEEK_END) followed by sf_read_int failed (%d should be %d).\n", data [0], orig [5]) ;
			exit (1) ;
			} ;
		} /* if (sfinfo.seekable) */

	sf_close (file) ;

	printf ("ok\n") ;
} /* sdlcomp_test_int */

static	
void	sdlcomp_test_double	(char *str, char *filename, int typemajor, int typeminor, double margin)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	int				k, m, seekpos ;
	unsigned int	datalen ;
	double			*orig, *data, *smooth ;

	printf ("    sdlcomp_test_double : %s ... ", str) ;
	
	datalen = BUFFER_SIZE ;

	orig = orig_buffer ;
	data = data_buffer ;
	smooth = smooth_buffer ;

	gen_signal (orig_buffer, datalen) ;
		
	sfinfo.samplerate  = SAMPLE_RATE ;
	sfinfo.samples     = 123456789 ;	/* Ridiculous value. */
	sfinfo.channels    = 1 ;
	sfinfo.pcmbitwidth = 16 ;
	sfinfo.format 	   = (typemajor | typeminor) ;

	if (! (file = sf_open_write (filename, &sfinfo)))
	{	printf ("sf_open_write failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if ((k = sf_write_double (file, orig, datalen, 0)) != datalen)
	{	printf ("sf_write_double failed with int write (%d => %d).\n", datalen, k) ;
		exit (1) ;
		} ;
	sf_close (file) ;
	
	memset (data, 0, datalen * sizeof (double)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	
	if (! (file = sf_open_read (filename, &sfinfo)))
	{	printf ("sf_open_read failed with error : ") ;
		sf_perror (NULL) ;
		exit (1) ;
		} ;
	
	if (sfinfo.format != (typemajor | typeminor))
	{	printf ("Returned format incorrect (0x%08X => 0x%08X).\n", (typemajor | typeminor), sfinfo.format) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples < datalen)
	{	printf ("Too few samples in file. (%d should be a little more than %d)\n", datalen, sfinfo.samples) ;
		exit (1) ;
		} ;
	
	if (sfinfo.samples > (datalen + 400))
	{	printf ("Too many samples in file. (%d should be a little more than %d)\n", sfinfo.samples, datalen) ;
		exit (1) ;
		} ;
	
	if (sfinfo.channels != 1)
	{	printf ("Incorrect number of channels in file.\n") ;
		exit (1) ;
		} ;

	if (sfinfo.pcmbitwidth != 16)
	{	printf ("Incorrect bit width (%d).\n", sfinfo.pcmbitwidth) ;
		exit (1) ;
		} ;

	if ((k = sf_read_double (file, data, datalen, 0)) != datalen)
	{	printf ("int read (%d).\n", k) ;
		exit (1) ;
		} ;

	memcpy (smooth, orig, datalen * sizeof (double)) ;
	smoothed_diff_double (data, datalen) ;
	smoothed_diff_double (smooth, datalen) ;

	for (k = 1 ; k < datalen ; k++)
	{	if (error_function (data [k], smooth [k], margin))
		{	printf ("Incorrect sample A (#%d : %d should be %d).\n", k, (int) data [k], (int) smooth [k]) ;
			exit (1) ;
			} ;
		} ;

	if ((k = sf_read_double (file, data, datalen, 0)) != sfinfo.samples - datalen)
	{	printf ("Incorrect read length A (%d should be %d).\n", k, sfinfo.samples - datalen) ;
		exit (1) ;
		} ;
		
	if ((sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_MS_ADPCM &&
		(sfinfo.format & SF_FORMAT_SUBMASK) != SF_FORMAT_GSM610)
		for (k = 0 ; k < sfinfo.samples - datalen ; k++)
			if (abs (data [k]) > decay_response (k))
			{	printf ("Incorrect sample B (#%d : abs (%d) should be < %d).\n", datalen + k, (int) data [k], (int) decay_response (k)) ;
				exit (1) ;
				} ;

	/* Now test sf_seek function. */
	if (sfinfo.seekable)
	{	if ((k = sf_seek (file, 0, SEEK_SET)) != 0)
		{	printf ("Seek to start of file failed (%d).\n", k) ;
			exit (1) ;
			} ;
	
		for (m = 0 ; m < 3 ; m++)
		{	if ((k = sf_read_double (file, data, datalen/7, 0)) != datalen / 7)
			{	printf ("Incorrect read length B (%d => %d).\n", datalen / 7, k) ;
				exit (1) ;
				} ;
	
			smoothed_diff_double (data, datalen/7) ;
			memcpy (smooth, orig + m * datalen/7, datalen/7 * sizeof (double)) ;
			smoothed_diff_double (smooth, datalen/7) ;
			
			for (k = 0 ; k < datalen/7 ; k++)
				if (error_function ((double) data [k], (double) smooth [k], margin))
				{	printf ("Incorrect sample C (#%d (%d) : %d => %d).\n", k, k + m * (datalen / 7), (int) smooth [k], (int) data [k]) ;
					for (m = 0 ; m < 10 ; m++)
						printf ("%d ", (int) data [k]) ;
					printf ("\n") ;
					exit (1) ;
					} ;
			} ; /* for (m = 0 ; m < 3 ; m++) */
	
		seekpos = BUFFER_SIZE / 10 ;
		
		/* Check seek from start of file. */
		if ((k = sf_seek (file, seekpos, SEEK_SET)) != seekpos)
		{	printf ("Seek to start of file + %d failed (%d).\n", seekpos, k) ;
			exit (1) ;
			} ;
		if ((k = sf_read_double (file, data, 1, 0)) != 1)
		{	printf ("sf_read_double (file, data, 1) returned %d.\n", k) ;
			exit (1) ;
			} ;
		
		if (error_function ((double) data [0], (double) orig [seekpos], margin))
		{	printf ("sf_seek (SEEK_SET) followed by sf_read_double failed (%d, %d).\n", (int) orig [1], (int) data [0]) ;
			exit (1) ;
			} ;
		
		if ((k = sf_seek (file, 0, SEEK_CUR)) != seekpos + 1)
		{	printf ("sf_seek (SEEK_CUR) with 0 offset failed (%d should be %d)\n", k, seekpos + 1) ;
			exit (1) ;
			} ;
	
		seekpos = sf_seek (file, 0, SEEK_CUR) + BUFFER_SIZE / 5 ;
		k = sf_seek (file, BUFFER_SIZE / 5, SEEK_CUR) ;
		sf_read_double (file, data, 1, 0) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (forwards, SEEK_CUR) followed by sf_read_double failed (%d, %d) (%d, %d).\n", (int) data [0], (int) orig [seekpos], k, seekpos + 1) ;
			exit (1) ;
			} ;
		
		seekpos = sf_seek (file, 0, SEEK_CUR) - 20 ;
		/* Check seek backward from current position. */
		k = sf_seek (file, -20, SEEK_CUR) ;
		sf_read_double (file, data, 1, 0) ;
		if (error_function ((double) data [0], (double) orig [seekpos], margin) || k != seekpos)
		{	printf ("sf_seek (backwards, SEEK_CUR) followed by sf_read_double failed (%d, %d) (%d, %d).\n", (int) data [0], (int) orig [seekpos], k, seekpos) ;
			exit (1) ;
			} ;
		
		/* Check that read past end of file returns number of items. */
		sf_seek (file, (int) datalen, SEEK_SET) ;
	
	 	if ((k = sf_read_double (file, data, datalen, 0)) != sfinfo.samples - datalen)
	 	{	printf ("Return value from sf_read_double past end of file incorrect (%d).\n", k) ;
	 		exit (1) ;
	 		} ;
		
		/* Check seek backward from end. */
		
		if ((k = sf_seek (file, 5 - (int) sfinfo.samples, SEEK_END)) != 5)
		{	printf ("sf_seek (SEEK_END) returned %d instead of %d.\n", k, 5) ;
			exit (1) ;
			} ;
	
		sf_read_double (file, data, 1, 0) ;
		if (error_function ((double) data [0], (double) orig [5], margin))
		{	printf ("sf_seek (SEEK_END) followed by sf_read_double failed (%d should be %d).\n", (int) data [0], (int) orig [5]) ;
			exit (1) ;
			} ;
		} /* if (sfinfo.seekable) */

	sf_close (file) ;

	printf ("ok\n") ;
} /* sdlcomp_test_double */

/*========================================================================================
**	Auxiliary functions
*/

static
int decay_response (int k)
{	if (k < 1)
		return ((int) 30000.0) ;
	return (int) (30000.0 / (0.5 * k * k)) ;
} /* decay_response */

static
void	gen_signal (double *data, unsigned int datalen)
{	unsigned int k, ramplen ;
	double	amp = 0.0 ;
	
	ramplen = datalen / 20 ;

	for (k = 0 ; k < datalen ; k++)
	{	if (k <= ramplen)
			amp = 30000.0 * k / ((double) ramplen) ;
		else if (k > datalen - ramplen)
			amp = 30000.0 * (datalen - k) / ((double) ramplen) ;
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

static	
void	smoothed_diff_short (short *data, unsigned int datalen)
{	unsigned int k ;
	double memory = 0.0 ;

	/* Calculate the smoothed sample-to-sample difference. */
	for (k = 0 ; k < datalen - 1 ; k++)
	{	memory = 0.7 * memory + (1 - 0.7) * (double) (data [k+1] - data [k]) ;
		data [k] = (short) memory ;
		} ;
	data [datalen-1] = data [datalen-2] ;
	
} /* smoothed_diff_short */

static	
void	smoothed_diff_int (int *data, unsigned int datalen)
{	unsigned int k ;
	double memory = 0.0 ;

	/* Calculate the smoothed sample-to-sample difference. */
	for (k = 0 ; k < datalen - 1 ; k++)
	{	memory = 0.7 * memory + (1 - 0.7) * (double) (data [k+1] - data [k]) ;
		data [k] = (int) memory ;
		} ;
	data [datalen-1] = data [datalen-2] ;
	
} /* smoothed_diff_int */

static	
void	smoothed_diff_double (double *data, unsigned int datalen)
{	unsigned int k ;
	double memory = 0.0 ;

	/* Calculate the smoothed sample-to-sample difference. */
	for (k = 0 ; k < datalen - 1 ; k++)
	{	memory = 0.7 * memory + (1 - 0.7) * (data [k+1] - data [k]) ;
		data [k] = memory ;
		} ;
	data [datalen-1] = data [datalen-2] ;
	
} /* smoothed_diff_double */
