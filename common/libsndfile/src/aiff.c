/*
** Copyright (C) 1999-2000 Erik de Castro Lopo <erikd@zip.com.au>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<ctype.h>

#include	"sndfile.h"
#include	"config.h"
#include	"sfendian.h"
#include	"common.h"
#include	"pcm.h"


/*------------------------------------------------------------------------------
 * Macros to handle big/little endian issues.
 */

#if (CPU_IS_LITTLE_ENDIAN == 1)
	#define	MAKE_MARKER(a,b,c,d)		((a)|((b)<<8)|((c)<<16)|((d)<<24))
#elif (CPU_IS_BIG_ENDIAN == 1)
	#define	MAKE_MARKER(a,b,c,d)		(((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
	#error "Cannot determine endian-ness of processor."
#endif

#define FORM_MARKER	(MAKE_MARKER ('F', 'O', 'R', 'M')) 
#define AIFF_MARKER	(MAKE_MARKER ('A', 'I', 'F', 'F')) 
#define AIFC_MARKER	(MAKE_MARKER ('A', 'I', 'F', 'C')) 
#define COMM_MARKER	(MAKE_MARKER ('C', 'O', 'M', 'M')) 
#define SSND_MARKER	(MAKE_MARKER ('S', 'S', 'N', 'D')) 
#define MARK_MARKER	(MAKE_MARKER ('M', 'A', 'R', 'K')) 
#define INST_MARKER	(MAKE_MARKER ('I', 'N', 'S', 'T')) 
#define APPL_MARKER	(MAKE_MARKER ('A', 'P', 'P', 'L')) 

#define c_MARKER	(MAKE_MARKER ('(', 'c', ')', ' ')) 
#define NAME_MARKER	(MAKE_MARKER ('N', 'A', 'M', 'E')) 
#define AUTH_MARKER	(MAKE_MARKER ('A', 'U', 'T', 'H')) 
#define ANNO_MARKER	(MAKE_MARKER ('A', 'N', 'N', 'O')) 
#define FVER_MARKER	(MAKE_MARKER ('F', 'V', 'E', 'R')) 

#define NONE_MARKER	(MAKE_MARKER ('N', 'O', 'N', 'E')) 

#define	REAL_COMM_SIZE			(2+4+2+10)

/*------------------------------------------------------------------------------
 * Typedefs for file chunks.
 */

typedef struct
{	short			numChannels ;
	unsigned int	numSampleFrames ;
	short			sampleSize ;
	unsigned char	sampleRate [10] ;
} COMM_CHUNK ; 

typedef struct
{	unsigned int	offset ;
	unsigned int	blocksize ;
} SSND_CHUNK ; 


typedef struct 
{	short           playMode;
    int		        beginLoop;
	int		        endLoop;
} INST_CHUNK ;

/*------------------------------------------------------------------------------
 * Private static functions.
 */

static	int		aiff_close	(SF_PRIVATE  *psf) ;

static	int     tenbytefloat2int (unsigned char *bytes) ;
static	void	uint2tenbytefloat (unsigned int num, unsigned char *bytes) ;

static 
void    endswap_comm_fmt (COMM_CHUNK *comm)
{	comm->numChannels     = ENDSWAP_SHORT (comm->numChannels) ;
	comm->numSampleFrames = ENDSWAP_INT (comm->numSampleFrames) ;
	comm->sampleSize      = ENDSWAP_SHORT (comm->sampleSize) ;
} /* endswap_comm_fmt */

static 
void    endswap_ssnd_fmt (SSND_CHUNK *ssnd)
{	ssnd->offset 	= ENDSWAP_INT (ssnd->offset) ;
	ssnd->blocksize = ENDSWAP_INT (ssnd->blocksize) ;
} /* endswap_ssnd_fmt */


/*------------------------------------------------------------------------------
** Public functions.
*/

int 	aiff_open_read	(SF_PRIVATE *psf)
{	COMM_CHUNK	comm_fmt ;
	SSND_CHUNK	ssnd_fmt ;
	int			marker, dword ;
	long		FORMsize, commsize, SSNDsize ;
	int			filetype, parsestage = 0, done = 0 ;
	
	while (! done)
	{	fread (&marker, sizeof (marker), 1, psf->file) ;
		switch (marker)
		{	case FORM_MARKER :
					if (parsestage != 0)
						return SFE_AIFF_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					FORMsize = BE2H_INT (dword) ;

					if (FORMsize != psf->filelength - 2 * sizeof (dword))
					{	dword = psf->filelength - 2 * sizeof (dword);
						psf_sprintf (psf, "FORM : %d (should be %d)\n", FORMsize, dword) ;
						FORMsize = dword ;
						}
					else
						psf_sprintf (psf, "FORM : %d\n", FORMsize) ;
					parsestage = 1 ;
					break ;
					
			case AIFC_MARKER :
			case AIFF_MARKER :
					if (parsestage != 1)
						return SFE_AIFF_NO_FORM ;
					filetype = marker ;
					psf_sprintf (psf, " %D\n", marker) ;
					parsestage = 2 ;
					break ;

			case COMM_MARKER :
					if (parsestage != 2)
						return SFE_AIFF_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					commsize = BE2H_INT (dword) ;
					
					/* The COMM chunk has an int aligned to a word boundary. Some procesors 
					** are not able to deal with this (ie bus fault) so we have to take
					** special care.
					*/

					fread (&(comm_fmt.numChannels), sizeof (comm_fmt.numChannels), 1, psf->file) ;
					fread (&(comm_fmt.numSampleFrames), sizeof (comm_fmt.numSampleFrames), 1, psf->file) ;
					fread (&(comm_fmt.sampleSize), sizeof (comm_fmt.sampleSize), 1, psf->file) ;
					fread (&(comm_fmt.sampleRate), sizeof (comm_fmt.sampleRate), 1, psf->file) ;

					if (CPU_IS_LITTLE_ENDIAN)
						endswap_comm_fmt (&comm_fmt) ;

					psf->sf.samplerate 		= tenbytefloat2int (comm_fmt.sampleRate) ;
					psf->sf.samples 		= comm_fmt.numSampleFrames ;
					psf->sf.channels 		= comm_fmt.numChannels ;
					psf->sf.pcmbitwidth 	= comm_fmt.sampleSize ;
					psf->sf.format 			= (SF_FORMAT_AIFF | SF_FORMAT_PCM);
					psf->sf.sections 		= 1 ;
					
					psf_sprintf (psf, " COMM : %d\n", commsize) ;
					psf_sprintf (psf, "  Sample Rate : %d\n", psf->sf.samplerate) ;
					psf_sprintf (psf, "  Samples     : %d\n", comm_fmt.numSampleFrames) ;
					psf_sprintf (psf, "  Channels    : %d\n", comm_fmt.numChannels) ;
					psf_sprintf (psf, "  Sample Size : %d\n", comm_fmt.sampleSize) ;
					
					if (commsize > REAL_COMM_SIZE)
					{	fread (&dword, sizeof (dword), 1, psf->file) ;
						if (dword != NONE_MARKER)
						{	psf_sprintf (psf, "AIFC : Unimplemented format : %D\n", dword) ;
							return SFE_UNIMPLEMENTED ;
							} ;
						fseek (psf->file, commsize - (long) (sizeof (dword) + REAL_COMM_SIZE), SEEK_CUR) ;
						} ;
					
					parsestage = 3 ;
					break ;

			case SSND_MARKER :
					if (parsestage != 3)
						return SFE_AIFF_NO_SSND ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					SSNDsize = BE2H_INT (dword) ;
					
					fread (&ssnd_fmt, sizeof (SSND_CHUNK), 1, psf->file) ;
					if (CPU_IS_LITTLE_ENDIAN)
						endswap_ssnd_fmt (&ssnd_fmt) ;
						
					psf->dataoffset = ftell (psf->file) ;
					psf->datalength = psf->filelength - psf->dataoffset ;
					if (SSNDsize != psf->datalength + sizeof (SSND_CHUNK))
						psf_sprintf (psf, " SSND : %d (should be %d)\n", SSNDsize, psf->datalength + sizeof (SSND_CHUNK)) ;
					else
						psf_sprintf (psf, " SSND : %d\n", SSNDsize) ;
					
					psf_sprintf (psf, "  Offset     : %d\n", ssnd_fmt.offset) ;
					psf_sprintf (psf, "  Block Size : %d\n", ssnd_fmt.blocksize) ;
					

					fseek (psf->file, psf->datalength, SEEK_CUR) ;
					dword = ftell (psf->file) ;
					if (dword != (off_t) (psf->dataoffset + psf->datalength))
						psf_sprintf (psf, "*** fseek past end error ***\n", dword, psf->dataoffset + psf->datalength) ;
					parsestage = 4 ;
					break ;

			case NAME_MARKER :
			case AUTH_MARKER :
			case ANNO_MARKER :
			case c_MARKER :
			case FVER_MARKER :
					if (parsestage < 2)
						return SFE_AIFF_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = BE2H_INT (dword) ;
					psf_sprintf (psf, " %D : %d\n", marker, dword) ;
					fseek (psf->file, dword, SEEK_CUR) ;					
					break ;

			case MARK_MARKER :
			case INST_MARKER :
			case APPL_MARKER :
					if (parsestage < 2)
						return SFE_AIFF_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = BE2H_INT (dword) ;
					psf_sprintf (psf, " %D : %d\n", marker, dword) ;
					fseek (psf->file, dword, SEEK_CUR) ;					
					break ;

			default : 
					if (isprint ((marker >> 24) & 0xFF) && isprint ((marker >> 16) & 0xFF)
						&& isprint ((marker >> 8) & 0xFF) && isprint (marker & 0xFF))
					{	fread (&dword, sizeof (dword), 1, psf->file) ;
						psf_sprintf (psf, "%D : %d (unknown marker)\n", marker, dword) ;
						fseek (psf->file, dword, SEEK_CUR) ;
						break ;
						} ;
					if ((dword = ftell (psf->file)) & 0x03)
					{	psf_sprintf (psf, "  Unknown chunk marker at position %d. Resynching.\n", dword - 4) ;
						fseek (psf->file, -3, SEEK_CUR) ;
						break ;
						} ;
					psf_sprintf (psf, "*** Unknown chunk marker : %X. Exiting parser.\n", marker) ;
					done = 1 ;
					break ;
			} ;	/* switch (marker) */

		if (ferror (psf->file))
		{	psf_sprintf (psf, "*** Error on file handle. ***\n") ;
			clearerr (psf->file) ;
			break ;
			} ;

		if (ftell (psf->file) >= (off_t) (psf->filelength - (2 * sizeof (dword))))
			break ;
		} ; /* while (1) */
		
	if (! psf->dataoffset)
		return SFE_AIFF_NO_DATA ;

	psf->current     = 0 ;
	psf->endian      = SF_ENDIAN_BIG ;			/* All AIF* files are big endian. */
	psf->sf.seekable = SF_TRUE ;
	psf->bytewidth   = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;

	psf->blockwidth  = psf->sf.channels * psf->bytewidth ;
	
	fseek (psf->file, psf->dataoffset, SEEK_SET) ;

	psf->close = (func_close) aiff_close ;

	switch (psf->bytewidth)
	{	case  1 :
				psf->read_short  = (func_short) pcm_read_sc2s ;
				psf->read_int    = (func_int) pcm_read_sc2i ;
				psf->read_double = (func_double) pcm_read_sc2d ;
				break ;
		case  2 :
				psf->read_short  = (func_short) pcm_read_bes2s ;
				psf->read_int    = (func_int) pcm_read_bes2i ;
				psf->read_double = (func_double) pcm_read_bes2d ;
				break ;
		case  3 :
				psf->read_short  = (func_short) pcm_read_bet2s ;
				psf->read_int    = (func_int) pcm_read_bet2i ;
				psf->read_double = (func_double) pcm_read_bet2d ;
				break ;
		case  4 :
				psf->read_short  = (func_short) pcm_read_bei2s ;
				psf->read_int    = (func_int) pcm_read_bei2i ;
				psf->read_double = (func_double) pcm_read_bei2d ;
				break ;
		default : 
				/* printf ("Weird bytewidth (%d)\n", psf->bytewidth) ; */
				return SFE_UNIMPLEMENTED ;
		} ;

	return 0 ;
} /* aiff_open_read */

/*------------------------------------------------------------------------------
 */

int 	aiff_open_write	(SF_PRIVATE *psf)
{	COMM_CHUNK			comm_fmt ;
	SSND_CHUNK			ssnd_fmt ;
	unsigned int		dword, FORMsize ;
	
	if ((psf->sf.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_AIFF)
		return	SFE_BAD_OPEN_FORMAT ;
	if ((psf->sf.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM)
		return	SFE_BAD_OPEN_FORMAT ;
	
	psf->endian      = SF_ENDIAN_BIG ;			/* All AIF* files are big endian. */
	psf->sf.seekable = SF_TRUE ;
	psf->bytewidth   = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;
	psf->dataoffset  = 5 * sizeof (dword) + REAL_COMM_SIZE + 4 * sizeof (dword) ;
	psf->datalength  = psf->blockwidth * psf->sf.samples ;
	psf->filelength  = psf->datalength + psf->dataoffset ;
	psf->error       = 0 ;

	FORMsize   = 0x7FFFFFFF ;  /* Correct this when closing file. */
	
	comm_fmt.numChannels     = psf->sf.channels ;
	comm_fmt.numSampleFrames = psf->sf.samples ;
	comm_fmt.sampleSize      = psf->sf.pcmbitwidth ;
	uint2tenbytefloat (psf->sf.samplerate, comm_fmt.sampleRate)  ;

	if (CPU_IS_LITTLE_ENDIAN)
		endswap_comm_fmt (&comm_fmt) ;

	ssnd_fmt.offset    = 0 ;
	ssnd_fmt.blocksize = 0 ;  /* Not normally used. */

	if (CPU_IS_LITTLE_ENDIAN)
		endswap_ssnd_fmt (&ssnd_fmt) ;

	dword = FORM_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
 	dword = H2BE_INT (FORMsize) ;
 	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	dword = AIFF_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	dword = COMM_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	dword = H2BE_INT (REAL_COMM_SIZE) ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;

	fwrite (&(comm_fmt.numChannels), sizeof (comm_fmt.numChannels), 1, psf->file) ;
	fwrite (&(comm_fmt.numSampleFrames), sizeof (comm_fmt.numSampleFrames), 1, psf->file) ;
	fwrite (&(comm_fmt.sampleSize), sizeof (comm_fmt.sampleSize), 1, psf->file) ;
	fwrite (&(comm_fmt.sampleRate), sizeof (comm_fmt.sampleRate), 1, psf->file) ;

	dword = SSND_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	dword = H2BE_INT (psf->datalength + sizeof (SSND_CHUNK)) ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;

	fwrite (&ssnd_fmt, sizeof (ssnd_fmt), 1, psf->file) ;
	
	psf->close = (func_close) aiff_close ;

	switch (psf->bytewidth)
	{	case  1 :
				psf->write_short  = (func_short) pcm_write_s2sc ;
				psf->write_int    = (func_int) pcm_write_i2sc ;
				psf->write_double = (func_double) pcm_write_d2sc ;
				break ;
		case  2 :
				psf->write_short  = (func_short) pcm_write_s2bes ;
				psf->write_int    = (func_int) pcm_write_i2bes ;
				psf->write_double = (func_double) pcm_write_d2bes ;
				break ;
		case  3 :
				psf->write_short  = (func_short) pcm_write_s2bet ;
				psf->write_int    = (func_int) pcm_write_i2bet ;
				psf->write_double = (func_double) pcm_write_d2bet ;
				break ;
		case  4 :
				psf->write_short  = (func_short) pcm_write_s2bei ;
				psf->write_int    = (func_int) pcm_write_i2bei ;
				psf->write_double = (func_double) pcm_write_d2bei ;
				break ;
		default : return SFE_UNIMPLEMENTED ;
		} ;

	return 0 ;
} /* aiff_open_write */

/*------------------------------------------------------------------------------
 */

int	aiff_close	(SF_PRIVATE  *psf)
{	unsigned int	dword ;

	if (psf->mode == SF_MODE_WRITE)
	{	/*  Now we know for certain the length of the file we can re-write 
		**	correct values for the FORM, COMM and SSND chunks.
		*/
		 
		fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;
		
 		dword = psf->filelength - 2 * sizeof (dword) ;
 		fseek (psf->file, sizeof (dword), SEEK_SET) ;
 		dword = H2BE_INT (dword) ;
 		fwrite (&dword, sizeof (dword), 1, psf->file) ;		/* FORM */
		
		fseek (psf->file, psf->dataoffset - (long)((sizeof (SSND_CHUNK) + sizeof (dword))), SEEK_SET) ;
		psf->datalength = psf->filelength - psf->dataoffset ;
 		dword = H2BE_INT (psf->datalength + sizeof (SSND_CHUNK)) ;
 		fwrite (&dword, sizeof (dword), 1, psf->file) ;		/* SSND */
		
 		fseek (psf->file, 5 * sizeof (dword) + sizeof (short), SEEK_SET) ;
		dword = psf->datalength / (psf->bytewidth * psf->sf.channels) ;
		dword = H2BE_INT (dword) ;
 		fwrite (&dword, sizeof (dword), 1, psf->file) ;		/* COMM.numSampleFrames */
		} ;

	if (psf->fdata)
		free (psf->fdata) ;
	psf->fdata = NULL ;
	
	return 0 ;
} /* aiff_close */

/*==========================================================================================
**	Rough hack at converting from 80 bit IEEE float in AIFF header to an int and
**	back again. It assumes that all sample rates are between 1 and 800MHz, which 
**	should be OK as other sound file formats use a 32 bit integer to store sample 
**	rate.
**	There is another (probably better) version in the source code to the SoX but it
**	has a copyright which probably prevents it from being allowable as GPL/LGPL.
*/

static
int     tenbytefloat2int (unsigned char *bytes)
{       int val = 3 ;

	if (bytes [0] & 0x80)   /* Negative number. */
		return 0 ;

	if (bytes [0] <= 0x3F)  /* Less than 1. */
		return 1 ;
		
	if (bytes [0] > 0x40)   /* Way too big. */
		return  0x4000000 ;

	if (bytes [0] == 0x40 && bytes [1] > 0x1C) /* Too big. */
		return 800000000 ;
		
	/* Ok, can handle it. */
		
	val = (bytes [2] << 23) | (bytes [3] << 15) | (bytes [4] << 7)  | (bytes [5] >> 1) ;
	
	val >>= (29 - bytes [1]) ;
	
	return val ;
} /* tenbytefloat2int */

static
void uint2tenbytefloat (unsigned int num, unsigned char *bytes)
{	int		count, mask = 0x40000000 ;

	memset (bytes, 0, 10) ;

	if (num <= 1)
	{	bytes [0] = 0x3F ;
		bytes [1] = 0xFF ;
		bytes [2] = 0x80 ;
		return ;
		} ;

	bytes [0] = 0x40 ;
	
	if (num >= mask)
	{	bytes [1] = 0x1D ;
		return ;
		} ;
	
	for (count = 0 ; count <= 32 ; count ++)
	{	if (num & mask)
			break ;
		mask >>= 1 ;
		} ;
		
	num <<= count + 1 ;
	bytes [1] = 29 - count ;
	bytes [2] = (num >> 24) & 0xFF ;
	bytes [3] = (num >> 16) & 0xFF ;
	bytes [4] = (num >>  8) & 0xFF ;
	bytes [5] = num & 0xFF ;
	
} /* uint2tenbytefloat */

