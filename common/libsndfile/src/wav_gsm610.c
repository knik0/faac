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
#include	<math.h>

#include	"sndfile.h"
#include	"config.h"
#include	"sfendian.h"
#include	"common.h"
#include	"wav.h"
#include	"GSM610/gsm.h"

#define		GSM610_BLOCKSIZE	65
#define		GSM610_SAMPLES		320

typedef struct
{	unsigned int	blocks ; 
	int				blockcount, samplecount ;
	unsigned char	block [GSM610_BLOCKSIZE] ;
	short			samples [GSM610_SAMPLES] ;
	gsm				gsm_data ;
} GSM610_PRIVATE ;


static	int	wav_gsm610_read_block (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610) ;
static	int wav_gsm610_read (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610, short *ptr, int len) ;

static	int	wav_gsm610_write_block (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610) ;
static	int wav_gsm610_write (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610, short *ptr, int len) ;



/*============================================================================================
** WAV GSM610 Reader initialisation function.
*/

int	wav_gsm610_reader_init (SF_PRIVATE *psf, WAV_FMT *fmt)
{	GSM610_PRIVATE	*pgsm610 ;
	int  true = 1 ;
	
	psf->sf.seekable = SF_FALSE ;
	
	if (psf->mode != SF_MODE_READ)
		return SFE_BAD_MODE_RW ;

	if (! (pgsm610 = malloc (sizeof (GSM610_PRIVATE))))
		return SFE_MALLOC_FAILED ;

	psf->fdata = (void*) pgsm610 ;

	memset (pgsm610, 0, sizeof (GSM610_PRIVATE)) ;

	if (! (pgsm610->gsm_data = gsm_create ()))
		return SFE_MALLOC_FAILED ;
	gsm_option (pgsm610->gsm_data,  GSM_OPT_WAV49, &true) ;

	if (psf->datalength % GSM610_BLOCKSIZE)
	{	psf_sprintf (psf, "*** Warning : data chunk seems to be truncated.\n") ;
		pgsm610->blocks = psf->datalength / GSM610_BLOCKSIZE + 1 ;
		}
	else
		pgsm610->blocks = psf->datalength / GSM610_BLOCKSIZE ;
	
	psf->sf.samples = GSM610_SAMPLES * pgsm610->blocks ;
	
	wav_gsm610_read_block (psf, pgsm610) ;	/* Read first block. */
	
	return 0 ;	
} /* wav_gsm610_reader_init */

/*============================================================================================
** WAV GSM610 writer initialisation function.
*/

int	wav_gsm610_writer_init (SF_PRIVATE *psf, WAV_FMT *fmt)
{	GSM610_PRIVATE	*pgsm610 ;
	int  true = 1 ;
	
	if (fmt->format != 0x0031)
		psf_sprintf (psf, "*** Warning : format tag != WAVE_FORMAT_GSM610.\n") ;
		
	if (psf->mode != SF_MODE_WRITE)
		return SFE_BAD_MODE_RW ;

	if (! (pgsm610 = malloc (sizeof (GSM610_PRIVATE))))
		return SFE_MALLOC_FAILED ;
		
	psf->fdata = (void*) pgsm610 ;

	memset (pgsm610, 0, sizeof (GSM610_PRIVATE)) ;
	
	if (! (pgsm610->gsm_data = gsm_create ()))
		return SFE_MALLOC_FAILED ;
	gsm_option (pgsm610->gsm_data,  GSM_OPT_WAV49, &true) ;

	pgsm610->blockcount  = 0 ;
	pgsm610->samplecount = 0 ;
	
	fmt->gsm610.blockalign      = GSM610_BLOCKSIZE ;
	fmt->gsm610.bitwidth        = 0 ;
	fmt->gsm610.extrabytes      = 2 ;
	fmt->gsm610.samplesperblock = GSM610_SAMPLES ;
	fmt->gsm610.bytespersec     = fmt->gsm610.samplerate * GSM610_BLOCKSIZE / GSM610_SAMPLES ;
	
	if (fmt->gsm610.bytespersec * GSM610_SAMPLES / GSM610_BLOCKSIZE < fmt->gsm610.samplerate) 
		fmt->gsm610.bytespersec ++ ;

	return 0 ;
} /* wav_gsm610_writer_init */

/*============================================================================================
** GSM 6.10 Read Functions.
*/

static
int		wav_gsm610_read_block (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610)
{	int	k ;
	
	pgsm610->blockcount ++ ;
	pgsm610->samplecount = 0 ;
	
	if (pgsm610->blockcount > pgsm610->blocks)
	{	memset (pgsm610->samples, 0, GSM610_SAMPLES * sizeof (short)) ;
		return 1 ;
		} ;

	if ((k = fread (pgsm610->block, 1, GSM610_BLOCKSIZE, psf->file)) != GSM610_BLOCKSIZE)
		psf_sprintf (psf, "*** Warning : short read (%d != %d).\n", k, GSM610_BLOCKSIZE) ;

	if (gsm_decode (pgsm610->gsm_data, pgsm610->block, pgsm610->samples) < 0)
	{	psf_sprintf (psf, "Error from gsm_decode() on frame : %d\n", pgsm610->blockcount) ;
		return 0 ;
		} ;
			
	if (gsm_decode (pgsm610->gsm_data, pgsm610->block+(GSM610_BLOCKSIZE+1)/2, pgsm610->samples+GSM610_SAMPLES/2) < 0)
	{	psf_sprintf (psf, "Error from gsm_decode() on frame : %d.5\n", pgsm610->blockcount) ;
		return 0 ;
		} ;

	return 1 ;
} /* wav_gsm610_read_block */

static
int wav_gsm610_read (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610, short *ptr, int len)
{	int		count, total = 0, index = 0 ;

	while (index < len)
	{	if (pgsm610->blockcount >= pgsm610->blocks && pgsm610->samplecount >= GSM610_SAMPLES)
		{	memset (&(ptr[index]), 0, (len - index) * sizeof (short)) ;
			return total ;
			} ;
		
		if (pgsm610->samplecount >= GSM610_SAMPLES)
			wav_gsm610_read_block (psf, pgsm610) ;
		
		count = GSM610_SAMPLES - pgsm610->samplecount ;
		count = (len - index > count) ? count : len - index ;
		
		memcpy (&(ptr[index]), &(pgsm610->samples [pgsm610->samplecount]), count * sizeof (short)) ;
		index += count ;
		pgsm610->samplecount += count ;
		total = index ;
		} ;

	return total ;		
} /* wav_gsm610_read */

int		wav_gsm610_read_s (SF_PRIVATE *psf, short *ptr, int len)
{	GSM610_PRIVATE 	*pgsm610 ; 
	int				total ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	total = wav_gsm610_read (psf, pgsm610, ptr, len) ;

	return total ;
} /* wav_gsm610_read_s */

int		wav_gsm610_read_i  (SF_PRIVATE *psf, int *ptr, int len)
{	GSM610_PRIVATE *pgsm610 ; 
	short		*sptr ;
	int			k, bufferlen, readcount = 0, count ;
	int			index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	sptr = (short*) psf->buffer ;
	bufferlen = ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (short) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : len ;
		count = wav_gsm610_read (psf, pgsm610, sptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [index+k] = (int) (sptr [k]) ;
		index += readcount ;
		total += count ;
		len -= readcount ;
		} ;
	return total ;
} /* wav_gsm610_read_i */

int		wav_gsm610_read_d  (SF_PRIVATE *psf, double *ptr, int len, int normalize)
{	GSM610_PRIVATE *pgsm610 ; 
	short		*sptr ;
	int			k, bufferlen, readcount = 0, count ;
	int			index = 0, total = 0 ;
	double		normfact ;
	
	normfact = (normalize ? 1.0 / ((double) 0x8000) : 1.0) ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	sptr = (short*) psf->buffer ;
	bufferlen = ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (short) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : len ;
		count = wav_gsm610_read (psf, pgsm610, sptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [index+k] = normfact * (double) (sptr [k]) ;
		index += readcount ;
		total += count ;
		len -= readcount ;
		} ;
	return total ;
} /* wav_gsm610_read_d */


off_t    wav_gsm610_seek   (SF_PRIVATE *psf, off_t offset, int whence)
{	GSM610_PRIVATE *pgsm610 ; 
	int			newblock, newsample ;
	
	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;

	if (! (psf->blockwidth && psf->datalength && psf->dataoffset))
	{	psf->error = SFE_BAD_SEEK ;
		return	((off_t) -1) ;
		} ;
		
	switch (whence)
	{	case SEEK_SET :
				if (offset < 0 || offset > pgsm610->blocks * GSM610_SAMPLES)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = offset / GSM610_SAMPLES ;
				newsample = offset % GSM610_SAMPLES ;
				break ;
				
		case SEEK_CUR :
				if (psf->current + offset < 0 || psf->current + offset > pgsm610->blocks * GSM610_SAMPLES)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = (psf->current + offset) / GSM610_SAMPLES ;
				newsample = (psf->current + offset) % GSM610_SAMPLES ;
				break ;
				
		case SEEK_END :
				if (offset > 0 || GSM610_SAMPLES * pgsm610->blocks + offset < 0)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				newblock  = (GSM610_SAMPLES * pgsm610->blocks + offset) / GSM610_SAMPLES ;
				newsample = (GSM610_SAMPLES * pgsm610->blocks + offset) % GSM610_SAMPLES ;
				break ;
				
		default : 
				psf->error = SFE_BAD_SEEK ;
				return	((off_t) -1) ;
		} ;
		
	if (psf->mode == SF_MODE_READ)
	{	fseek (psf->file, (int) (psf->dataoffset + newblock * GSM610_BLOCKSIZE), SEEK_SET) ;
		pgsm610->blockcount  = newblock ;
		wav_gsm610_read_block (psf, pgsm610) ;
		pgsm610->samplecount = newsample ;
		}
	else
	{	/* What to do about write??? */ 
		psf->error = SFE_BAD_SEEK ;
		return	((off_t) -1) ;
		} ;

	psf->current = newblock * GSM610_SAMPLES + newsample ;
	return psf->current ;
} /* wav_gsm610_seek */

/*==========================================================================================
** GSM 6.10 Write Functions.
*/



/*==========================================================================================
*/

static
int		wav_gsm610_write_block (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610)
{	int k ;

	/* Encode the samples. */
	gsm_encode (pgsm610->gsm_data, pgsm610->samples, pgsm610->block) ;
	gsm_encode (pgsm610->gsm_data, pgsm610->samples+GSM610_SAMPLES/2, pgsm610->block+GSM610_BLOCKSIZE/2) ;

	/* Write the block to disk. */
	if ((k = fwrite (pgsm610->block, 1, GSM610_BLOCKSIZE, psf->file)) != GSM610_BLOCKSIZE)
		psf_sprintf (psf, "*** Warning : short write (%d != %d).\n", k, GSM610_BLOCKSIZE) ;

	pgsm610->samplecount = 0 ;
	pgsm610->blockcount ++ ;

	/* Set samples to zero for next block. */
	memset (pgsm610->samples, 0, GSM610_SAMPLES * sizeof (short)) ;
			
	return 1 ;
} /* wav_gsm610_write_block */

static
int wav_gsm610_write (SF_PRIVATE *psf, GSM610_PRIVATE *pgsm610, short *ptr, int len)
{	int		count, total = 0, index = 0 ;
	
	while (index < len)
	{	count = GSM610_SAMPLES - pgsm610->samplecount ;

		if (count > len - index)
			count = len - index ;

		memcpy (&(pgsm610->samples [pgsm610->samplecount]), &(ptr [index]), count * sizeof (short)) ;
		index += count ;
		pgsm610->samplecount += count ;
		total = index ;

		if (pgsm610->samplecount >= GSM610_SAMPLES)
			wav_gsm610_write_block (psf, pgsm610) ;	
		} ;

	return total ;		
} /* wav_gsm610_write */

int		wav_gsm610_write_s (SF_PRIVATE *psf, short *ptr, int len)
{	GSM610_PRIVATE 	*pgsm610 ; 
	int				total ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	total = wav_gsm610_write (psf, pgsm610, ptr, len) ;

	return total ;
} /* wav_gsm610_write_s */

int		wav_gsm610_write_i  (SF_PRIVATE *psf, int *ptr, int len)
{	GSM610_PRIVATE *pgsm610 ; 
	short		*sptr ;
	int			k, bufferlen, writecount = 0, count ;
	int			index = 0, total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	sptr = (short*) psf->buffer ;
	bufferlen = ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (short) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : len ;
		for (k = 0 ; k < writecount ; k++)
			sptr [k] = (short) ptr [index+k] ;
		count = wav_gsm610_write (psf, pgsm610, sptr, writecount) ;
		index += writecount ;
		total += count ;
		len -= writecount ;
		} ;
	return total ;
} /* wav_gsm610_write_i */

int		wav_gsm610_write_d  (SF_PRIVATE *psf, double *ptr, int len, int normalize)
{	GSM610_PRIVATE *pgsm610 ; 
	short		*sptr ;
	int			k, bufferlen, writecount = 0, count ;
	int			index = 0, total = 0 ;
	double		normfact ;
	
	normfact = (normalize ? (double) 0x8000 : 1.0) ;

	if (! psf->fdata)
		return 0 ;
	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;
	
	sptr = (short*) psf->buffer ;
	bufferlen = ((SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth) / sizeof (short) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : len ;
		for (k = 0 ; k < writecount ; k++)
			sptr [k] = (short) (normfact * ptr [index+k])  ;
		count = wav_gsm610_write (psf, pgsm610, sptr, writecount) ;
		index += writecount ;
		total += count ;
		len -= writecount ;
		} ;
	return total ;
} /* wav_gsm610_write_d */

int	wav_gsm610_close	(SF_PRIVATE  *psf)
{	GSM610_PRIVATE *pgsm610 ; 
	unsigned int		dword ;

	if (! psf->fdata)
		return wav_close (psf) ;

	pgsm610 = (GSM610_PRIVATE*) psf->fdata ;

	if (psf->mode == SF_MODE_WRITE)
	{	/*	If a block has been partially assembled, write it out
		**	as the final block.
		*/
	
		if (pgsm610->samplecount && pgsm610->samplecount < GSM610_SAMPLES)
			wav_gsm610_write_block (psf, pgsm610) ;	

		/*  Now we know for certain the length of the file we can
		**  re-write correct values for the RIFF and data chunks.
		*/
		 
		fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;

		/* Fix RIFF size. */
		dword = H2LE_INT (psf->filelength - 2 * sizeof (dword)) ;
		fseek (psf->file, sizeof (dword), SEEK_SET) ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		
		psf->datalength = psf->filelength - psf->dataoffset ;
		fseek (psf->file, (int) (psf->dataoffset - sizeof (dword)), SEEK_SET) ;
		dword = H2LE_INT (psf->datalength) ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		} ;

	if (pgsm610->gsm_data)
		gsm_destroy (pgsm610->gsm_data) ;

	if (psf->fdata)
		free (psf->fdata) ;
	psf->fdata = NULL ;

	return 0 ;
} /* wav_gsm610_close */

