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
#include	"ulaw.h"
#include	"alaw.h"
#include	"wav.h"

/*------------------------------------------------------------------------------
** List of known WAV format tags
*/

enum
{	
	WAVE_FORMAT_UNKNOWN				= 0x0000,		/* Microsoft Corporation */
	WAVE_FORMAT_PCM     		    = 0x0001, 		/* Microsoft PCM format */

	WAVE_FORMAT_MS_ADPCM			= 0x0002,		/* Microsoft ADPCM */
	WAVE_FORMAT_IEEE_FLOAT			= 0x0003,		/* Micrososft 32 bit float format */
	
	WAVE_FORMAT_IBM_CVSD			= 0x0005,		/* IBM Corporation */
	WAVE_FORMAT_ALAW				= 0x0006,		/* Microsoft Corporation */
	WAVE_FORMAT_MULAW				= 0x0007,		/* Microsoft Corporation */
	WAVE_FORMAT_OKI_ADPCM			= 0x0010,		/* OKI */
	WAVE_FORMAT_IMA_ADPCM			= 0x0011,		/* Intel Corporation */
	WAVE_FORMAT_MEDIASPACE_ADPCM	= 0x0012,		/* Videologic */
	WAVE_FORMAT_SIERRA_ADPCM		= 0x0013,		/* Sierra Semiconductor Corp */
	WAVE_FORMAT_G723_ADPCM			= 0x0014,		/* Antex Electronics Corporation */
	WAVE_FORMAT_DIGISTD				= 0x0015,		/* DSP Solutions, Inc. */
	WAVE_FORMAT_DIGIFIX				= 0x0016,		/* DSP Solutions, Inc. */
	WAVE_FORMAT_DIALOGIC_OKI_ADPCM	= 0x0017,		/*  Dialogic Corporation  */
	WAVE_FORMAT_MEDIAVISION_ADPCM	= 0x0018,		/*  Media Vision, Inc. */

	WAVE_FORMAT_YAMAHA_ADPCM		= 0x0020,		/* Yamaha Corporation of America */
	WAVE_FORMAT_SONARC				= 0x0021,		/* Speech Compression */
	WAVE_FORMAT_DSPGROUP_TRUESPEECH = 0x0022,		/* DSP Group, Inc */
	WAVE_FORMAT_ECHOSC1				= 0x0023,		/* Echo Speech Corporation */
	WAVE_FORMAT_AUDIOFILE_AF18  	= 0x0024,		/* Audiofile, Inc. */
	WAVE_FORMAT_APTX				= 0x0025,		/* Audio Processing Technology */
	WAVE_FORMAT_AUDIOFILE_AF10  	= 0x0026,		/* Audiofile, Inc. */

	WAVE_FORMAT_DOLBY_AC2			= 0x0030,		/* Dolby Laboratories */
	WAVE_FORMAT_GSM610				= 0x0031,		/* Microsoft Corporation */
	WAVE_FORMAT_MSNAUDIO			= 0x0032,		/* Microsoft Corporation */
	WAVE_FORMAT_ANTEX_ADPCME		= 0x0033, 		/* Antex Electronics Corporation */
	WAVE_FORMAT_CONTROL_RES_VQLPC	= 0x0034,		/* Control Resources Limited */
	WAVE_FORMAT_DIGIREAL			= 0x0035,		/* DSP Solutions, Inc. */
	WAVE_FORMAT_DIGIADPCM			= 0x0036,		/* DSP Solutions, Inc. */
	WAVE_FORMAT_CONTROL_RES_CR10	= 0x0037,		/* Control Resources Limited */
	WAVE_FORMAT_NMS_VBXADPCM		= 0x0038,		/* Natural MicroSystems */
	WAVE_FORMAT_ROCKWELL_ADPCM		= 0x003B,		/* Rockwell International */
	WAVE_FORMAT_ROCKWELL_DIGITALK	= 0x003C, 		/* Rockwell International */

	WAVE_FORMAT_G721_ADPCM			= 0x0040,		/* Antex Electronics Corporation */
	WAVE_FORMAT_MPEG				= 0x0050,		/* Microsoft Corporation */

	WAVE_FORMAT_MPEGLAYER3			= 0x0055,		/* MPEG 3 Layer 1 */

	IBM_FORMAT_MULAW				= 0x0101,		/* IBM mu-law format */
	IBM_FORMAT_ALAW					= 0x0102,		/* IBM a-law format */
	IBM_FORMAT_ADPCM				= 0x0103,		/* IBM AVC Adaptive Differential PCM format */

	WAVE_FORMAT_CREATIVE_ADPCM		= 0x0200,		/* Creative Labs, Inc */

	WAVE_FORMAT_FM_TOWNS_SND		= 0x0300,		/* Fujitsu Corp. */
	WAVE_FORMAT_OLIGSM				= 0x1000,		/* Ing C. Olivetti & C., S.p.A. */
	WAVE_FORMAT_OLIADPCM			= 0x1001,		/* Ing C. Olivetti & C., S.p.A. */
	WAVE_FORMAT_OLICELP				= 0x1002,		/* Ing C. Olivetti & C., S.p.A. */
	WAVE_FORMAT_OLISBC				= 0x1003,		/* Ing C. Olivetti & C., S.p.A. */
	WAVE_FORMAT_OLIOPR				= 0x1004,		/* Ing C. Olivetti & C., S.p.A. */

	WAVE_FORMAT_EXTENSIBLE			= 0xFFFE
} ;

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

#define RIFF_MARKER	(MAKE_MARKER ('R', 'I', 'F', 'F')) 
#define WAVE_MARKER	(MAKE_MARKER ('W', 'A', 'V', 'E')) 
#define fmt_MARKER	(MAKE_MARKER ('f', 'm', 't', ' ')) 
#define data_MARKER	(MAKE_MARKER ('d', 'a', 't', 'a')) 
#define cue_MARKER	(MAKE_MARKER ('c', 'u', 'e', ' ')) 
#define LIST_MARKER	(MAKE_MARKER ('L', 'I', 'S', 'T')) 
#define slnt_MARKER	(MAKE_MARKER ('s', 'l', 'n', 't')) 
#define wavl_MARKER	(MAKE_MARKER ('w', 'a', 'v', 'l')) 
#define INFO_MARKER	(MAKE_MARKER ('I', 'N', 'F', 'O')) 
#define plst_MARKER	(MAKE_MARKER ('p', 'l', 's', 't')) 
#define adtl_MARKER	(MAKE_MARKER ('a', 'd', 't', 'l')) 
#define labl_MARKER	(MAKE_MARKER ('l', 'a', 'b', 'l')) 
#define note_MARKER	(MAKE_MARKER ('n', 'o', 't', 'e')) 
#define fact_MARKER	(MAKE_MARKER ('f', 'a', 'c', 't')) 
#define smpl_MARKER	(MAKE_MARKER ('s', 'm', 'p', 'l')) 
#define bext_MARKER	(MAKE_MARKER ('b', 'e', 'x', 't')) 
#define MEXT_MARKER	(MAKE_MARKER ('M', 'E', 'X', 'T')) 
#define DISP_MARKER	(MAKE_MARKER ('D', 'I', 'S', 'P')) 

/*------------------------------------------------------------------------------
 * Private static functions.
 */

static int		read_fmt_chunk	(SF_PRIVATE *psf, WAV_FMT *wav_fmt) ;
static int		write_header (SF_PRIVATE *psf, WAV_FMT *wav_fmt, unsigned int size, int do_fact) ;

static const 	char* wav_format_str (int k) ;

static void		le2h_wav_fmt (WAV_FMT *fmt) ;
static void		h2le_wav_fmt (WAV_FMT *fmt) ;

/*------------------------------------------------------------------------------
 * Public functions.
 */

int 	wav_open_read	(SF_PRIVATE *psf)
{	WAV_FMT			wav_fmt ;
	FACT_CHUNK		fact_chunk ;
	unsigned int	dword, marker, RIFFsize ;
	int				parsestage = 0, error, format = 0 ;
	
	psf->sf.seekable = SF_TRUE ;

	while (1)
	{	fread (&marker, sizeof (marker), 1, psf->file) ;
		switch (marker)
		{	case RIFF_MARKER :
					if (parsestage != 0)
						return SFE_WAV_NO_RIFF ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					RIFFsize = LE2H_INT (dword) ;	
					if (psf->filelength  < RIFFsize + 2 * sizeof (dword))
					{	dword = psf->filelength - 2 * sizeof (dword);
						psf_sprintf (psf, "RIFF : %d (should be %d)\n", RIFFsize, dword) ;
						RIFFsize = dword ;
						}
					else
						psf_sprintf (psf, "RIFF : %d\n", RIFFsize) ;
					parsestage = 1 ;
					break ;
					
			case WAVE_MARKER :
					if (parsestage != 1)
						return SFE_WAV_NO_WAVE ;
					psf_sprintf (psf, "WAVE\n") ;
					parsestage = 2 ;
					break ;
			
			case fmt_MARKER :
					if (parsestage != 2)
						return SFE_WAV_NO_FMT ;
					if ((error = read_fmt_chunk (psf, &wav_fmt)))
						return error ;
						
					format     = wav_fmt.format ;
					parsestage = 3 ;
					break ;
					
			case data_MARKER :
					if (parsestage < 3)
						return SFE_WAV_NO_DATA ;
					fread (&dword, sizeof (dword), 1, psf->file) ;
					psf->datalength = LE2H_INT (dword) ;
					psf->dataoffset = ftell (psf->file) ;
					
					if (psf->filelength < psf->dataoffset + psf->datalength)
					{	psf_sprintf (psf, "data : %d (should be %d)\n", psf->datalength, psf->filelength - psf->dataoffset) ;
						psf->datalength = psf->filelength - psf->dataoffset ;
						}
					else
						psf_sprintf (psf, "data : %d\n", psf->datalength) ;

					if (format == WAVE_FORMAT_MS_ADPCM && psf->datalength % 2)
					{	psf->datalength ++ ;
						psf_sprintf (psf, "*** Data length odd. Increasing it by 1.\n") ;
						} ;
		
					fseek (psf->file, psf->datalength, SEEK_CUR) ;
					dword = ftell (psf->file) ;
					if (dword != (off_t) (psf->dataoffset + psf->datalength))
						psf_sprintf (psf, "*** fseek past end error ***\n", dword, psf->dataoffset + psf->datalength) ;
					break ;

			case fact_MARKER :
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = LE2H_INT (dword) ;
					fread (&fact_chunk, sizeof (fact_chunk), 1, psf->file) ;
					if (dword > sizeof (fact_chunk))
						fseek (psf->file, (int) (dword - sizeof (fact_chunk)), SEEK_CUR) ;
					fact_chunk.samples = LE2H_INT (fact_chunk.samples) ;
					psf_sprintf (psf, "%D : %d\n", marker, dword) ;
					psf_sprintf (psf, "  samples : %d\n", fact_chunk.samples) ;
					break ;

			case cue_MARKER :
			case LIST_MARKER :
			case INFO_MARKER :
			case smpl_MARKER :
			case bext_MARKER :
			case MEXT_MARKER :
			case DISP_MARKER :
					fread (&dword, sizeof (dword), 1, psf->file) ;
					dword = LE2H_INT (dword) ;
					psf_sprintf (psf, "%D : %d\n", marker, dword) ;
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
					break ;
			} ;	/* switch (dword) */

		if (ferror (psf->file))
		{	psf_sprintf (psf, "*** Error on file handle. ***\n", marker) ;
			clearerr (psf->file) ;
			break ;
			} ;

		if (ftell (psf->file) >= (int) (psf->filelength - (2 * sizeof (dword))))
			break ;
		} ; /* while (1) */
		
	if (! psf->dataoffset)
		return SFE_WAV_NO_DATA ;

	psf->current     = 0 ;
	psf->endian      = SF_ENDIAN_LITTLE ;		/* All WAV files are little endian. */
	psf->sf.sections = 1 ;
	
	fseek (psf->file, psf->dataoffset, SEEK_SET) ;
	
	psf->close = (func_close) wav_close ;

	if (psf->blockwidth)
	{	if (psf->filelength - psf->dataoffset < psf->datalength)
			psf->sf.samples = (psf->filelength - psf->dataoffset) / psf->blockwidth ;
		else
			psf->sf.samples = psf->datalength / psf->blockwidth ;
		} ;

	switch (format)
	{	case 	WAVE_FORMAT_PCM :
		case	WAVE_FORMAT_EXTENSIBLE :
					break ;
					
		case	WAVE_FORMAT_MULAW :
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_ULAW) ;
					psf->read_short  = (func_short)  ulaw_read_ulaw2s ;
					psf->read_int    = (func_int)    ulaw_read_ulaw2i ;
					psf->read_double = (func_double) ulaw_read_ulaw2d ;
					return 0 ;
	
		case	WAVE_FORMAT_ALAW :
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_ALAW) ;
					psf->read_short  = (func_short)  alaw_read_alaw2s ;
					psf->read_int    = (func_int)    alaw_read_alaw2i ;
					psf->read_double = (func_double) alaw_read_alaw2d ;
					return 0 ;
		
		case	WAVE_FORMAT_MS_ADPCM : 
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_MS_ADPCM) ;
					if ((error = msadpcm_reader_init (psf, &wav_fmt)))
						return error ;
					psf->read_short  = (func_short)  msadpcm_read_s ;
					psf->read_int    = (func_int)    msadpcm_read_i ;
					psf->read_double = (func_double) msadpcm_read_d ;
					psf->seek_func   = (func_seek)   msadpcm_seek ;
					return 0 ;

		case	WAVE_FORMAT_IMA_ADPCM :
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_IMA_ADPCM) ;
					if ((error = wav_ima_reader_init (psf, &wav_fmt)))
						return error ;
					psf->read_short  = (func_short)  ima_read_s ;
					psf->read_int    = (func_int)    ima_read_i ;
					psf->read_double = (func_double) ima_read_d ;
					psf->seek_func   = (func_seek)   ima_seek ;
					return 0 ;
		
		case	WAVE_FORMAT_GSM610 :
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_GSM610) ;
					if ((error = wav_gsm610_reader_init (psf, &wav_fmt)))
						return error ;
					psf->read_short  = (func_short)  wav_gsm610_read_s ;
					psf->read_int    = (func_int)    wav_gsm610_read_i ;
					psf->read_double = (func_double) wav_gsm610_read_d ;
					psf->seek_func   = NULL ;  /* Not seekable */
					return 0 ;
		
		case	WAVE_FORMAT_IEEE_FLOAT :
					psf->sf.format   = (SF_FORMAT_WAV | SF_FORMAT_FLOAT) ;
					if (CAN_READ_WRITE_x86_IEEE)
					{	psf->read_short  = (func_short)  pcm_read_f2s ;
						psf->read_int    = (func_int)    pcm_read_f2i ;
						psf->read_double = (func_double) pcm_read_f2d ;
						}
					else 
					{	psf->read_short  = (func_short)  wav_read_x86f2s ;
						psf->read_int    = (func_int)    wav_read_x86f2i ;
						psf->read_double = (func_double) wav_read_x86f2d ;
						} ;
					return 0 ;
		
		default :	return SFE_UNIMPLEMENTED ;
		} ;

	psf->sf.format = (SF_FORMAT_WAV | SF_FORMAT_PCM) ;
	switch (psf->bytewidth)
	{	case  1 :
				psf->read_short  = (func_short)  pcm_read_uc2s ;
				psf->read_int    = (func_int)    pcm_read_uc2i ;
				psf->read_double = (func_double) pcm_read_uc2d ;
				break ;
		case  2 :
				psf->read_short  = (func_short)  pcm_read_les2s ;
				psf->read_int    = (func_int)    pcm_read_les2i ;
				psf->read_double = (func_double) pcm_read_les2d ;
				break ;
		case  3 :
				psf->read_short  = (func_short)  pcm_read_let2s ;
				psf->read_int    = (func_int)    pcm_read_let2i ;
				psf->read_double = (func_double) pcm_read_let2d ;
				break ;
		case  4 :
				psf->read_short  = (func_short)  pcm_read_lei2s ;
				psf->read_int    = (func_int)    pcm_read_lei2i ;
				psf->read_double = (func_double) pcm_read_lei2d ;
				break ;
		default : return SFE_UNIMPLEMENTED ;
		} ;

	return 0 ;
} /* wav_open_read */

/*------------------------------------------------------------------------------
 */

int 	wav_open_write	(SF_PRIVATE *psf)
{	WAV_FMT			wav_fmt ;
	unsigned int	dword, subformat ;
	int				error ;
	
	if ((psf->sf.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV)
		return	SFE_BAD_OPEN_FORMAT ;
	
	psf->endian      = SF_ENDIAN_LITTLE ;		/* All WAV files are little endian. */
	psf->sf.seekable = SF_TRUE ;
	psf->error       = 0 ;

	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;
	if (subformat == SF_FORMAT_ULAW || subformat == SF_FORMAT_ALAW)
		psf->bytewidth = 1 ;
	else
		psf->bytewidth = BITWIDTH2BYTES (psf->sf.pcmbitwidth) ;

	psf->blockwidth  = psf->bytewidth * psf->sf.channels ;

	wav_fmt.min.channels    = psf->sf.channels ;
	wav_fmt.min.samplerate  = psf->sf.samplerate ;
	wav_fmt.min.bytespersec = psf->sf.samplerate * psf->bytewidth * psf->sf.channels ;
	wav_fmt.min.blockalign  = psf->bytewidth * psf->sf.channels ;
	wav_fmt.min.bitwidth    = psf->sf.pcmbitwidth ;


	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case	SF_FORMAT_PCM : 
					wav_fmt.format = WAVE_FORMAT_PCM ;

					psf->dataoffset  = 7 * sizeof (dword) + sizeof (MIN_WAV_FMT) ;
					psf->datalength  = psf->blockwidth * psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (MIN_WAV_FMT), 0) ;
					break ;

		case	SF_FORMAT_FLOAT : 
					wav_fmt.format = WAVE_FORMAT_IEEE_FLOAT ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (MIN_WAV_FMT) + sizeof (FACT_CHUNK) ;
					psf->datalength  = psf->blockwidth * psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (MIN_WAV_FMT), 1) ;
					break ;

		case	SF_FORMAT_ULAW : 
					wav_fmt.format = WAVE_FORMAT_MULAW ;

					wav_fmt.size20.bitwidth = 8 ;
					wav_fmt.size20.extrabytes = 2 ;
					wav_fmt.size20.dummy = 0 ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (WAV_FMT_SIZE20) + sizeof (FACT_CHUNK) ;
					
					psf->datalength  = psf->blockwidth * psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (WAV_FMT_SIZE20), 1) ;
					break ;
					
		case	SF_FORMAT_ALAW :
					wav_fmt.format = WAVE_FORMAT_ALAW ;

					wav_fmt.size20.bitwidth = 8 ;
					wav_fmt.size20.extrabytes = 2 ;
					wav_fmt.size20.dummy = 0 ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (WAV_FMT_SIZE20) + sizeof (FACT_CHUNK) ;
					
					psf->datalength  = psf->blockwidth * psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (WAV_FMT_SIZE20), 1) ;
					break ;

		case	SF_FORMAT_IMA_ADPCM : 
					wav_fmt.format = WAVE_FORMAT_IMA_ADPCM ;
					if ((error = wav_ima_writer_init (psf, &wav_fmt)))
						return error ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (IMA_ADPCM_WAV_FMT) + sizeof (FACT_CHUNK) ;
					if (psf->sf.samples % wav_fmt.ima.samplesperblock)
						psf->datalength  = ((psf->sf.samples / wav_fmt.ima.samplesperblock) + 1) * wav_fmt.ima.samplesperblock ;
					else
						psf->datalength  = psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (IMA_ADPCM_WAV_FMT), 1) ;
					break ;

		case	SF_FORMAT_MS_ADPCM : 
					wav_fmt.format = WAVE_FORMAT_MS_ADPCM ;
					msadpcm_writer_init (psf, &wav_fmt) ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (MS_ADPCM_WAV_FMT) + sizeof (FACT_CHUNK) ;
					if (psf->sf.samples % wav_fmt.msadpcm.samplesperblock)
						psf->datalength  = ((psf->sf.samples / wav_fmt.msadpcm.samplesperblock) + 1) * wav_fmt.msadpcm.samplesperblock ;
					else
						psf->datalength  = psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (MS_ADPCM_WAV_FMT), 1) ;
					break ;

		case	SF_FORMAT_GSM610 : 
					wav_fmt.format = WAVE_FORMAT_GSM610 ;
					wav_gsm610_writer_init (psf, &wav_fmt) ;

					psf->dataoffset  = 9 * sizeof (dword) + sizeof (GSM610_WAV_FMT) + sizeof (FACT_CHUNK) ;
					if (psf->sf.samples % wav_fmt.gsm610.samplesperblock)
						psf->datalength  = ((psf->sf.samples / wav_fmt.gsm610.samplesperblock) + 1) * wav_fmt.gsm610.samplesperblock ;
					else
						psf->datalength  = psf->sf.samples ;
					psf->filelength  = psf->datalength + psf->dataoffset ;

					write_header (psf, &wav_fmt, sizeof (GSM610_WAV_FMT), 1) ;
					break ;

		default : 	return SFE_UNIMPLEMENTED ;
		} ;


	dword = data_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;

	dword = H2LE_INT (psf->datalength) ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	psf->close = (func_close) wav_close ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_FLOAT)
	{	psf->sf.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT) ;
		if (CAN_READ_WRITE_x86_IEEE)
		{	psf->write_short  = (func_short)  pcm_write_s2f ;
			psf->write_int    = (func_int)    pcm_write_i2f ;
			psf->write_double = (func_double) pcm_write_d2f ;
			}
		else 
		{	psf->write_short  = (func_short)  wav_write_s2x86f ;
			psf->write_int    = (func_int)    wav_write_i2x86f ;
			psf->write_double = (func_double) wav_write_d2x86f ;
			} ;
		return	 0 ;
		} ;
	
	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_IMA_ADPCM)
	{	psf->sf.format    = (SF_FORMAT_WAV | SF_FORMAT_IMA_ADPCM) ;
		psf->write_short  = (func_short)  ima_write_s ;
		psf->write_int    = (func_int)    ima_write_i ;
		psf->write_double = (func_double) ima_write_d ;
		psf->seek_func    = (func_seek)   ima_seek ;
		psf->close        = (func_close)  wav_ima_close ;
		return 0 ;
		} ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_MS_ADPCM)
	{	psf->sf.format    = (SF_FORMAT_WAV | SF_FORMAT_MS_ADPCM) ;
		psf->write_short  = (func_short)  msadpcm_write_s ;
		psf->write_int    = (func_int)    msadpcm_write_i ;
		psf->write_double = (func_double) msadpcm_write_d ;
		psf->seek_func    = (func_seek)   msadpcm_seek ;
		psf->close        = (func_close)  msadpcm_close ;
		return 0 ;
		} ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_GSM610)
	{	psf->sf.format    = (SF_FORMAT_WAV | SF_FORMAT_GSM610) ;
		psf->write_short  = (func_short)  wav_gsm610_write_s ;
		psf->write_int    = (func_int)    wav_gsm610_write_i ;
		psf->write_double = (func_double) wav_gsm610_write_d ;
		psf->seek_func    = NULL ; /* Not seekable */
		psf->close        = (func_close)  wav_gsm610_close ;
		return 0 ;
		} ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_ULAW)
	{	psf->sf.format    = (SF_FORMAT_WAV | SF_FORMAT_ULAW) ;
		psf->write_short  = (func_short)  ulaw_write_s2ulaw ;
		psf->write_int    = (func_int)    ulaw_write_i2ulaw ;
		psf->write_double = (func_double) ulaw_write_d2ulaw ;
		return 0 ;
		} ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) == SF_FORMAT_ALAW)
	{	psf->sf.format    = (SF_FORMAT_WAV | SF_FORMAT_ALAW) ;
		psf->write_short  = (func_short)  alaw_write_s2alaw ;
		psf->write_int    = (func_int)    alaw_write_i2alaw ;
		psf->write_double = (func_double) alaw_write_d2alaw ;
		return 0 ;
		} ;

	if ((psf->sf.format & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM)
		return SFE_UNIMPLEMENTED ;

	switch (psf->bytewidth)
	{	case  1 :
				psf->write_short  = (func_short)  pcm_write_s2uc ;
				psf->write_int    = (func_int)    pcm_write_i2uc ;
				psf->write_double = (func_double) pcm_write_d2uc ;
				break ;
		case  2 :
				psf->write_short  = (func_short)  pcm_write_s2les ;
				psf->write_int    = (func_int)    pcm_write_i2les ;
				psf->write_double = (func_double) pcm_write_d2les ;
				break ;
		case  3 :
				psf->write_short  = (func_short)  pcm_write_s2let ;
				psf->write_int    = (func_int)    pcm_write_i2let ;
				psf->write_double = (func_double) pcm_write_d2let ;
				break ;
		case  4 :
				psf->write_short  = (func_short)  pcm_write_s2lei ;
				psf->write_int    = (func_int)    pcm_write_i2lei ;
				psf->write_double = (func_double) pcm_write_d2lei ;
				break ;
		default : return SFE_UNIMPLEMENTED ;
		} ;

	return 0 ;
} /* wav_open_write */

/*------------------------------------------------------------------------------
 */

int	wav_close	(SF_PRIVATE  *psf)
{	unsigned int		dword ;

	if (psf->mode == SF_MODE_WRITE)
	{	/*  Now we know for certain the length of the file we can
		 *  re-write correct values for the RIFF and data chunks.
		 */

		fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;

		/* Fix RIFF size. */
		dword = H2LE_INT (psf->filelength - 2 * sizeof (dword)) ;
		fseek (psf->file, sizeof (dword), SEEK_SET) ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		
		psf->datalength = psf->filelength - psf->dataoffset ;
		psf->sf.samples = psf->datalength / (psf->sf.channels * psf->bytewidth) ;
		fseek (psf->file, (int) (psf->dataoffset - sizeof (dword)), SEEK_SET) ;
		dword = H2LE_INT (psf->datalength) ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		} ;

	if (psf->fdata)
		free (psf->fdata) ;
	psf->fdata = NULL ;

	return 0 ;
} /* wav_close */


/*=========================================================================
 * Private functions.
 */

static
int read_fmt_chunk (SF_PRIVATE *psf, WAV_FMT *wav_fmt)
{	unsigned int	dword, bytesread, k, structsize, bytespersec = 0  ;
	
	memset (wav_fmt, 0, sizeof (WAV_FMT)) ;
	bytesread = 0 ;

	fread (&dword, sizeof (dword), 1, psf->file) ;
	structsize = LE2H_INT (dword) ;
	
	psf_sprintf (psf, "fmt  : %d\n", structsize) ;
	
	if (structsize < 16)
		return SFE_WAV_FMT_SHORT ;
	if (structsize > sizeof (WAV_FMT))
		return SFE_WAV_FMT_TOO_BIG ;
					
	fread (wav_fmt, structsize, 1, psf->file) ;
	bytesread += structsize ;
	if (CPU_IS_BIG_ENDIAN)
		le2h_wav_fmt (wav_fmt) ;

	psf_sprintf (psf, "  Format        : 0x%X => %s\n", wav_fmt->format, wav_format_str (wav_fmt->format)) ;
	psf_sprintf (psf, "  Channels      : %d\n", wav_fmt->min.channels) ;
	psf_sprintf (psf, "  Sample Rate   : %d\n", wav_fmt->min.samplerate) ;
	psf_sprintf (psf, "  Block Align   : %d\n", wav_fmt->min.blockalign) ;
	psf_sprintf (psf, "  Bit Width     : %d\n", wav_fmt->min.bitwidth) ;
	

	psf->sf.samplerate		= wav_fmt->min.samplerate ;
	psf->sf.samples 		= 0 ;					/* Correct this when reading data chunk. */
	psf->sf.channels		= wav_fmt->min.channels ;
	
	switch (wav_fmt->format)
	{	case WAVE_FORMAT_PCM :
		case WAVE_FORMAT_IEEE_FLOAT :
				bytespersec = wav_fmt->min.samplerate * wav_fmt->min.blockalign ;
				if (wav_fmt->min.bytespersec != bytespersec)
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->min.bytespersec, bytespersec) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->min.bytespersec) ;
		
				psf->sf.pcmbitwidth	= wav_fmt->min.bitwidth ;
				psf->bytewidth      = BITWIDTH2BYTES (wav_fmt->min.bitwidth) ;
				break ;

		case WAVE_FORMAT_ALAW :
		case WAVE_FORMAT_MULAW :
				if (wav_fmt->min.bytespersec / wav_fmt->min.blockalign != wav_fmt->min.samplerate)
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->min.bytespersec, wav_fmt->min.samplerate * wav_fmt->min.blockalign) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->min.bytespersec) ;

				psf->sf.pcmbitwidth	= 16 ;
				psf->bytewidth      = 1 ;
				if (structsize >= 18)
					psf_sprintf (psf, "  Extra Bytes   : %d\n", wav_fmt->size20.extrabytes) ;
				break ;

		case WAVE_FORMAT_MS_ADPCM :
				if (wav_fmt->msadpcm.bitwidth != 4)
					return SFE_WAV_ADPCM_NOT4BIT ;
				if (wav_fmt->msadpcm.channels < 1 || wav_fmt->msadpcm.channels > 2)
					return SFE_WAV_ADPCM_CHANNELS ;

				bytespersec = (wav_fmt->msadpcm.samplerate * wav_fmt->msadpcm.blockalign) / wav_fmt->msadpcm.samplesperblock ;
				if (wav_fmt->min.bytespersec == bytespersec)
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->min.bytespersec) ;
				else if (wav_fmt->min.bytespersec == (wav_fmt->msadpcm.samplerate / wav_fmt->msadpcm.samplesperblock) * wav_fmt->msadpcm.blockalign) 
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d (MS BUG!))\n", wav_fmt->min.bytespersec, bytespersec) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->min.bytespersec, bytespersec) ;

				psf->sf.pcmbitwidth = 16 ;
				psf->bytewidth      = 2 ;
				psf_sprintf (psf, "  Extra Bytes   : %d\n", wav_fmt->msadpcm.extrabytes) ;
				psf_sprintf (psf, "  Samples/Block : %d\n", wav_fmt->msadpcm.samplesperblock) ;
				if (wav_fmt->msadpcm.numcoeffs > sizeof (MS_ADPCM_WAV_FMT) / sizeof (int))
				{	psf_sprintf (psf, "  No. of Coeffs : %d ****\n", wav_fmt->msadpcm.numcoeffs) ;
					wav_fmt->msadpcm.numcoeffs = sizeof (MS_ADPCM_WAV_FMT) / sizeof (int) ;
					}
				else
					psf_sprintf (psf, "  No. of Coeffs : %d\n", wav_fmt->msadpcm.numcoeffs) ;
				psf_sprintf (psf, "    Coeff 1 : ") ;
				for (k = 0 ; k < wav_fmt->msadpcm.numcoeffs ; k++)
					psf_sprintf (psf, "%d ", wav_fmt->msadpcm.coeffs [k].coeff1) ;
				psf_sprintf (psf, "\n    Coeff 2 : ") ;
				for (k = 0 ; k < wav_fmt->msadpcm.numcoeffs ; k++)
					psf_sprintf (psf, "%d ", wav_fmt->msadpcm.coeffs [k].coeff2) ;
				psf_sprintf (psf, "\n") ;
				break ;
				
		case WAVE_FORMAT_IMA_ADPCM :
				if (wav_fmt->ima.bitwidth != 4)
					return SFE_WAV_ADPCM_NOT4BIT ;
				if (wav_fmt->ima.channels < 1 || wav_fmt->ima.channels > 2)
					return SFE_WAV_ADPCM_CHANNELS ;

				bytespersec = (wav_fmt->ima.samplerate * wav_fmt->ima.blockalign) / wav_fmt->ima.samplesperblock ;
				if (wav_fmt->ima.bytespersec != bytespersec)
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->ima.bytespersec, bytespersec) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->ima.bytespersec) ;

				psf->sf.pcmbitwidth = 16 ;
				psf->bytewidth      = 2 ;
				psf_sprintf (psf, "  Extra Bytes   : %d\n", wav_fmt->ima.extrabytes) ;
				psf_sprintf (psf, "  Samples/Block : %d\n", wav_fmt->ima.samplesperblock) ;
				break ;
				
		case WAVE_FORMAT_EXTENSIBLE :
				if (wav_fmt->ext.bytespersec / wav_fmt->ext.blockalign != wav_fmt->ext.samplerate)
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->ext.bytespersec, wav_fmt->ext.samplerate * wav_fmt->ext.blockalign) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->ext.bytespersec) ;

				psf_sprintf (psf, "  Valid Bits    : %d\n", wav_fmt->ext.validbits) ;
				psf_sprintf (psf, "  Channel Mask  : 0x%X\n", wav_fmt->ext.channelmask) ;
				psf_sprintf (psf, "  Subformat\n") ;
				psf_sprintf (psf, "    esf_field1 : 0x%X\n", wav_fmt->ext.esf.esf_field1) ;
				psf_sprintf (psf, "    esf_field2 : 0x%X\n", wav_fmt->ext.esf.esf_field2) ;
				psf_sprintf (psf, "    esf_field3 : 0x%X\n", wav_fmt->ext.esf.esf_field3) ;
				psf_sprintf (psf, "    esf_field4 : ") ;
				for (k = 0 ; k < 8 ; k++)
					psf_sprintf (psf, "0x%X ", wav_fmt->ext.esf.esf_field4 [k] & 0xFF) ;
				psf_sprintf (psf, "\n") ;
				psf->sf.pcmbitwidth = wav_fmt->ext.bitwidth ;
				psf->bytewidth      = BITWIDTH2BYTES (wav_fmt->ext.bitwidth) ;
				break ;

		case WAVE_FORMAT_GSM610 :
				if (wav_fmt->gsm610.channels != 1 || wav_fmt->gsm610.blockalign != 65)
					return SFE_WAV_GSM610_FORMAT ;

				if (wav_fmt->gsm610.samplesperblock != 320)
					return SFE_WAV_GSM610_FORMAT ;

				bytespersec = (wav_fmt->gsm610.samplerate * wav_fmt->gsm610.blockalign) / wav_fmt->gsm610.samplesperblock ;
				if (wav_fmt->gsm610.bytespersec != bytespersec)
					psf_sprintf (psf, "  Bytes/sec     : %d (should be %d)\n", wav_fmt->gsm610.bytespersec, bytespersec) ;
				else
					psf_sprintf (psf, "  Bytes/sec     : %d\n", wav_fmt->gsm610.bytespersec) ;

				psf->sf.pcmbitwidth = 16 ;
				psf->bytewidth      = 2 ;
				psf_sprintf (psf, "  Extra Bytes   : %d\n", wav_fmt->gsm610.extrabytes) ;
				psf_sprintf (psf, "  Samples/Block : %d\n", wav_fmt->gsm610.samplesperblock) ;
				break ;
		default : break ;
		} ;
	
	psf->blockwidth = wav_fmt->min.channels * psf->bytewidth ;

	return 0 ;
} /* read_fmt_chunk */

static
int write_header (SF_PRIVATE *psf, WAV_FMT *wav_fmt, unsigned int size, int do_fact)
{	FACT_CHUNK		fact_chunk ;
	unsigned int dword, RIFFsize ;

	RIFFsize   = psf->filelength - 2 * sizeof (dword) ;

	dword = RIFF_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	dword = H2LE_INT (RIFFsize) ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	dword = WAVE_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	dword = fmt_MARKER ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	
	dword = H2LE_INT (size) ;
	fwrite (&dword, sizeof (dword), 1, psf->file) ;
	if (CPU_IS_BIG_ENDIAN)
		h2le_wav_fmt (wav_fmt) ;	
	fwrite (wav_fmt, size, 1, psf->file) ;
	
	if (do_fact)
	{	dword = fact_MARKER ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		dword = H2LE_INT (sizeof (FACT_CHUNK)) ;
		fwrite (&dword, sizeof (dword), 1, psf->file) ;
		fact_chunk.samples = H2LE_INT (psf->sf.samples) ;
		fwrite (&fact_chunk, sizeof (fact_chunk), 1, psf->file) ;
		} ;

	return 0 ;
} /* write_header */

static 
void	le2h_wav_fmt (WAV_FMT *fmt)
{	int		k ;

	fmt->min.format      = ENDSWAP_SHORT (fmt->min.format) ;
	fmt->min.channels    = ENDSWAP_SHORT (fmt->min.channels) ;
	fmt->min.samplerate  = ENDSWAP_INT   (fmt->min.samplerate) ;
	fmt->min.bytespersec = ENDSWAP_INT   (fmt->min.bytespersec) ;
	fmt->min.blockalign  = ENDSWAP_SHORT (fmt->min.blockalign) ;
	fmt->min.bitwidth    = ENDSWAP_SHORT (fmt->min.bitwidth) ;

	switch (fmt->format)
	{	case WAVE_FORMAT_MS_ADPCM :	
				fmt->msadpcm.extrabytes      = ENDSWAP_SHORT (fmt->msadpcm.extrabytes) ;
				fmt->msadpcm.samplesperblock = ENDSWAP_SHORT (fmt->msadpcm.samplesperblock) ;
				fmt->msadpcm.numcoeffs       = ENDSWAP_SHORT (fmt->msadpcm.numcoeffs) ;
				for (k = 0 ; k < fmt->msadpcm.numcoeffs  ; k++)
				{	fmt->msadpcm.coeffs [k].coeff1 = ENDSWAP_SHORT (fmt->msadpcm.coeffs [k].coeff1) ;
					fmt->msadpcm.coeffs [k].coeff2 = ENDSWAP_SHORT (fmt->msadpcm.coeffs [k].coeff2) ;
					} ;
				break ;
				
		case WAVE_FORMAT_IMA_ADPCM :	
				fmt->ima.extrabytes      = ENDSWAP_SHORT (fmt->ima.extrabytes) ;
				fmt->ima.samplesperblock = ENDSWAP_SHORT (fmt->ima.samplesperblock) ;
				break ;
				
		case WAVE_FORMAT_ALAW :	
		case WAVE_FORMAT_MULAW :	
				fmt->size20.extrabytes = ENDSWAP_SHORT (fmt->size20.extrabytes) ;
				fmt->size20.dummy      = ENDSWAP_SHORT (fmt->size20.dummy) ;
				break ;
				
		case	WAVE_FORMAT_GSM610 :
				fmt->gsm610.extrabytes      = ENDSWAP_SHORT (fmt->gsm610.extrabytes) ;
				fmt->gsm610.samplesperblock = ENDSWAP_SHORT (fmt->gsm610.samplesperblock) ;
				break ;
				
		default : break ;
		} ;

} /* le2h_wav_fmt */

static 
void	h2le_wav_fmt (WAV_FMT *fmt)
{	int		k ;

	switch (fmt->format)
	{	case WAVE_FORMAT_MS_ADPCM :	
				for (k = 0 ; k < fmt->msadpcm.numcoeffs  ; k++)
				{	fmt->msadpcm.coeffs [k].coeff1 = ENDSWAP_SHORT (fmt->msadpcm.coeffs [k].coeff1) ;
					fmt->msadpcm.coeffs [k].coeff2 = ENDSWAP_SHORT (fmt->msadpcm.coeffs [k].coeff2) ;
					} ;
				fmt->msadpcm.numcoeffs       = ENDSWAP_SHORT (fmt->msadpcm.numcoeffs) ;
				fmt->msadpcm.extrabytes      = ENDSWAP_SHORT (fmt->msadpcm.extrabytes) ;
				fmt->msadpcm.samplesperblock = ENDSWAP_SHORT (fmt->msadpcm.samplesperblock) ;
				break ;
				
		case WAVE_FORMAT_IMA_ADPCM :	
				fmt->ima.extrabytes      = ENDSWAP_SHORT (fmt->ima.extrabytes) ;
				fmt->ima.samplesperblock = ENDSWAP_SHORT (fmt->ima.samplesperblock) ;
				break ;
				
		case WAVE_FORMAT_ALAW :	
		case WAVE_FORMAT_MULAW :	
				fmt->size20.extrabytes = ENDSWAP_SHORT (fmt->size20.extrabytes) ;
				fmt->size20.dummy      = ENDSWAP_SHORT (fmt->size20.dummy) ;
				break ;
				
		case	WAVE_FORMAT_GSM610 :
				fmt->gsm610.extrabytes      = ENDSWAP_SHORT (fmt->gsm610.extrabytes) ;
				fmt->gsm610.samplesperblock = ENDSWAP_SHORT (fmt->gsm610.samplesperblock) ;
				break ;
				
		default : break ;
		} ;

	fmt->min.format      = ENDSWAP_SHORT (fmt->min.format) ;
	fmt->min.channels    = ENDSWAP_SHORT (fmt->min.channels) ;
	fmt->min.samplerate  = ENDSWAP_INT   (fmt->min.samplerate) ;
	fmt->min.bytespersec = ENDSWAP_INT   (fmt->min.bytespersec) ;
	fmt->min.blockalign  = ENDSWAP_SHORT (fmt->min.blockalign) ;
	fmt->min.bitwidth    = ENDSWAP_SHORT (fmt->min.bitwidth) ;

} /* h2le_wav_fmt */

static
const char* wav_format_str (int k)
{	switch (k)
	{	case WAVE_FORMAT_UNKNOWN :
			return "WAVE_FORMAT_UNKNOWN" ;
		case WAVE_FORMAT_PCM          :
			return "WAVE_FORMAT_PCM         " ;
		case WAVE_FORMAT_MS_ADPCM :
			return "WAVE_FORMAT_MS_ADPCM" ;
		case WAVE_FORMAT_IEEE_FLOAT :
			return "WAVE_FORMAT_IEEE_FLOAT" ;
		case WAVE_FORMAT_IBM_CVSD :
			return "WAVE_FORMAT_IBM_CVSD" ;
		case WAVE_FORMAT_ALAW :
			return "WAVE_FORMAT_ALAW" ;
		case WAVE_FORMAT_MULAW :
			return "WAVE_FORMAT_MULAW" ;
		case WAVE_FORMAT_OKI_ADPCM :
			return "WAVE_FORMAT_OKI_ADPCM" ;
		case WAVE_FORMAT_IMA_ADPCM :
			return "WAVE_FORMAT_IMA_ADPCM" ;
		case WAVE_FORMAT_MEDIASPACE_ADPCM :
			return "WAVE_FORMAT_MEDIASPACE_ADPCM" ;
		case WAVE_FORMAT_SIERRA_ADPCM :
			return "WAVE_FORMAT_SIERRA_ADPCM" ;
		case WAVE_FORMAT_G723_ADPCM :
			return "WAVE_FORMAT_G723_ADPCM" ;
		case WAVE_FORMAT_DIGISTD :
			return "WAVE_FORMAT_DIGISTD" ;
		case WAVE_FORMAT_DIGIFIX :
			return "WAVE_FORMAT_DIGIFIX" ;
		case WAVE_FORMAT_DIALOGIC_OKI_ADPCM :
			return "WAVE_FORMAT_DIALOGIC_OKI_ADPCM" ;
		case WAVE_FORMAT_MEDIAVISION_ADPCM :
			return "WAVE_FORMAT_MEDIAVISION_ADPCM" ;
		case WAVE_FORMAT_YAMAHA_ADPCM :
			return "WAVE_FORMAT_YAMAHA_ADPCM" ;
		case WAVE_FORMAT_SONARC :
			return "WAVE_FORMAT_SONARC" ;
		case WAVE_FORMAT_DSPGROUP_TRUESPEECH  :
			return "WAVE_FORMAT_DSPGROUP_TRUESPEECH " ;
		case WAVE_FORMAT_ECHOSC1 :
			return "WAVE_FORMAT_ECHOSC1" ;
		case WAVE_FORMAT_AUDIOFILE_AF18   :
			return "WAVE_FORMAT_AUDIOFILE_AF18  " ;
		case WAVE_FORMAT_APTX :
			return "WAVE_FORMAT_APTX" ;
		case WAVE_FORMAT_AUDIOFILE_AF10   :
			return "WAVE_FORMAT_AUDIOFILE_AF10  " ;
		case WAVE_FORMAT_DOLBY_AC2 :
			return "WAVE_FORMAT_DOLBY_AC2" ;
		case WAVE_FORMAT_GSM610 :
			return "WAVE_FORMAT_GSM610" ;
		case WAVE_FORMAT_MSNAUDIO :
			return "WAVE_FORMAT_MSNAUDIO" ;
		case WAVE_FORMAT_ANTEX_ADPCME :
			return "WAVE_FORMAT_ANTEX_ADPCME" ;
		case WAVE_FORMAT_CONTROL_RES_VQLPC :
			return "WAVE_FORMAT_CONTROL_RES_VQLPC" ;
		case WAVE_FORMAT_DIGIREAL :
			return "WAVE_FORMAT_DIGIREAL" ;
		case WAVE_FORMAT_DIGIADPCM :
			return "WAVE_FORMAT_DIGIADPCM" ;
		case WAVE_FORMAT_CONTROL_RES_CR10 :
			return "WAVE_FORMAT_CONTROL_RES_CR10" ;
		case WAVE_FORMAT_NMS_VBXADPCM :
			return "WAVE_FORMAT_NMS_VBXADPCM" ;
		case WAVE_FORMAT_ROCKWELL_ADPCM :
			return "WAVE_FORMAT_ROCKWELL_ADPCM" ;
		case WAVE_FORMAT_ROCKWELL_DIGITALK :
			return "WAVE_FORMAT_ROCKWELL_DIGITALK" ;
		case WAVE_FORMAT_G721_ADPCM :
			return "WAVE_FORMAT_G721_ADPCM" ;
		case WAVE_FORMAT_MPEG :
			return "WAVE_FORMAT_MPEG" ;
		case WAVE_FORMAT_MPEGLAYER3 :
			return "WAVE_FORMAT_MPEGLAYER3" ;
		case IBM_FORMAT_MULAW :
			return "IBM_FORMAT_MULAW" ;
		case IBM_FORMAT_ALAW :
			return "IBM_FORMAT_ALAW" ;
		case IBM_FORMAT_ADPCM :
			return "IBM_FORMAT_ADPCM" ;
		case WAVE_FORMAT_CREATIVE_ADPCM :
			return "WAVE_FORMAT_CREATIVE_ADPCM" ;
		case WAVE_FORMAT_FM_TOWNS_SND :
			return "WAVE_FORMAT_FM_TOWNS_SND" ;
		case WAVE_FORMAT_OLIGSM :
			return "WAVE_FORMAT_OLIGSM" ;
		case WAVE_FORMAT_OLIADPCM :
			return "WAVE_FORMAT_OLIADPCM" ;
		case WAVE_FORMAT_OLICELP :
			return "WAVE_FORMAT_OLICELP" ;
		case WAVE_FORMAT_OLISBC :
			return "WAVE_FORMAT_OLISBC" ;
		case WAVE_FORMAT_OLIOPR :
			return "WAVE_FORMAT_OLIOPR" ;
		case WAVE_FORMAT_EXTENSIBLE :
			return "WAVE_FORMAT_EXTENSIBLE" ;
		break ;
		} ;
	return "Unknown format" ;
} /* wav_format_str */
