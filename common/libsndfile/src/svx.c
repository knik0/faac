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
#include	<stdarg.h>

#include	"sndfile.h"
#include	"config.h"
#include	"sfendian.h"
#include	"common.h"
#include	"pcm.h"


/*------------------------------------------------------------------------------
 * Macros to handle big/little endian issues.
*/

#if (CPU_IS_LITTLE_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		((a)|((b)<<8)|((c)<<16)|((d)<<24))
#elif (CPU_IS_BIG_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		(((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#	error "Cannot determine endian-ness of processor."
#endif

#define FORM_MARKER	(MAKE_MARKER ('F', 'O', 'R', 'M'))
#define SVX8_MARKER	(MAKE_MARKER ('8', 'S', 'V', 'X'))
#define SV16_MARKER	(MAKE_MARKER ('1', '6', 'S', 'V'))
#define VHDR_MARKER	(MAKE_MARKER ('V', 'H', 'D', 'R'))
#define BODY_MARKER	(MAKE_MARKER ('B', 'O', 'D', 'Y')) 
 
#define ATAK_MARKER	(MAKE_MARKER ('A','T','A','K'))
#define RLSE_MARKER	(MAKE_MARKER ('R','L','S','E'))

#define c_MARKER	(MAKE_MARKER ('(', 'c', ')', ' ')) 
#define NAME_MARKER	(MAKE_MARKER ('N', 'A', 'M', 'E')) 
#define AUTH_MARKER	(MAKE_MARKER ('A', 'U', 'T', 'H')) 
#define ANNO_MARKER	(MAKE_MARKER ('A', 'N', 'N', 'O')) 
#define CHAN_MARKER	(MAKE_MARKER ('C', 'H', 'A', 'N')) 


/*------------------------------------------------------------------------------
 * Typedefs for file chunks.
*/

typedef struct
{	unsigned int	oneShotHiSamples, repeatHiSamples, samplesPerHiCycle ;
	unsigned short	samplesPerSec ;
	unsigned char	octave, compression ;
	unsigned int	volume ;
} VHDR_CHUNK ;

/*------------------------------------------------------------------------------
 * Private static functions.
*/

static	int		svx_close	(SF_PRIVATE  *psf) ;

static
void endswap_vhdr_chunk (VHDR_CHUNK *vhdr)
{	vhdr->oneShotHiSamples  = ENDSWAP_INT (vhdr->oneShotHiSamples) ;
	vhdr->repeatHiSamples   = ENDSWAP_INT (vhdr->repeatHiSamples) ;
	vhdr->samplesPerHiCycle = ENDSWAP_INT (vhdr->samplesPerHiCycle) ;
	vhdr->samplesPerSec     = ENDSWAP_SHORT (vhdr->samplesPerSec) ;
	vhdr->volume            = ENDSWAP_INT (vhdr->volume) ;
} /* endswap_vhdr_chunk */

/*------------------------------------------------------------------------------
** Public functions.
*/

int 	svx_open_read	(SF_PRIVATE *psf)
{	VHDR_CHUNK		vhdr ;
	unsigned int	FORMsize, vhdrsize, dword, marker ;
	int				filetype = 0, parsestage = 0, done = 0 ;
	
	/* Set default number of channels. */
	psf->sf.channels = 1 ;

	while (! done)
	{	fread (&marker, sizeof (marker), 1, psf->file) ;
		switch (marker)
		{	case FORM_MARKER :
					if (parsestage != 0)
						return SFE_SVX_NO_FORM ;
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

			case SVX8_MARKER :
			case SV16_MARKER :
					if (parsestage != 1)
						return SFE_SVX_NO_FORM ;
					filetype = marker ;
					psf_sprintf (psf, " %D\n", marker) ;
					parsestage = 2 ;
					break ;

			case VHDR_MARKER :
					if (parsestage != 2)
						return SFE_SVX_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					vhdrsize = BE2H_INT (dword) ;

					psf_sprintf (psf, " VHDR : %d\n", vhdrsize) ;

					fread (&vhdr, sizeof (vhdr), 1, psf->file) ;
					if (CPU_IS_LITTLE_ENDIAN)
						endswap_vhdr_chunk (&vhdr) ;

					psf_sprintf (psf, "  OneShotHiSamples  : %d\n", vhdr.oneShotHiSamples) ;
					psf_sprintf (psf, "  RepeatHiSamples   : %d\n", vhdr.repeatHiSamples) ;
					psf_sprintf (psf, "  samplesPerHiCycle : %d\n", vhdr.samplesPerHiCycle) ;
					psf_sprintf (psf, "  Sample Rate       : %d\n", vhdr.samplesPerSec) ;
					psf_sprintf (psf, "  Octave            : %d\n", vhdr.octave) ;

					psf_sprintf (psf, "  Compression       : %d => ", vhdr.compression) ;
					
					switch (vhdr.compression)
					{	case 0 : psf_sprintf (psf, "None.\n") ;
								break ;
						case 1 : psf_sprintf (psf, "Fibonacci delta\n") ;
								break ;
						case 2 : psf_sprintf (psf, "Exponential delta\n") ;
								break ;
						} ;

					psf_sprintf (psf, "  Volume            : %d\n", vhdr.volume) ;

					psf->sf.samplerate 	= vhdr.samplesPerSec ;

					if (filetype == SVX8_MARKER)
						psf->sf.pcmbitwidth = 8 ;
					else if (filetype == SV16_MARKER)
						psf->sf.pcmbitwidth = 16 ;
						
					parsestage = 3 ;
					break ;

			case BODY_MARKER :
					if (parsestage != 3)
						return SFE_SVX_NO_BODY ;
					fread (&dword, sizeof (dword), 1, psf->file) ;

					psf->datalength = BE2H_INT (dword) ;
					psf->dataoffset = ftell (psf->file) ;
					
					if (psf->datalength > psf->filelength - psf->dataoffset)
					{	psf_sprintf (psf, " BODY : %d (should be %d)\n", psf->datalength, psf->filelength - psf->dataoffset) ;
						psf->datalength = psf->filelength - psf->dataoffset ;
						} 
					else
						psf_sprintf (psf, " BODY : %d\n", psf->datalength) ;

					fseek (psf->file, psf->datalength, SEEK_CUR) ;
					parsestage = 4 ;
					break ;

			case NAME_MARKER :
			case ANNO_MARKER :
					if (parsestage < 2)
						return SFE_SVX_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = BE2H_INT (dword) ;
					psf_sprintf (psf, " %D : %d\n", marker, dword) ;
					fseek (psf->file, (int) dword, SEEK_CUR) ;					
					break ;

			case AUTH_MARKER :
			case c_MARKER :
			case CHAN_MARKER :
					if (parsestage < 2)
						return SFE_SVX_NO_FORM ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = BE2H_INT (dword) ;
					psf_sprintf (psf, " %D : %d\n", marker, dword) ;
					fseek (psf->file, (int) dword, SEEK_CUR) ;					
					break ;

			default : 
					if (isprint ((marker >> 24) & 0xFF) && isprint ((marker >> 16) & 0xFF)
						&& isprint ((marker >> 8) & 0xFF) && isprint (marker & 0xFF))
					{	fread (&dword, sizeof (dword), 1, psf->file) ;
						psf_sprintf (psf, "%D : %d (unknown marker)\n", marker, dword) ;
						fseek (psf->file, (int) dword, SEEK_CUR) ;
						break ;
						} ;
					if ((dword = ftell (psf->file)) & 0x03)
					{	psf_sprintf (psf, "  Unknown chunk marker at position %d. Resynching.\n", dword - 4) ;
						fseek (psf->file, -3, SEEK_CUR) ;
						break ;
						} ;
					psf_sprintf (psf, "*** Unknown chunk marker : %X. Exiting parser.\n", marker) ;
					done = 1 ;
			} ;	/* switch (marker) */

		if (ferror (psf->file))
		{	psf_sprintf (psf, "*** Error on file handle. ***\n", marker) ;
			clearerr (psf->file) ;
			break ;
			} ;

		if (ftell (psf->file) >= (off_t) (psf->filelength - (2 * sizeof (dword))))
			break ;
		} ; /* while (1) */
		
	if (vhdr.compression)
		return SFE_SVX_BAD_COMP ;
		
	if (! psf->dataoffset)
		return SFE_SVX_NO_DATA ;

	psf->sf.format 		= (SF_FORMAT_SVX | SF_FORMAT_PCM);
	psf->sf.sections 	= 1 ;
					
	psf->current     = 0 ;
	psf->endian      = SF_ENDIAN_BIG ;			/* All SVX files are big endian. */
	psf->sf.seekable = SF_TRUE ;
	psf->bytewidth   = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;

	psf->blockwidth  = psf->sf.channels * psf->bytewidth ;
	
	if (psf->blockwidth)
		psf->sf.samples  = psf->datalength / psf->blockwidth ;

	fseek (psf->file, psf->dataoffset, SEEK_SET) ;

	psf->close = (func_close) svx_close ;

	switch (psf->bytewidth)
	{	case  1 :
				psf->read_short  = (func_short)  pcm_read_sc2s ;
				psf->read_int    = (func_int)    pcm_read_sc2i ;
				psf->read_double = (func_double) pcm_read_sc2d ;
				break ;
		case  2 :
				psf->read_short  = (func_short)  pcm_read_bes2s ;
				psf->read_int    = (func_int)    pcm_read_bes2i ;
				psf->read_double = (func_double) pcm_read_bes2d ;
				break ;
		default : 
				/* printf ("Weird bytewidth (%d)\n", psf->bytewidth) ; */
				return SFE_UNIMPLEMENTED ;
		} ;

	return 0 ;
} /* svx_open_read */

int 	svx_open_write	(SF_PRIVATE *psf)
{	static	char 	annotation	[] = "libsndfile by Erik de Castro Lopo\0\0\0" ;
	VHDR_CHUNK		vhdr ;
	unsigned int	FORMsize ;
	
	if ((psf->sf.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_SVX)
		return	SFE_BAD_OPEN_FORMAT ;
	if ((psf->sf.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM)
		return	SFE_BAD_OPEN_FORMAT ;
	if (psf->sf.pcmbitwidth != 8 && psf->sf.pcmbitwidth != 16)
		return	SFE_BAD_OPEN_FORMAT ;
	
	psf->endian      = SF_ENDIAN_BIG ;			/* All SVX files are big endian. */
	psf->sf.seekable = SF_TRUE ;
	psf->bytewidth   = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;
	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;
	psf->datalength  = psf->blockwidth * psf->sf.samples ;
	psf->filelength  = psf->datalength + psf->dataoffset ;
	psf->error       = 0 ;

	FORMsize   = 0x7FFFFFFF ;   /* Correct this when closing file. */
	
	vhdr.oneShotHiSamples  = psf->sf.samples ;
	vhdr.repeatHiSamples   = 0 ;
	vhdr.samplesPerHiCycle = 0 ;
	vhdr.samplesPerSec     = psf->sf.samplerate ;
	vhdr.octave            = 1 ;
	vhdr.compression       = 0 ;
	vhdr.volume            = (psf->bytewidth == 1) ? 255 : 0xFFFF ;

	if (CPU_IS_LITTLE_ENDIAN)
		endswap_vhdr_chunk (&vhdr) ;

	psf_hprintf (psf, "mL"  , FORM_MARKER, FORMsize) ;
	psf_hprintf (psf, "m"   , (psf->bytewidth == 1) ? SVX8_MARKER : SV16_MARKER) ;
	psf_hprintf (psf, "mLb" , VHDR_MARKER, sizeof (VHDR_CHUNK), &vhdr, sizeof (VHDR_CHUNK)) ;
	psf_hprintf (psf, "mSmS", NAME_MARKER, psf->filename, ANNO_MARKER, annotation) ;
	psf_hprintf (psf, "mL"  , BODY_MARKER, psf->datalength) ;


	fwrite (psf->header, psf->headindex, 1, psf->file) ;

	psf->dataoffset  = ftell (psf->file) ;
	
	psf->close = (func_close) svx_close ;

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
} /* svx_open_write */

/*------------------------------------------------------------------------------
*/

int	svx_close	(SF_PRIVATE  *psf)
{	
	if (psf->mode == SF_MODE_WRITE)
	{	/*  Now we know for certain the length of the file we can re-write 
		**	correct values for the FORM, 8SVX and BODY chunks.
		*/
                
		fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;

		psf_hsetf (psf, FORM_MARKER, "L", psf->filelength - 8) ;
		psf_hsetf (psf, BODY_MARKER, "L", psf->filelength - psf->dataoffset) ;

		fseek (psf->file, 0, SEEK_SET) ;
		fwrite (psf->header, psf->headindex, 1, psf->file) ;
		} ;

	if (psf->fdata)
		free (psf->fdata) ;
	psf->fdata = NULL ;
	
	return 0 ;
} /* svx_close */

