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
#include	"au.h"
#include	"ulaw.h"
#include	"alaw.h"


/*------------------------------------------------------------------------------
** Macros to handle big/little endian issues.
*/

#if (CPU_IS_LITTLE_ENDIAN == 1)
	#define	MAKE_MARKER(a,b,c,d)		((a)|((b)<<8)|((c)<<16)|((d)<<24))
#elif (CPU_IS_BIG_ENDIAN == 1)
	#define	MAKE_MARKER(a,b,c,d)		(((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
	#error "Cannot determine endian-ness of processor."
#endif

#define DOTSND_MARKER	(MAKE_MARKER ('.', 's', 'n', 'd')) 
#define DNSDOT_MARKER	(MAKE_MARKER ('d', 'n', 's', '.')) 

/*------------------------------------------------------------------------------
** Known AU file encoding types.
*/

enum
{	AU_ENCODING_ULAW_8					= 1,	/* 8-bit u-law samples */
	AU_ENCODING_PCM_8					= 2,	/* 8-bit linear samples */
	AU_ENCODING_PCM_16					= 3,	/* 16-bit linear samples */
	AU_ENCODING_PCM_24					= 4,	/* 24-bit linear samples */
	AU_ENCODING_PCM_32					= 5,	/* 32-bit linear samples */
	
	AU_ENCODING_FLOAT					= 6,	/* floating-point samples */
	AU_ENCODING_DOUBLE					= 7,	/* double-precision float samples */
	AU_ENCODING_INDIRECT				= 8,	/* fragmented sampled data */
	AU_ENCODING_NESTED					= 9,	/* ? */
	AU_ENCODING_DSP_CORE				= 10,	/* DSP program */
	AU_ENCODING_DSP_DATA_8				= 11,	/* 8-bit fixed-point samples */
	AU_ENCODING_DSP_DATA_16				= 12,	/* 16-bit fixed-point samples */
	AU_ENCODING_DSP_DATA_24				= 13,	/* 24-bit fixed-point samples */
	AU_ENCODING_DSP_DATA_32				= 14,	/* 32-bit fixed-point samples */

	AU_ENCODING_DISPLAY					= 16,	/* non-audio display data */
	AU_ENCODING_MULAW_SQUELCH			= 17,	/* ? */
	AU_ENCODING_EMPHASIZED				= 18,	/* 16-bit linear with emphasis */
	AU_ENCODING_NEXT					= 19,	/* 16-bit linear with compression (NEXT) */
	AU_ENCODING_COMPRESSED_EMPHASIZED	= 20,	/* A combination of the two above */
	AU_ENCODING_DSP_COMMANDS			= 21,	/* Music Kit DSP commands */
	AU_ENCODING_DSP_COMMANDS_SAMPLES	= 22,	/* ? */

	AU_ENCODING_ADPCM_G721_32			= 23,	/* G721 32 kbs ADPCM - 4 bits per sample. */
	AU_ENCODING_ADPCM_G722				= 24,
	AU_ENCODING_ADPCM_G723_24			= 25,	/* G723 24 kbs ADPCM - 3 bits per sample. */
	AU_ENCODING_ADPCM_G723_5			= 26,
	
	AU_ENCODING_ALAW_8					= 27
} ;

/*------------------------------------------------------------------------------
** Typedefs.
*/
 
typedef	struct
{	int		dataoffset ;
	int		datasize ;
	int		encoding ;
    int		samplerate ;
    int		channels ;
} AU_FMT ;


/*------------------------------------------------------------------------------
** Private static functions.
*/

static	int		au_close		(SF_PRIVATE *psf) ;

static	int 	get_encoding	(unsigned int format, unsigned int	bitwidth) ;
static	char* 	get_encoding_str(int format) ;
static	void	endswap_au_fmt	(AU_FMT *pau_fmt) ;


/*------------------------------------------------------------------------------
** Public functions.
*/

int 	au_open_read	(SF_PRIVATE *psf)
{	AU_FMT			au_fmt ;
	unsigned int	marker, dword ;
	int				big_endian_file, error = SFE_NO_ERROR ;
	
	fread (&marker, sizeof (marker), 1, psf->file) ;
	if (marker == DOTSND_MARKER)
		big_endian_file = 1 ;
	else if (marker == DNSDOT_MARKER)
		big_endian_file = 0 ;
	else
		return SFE_AU_NO_DOTSND ;
		
	psf_sprintf (psf, "%D\n", marker) ;
	
	fread (&au_fmt, sizeof (AU_FMT), 1, psf->file) ;
	
	if (CPU_IS_LITTLE_ENDIAN && big_endian_file)
		endswap_au_fmt (&au_fmt) ;
	else if (CPU_IS_BIG_ENDIAN && ! big_endian_file)
		endswap_au_fmt (&au_fmt) ;

	psf_sprintf (psf, "  Data Offset : %d\n", au_fmt.dataoffset) ;
	
	if (au_fmt.dataoffset + au_fmt.datasize != psf->filelength)
	{	dword = psf->filelength - au_fmt.dataoffset ;
		psf_sprintf (psf, "  Data Size   : %d (should be %d)\n", au_fmt.datasize, dword) ;
		au_fmt.datasize = dword ;
		}
	else
		psf_sprintf (psf, "  Data Size   : %d\n", au_fmt.datasize) ;
		
 	psf->dataoffset = au_fmt.dataoffset ;
	psf->datalength = psf->filelength - psf->dataoffset ;

 	psf->current  = 0 ;
	psf->endian   = big_endian_file ? SF_ENDIAN_BIG : SF_ENDIAN_LITTLE ;
	psf->sf.seekable = SF_TRUE ;
 	
 	if (fseek (psf->file, psf->dataoffset, SEEK_SET))
		return SFE_BAD_SEEK ;

	psf->close = (func_close) au_close ;
	
	psf->sf.samplerate	= au_fmt.samplerate ;
	psf->sf.channels 	= au_fmt.channels ;
					
	/* Only fill in type major. */
	psf->sf.format = big_endian_file ? SF_FORMAT_AU : SF_FORMAT_AULE ;

	psf->sf.sections 	= 1 ;

	psf_sprintf (psf, "  Encoding    : %d => %s\n", au_fmt.encoding, get_encoding_str (au_fmt.encoding)) ;

	psf_sprintf (psf, "  Sample Rate : %d\n", au_fmt.samplerate) ;
	psf_sprintf (psf, "  Channels    : %d\n", au_fmt.channels) ;
	
	switch (au_fmt.encoding)
	{	case  AU_ENCODING_ULAW_8 :	
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				psf->bytewidth   	= 1 ;	/* Before decoding */
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
					
				psf->sf.format |= SF_FORMAT_ULAW ;
					
				psf->read_short  = (func_short)  ulaw_read_ulaw2s ;
				psf->read_int    = (func_int)    ulaw_read_ulaw2i ;
				psf->read_double = (func_double) ulaw_read_ulaw2d ;

				break ;
										
		case  AU_ENCODING_PCM_8 :	
				psf->sf.pcmbitwidth = 8 ;
				psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;

				psf->sf.format |= SF_FORMAT_PCM ;
					
				psf->read_short  = (func_short)  pcm_read_sc2s ;
				psf->read_int    = (func_int)    pcm_read_sc2i ;
				psf->read_double = (func_double) pcm_read_sc2d ;
				break ;

		case  AU_ENCODING_PCM_16 :	
				psf->sf.pcmbitwidth = 16 ;
				psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;

				psf->sf.format |= SF_FORMAT_PCM ;
					
				if (big_endian_file)
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

		case  AU_ENCODING_PCM_24 :	
				psf->sf.pcmbitwidth = 24 ;
				psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
					
				psf->sf.format |= SF_FORMAT_PCM ;

				if (big_endian_file)
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

		case  AU_ENCODING_PCM_32 :	
				psf->sf.pcmbitwidth = 32 ;
				psf->bytewidth      = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
					
				psf->sf.format |= SF_FORMAT_PCM ;
					
				if (big_endian_file)
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
					
		case  AU_ENCODING_ALAW_8 :
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				psf->bytewidth   	= 1 ;	/* Before decoding */
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
				
				psf->sf.format |= SF_FORMAT_ALAW ;
				
				psf->read_short  = (func_short)  alaw_read_alaw2s ;
				psf->read_int    = (func_int)    alaw_read_alaw2i ;
				psf->read_double = (func_double) alaw_read_alaw2d ;
				break ;
					
		case  AU_ENCODING_ADPCM_G721_32 :  /* G721 32kbs ADPCM */
				if (psf->sf.channels != 1)
				{	psf_sprintf (psf, "Channels != 1\n") ;
					break ;
					} ;
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				
				psf->sf.format |= SF_FORMAT_G721_32 ;
				
				error = au_g72x_reader_init (psf, AU_H_G721_32) ;
				psf->sf.seekable = SF_FALSE ;
				break ;
										
		case  AU_ENCODING_ADPCM_G723_24 :  /* G723 24kbs ADPCM */
				if (psf->sf.channels != 1)
				{	psf_sprintf (psf, "Channels != 1\n") ;
					break ;
					} ;
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				
				psf->sf.format |= SF_FORMAT_G723_24 ;
				
				error = au_g72x_reader_init (psf, AU_H_G723_24) ;
				psf->sf.seekable = SF_FALSE ;
				break ;
										
		case  AU_ENCODING_NEXT :	
				error = SFE_AU_UNKNOWN_FORMAT ;
				break ;

		default : 
				error = SFE_AU_UNKNOWN_FORMAT ;
				break ;
		} ;

	if (error)
		return error ;

	if (! psf->sf.samples && psf->blockwidth)
		psf->sf.samples = au_fmt.datasize / psf->blockwidth ;


	return 0 ;
} /* au_open_read */

/*------------------------------------------------------------------------------
*/

int 	au_nh_open_read	(SF_PRIVATE *psf)
{	if (fseek (psf->file, psf->dataoffset, SEEK_SET))
		return SFE_BAD_SEEK ;

	psf_sprintf (psf, "Setting up for 8kHz, mono, u-law.\n") ;
	
	psf->sf.format = SF_FORMAT_AU | SF_FORMAT_ULAW ;

 	psf->dataoffset = 0 ;
 	psf->current  = 0 ;
	psf->endian   = 0 ;  /* Irrelevant but it must be something. */
	psf->sf.seekable = SF_TRUE ;
	psf->sf.samplerate	= 8000 ;
	psf->sf.channels 	= 1 ;
	psf->sf.sections 	= 1 ;
	psf->sf.pcmbitwidth = 16 ;	/* After decoding */
	psf->bytewidth   	= 1 ;	/* Before decoding */
					
	psf->read_short  = (func_short)  ulaw_read_ulaw2s ;
	psf->read_int    = (func_int)    ulaw_read_ulaw2i ;
	psf->read_double = (func_double) ulaw_read_ulaw2d ;
	psf->close 	     = (func_close)  au_close ;

	psf->blockwidth = 1 ;
	psf->sf.samples = psf->filelength ;
	psf->datalength = psf->filelength ;

	return 0 ;
} /* au_open_read */

/*------------------------------------------------------------------------------
*/

int 	au_open_write	(SF_PRIVATE *psf)
{	AU_FMT			au_fmt ;
	unsigned int	dword, encoding, format, subformat, big_endian_file ;
	int				error = 0 ;
	
	format = psf->sf.format & SF_FORMAT_TYPEMASK ;
	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;
	if (format == SF_FORMAT_AU)
		big_endian_file = 1 ;
	else if (format == SF_FORMAT_AULE)
		big_endian_file = 0 ;
	else
		return	SFE_BAD_OPEN_FORMAT ;
		
	if (subformat == SF_FORMAT_ULAW || subformat == SF_FORMAT_ALAW || 
		subformat == SF_FORMAT_G721_32 || subformat == SF_FORMAT_G723_24)
		psf->bytewidth = 1 ;
	else
		psf->bytewidth = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
		
	psf->endian      = big_endian_file ? SF_ENDIAN_BIG : SF_ENDIAN_LITTLE ;
	psf->sf.seekable = SF_TRUE ;
	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;
 	psf->dataoffset  = 6 * sizeof (dword) ;
	psf->datalength  = psf->blockwidth * psf->sf.samples ;
	psf->filelength  = psf->datalength + psf->dataoffset ;
	psf->error       = 0 ;

	encoding = get_encoding (subformat, psf->bytewidth * 8) ;
	if (! encoding)
		return	SFE_BAD_OPEN_FORMAT ;

	au_fmt.dataoffset = 24 ;
	au_fmt.datasize   = psf->datalength ;
	au_fmt.encoding   = encoding ;
	au_fmt.samplerate = psf->sf.samplerate ;
	au_fmt.channels   = psf->sf.channels ;
	
	if (CPU_IS_LITTLE_ENDIAN && big_endian_file)
		endswap_au_fmt (&au_fmt) ;
	else if (CPU_IS_BIG_ENDIAN && ! big_endian_file)
		endswap_au_fmt (&au_fmt) ;
	
	dword = big_endian_file ? DOTSND_MARKER : DNSDOT_MARKER ;	/* Marker */
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	fwrite (&au_fmt, sizeof (AU_FMT), 1, psf->file) ;
	
	psf->close = (func_close) au_close ;
	
	switch (encoding)
	{	case  AU_ENCODING_ULAW_8 :	/* 8-bit Ulaw encoding. */
				psf->write_short  = (func_short)  ulaw_write_s2ulaw ;
				psf->write_int    = (func_int)    ulaw_write_i2ulaw ;
				psf->write_double = (func_double) ulaw_write_d2ulaw ;
				break ;
	
		case  AU_ENCODING_PCM_8 :	/* 8-bit linear PCM. */
				psf->write_short  = (func_short)  pcm_write_s2sc ;
				psf->write_int    = (func_int)    pcm_write_i2sc ;
				psf->write_double = (func_double) pcm_write_d2sc ;
				break ;

		case  AU_ENCODING_PCM_16 :	/* 16-bit linear PCM. */
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

		case  AU_ENCODING_PCM_24 :	/* 24-bit linear PCM */
				if (big_endian_file)
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

		case  AU_ENCODING_PCM_32 :	/* 32-bit linear PCM. */
				if (big_endian_file)
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
				
		case  AU_ENCODING_ALAW_8 :	/* 8-bit Alaw encoding. */
				psf->write_short  = (func_short)  alaw_write_s2alaw ;
				psf->write_int    = (func_int)    alaw_write_i2alaw ;
				psf->write_double = (func_double) alaw_write_d2alaw ;
				break ;
	
		case  AU_ENCODING_ADPCM_G721_32 :  
				if (psf->sf.channels != 1)
				{	psf_sprintf (psf, "Channels != 1\n") ;
					break ;
					} ;
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				psf->bytewidth   	= 0 ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
				
				psf->sf.format |= SF_FORMAT_G721_32 ;
				
				error = au_g72x_writer_init (psf, AU_H_G721_32) ;
				break ;

		case  AU_ENCODING_ADPCM_G723_24 :  
				if (psf->sf.channels != 1)
				{	psf_sprintf (psf, "Channels != 1\n") ;
					break ;
					} ;
				psf->sf.pcmbitwidth = 16 ;	/* After decoding */
				psf->bytewidth   	= 0 ;
				psf->blockwidth = psf->sf.channels * psf->bytewidth ;
				
				psf->sf.format |= SF_FORMAT_G721_32 ;
				
				error = au_g72x_writer_init (psf, AU_H_G723_24) ;
				break ;

		default :   break ;
		} ;
		
	return error ;
} /* au_open_write */

/*------------------------------------------------------------------------------
*/

int	au_close	(SF_PRIVATE  *psf)
{	unsigned int	dword ;

	if (psf->mode == SF_MODE_WRITE)
	{	/*  Now we know for certain the length of the file we can
		 *  re-write correct values for the datasize header element.
		 */

		fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;

		psf->datalength = psf->filelength - psf->dataoffset ;
		fseek (psf->file, 2 * sizeof (dword), SEEK_SET) ;
		
		if (psf->endian == SF_ENDIAN_BIG)
			dword = H2BE_INT (psf->datalength) ;
		else if (psf->endian == SF_ENDIAN_LITTLE)
			dword = H2LE_INT (psf->datalength) ;
		else
			dword = 0xFFFFFFFF ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		} ;

	if (psf->fdata)
		free (psf->fdata) ;
	psf->fdata = NULL ;
	
	return 0 ;
} /* au_close */

static	
int get_encoding (unsigned int format, unsigned int	bitwidth)
{	if (format == SF_FORMAT_ULAW)
		return AU_ENCODING_ULAW_8 ;
		
	if (format == SF_FORMAT_ALAW)
		return AU_ENCODING_ALAW_8 ;

	if (format == SF_FORMAT_G721_32)
		return AU_ENCODING_ADPCM_G721_32 ;

	if (format == SF_FORMAT_G723_24)
		return AU_ENCODING_ADPCM_G723_24 ;

	if (format != SF_FORMAT_PCM)
		return 0 ;

	/* For PCM encoding, the header encoding field depends on the bitwidth. */
	switch (bitwidth)
	{	case	8  : return AU_ENCODING_PCM_8 ;
		case	16 : return AU_ENCODING_PCM_16 ;
		case	24 : return AU_ENCODING_PCM_24 ;
		case	32 : return	AU_ENCODING_PCM_32 ;
		default : break ;
		} ;
	return 0 ;
} /* get encoding */

static
void	endswap_au_fmt (AU_FMT *pau_fmt)
{	pau_fmt->dataoffset = ENDSWAP_INT (pau_fmt->dataoffset) ;
	pau_fmt->datasize   = ENDSWAP_INT (pau_fmt->datasize) ;
	pau_fmt->encoding   = ENDSWAP_INT (pau_fmt->encoding) ;
    pau_fmt->samplerate = ENDSWAP_INT (pau_fmt->samplerate) ;
    pau_fmt->channels   = ENDSWAP_INT (pau_fmt->channels) ;
} /* endswap_au_fmt */

static	
char* get_encoding_str (int format)
{	switch (format)
	{	case  AU_ENCODING_ULAW_8 :	
				return "8-bit ISDN u-law" ;
										
		case  AU_ENCODING_PCM_8 :	
				return "8-bit linear PCM" ;

		case  AU_ENCODING_PCM_16 :	
				return "16-bit linear PCM" ;

		case  AU_ENCODING_PCM_24 :	
				return "24-bit linear PCM" ;

		case  AU_ENCODING_PCM_32 :	
				return "32-bit linear PCM" ;
					
		case  AU_ENCODING_ALAW_8 :
				return "8-bit ISDN A-law" ;
					
		case  AU_ENCODING_ADPCM_G721_32 :  
				return "G721 32kbs ADPCM" ;
										
		case  AU_ENCODING_ADPCM_G723_24 :  
				return "G723 24kbs ADPCM" ;
										
		case  AU_ENCODING_NEXT :	
				return "Weird NeXT encoding format (unsupported)" ;
		} ;
	return "Unknown!!" ;
} /* get_encoding_str */	


