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
** Private static functions.
*/

/*------------------------------------------------------------------------------
** Public functions.
*/

int 	raw_open_read	(SF_PRIVATE *psf)
{	unsigned int subformat ;

	if(! psf->sf.channels || ! psf->sf.pcmbitwidth)
		return SFE_RAW_READ_BAD_SPEC ;
		
	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;

	if (subformat == SF_FORMAT_PCM_BE)
		psf->endian = SF_ENDIAN_BIG ;
	else if (subformat == SF_FORMAT_PCM_LE)
		psf->endian = SF_ENDIAN_LITTLE ;
	else if (subformat == SF_FORMAT_PCM_S8 || subformat == SF_FORMAT_PCM_U8)
		psf->endian = 0 ;
	else
		return SFE_RAW_READ_BAD_SPEC ;
		
	psf->sf.seekable = SF_TRUE ;
 	
	psf->sf.sections = 1 ;

	switch (psf->sf.pcmbitwidth)
	{	case   8 :	if (subformat == SF_FORMAT_PCM_S8)
					{	psf->read_short  = (func_short)  pcm_read_sc2s ;
						psf->read_int    = (func_int)    pcm_read_sc2i ;
						psf->read_double = (func_double) pcm_read_sc2d ;
	  					}
					else if (subformat == SF_FORMAT_PCM_U8)
					{	psf->read_short  = (func_short)  pcm_read_uc2s ;
						psf->read_int    = (func_int)    pcm_read_uc2i ;
						psf->read_double = (func_double) pcm_read_uc2d ;
	  					}
					break ;					
	
		case  16 :	if (subformat == SF_FORMAT_PCM_BE)
					{	psf->read_short  = (func_short)  pcm_read_bes2s ;
						psf->read_int    = (func_int)    pcm_read_bes2i ;
						psf->read_double = (func_double) pcm_read_bes2d ;
						}
					else
					{	psf->read_short  = (func_short)  pcm_read_les2s ;
						psf->read_int    = (func_int)    pcm_read_les2i ;
						psf->read_double = (func_double) pcm_read_les2d ;
						} ;
					break ;

		case  24 :	if (subformat == SF_FORMAT_PCM_BE)
					{	psf->read_short  = (func_short)  pcm_read_bet2s ;
						psf->read_int    = (func_int)    pcm_read_bet2i ;
						psf->read_double = (func_double) pcm_read_bet2d ;
						}
					else
					{	psf->read_short  = (func_short)  pcm_read_let2s ;
						psf->read_int    = (func_int)    pcm_read_let2i ;
						psf->read_double = (func_double) pcm_read_let2d ;
						} ;
					break ;

		case  32 :	if (subformat == SF_FORMAT_PCM_BE)
					{	psf->read_short  = (func_short)  pcm_read_bei2s ;
						psf->read_int    = (func_int)    pcm_read_bei2i ;
						psf->read_double = (func_double) pcm_read_bei2d ;
						}
					else
					{	psf->read_short  = (func_short)  pcm_read_lei2s ;
						psf->read_int    = (func_int)    pcm_read_lei2i ;
						psf->read_double = (func_double) pcm_read_lei2d ;
						} ;
					break ;
					
		default :   return SFE_RAW_BAD_BITWIDTH ;
					break ;
		} ;

	psf->dataoffset = 0 ;
	psf->bytewidth  = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
	psf->blockwidth = psf->sf.channels * psf->bytewidth ;

	if (psf->blockwidth)
		psf->sf.samples = psf->filelength / psf->blockwidth ;

 	psf->datalength = psf->filelength - psf->dataoffset ;

 	psf->current  = 0 ;
	
	return 0 ;
} /* raw_open_read */

/*------------------------------------------------------------------------------
*/

int 	raw_open_write	(SF_PRIVATE *psf)
{	unsigned int	subformat, big_endian_file ;
	
	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;

	if (subformat == SF_FORMAT_PCM_BE)
		big_endian_file = 1 ;
	else if (subformat == SF_FORMAT_PCM_LE)
		big_endian_file = 0 ;
	else if (subformat == SF_FORMAT_PCM_S8 || subformat == SF_FORMAT_PCM_U8)
		big_endian_file = 0 ;
	else
		return	SFE_BAD_OPEN_FORMAT ;
		
	psf->bytewidth = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
		
	psf->endian      = big_endian_file ? SF_ENDIAN_BIG : SF_ENDIAN_LITTLE ;
	psf->sf.seekable = SF_TRUE ;
	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;
 	psf->dataoffset  = 0 ;
	psf->datalength  = psf->blockwidth * psf->sf.samples ;
	psf->filelength  = psf->datalength ;
	psf->error       = 0 ;

	switch (psf->sf.pcmbitwidth)
	{	case   8 :	if (subformat == SF_FORMAT_PCM_S8)
					{	psf->write_short  = (func_short)  pcm_write_s2sc ;
						psf->write_int    = (func_int)    pcm_write_i2sc ;
						psf->write_double = (func_double) pcm_write_d2sc ;
	  					}
					else if (subformat == SF_FORMAT_PCM_U8)
					{	psf->write_short  = (func_short)  pcm_write_s2uc ;
						psf->write_int    = (func_int)    pcm_write_i2uc ;
						psf->write_double = (func_double) pcm_write_d2uc ;
	  					}
					break ;					
	
		case  16 :	if (big_endian_file)
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

		case  24 :	if (big_endian_file)
					{	psf->write_short  = (func_short)  pcm_write_s2bet ;
						psf->write_int    = (func_int)    pcm_write_i2bet ;
						psf->write_double = (func_double) pcm_write_d2bet ;
						}
					else
					{	psf->write_short  = (func_short)  pcm_write_s2let ;
						psf->write_int    = (func_int)    pcm_write_i2let ;
						psf->write_double = (func_double) pcm_write_d2let ;
						} ;
					break ;

		case  32 :	if (big_endian_file)
					{	psf->write_short  = (func_short)  pcm_write_s2bei ;
						psf->write_int    = (func_int)    pcm_write_i2bei ;
						psf->write_double = (func_double) pcm_write_d2bei ;
						}
					else
					{	psf->write_short  = (func_short)  pcm_write_s2lei ;
						psf->write_int    = (func_int)    pcm_write_i2lei ;
						psf->write_double = (func_double) pcm_write_d2lei ;
						} ;
					break ;
					
		default :   return	SFE_BAD_OPEN_FORMAT ;
		} ;
		
	return 0 ;
} /* raw_open_write */

/*------------------------------------------------------------------------------
*/


/*==============================================================================
*/

