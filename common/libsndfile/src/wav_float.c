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

/*--------------------------------------------------------------------------------------------
**	Prototypes for private functions.
*/

static	void	x86f2s_array (float *buffer, unsigned int count, short *ptr, int index) ;
static	void	x86f2i_array (float *buffer, unsigned int count, int *ptr, int index) ;
static	void	x86f2d_array (float *buffer, unsigned int count, double *ptr, int index, double normfact) ;

static	void	s2x86f_array (short *ptr, int index, float *buffer, unsigned int count) ;
static	void	i2x86f_array (int *ptr, int index, float *buffer, unsigned int count) ;
static	void	d2x86f_array (double *ptr, int index, float *buffer, unsigned int count, double normfact) ;

/*--------------------------------------------------------------------------------------------
**	Exported functions.
*/

int		wav_read_x86f2s (SF_PRIVATE *psf, short *ptr, int len)
{	unsigned int	bytecount, readcount, bufferlen, thisread ;
	int				index = 0, total = 0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	readcount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		thisread = fread (psf->buffer, 1, readcount, psf->file) ;
		x86f2s_array ((float*) (psf->buffer), thisread / psf->bytewidth, ptr, index) ;
		total += thisread ;
		if (thisread < readcount)
			break ;
		index += thisread / psf->bytewidth ;
		bytecount -= thisread ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_READ ;
	
	return total ;
} /* wav_read_x86f2s */

int		wav_read_x86f2i (SF_PRIVATE *psf, int *ptr, int len)
{	unsigned int	bytecount, readcount, bufferlen, thisread ;
	int				index = 0, total = 0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	readcount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		thisread = fread (psf->buffer, 1, readcount, psf->file) ;
		x86f2i_array ((float*) (psf->buffer), thisread / psf->bytewidth, ptr, index) ;
		total += thisread ;
		if (thisread < readcount)
			break ;
		index += thisread / psf->bytewidth ;
		bytecount -= thisread ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_READ ;
	
	return total ;
} /* wav_read_x86f2i */


int		wav_read_x86f2d (SF_PRIVATE *psf, double *ptr, int len, int normalize)
{	unsigned int	bytecount, readcount, bufferlen, thisread ;
	int				index = 0, total = 0 ;
	double			normfact ;

	normfact = normalize ? 1.0 : 1.0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	readcount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		thisread = fread (psf->buffer, 1, readcount, psf->file) ;
		x86f2d_array ((float*) (psf->buffer), thisread / psf->bytewidth, ptr, index, normfact) ;
		total += thisread ;
		if (thisread < readcount)
			break ;
		index += thisread / psf->bytewidth ;
		bytecount -= thisread ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_READ ;
	
	return total ;
} /* wav_read_x86f2d */

int	wav_write_s2x86f (SF_PRIVATE *psf, short *ptr, int len)
{	unsigned int	bytecount, writecount, bufferlen, thiswrite ;
	int				index = 0, total = 0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	writecount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		s2x86f_array (ptr, index, (float*) (psf->buffer), writecount / psf->bytewidth) ;
		thiswrite = fwrite (psf->buffer, 1, writecount, psf->file) ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;
		index += thiswrite / psf->bytewidth ;
		bytecount -= thiswrite ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_WRITE ;
	
	return total ;
} /* wav_write_s2x86f */

int	wav_write_i2x86f (SF_PRIVATE *psf, int *ptr, int len) 
{	unsigned int	bytecount, writecount, bufferlen, thiswrite ;
	int				index = 0, total = 0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	writecount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		i2x86f_array (ptr, index, (float*) (psf->buffer), writecount / psf->bytewidth) ;
		thiswrite = fwrite (psf->buffer, 1, writecount, psf->file) ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;
		index += thiswrite / psf->bytewidth ;
		bytecount -= thiswrite ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_WRITE ;
	
	return total ;
} /* wav_write_i2x86f */

int	wav_write_d2x86f (SF_PRIVATE *psf, double *ptr, int len, int normalize)
{	unsigned int	bytecount, writecount, bufferlen, thiswrite ;
	int				index = 0, total = 0 ;
	double			normfact ;
	
	normfact = (normalize) ? 1.0 : 1.0 ;

	bufferlen = (SF_BUFFER_LEN / psf->blockwidth) * psf->blockwidth ;
	bytecount = len * psf->bytewidth ;
	while (bytecount > 0)
	{	writecount = (bytecount >= bufferlen) ? bufferlen : bytecount ;
		d2x86f_array (ptr, index, (float*) (psf->buffer), writecount / psf->bytewidth, normfact) ;
		thiswrite = fwrite (psf->buffer, 1, writecount, psf->file) ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;
		index += thiswrite / psf->bytewidth ;
		bytecount -= thiswrite ;
		} ;

	total /= psf->bytewidth ;
	if (total < len)
		psf->error = SFE_SHORT_WRITE ;
	
	return total ;
} /* wav_write_d2x86f */

/*==============================================================================================
**	Private functions.
*/

static
float	read_x86float (unsigned char *cptr)
{	int		exponent, mantissa ;
	float	fvalue ;

	exponent = ((cptr [3] & 0x7F) << 1) | ((cptr [2] & 0x80) ? 1 : 0);
	mantissa = ((cptr [2] & 0x7F) << 16) | (cptr [1] << 8) | (cptr [0]) ;

	if (! (exponent || mantissa))
		return 0.0 ;

	mantissa |= 0x800000 ;
	exponent = exponent ? exponent - 127 : 0 ;
                
	fvalue = mantissa ? ((float) mantissa) / ((float) 0x800000) : 0.0 ;
                
	if (cptr [3] & 0x80)
		fvalue *= -1 ;
                
	if (exponent > 0)
		fvalue *= (1 << exponent) ;
	else if (exponent < 0)
		fvalue /= (1 << abs (exponent)) ;

	return fvalue ;
} /* read_x86float */

static
void	write_x86float (double in, unsigned char *out)
{	int		exponent, mantissa ;

	*((int*) out) = 0 ;
	
	if (in == 0.0)
		return ;
	
	if (in < 0.0)
	{	in *= -1.0 ;
		out [3] |= 0x80 ;
		} ;
		
	in = frexp (in, &exponent) ;
	
	exponent += 126 ;
	
	if (exponent & 0x01)
		out [2] |= 0x80 ;
	out [3] |= (exponent >> 1) & 0x7F ;
	
	in *= (float) 0x1000000 ;
	mantissa = (((int) in) & 0x7FFFFF) ;
	
	out [0]  = mantissa & 0xFF ;
	out [1]  = (mantissa >> 8) & 0xFF ;
	out [2] |= (mantissa >> 16) & 0x7F ;
	
	return ;
} /* write_x86float */

/*----------------------------------------------------------------------------------------------
*/

static
void	x86f2s_array (float *buffer, unsigned int count, short *ptr, int index)
{	int	k ;
	
	for (k = 0 ; k < count ; k++)
	{	ptr [index] = ((short) read_x86float ((unsigned char *) (buffer +k))) ;
		index ++ ;
		} ;
} /* x86f2s_array */

static
void	x86f2i_array (float *buffer, unsigned int count, int *ptr, int index)
{	int	k ;
	
	for (k = 0 ; k < count ; k++)
	{	ptr [index] = ((int) read_x86float ((unsigned char *) (buffer +k))) ;
		index ++ ;
		} ;
} /* x86f2i_array */

static
void	x86f2d_array (float *buffer, unsigned int count, double *ptr, int index, double normfact)
{	int	k ;
	
	for (k = 0 ; k < count ; k++)
	{	ptr [index] = ((double) read_x86float ((unsigned char *) (buffer +k))) * normfact  ;
		index ++ ;
		} ;
} /* x86f2d_array */

static 
void	s2x86f_array (short *ptr, int index, float *buffer, unsigned int count)
{	int		k ;
	float	value ;
	for (k = 0 ; k < count ; k++)
	{	value = (float) (ptr [index]) ;
		write_x86float (value, (unsigned char*) (buffer + k)) ;
		index ++ ;
		} ;
} /* s2x86f_array */

static 
void	i2x86f_array (int *ptr, int index, float *buffer, unsigned int count)
{	int		k ;
	float	value ;
	for (k = 0 ; k < count ; k++)
	{	value = (float) (ptr [index]) ;
		write_x86float (value, (unsigned char*) (buffer + k)) ;
		index ++ ;
		} ;
} /* i2x86f_array */

static 
void	d2x86f_array (double *ptr, int index, float *buffer, unsigned int count, double normfact)
{	int		k ;
	float	value ;
	for (k = 0 ; k < count ; k++)
	{	value = (float) (ptr [index] * normfact) ;
		write_x86float (value, (unsigned char*) (buffer + k)) ;
		index ++ ;
		} ;
} /* d2x86f_array */

