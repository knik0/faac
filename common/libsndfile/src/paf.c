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
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>

#include	"sndfile.h"
#include	"config.h"
#include	"sfendian.h"
#include	"common.h"
#include	"pcm.h"


/*------------------------------------------------------------------------------
** Macros to handle big/little endian issues.
*/

#if (CPU_IS_LITTLE_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		((a)|((b)<<8)|((c)<<16)|((d)<<24))
#elif (CPU_IS_BIG_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		(((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#	error "Cannot determine endian-ness of processor."
#endif

#define FAP_MARKER	(MAKE_MARKER ('f', 'a', 'p', ' ')) 
#define PAF_MARKER	(MAKE_MARKER (' ', 'p', 'a', 'f')) 

/*------------------------------------------------------------------------------
** Other defines.
*/

#define	PAF_HEADER_LENGTH 	2048

/*------------------------------------------------------------------------------
** Typedefs.
*/

typedef	struct
{	unsigned int	version ;
	unsigned int	endianness ;
    unsigned int	samplerate ;
    unsigned int	format ;
	unsigned int	channels ;
	unsigned int	source ;
} PAF_FMT ;

typedef struct
{	unsigned int	index, blocks, channels, samplesperblock, blockcount, blocksize, samplecount ;
	unsigned char	*block ;
	int				*samples ;
	unsigned char	data [4] ;
} PAF24_PRIVATE ;



/*------------------------------------------------------------------------------
** Private static functions.
*/

static int paf24_reader_init (SF_PRIVATE  *psf) ;
static int paf24_writer_init (SF_PRIVATE  *psf) ;

static int paf24_read_block (SF_PRIVATE  *psf, PAF24_PRIVATE *ppaf24) ;
	
static int paf24_close (SF_PRIVATE  *psf) ;

static int paf24_read_s (SF_PRIVATE *psf, short *ptr, int len) ;
static int paf24_read_i (SF_PRIVATE *psf, int *ptr, int len) ;
static int paf24_read_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

static int paf24_write_s (SF_PRIVATE *psf, short *ptr, int len) ;
static int paf24_write_i (SF_PRIVATE *psf, int *ptr, int len) ;
static int paf24_write_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

static off_t paf24_seek (SF_PRIVATE *psf, off_t offset, int whence) ;

static void	endswap_paf_fmt (PAF_FMT *ppaf_fmt) ;

/*------------------------------------------------------------------------------
** Public functions.
*/

int 	paf_open_read	(SF_PRIVATE *psf)
{	PAF_FMT			paf_fmt ;
	unsigned int	marker ;
	int				error ;
	
	fread (&marker, sizeof (marker), 1, psf->file) ;
	if (marker != PAF_MARKER && marker != FAP_MARKER)
		return SFE_PAF_NO_MARKER ;
		
	psf_sprintf (psf, "Signature   : %D\n", marker) ;

	fread (&paf_fmt, sizeof (PAF_FMT), 1, psf->file) ;
	
	if (CPU_IS_LITTLE_ENDIAN && marker == PAF_MARKER)
		endswap_paf_fmt (&paf_fmt) ;
	else if (CPU_IS_BIG_ENDIAN && marker == FAP_MARKER)
		endswap_paf_fmt (&paf_fmt) ;

	psf_sprintf (psf, "Version     : %d\n", paf_fmt.version) ;
	if (paf_fmt.version != 0)
	{	psf_sprintf (psf, "*** Bad version number. Should be zero.\n") ;
		return SFE_PAF_VERSION ;
		} ;
		
	psf_sprintf (psf, "Endianness  : %d => ", paf_fmt.endianness) ;
	if (paf_fmt.endianness)
		psf_sprintf (psf, "Little\n", paf_fmt.endianness) ;
	else
		psf_sprintf (psf, "Big\n", paf_fmt.endianness) ;
	psf_sprintf (psf, "Sample Rate : %d\n", paf_fmt.samplerate) ;
	

	if (psf->filelength < PAF_HEADER_LENGTH)
		return SFE_PAF_SHORT_HEADER ;
		
 	psf->dataoffset = PAF_HEADER_LENGTH ;
	psf->datalength = psf->filelength - psf->dataoffset ;

 	psf->current  = 0 ;
	psf->endian   = paf_fmt.endianness ? SF_ENDIAN_LITTLE : SF_ENDIAN_BIG ;
	psf->sf.seekable = SF_TRUE ;
 	
 	if (fseek (psf->file, psf->dataoffset, SEEK_SET))
		return SFE_BAD_SEEK ;

	psf->sf.samplerate	= paf_fmt.samplerate ;
	psf->sf.channels 	= paf_fmt.channels ;
					
	/* Only fill in type major. */
	psf->sf.format = SF_FORMAT_PAF ;

	psf->sf.sections 	= 1 ;

	psf_sprintf (psf, "Format      : %d => ", paf_fmt.format) ;

	switch (paf_fmt.format)
	{	case  0 :	psf_sprintf (psf, "16 bit linear PCM\n") ;
					psf->sf.pcmbitwidth = 16 ;
					psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;

					psf->sf.format |= paf_fmt.endianness ? SF_FORMAT_PCM_LE : SF_FORMAT_PCM_BE ; ;
					
					if (psf->endian == SF_ENDIAN_BIG)
					{	psf->read_short  = (func_short)  pcm_read_bes2s ;
						psf->read_int    = (func_int)    pcm_read_bes2i ;
						psf->read_double = (func_double) pcm_read_bes2d ;
						}
					else
					{	psf->read_short  = (func_short)  pcm_read_les2s ;
						psf->read_int    = (func_int)    pcm_read_les2i ;
						psf->read_double = (func_double) pcm_read_les2d ;
						} ;
					psf->blockwidth = psf->bytewidth * psf->sf.channels ;

	psf_sprintf (psf, "X blockwidth : %d\n", psf->blockwidth) ;

					if (psf->blockwidth)
						psf->sf.samples = psf->datalength / psf->blockwidth ;
					else
						psf_sprintf (psf, "*** Warning : blockwidth == 0.\n") ;
					
	psf_sprintf (psf, "X samples : %d\n", psf->sf.samples) ;
					break ;

		case  1 :	psf_sprintf (psf, "24 bit linear PCM\n") ;
					psf->sf.pcmbitwidth = 24 ;
					psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;

					psf->sf.format |= paf_fmt.endianness ? SF_FORMAT_PCM_LE : SF_FORMAT_PCM_BE ; ;
					
					psf->read_short  = (func_short)  paf24_read_s ;
					psf->read_int    = (func_int)    paf24_read_i ;
					psf->read_double = (func_double) paf24_read_d ;

					if ((error = paf24_reader_init (psf)))
						return error ;

					psf->seek_func = (func_seek) paf24_seek ;
					psf->close = (func_close) paf24_close ;

					psf->blockwidth = psf->bytewidth * psf->sf.channels ;
	psf_sprintf (psf, "X blockwidth : %d\n", psf->blockwidth) ;
					psf->sf.samples = 10 * psf->datalength / (32 * psf->sf.channels) ;
	psf_sprintf (psf, "X samples : %d\n", psf->sf.samples) ;
					break ;
		
		default :   psf_sprintf (psf, "Unknown\n") ;
					return SFE_PAF_UNKNOWN_FORMAT ;
					break ;
		} ;

	psf_sprintf (psf, "Channels    : %d\n", paf_fmt.channels) ;
	psf_sprintf (psf, "Source      : %d => ", paf_fmt.source) ;
	
	switch (paf_fmt.source)
	{	case  1 : psf_sprintf (psf, "Analog Recording\n") ;
					break ;
		case  2 : psf_sprintf (psf, "Digital Transfer\n") ;
					break ;
		case  3 : psf_sprintf (psf, "Multi-track Mixdown\n") ;
					break ;
		case  5 : psf_sprintf (psf, "Audio Resulting From DSP Processing\n") ;
					break ;
		default : psf_sprintf (psf, "Unknown\n") ;
					break ;
		} ;

	return 0 ;
} /* paf_open_read */

/*------------------------------------------------------------------------------
*/

int 	paf_open_write	(SF_PRIVATE *psf)
{	PAF_FMT			paf_fmt ;
	int				format, subformat, error, count ;
	unsigned int	marker, big_endian_file ;
	
	format = psf->sf.format & SF_FORMAT_TYPEMASK ;
	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;

	if (format != SF_FORMAT_PAF)
		return	SFE_BAD_OPEN_FORMAT ;

	if (subformat == SF_FORMAT_PCM_BE)
	{	big_endian_file = 1 ;
		paf_fmt.endianness = 0 ;
		}
	else if (subformat == SF_FORMAT_PCM_LE)
	{	big_endian_file = 0 ;
		paf_fmt.endianness = 1 ;
		}
	else
		return	SFE_BAD_OPEN_FORMAT ;
		
	paf_fmt.version    = 0 ;
	paf_fmt.samplerate = psf->sf.samplerate ;
	
	switch (psf->sf.pcmbitwidth)
	{	case  16 : 	paf_fmt.format = 0 ;
					psf->bytewidth = 2 ;
					break ;

		case  24 :	paf_fmt.format = 1 ;
					psf->bytewidth = 3 ;
					break ;
	
		default : return SFE_PAF_UNKNOWN_FORMAT ;
		} ;
		
	paf_fmt.channels   = psf->sf.channels ;
	paf_fmt.source     = 0 ;

	psf->bytewidth = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
		
	psf->endian      = big_endian_file ? SF_ENDIAN_BIG : SF_ENDIAN_LITTLE ;
	psf->sf.seekable = SF_TRUE ;
	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;
 	psf->dataoffset  = PAF_HEADER_LENGTH ;
	psf->datalength  = psf->blockwidth * psf->sf.samples ;
	psf->filelength  = psf->datalength + psf->dataoffset ;
	psf->error       = 0 ;

	switch (paf_fmt.format)
	{	case  0 :	/* 16-bit linear PCM. */
					if (big_endian_file)
					{	psf->write_short  = (func_short)  pcm_write_s2bes ;
						psf->write_int    = (func_int)    pcm_write_i2bes ;
						psf->write_double = (func_double) pcm_write_d2bes ;
						}
					else
					{	psf->write_short  = (func_short)  pcm_write_s2les ;
						psf->write_int    = (func_int)    pcm_write_i2les ;
						psf->write_double = (func_double) pcm_write_d2les ;
						} ;
					break ;

		case  1 :	/* 24-bit linear PCM */
					psf->write_short  = (func_short)  paf24_write_s ;
					psf->write_int    = (func_int)    paf24_write_i ;
					psf->write_double = (func_double) paf24_write_d ;
					
					if ((error = paf24_writer_init (psf)))
						return error ;
					
					psf->seek_func = (func_seek) paf24_seek ;
					psf->close = (func_close) paf24_close ;
					break ;

		default :   break ;
		} ;
		
	if (big_endian_file)
	{	if (CPU_IS_LITTLE_ENDIAN)
			endswap_paf_fmt	(&paf_fmt) ;
		marker = PAF_MARKER ;
		}
	else
	{	if (CPU_IS_BIG_ENDIAN)
			endswap_paf_fmt	(&paf_fmt) ;
		marker = FAP_MARKER ;
		} ;
	
	fwrite (&marker, sizeof (marker), 1, psf->file) ;
	fwrite (&paf_fmt, sizeof (PAF_FMT), 1, psf->file) ;

	/* Fill the file from current position to dataoffset with zero bytes. */
	memset (psf->buffer, 0, sizeof (psf->buffer)) ;
	count = psf->dataoffset - ftell (psf->file) ;
	while (count > 0)
	{	int current = (count > sizeof (psf->buffer)) ? sizeof (psf->buffer) : count ; 
		fwrite (psf->buffer, current, 1, psf->file) ;
		count -= current ;
		} ;
		
	return 0 ;
} /* paf_open_write */

/*===============================================================================
**	24 bit PAF files have a really weird encoding. 
**  For a mono file, 10 samples (each being 3 bytes) are packed into a 32 byte 
**	block. The 8 ints in this 32 byte block are then endian swapped (as ints) 
**	if necessary before being written to disk.
**  For a stereo file, blocks of 10 samples from the same channel are encoded 
**  into 32 bytes as fro the mono case. The 32 block bytes are then interleaved 
**	on disk.
**	Reading has to reverse the above process :-).
**	Weird!!!
**
**	The code below attempts to gain efficiency while maintaining readability.
*/

static 
int paf24_reader_init (SF_PRIVATE  *psf)
{	PAF24_PRIVATE	*ppaf24 ; 
	unsigned int	paf24size ;	
	
	paf24size = sizeof (PAF24_PRIVATE) + psf->sf.channels * (32 + 10 * sizeof (int)) ;
	if (! (psf->fdata = malloc (paf24size)))
		return SFE_MALLOC_FAILED ;
		
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	memset (ppaf24, 0, paf24size) ;
	
	ppaf24->channels = psf->sf.channels ;
	ppaf24->block    = (unsigned char*) ppaf24->data ;
	ppaf24->samples  = (int*) (ppaf24->data + 32 * ppaf24->channels) ;
	
	ppaf24->blocksize = 32 * ppaf24->channels ;
	ppaf24->samplesperblock = 10 ;
	
	if (psf->datalength % ppaf24->blocksize)
	{	psf_sprintf (psf, "*** Warning : file seems to be truncated.\n") ;
		ppaf24->blocks = psf->datalength / ppaf24->blocksize  + 1 ;
		}
	else
		ppaf24->blocks = psf->datalength / ppaf24->blocksize ;
		
	psf->sf.samples = ppaf24->samplesperblock * ppaf24->blocks ;

	paf24_read_block (psf, ppaf24) ;	/* Read first block. */
	
	return 0 ;
} /* paf24_reader_init */


static 
int	paf24_read_block (SF_PRIVATE  *psf, PAF24_PRIVATE *ppaf24)
{	int				k, *iptr, newsample, channel ;
	unsigned char	*cptr ;
	
	ppaf24->blockcount ++ ;
	ppaf24->samplecount = 0 ;
	
	if (ppaf24->blockcount > ppaf24->blocks)
	{	memset (ppaf24->samples, 0, ppaf24->samplesperblock * ppaf24->channels) ;
		return 1 ;
		} ;
		
	/* Read the block. */

	if ((k = fread (ppaf24->block, 1, ppaf24->blocksize, psf->file)) != ppaf24->blocksize)
		psf_sprintf (psf, "*** Warning : short read (%d != %d).\n", k, ppaf24->blocksize) ;

	/* Do endian swapping if necessary. */

	iptr = (int*) (ppaf24->data) ;
	if ((CPU_IS_BIG_ENDIAN && psf->endian == SF_ENDIAN_LITTLE) ||
			(CPU_IS_LITTLE_ENDIAN && psf->endian == SF_ENDIAN_BIG))
	{	for (k = 0 ; k < 8 * ppaf24->channels ; k++)
			iptr [k] = ENDSWAP_INT (iptr [k]) ;
		} ;
		
	/* Unpack block. */	

	for (k = 0 ; k < 10 * ppaf24->channels ; k++)
	{	channel = k % ppaf24->channels ;
		cptr = ppaf24->block + 32 * channel + 3 * (k / ppaf24->channels) ;
		newsample = (cptr [0] << 8) | (cptr [1] << 16) | (cptr [2] << 24) ;
		ppaf24->samples [k] = newsample / 256 ;
		} ;

	return 1 ;
} /* paf24_read_block */

static
int paf24_read (SF_PRIVATE *psf, PAF24_PRIVATE *ppaf24, int *ptr, int len)
{	int		count, total = 0, index = 0 ;
	
	while (index < len)
	{	if (ppaf24->blockcount >= ppaf24->blocks && ppaf24->samplecount >= ppaf24->samplesperblock)
		{	memset (&(ptr[index]), 0, (len - index) * sizeof (int)) ;
			return total ;
			} ;
		
		if (ppaf24->samplecount >= ppaf24->samplesperblock)
			paf24_read_block (psf, ppaf24) ;
		
		count = (ppaf24->samplesperblock - ppaf24->samplecount) * ppaf24->channels ;
		count = (len - index > count) ? count : len - index ;
		
		memcpy (&(ptr[index]), &(ppaf24->samples [ppaf24->samplecount * ppaf24->channels]), count * sizeof (int)) ;
		index += count ;
		ppaf24->samplecount += count / ppaf24->channels ;
		total = index ;
		} ;

	return total ;		
} /* paf24_read */

static 
int paf24_read_s (SF_PRIVATE *psf, short *ptr, int len)
{	PAF24_PRIVATE 	*ppaf24 ; 
	int				*iptr ;
	int				k, bufferlen, readcount = 0, count ;
	int				index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	iptr = (int*) psf->buffer ;
	bufferlen = psf->sf.channels * ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (int) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : len ;
		count = paf24_read (psf, ppaf24, iptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [index+k] = (short) (iptr [k] / 256) ;
		index += readcount ;
		total += count ;
		len -= readcount ;
		} ;
	return total ;
} /* paf24_read_s */

static 
int paf24_read_i (SF_PRIVATE *psf, int *ptr, int len) 
{	PAF24_PRIVATE *ppaf24 ; 
	int				total ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	total = paf24_read (psf, ppaf24, ptr, len) ;

	return total ;
} /* paf24_read_i */

static 
int paf24_read_d (SF_PRIVATE *psf, double *ptr, int len, int normalize)
{	PAF24_PRIVATE 	*ppaf24 ; 
	int				*iptr ;
	int				k, bufferlen, readcount = 0, count ;
	int				index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	iptr = (int*) psf->buffer ;
	bufferlen = psf->sf.channels * ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (int) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : len ;
		count = paf24_read (psf, ppaf24, iptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [index+k] = (double) (iptr [k]) ;
		index += readcount ;
		total += count ;
		len -= readcount ;
		} ;
	return total ;
} /* paf24_read_d */

/*---------------------------------------------------------------------------
*/

static 
int paf24_writer_init (SF_PRIVATE  *psf)
{	PAF24_PRIVATE	*ppaf24 ; 
	unsigned int	paf24size ;	
	
	paf24size = sizeof (PAF24_PRIVATE) + psf->sf.channels * (32 + 10 * sizeof (int)) ;
	if (! (psf->fdata = malloc (paf24size)))
		return SFE_MALLOC_FAILED ;
		
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	memset (ppaf24, 0, paf24size) ;
	
	ppaf24->channels = psf->sf.channels ;
	ppaf24->block    = (unsigned char*) ppaf24->data ;
	ppaf24->samples  = (int*) (ppaf24->data + 32 * ppaf24->channels) ;
	
	ppaf24->blocksize = 32 * ppaf24->channels ;
	ppaf24->samplesperblock = 10 ;
	
	if (psf->datalength % ppaf24->blocksize)
	{	psf_sprintf (psf, "*** Warning : file seems to be truncated.\n") ;
		ppaf24->blocks = psf->datalength / ppaf24->blocksize  + 1 ;
		}
	else
		ppaf24->blocks = psf->datalength / ppaf24->blocksize ;
		
	psf->sf.samples = ppaf24->samplesperblock * ppaf24->blocks ;

	return 0 ;
} /* paf24_writer_init */


static 
int	paf24_write_block (SF_PRIVATE  *psf, PAF24_PRIVATE *ppaf24)
{	int				k, *iptr, nextsample, channel ;
	unsigned char	*cptr ;
	
	/* First pack block. */	

	for (k = 0 ; k < 10 * ppaf24->channels ; k++)
	{	channel = k % ppaf24->channels ;
		cptr = ppaf24->block + 32 * channel + 3 * (k / ppaf24->channels) ;
		nextsample = ppaf24->samples [k] ;
		cptr [0] = nextsample & 0xFF ;
		cptr [1] = (nextsample >> 8) & 0xFF ;
		cptr [2] = (nextsample >> 16) & 0xFF ;
		} ;
	
	/* Do endian swapping if necessary. */

	iptr = (int*) (ppaf24->data) ;
	if ((CPU_IS_BIG_ENDIAN && psf->endian == SF_ENDIAN_LITTLE) ||
			(CPU_IS_LITTLE_ENDIAN && psf->endian == SF_ENDIAN_BIG))
	{	for (k = 0 ; k < 8 * ppaf24->channels ; k++)
			iptr [k] = ENDSWAP_INT (iptr [k]) ;
		} ;
		
	/* Write block to disk. */
	
	if ((k = fwrite (ppaf24->block, 1, ppaf24->blocksize, psf->file)) != ppaf24->blocksize)
		psf_sprintf (psf, "*** Warning : short write (%d != %d).\n", k, ppaf24->blocksize) ;

	ppaf24->blockcount ++ ;
	ppaf24->samplecount = 0 ;
	
	return 1 ;
} /* paf24_write_block */

static
int paf24_write (SF_PRIVATE *psf, PAF24_PRIVATE *ppaf24, int *ptr, int len)
{	int		count, total = 0, index = 0 ;
	
	while (index < len)
	{	count = (ppaf24->samplesperblock - ppaf24->samplecount) * ppaf24->channels ;

		if (count > len - index)
			count = len - index ;

		memcpy (&(ppaf24->samples [ppaf24->samplecount * ppaf24->channels]), &(ptr [index]), count * sizeof (int)) ;
		index += count ;
		ppaf24->samplecount += count / ppaf24->channels ;
		total = index ;

		if (ppaf24->samplecount >= ppaf24->samplesperblock)
			paf24_write_block (psf, ppaf24) ;	
		} ;

	return total ;		
} /* paf24_write */

static 
int paf24_write_s (SF_PRIVATE *psf, short *ptr, int len)
{	PAF24_PRIVATE 	*ppaf24 ; 
	int				*iptr ;
	int				k, bufferlen, writecount = 0, count ;
	int				index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	iptr = (int*) psf->buffer ;
	bufferlen = psf->sf.channels * ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (int) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : len ;
		for (k = 0 ; k < writecount ; k++)
			iptr [k] = ((int) ptr [index+k]) * 256 ;
		count = paf24_write (psf, ppaf24, iptr, writecount) ;
		index += writecount ;
		total += count ;
		len -= writecount ;
		} ;
	return total ;
} /* paf24_write_s */

static 
int paf24_write_i (SF_PRIVATE *psf, int *ptr, int len)
{	PAF24_PRIVATE 	*ppaf24 ; 
	int				total = 0 ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	total = paf24_write (psf, ppaf24, ptr, len) ;

	return total ;
} /* paf24_write_i */

static 
int paf24_write_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) 
{	PAF24_PRIVATE 	*ppaf24 ; 
	int				*iptr ;
	int				k, bufferlen, writecount = 0, count ;
	int				index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	iptr = (int*) psf->buffer ;
	bufferlen = psf->sf.channels * ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (int) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : len ;
		for (k = 0 ; k < writecount ; k++)
			iptr [k] = (int) ptr [index+k] ;
		count = paf24_write (psf, ppaf24, iptr, writecount) ;
		index += writecount ;
		total += count ;
		len -= writecount ;
		} ;
	return total ;
} /* paf24_write_d */

/*---------------------------------------------------------------------------
*/

static 
off_t paf24_seek (SF_PRIVATE *psf, off_t offset, int whence)
{	PAF24_PRIVATE	*ppaf24 ; 
	int				newblock, newsample ;
	
	if (! psf->fdata)
		return 0 ;
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;

	if (! (psf->blockwidth && psf->datalength && psf->dataoffset))
	{	psf->error = SFE_BAD_SEEK ;
		return	((off_t) -1) ;
		} ;
		
	switch (whence)
	{	case SEEK_SET :
				if (offset < 0 || offset > ppaf24->blocks * ppaf24->samplesperblock)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = offset / ppaf24->samplesperblock ;
				newsample = offset % ppaf24->samplesperblock ;
				break ;
				
		case SEEK_CUR :
				if (psf->current + offset < 0 || psf->current + offset > ppaf24->blocks * ppaf24->samplesperblock)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = (psf->current + offset) / ppaf24->samplesperblock ;
				newsample = (psf->current + offset) % ppaf24->samplesperblock ;
				break ;
				
		case SEEK_END :
				if (offset > 0 || ppaf24->samplesperblock * ppaf24->blocks + offset < 0)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = (ppaf24->samplesperblock * ppaf24->blocks + offset) / ppaf24->samplesperblock ;
				newsample = (ppaf24->samplesperblock * ppaf24->blocks + offset) % ppaf24->samplesperblock ;
				break ;
				
		default : 
				psf->error = SFE_BAD_SEEK ;
				return	((off_t) -1) ;
		} ;
		
	if (psf->mode == SF_MODE_READ)
	{	fseek (psf->file, (int) (psf->dataoffset + newblock * ppaf24->blocksize), SEEK_SET) ;
		ppaf24->blockcount  = newblock ;
		paf24_read_block (psf, ppaf24) ;
		ppaf24->samplecount = newsample ;
		}
	else
	{	/* What to do about write??? */ 
		psf->error = SFE_BAD_SEEK ;
		return	((off_t) -1) ;
		} ;

	psf->current = newblock * ppaf24->samplesperblock + newsample ;
	return psf->current ;
} /* paf24_seek */

/*---------------------------------------------------------------------------
*/

static 
int paf24_close (SF_PRIVATE  *psf)
{	PAF24_PRIVATE *ppaf24 ; 
	
	if (! psf->fdata)
		return 0 ;
		
	ppaf24 = (PAF24_PRIVATE*) psf->fdata ;
	
	if (psf->mode == SF_MODE_WRITE)
	{	if (ppaf24->samplecount && ppaf24->samplecount < ppaf24->samplesperblock)
			paf24_write_block (psf, ppaf24) ;	
		} ;
		
	free (psf->fdata) ;
	psf->fdata = NULL ;

	return 0 ;
} /* paf24_close */

/*---------------------------------------------------------------------------
*/

static
void	endswap_paf_fmt (PAF_FMT *ppaf_fmt)
{	ppaf_fmt->version    = ENDSWAP_INT (ppaf_fmt->version) ;
	ppaf_fmt->endianness = ENDSWAP_INT (ppaf_fmt->endianness) ;
	ppaf_fmt->samplerate = ENDSWAP_INT (ppaf_fmt->samplerate) ;
	ppaf_fmt->format     = ENDSWAP_INT (ppaf_fmt->format) ;
	ppaf_fmt->channels   = ENDSWAP_INT (ppaf_fmt->channels) ;
	ppaf_fmt->source     = ENDSWAP_INT (ppaf_fmt->source) ;
} /* endswap_paf_fmt */
