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

/* For the Metrowerks CodeWarrior Pro Compiler (mainly MacOS) */

#if	(defined (__MWERKS__))
#include	<stat.h>
#else
#include	<sys/stat.h>
#endif

#include	"sndfile.h"
#include	"config.h"
#include	"sfendian.h"
#include	"common.h"

#if (CPU_IS_LITTLE_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		((a)|((b)<<8)|((c)<<16)|((d)<<24))
#elif (CPU_IS_BIG_ENDIAN == 1)
#	define	MAKE_MARKER(a,b,c,d)		(((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#	error "Cannot determine endian-ness of processor."
#endif

#define		SNDFILE_MAGICK	0x1234C0DE

typedef struct
{	int 	error ;
	char	*str ;
} ErrorStruct ;

static
ErrorStruct SndfileErrors [] = 
{ 
	{	SFE_NO_ERROR			, "No Error." },
	{	SFE_BAD_FILE			, "File does not exist or is not a regular file (possibly a pipe?)." },
	{	SFE_OPEN_FAILED			, "Could not open file." },
	{	SFE_BAD_OPEN_FORMAT		, "Bad format specified for file open." },
	{	SFE_BAD_SNDFILE_PTR		, "Not a valid SNDFILE* pointer." },
	{	SFE_BAD_SF_INFO_PTR		, "NULL SF_INFO pointer passed to libsndfile." },
	{	SFE_BAD_INT_FD			, "Bad file descriptor." },
	{	SFE_BAD_INT_PTR			, "Internal error, Bad pointer." },
	{	SFE_MALLOC_FAILED		, "Internal malloc () failed." },
	{	SFE_BAD_SEEK			, "Internal fseek() failed." },
	{	SFE_NOT_SEEKABLE		, "Seek attempted on unseekable file type." },
	{	SFE_UNIMPLEMENTED		, "File contains data in an unimplemented format." },
	{	SFE_BAD_READ_ALIGN  	, "Attempt to read a non-integer number of channels." },
	{	SFE_BAD_WRITE_ALIGN 	, "Attempt to write a non-integer number of channels." },
	{	SFE_UNKNOWN_FORMAT		, "File contains data in an unknown format." },
	{	SFE_NOT_READMODE		, "Read attempted on file currently open for write." },
	{	SFE_NOT_WRITEMODE		, "Write attempted on file currently open for read." },
	{	SFE_BAD_MODE_RW			, "This file format does not support read/write mode." },
	{	SFE_BAD_SF_INFO			, "Internal error : SF_INFO struct incomplete." },

	{	SFE_SHORT_READ			, "Short read error." },
	{	SFE_SHORT_WRITE			, "Short write error." },
	{	SFE_INTERNAL			, "Unspecified internal error." },
	
	{	SFE_WAV_NO_RIFF			, "Error in WAV file. No 'RIFF' chunk marker." },
	{	SFE_WAV_NO_WAVE			, "Error in WAV file. No 'WAVE' chunk marker." },
	{	SFE_WAV_NO_FMT			, "Error in WAV file. No 'fmt ' chunk marker." },
	{	SFE_WAV_FMT_SHORT		, "Error in WAV file. Short 'fmt ' chunk." },

	{	SFE_WAV_FMT_TOO_BIG		, "Error in WAV file. 'fmt ' chunk too large." },

	{	SFE_WAV_BAD_FORMAT		, "Error in WAV file. Errors in 'fmt ' chunk." },
	{	SFE_WAV_BAD_BLOCKALIGN	, "Error in WAV file. Block alignment in 'fmt ' chunk is incorrect." },
	{	SFE_WAV_NO_DATA			, "Error in WAV file. No 'data' chunk marker." },
	{	SFE_WAV_UNKNOWN_CHUNK	, "Error in WAV file. File contains an unknown chunk marker." },
	
	{	SFE_WAV_ADPCM_NOT4BIT	, "Error in ADPCM WAV file. Invalid bit width."},
	{	SFE_WAV_ADPCM_CHANNELS	, "Error in ADPCM WAV file. Invalid number of channels."},
	{	SFE_WAV_GSM610_FORMAT	, "Error in GSM610 WAV file. Invalid format chunk."},
  
	{	SFE_AIFF_NO_FORM		, "Error in AIFF file, bad 'FORM' marker."},
	{	SFE_AIFF_UNKNOWN_CHUNK	, "Error in AIFF file, unknown chunk."},
	{	SFE_COMM_CHUNK_SIZE		, "Error in AIFF file, bad 'COMM' chunk size."},
	{	SFE_AIFF_NO_SSND		, "Error in AIFF file, bad 'SSND' chunk."},
	{	SFE_AIFF_NO_DATA		, "Error in AIFF file, no sound data."},
	
	{	SFE_AU_UNKNOWN_FORMAT	, "Error in AU file, unknown format."},
	{	SFE_AU_NO_DOTSND		, "Error in AU file, missing '.snd' or 'dns.' marker."},

	{	SFE_RAW_READ_BAD_SPEC	, "Error while opening RAW file for read. Must specify format, pcmbitwidth and channels."},
	{	SFE_RAW_BAD_BITWIDTH	, "Error. RAW file bitwidth must be a multiple of 8."},

	{	SFE_PAF_NO_MARKER		, "Error in PAF file, no marker."},
	{	SFE_PAF_VERSION			, "Error in PAF file, bad version."}, 
	{	SFE_PAF_UNKNOWN_FORMAT	, "Error in PAF file, unknown format."}, 
	{	SFE_PAF_SHORT_HEADER	, "Error in PAF file. File shorter than minimal header."},
	
	{	SFE_SVX_NO_FORM			, "Error in 8SVX / 16SV file, no 'FORM' marker."}, 
	{	SFE_SVX_NO_BODY			, "Error in 8SVX / 16SV file, no 'BODY' marker."}, 
	{	SFE_SVX_NO_DATA			, "Error in 8SVX / 16SV file, no sound data."},
	{	SFE_SVX_BAD_COMP		, "Error in 8SVX / 16SV file, unsupported compression format."},
	
	{	SFE_NIST_BAD_HEADER		, "Error in NIST file, bad header."},
	{	SFE_NIST_BAD_ENCODING	, "Error in NIST file, unsupported compression format."},

	{	SFE_MAX_ERROR			, "Maximum error number." }
} ;

/*------------------------------------------------------------------------------
** Private (static) variables.
*/

static	int		sf_errno = 0 ;
static	char	sf_strbuffer [SF_BUFFER_LEN] = { 0 } ;

/*------------------------------------------------------------------------------
** Private static functions.
*/


#define	VALIDATE_SNDFILE_AND_ASSIGN_PSF(a,b)		\
		{	if (! (a))								\
				return SFE_BAD_SNDFILE_PTR ;		\
			(b) = (SF_PRIVATE*) (a) ;				\
			if (!((b)->file))						\
				return SFE_BAD_INT_FD ;				\
			if ((b)->Magick != SNDFILE_MAGICK)			\
				return	SFE_BAD_SNDFILE_PTR ;		\
			(b)->error = 0 ;						\
			} 

static
int does_extension_match (const char *ext, const char *test)
{	char c1, c2 ;

	if ((! ext) || (! test))
		return 0 ;

	if (strlen (ext) != strlen (test))
		return 0 ;

	while (*ext && *test)
	{	c1 = tolower (*ext) ;
		c2 = tolower (*test) ;
		if (c1 > c2)
			return 0 ;
		if (c1 < c2)
			return 0 ;
		ext ++ ;
		test ++ ;
		} 

	return 1 ;
} /* does_extension_match */

static
int is_au_snd_file (const char *filename)
{	const char *cptr ;

	if (! (cptr = strrchr (filename, '.')))
		return 0 ;
	cptr ++ ;
	
	if (does_extension_match (cptr, "au"))
		return 1 ;
		
	if (does_extension_match (cptr, "snd"))
		return 1 ;
		
	return 0 ;
} /* is_au_snd_file */

static
int guess_file_type (FILE *file, const char *filename)
{	unsigned int buffer [3] ;

	fread (buffer, sizeof(buffer), 1, file) ;
	fseek (file, 0, SEEK_SET) ;

	if (buffer [0] == MAKE_MARKER ('R','I','F','F') && buffer [2] == MAKE_MARKER ('W','A','V','E'))
		return SF_FORMAT_WAV ;
		
	if (buffer [0] == MAKE_MARKER ('F','O','R','M'))
	{	if (buffer [2] == MAKE_MARKER ('A','I','F','F') || buffer [2] == MAKE_MARKER ('A','I','F','C'))
			return SF_FORMAT_AIFF ;
		if (buffer [2] == MAKE_MARKER ('8','S','V','X') || buffer [2] == MAKE_MARKER ('1','6','S','V'))
			return SF_FORMAT_SVX ;
		return 0 ;
		} ;
		
	if ((buffer [0] == MAKE_MARKER ('.','s','n','d') || buffer [0] == MAKE_MARKER ('d','n','s','.')))
		return SF_FORMAT_AU ;
		
	if ((buffer [0] == MAKE_MARKER ('f','a','p',' ') || buffer [0] == MAKE_MARKER (' ','p','a','f')))
		return SF_FORMAT_PAF ;
	
	if (buffer [0] == MAKE_MARKER ('N','I','S','T'))
		return SF_FORMAT_NIST ;
		
	if (filename && is_au_snd_file (filename))
		return SF_FORMAT_AU | SF_FORMAT_ULAW ;

	/* Default to header-less RAW PCM file type. */
	return SF_FORMAT_RAW ;
} /* guess_file_type */


static
int validate_sfinfo (SF_INFO *sfinfo)
{	if (! sfinfo->samplerate)
		return 0 ;	
	if (! sfinfo->samples)
		return 0 ;	
	if (! sfinfo->channels)
		return 0 ;	
	if (! sfinfo->pcmbitwidth)
		return 0 ;	
	if (! sfinfo->format & SF_FORMAT_TYPEMASK)
		return 0 ;	
	if (! sfinfo->format & SF_FORMAT_SUBMASK)
		return 0 ;	
	if (! sfinfo->sections)
		return 0 ;	
	return 1 ;
} /* validate_sfinfo */

static
int validate_psf (SF_PRIVATE *psf)
{	if (! psf->blockwidth)
		return 0 ;	
	if (! psf->bytewidth)
		return 0 ;	
	if (! psf->datalength)
		return 0 ;
	if (psf->blockwidth != psf->sf.channels * psf->bytewidth)
		return 0 ;	
	return 1 ;
} /* validate_psf */

static
void save_header_info (SF_PRIVATE *psf)
{	memset (sf_strbuffer, 0, sizeof (sf_strbuffer)) ;
	strcpy (sf_strbuffer, psf->strbuffer) ;
} /* validate_psf */

static	
void copy_filename (SF_PRIVATE *psf, const char *path)
{	const char *cptr ;

	if ((cptr = strrchr (path, '/')) || (cptr = strrchr (path, '\\')))
		cptr ++ ;
	else
		cptr = path ;
		
	memset (psf->filename, 0, SF_FILENAME_LEN) ;
	strncpy (psf->filename, cptr, SF_FILENAME_LEN - 1) ;
	psf->filename [SF_FILENAME_LEN - 1] = 0 ;
} /* copy_filename */

/*------------------------------------------------------------------------------
**	Public functions.
*/

SNDFILE* 	sf_open_read	(const char *path, SF_INFO *sfinfo)
{	SF_PRIVATE 	*psf ;
	int			filetype, seekable ;
	
	if (! sfinfo)
	{	sf_errno = SFE_BAD_SF_INFO_PTR ;
		return	NULL ;
		} ;

	sf_errno = 0 ;
	sf_strbuffer [0] = 0 ;
	
	psf = malloc (sizeof (SF_PRIVATE)) ; 
	if (! psf)
	{	sf_errno = SFE_MALLOC_FAILED ;
		return	NULL ;
		} ;

	memset (psf, 0, sizeof (SF_PRIVATE)) ;
	psf->Magick = SNDFILE_MAGICK ;

	if (! strcmp (path, "-"))
	{	psf->file = stdin ;
		seekable = SF_FALSE ;
		}
	else
	{	/* fopen with 'b' means binary file mode for Win32 systems. */
		psf->file = fopen (path, "rb") ;
		seekable = SF_TRUE ;
		} ;
		 
	if (! psf->file) 
	{	sf_errno = SFE_OPEN_FAILED ;
		sf_close (psf) ;
		return NULL ;
		} ;
	
	psf->mode = SF_MODE_READ ;
	
	psf_sprintf (psf, "%s\n", path) ;
	if (seekable)
	{	fseek (psf->file, 0, SEEK_END) ;
		psf->filelength = ftell (psf->file) ;
		fseek (psf->file, 0, SEEK_SET) ;
		psf_sprintf (psf, "size : %d\n", psf->filelength) ;
		}
	else
		psf->filelength = 2048 ;
	
	copy_filename (psf, path) ;
	
	filetype = guess_file_type (psf->file, psf->filename) ;
	fseek (psf->file, 0, SEEK_SET) ;
	switch (filetype)
	{	case	SF_FORMAT_WAV :
				sf_errno = wav_open_read (psf) ;
				break ;

		case	SF_FORMAT_AIFF :
				sf_errno = aiff_open_read (psf) ;
				break ;

		case	SF_FORMAT_AU :
				sf_errno = au_open_read (psf) ;
				break ;

		case	SF_FORMAT_AU | SF_FORMAT_ULAW :
				psf_sprintf (psf, "Headers-less u-law encoded file.\n") ;
				sf_errno = au_nh_open_read (psf) ;
				break ;

		case	SF_FORMAT_RAW :
				/* For RAW files, need the sfinfo struct data to
				** figure out the bitwidth, endian-ness etc.
				*/
				memcpy (&(psf->sf), sfinfo, sizeof (SF_INFO)) ;
				sf_errno = raw_open_read (psf) ;
				break ;

		case	SF_FORMAT_PAF :
				sf_errno = paf_open_read (psf) ;
				break ;

		case	SF_FORMAT_SVX :
				sf_errno = svx_open_read (psf) ;
				break ;

		default :	
				sf_errno = SFE_UNKNOWN_FORMAT ;
		} ;
	
	/* Both the file format and the file must be seekable to enable sf_seek(). */
	psf->sf.seekable = (psf->sf.seekable && seekable) ? SF_TRUE : SF_FALSE ;
		
	if (sf_errno)
	{	save_header_info (psf) ;
		sf_close (psf) ;
		return NULL ;
		} ;

	if (! validate_sfinfo (&(psf->sf)))
	{	save_header_info (psf) ;
		sf_errno = SFE_BAD_SF_INFO ;
		sf_close (psf) ;
		return NULL ;
		} ;
		
	if (! validate_psf (psf))
	{	save_header_info (psf) ;
		sf_errno = SFE_INTERNAL ;
		sf_close (psf) ;
		return NULL ;
		} ;
		
	memcpy (sfinfo, &(psf->sf), sizeof (SF_INFO)) ;

	return (SNDFILE*) psf ;
} /* sf_open_read */

/*------------------------------------------------------------------------------
*/

SNDFILE* 	sf_open_write	(const char *path, const SF_INFO *sfinfo)
{	SF_PRIVATE 	*psf ;
	
	if (! sfinfo)
	{	sf_errno = SFE_BAD_SF_INFO_PTR ;
		return	NULL ;
		} ;
		
	if (! sf_format_check (sfinfo))
	{	sf_errno = SFE_BAD_OPEN_FORMAT ;
		return	NULL ;
		} ;

	sf_errno = 0 ;
	sf_strbuffer [0] = 0 ;
	
	psf = malloc (sizeof (SF_PRIVATE)) ; 
	if (! psf)
	{	sf_errno = SFE_MALLOC_FAILED ;
		return	NULL ;
		} ;
	
	memset (psf, 0, sizeof (SF_PRIVATE)) ;
	memcpy (&(psf->sf), sfinfo, sizeof (SF_INFO)) ;

	psf->Magick = SNDFILE_MAGICK ;

	/* fopen with 'b' means binary file mode for Win32 systems. */
	
	if (! (psf->file = fopen (path, "wb")))		
	{	sf_errno = SFE_OPEN_FAILED ;
		sf_close (psf) ;
		return NULL ;
		} ;
		
	psf->mode = SF_MODE_WRITE ;
	
	psf->filelength = ftell (psf->file) ;
	fseek (psf->file, 0, SEEK_SET) ;

	copy_filename (psf, path) ;
	
	switch (sfinfo->format & SF_FORMAT_TYPEMASK)
	{	case	SF_FORMAT_WAV :
				if ((sf_errno = wav_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;

		case	SF_FORMAT_AIFF :
				if ((sf_errno = aiff_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;

		case	SF_FORMAT_AU :
		case	SF_FORMAT_AULE :
				if ((sf_errno = au_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;
				
		case    SF_FORMAT_RAW :
				if ((sf_errno = raw_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;

		case    SF_FORMAT_PAF :
				if ((sf_errno = paf_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;

		case    SF_FORMAT_SVX :
				if ((sf_errno = svx_open_write (psf)))
				{	sf_close (psf) ;
					return NULL ;
					} ;
				break ;

		default :	
				sf_errno = SFE_UNKNOWN_FORMAT ;
				sf_close (psf) ;
				return NULL ;
		} ;				

	return (SNDFILE*) psf ;
} /* sf_open_write */

/*------------------------------------------------------------------------------
*/

int	sf_perror	(SNDFILE *sndfile)
{	SF_PRIVATE 	*psf ;
	int 		k, errnum ;

	if (! sndfile)
	{	errnum = sf_errno ;
		}
	else
	{	VALIDATE_SNDFILE_AND_ASSIGN_PSF(sndfile,psf) ;
		errnum = psf->error ;
		} ;
		
	errnum = (errnum >= SFE_MAX_ERROR || errnum < 0) ? 0 : errnum ;

	for (k = 0 ; SndfileErrors[k].str ; k++)
		if (errnum == SndfileErrors[k].error)
		{	printf ("%s\n", SndfileErrors[k].str) ;
			return SFE_NO_ERROR ;
			} ;
	
	printf ("No error string for error number %d.\n", errnum) ;
	return SFE_NO_ERROR ;
} /* sf_perror */


/*------------------------------------------------------------------------------
*/

int	sf_error_str	(SNDFILE *sndfile, char *str, size_t maxlen)
{	SF_PRIVATE 	*psf ;
	int 		errnum, k ;

	if (! sndfile)
	{	errnum = sf_errno ;
		}
	else
	{	VALIDATE_SNDFILE_AND_ASSIGN_PSF(sndfile,psf) ;
		errnum = psf->error ;
		} ;
		
	errnum = (errnum >= SFE_MAX_ERROR || errnum < 0) ? 0 : errnum ;

	for (k = 0 ; SndfileErrors[k].str ; k++)
		if (errnum == SndfileErrors[k].error)
		{	strncpy (str, SndfileErrors [errnum].str, maxlen) ;
			str [maxlen-1] = 0 ;
			return SFE_NO_ERROR ;
			} ;
			
	strncpy (str, "No error defined for this error number. This is a bug in libsndfile.", maxlen) ;		
	str [maxlen-1] = 0 ;
			
	return SFE_NO_ERROR ;
} /* sf_error_str */

/*------------------------------------------------------------------------------
*/

int	sf_error_number	(int errnum, char *str, size_t maxlen)
{	int 		k ;

	errnum = (errnum >= SFE_MAX_ERROR || errnum < 0) ? 0 : errnum ;

	for (k = 0 ; SndfileErrors[k].str ; k++)
		if (errnum == SndfileErrors[k].error)
		{	strncpy (str, SndfileErrors [errnum].str, maxlen) ;
			str [maxlen-1] = 0 ;
			return SFE_NO_ERROR ;
			} ;
			
	strncpy (str, "No error defined for this error number. This is a bug in libsndfile.", maxlen) ;		
	str [maxlen-1] = 0 ;
			
	return SFE_NO_ERROR ;
} /* sf_error_number */

/*------------------------------------------------------------------------------
*/

int		sf_format_check	(const SF_INFO *info)
{	int	subformat = info->format & SF_FORMAT_SUBMASK ;

	/* This is the place where each file format can check if the suppiled
	** SF_INFO struct is valid.
	** Return 0 on failure, 1 ons success. 
	*/
	
	if (info->channels < 1 || info->channels > 256)
		return 0 ;

	switch (info->format & SF_FORMAT_TYPEMASK)
	{	case SF_FORMAT_WAV :
				if (subformat == SF_FORMAT_PCM && (info->pcmbitwidth >= 8 && info->pcmbitwidth <= 32))
					return 1 ;
				if (subformat == SF_FORMAT_FLOAT && info->pcmbitwidth == 32)
					return 1 ;
				if (subformat == SF_FORMAT_IMA_ADPCM && info->pcmbitwidth == 16 && info->channels <= 2)
					return 1 ;
				if (subformat == SF_FORMAT_MS_ADPCM && info->pcmbitwidth == 16 && info->channels <= 2)
					return 1 ;
				if (subformat == SF_FORMAT_GSM610 && info->pcmbitwidth == 16 && info->channels == 1)
					return 1 ;
				if (subformat == SF_FORMAT_ULAW || subformat == SF_FORMAT_ALAW)
					return 1 ;
				break ;
				
		case SF_FORMAT_AIFF :
				if (subformat == SF_FORMAT_PCM && (info->pcmbitwidth >= 8 && info->pcmbitwidth <= 32))
					return 1 ;
				break ;
				
		case SF_FORMAT_AU :
		case SF_FORMAT_AULE :
				if (subformat == SF_FORMAT_PCM && (info->pcmbitwidth >= 8 && info->pcmbitwidth <= 32))
					return 1 ;
				if (subformat == SF_FORMAT_ULAW)
					return 1 ;
				if (subformat == SF_FORMAT_ALAW)
					return 1 ;
				if (subformat == SF_FORMAT_G721_32 && info->channels == 1)
					return 1 ;
				if (subformat == SF_FORMAT_G723_24 && info->channels == 1)
					return 1 ;
				break ;
				
		case SF_FORMAT_RAW :
				if (subformat == SF_FORMAT_PCM_S8 && info->pcmbitwidth == 8)
					return 1 ;
				if (subformat == SF_FORMAT_PCM_U8 && info->pcmbitwidth == 8)
					return 1 ;
				if (subformat != SF_FORMAT_PCM_BE && subformat != SF_FORMAT_PCM_LE)
					break ;
				if (info->pcmbitwidth % 8 || info->pcmbitwidth > 32)
					break ;
				return 1 ;
				break ;
				
		case SF_FORMAT_PAF :
				if (subformat == SF_FORMAT_PCM_S8 && info->pcmbitwidth == 8)
					return 1 ;
				if (subformat != SF_FORMAT_PCM_BE && subformat != SF_FORMAT_PCM_LE)
					break ;
				if (info->pcmbitwidth % 8 || info->pcmbitwidth > 24)
					break ;
				return 1 ;
				break ;
				
		case SF_FORMAT_SVX :
				if (subformat == SF_FORMAT_PCM && (info->pcmbitwidth == 8 || info->pcmbitwidth == 16))
					return 1 ;
				break ;
				
		default : break ;
		} ;

	return 0 ;
} /* sf_format_check */

/*------------------------------------------------------------------------------
*/

size_t	sf_get_header_info	(SNDFILE *sndfile, char *buffer, size_t bufferlen, size_t offset)
{	SF_PRIVATE	*psf ;
	int			len ;
	
	if (! sndfile)
	{	strncpy (buffer, sf_strbuffer, bufferlen - 1) ;
		buffer [bufferlen - 1] = 0 ;
		return strlen (sf_strbuffer) ;
		} ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	len = strlen (psf->strbuffer) ;
	if (offset < 0 || offset >= len)
		return 0 ;
	
	strncpy (buffer, psf->strbuffer, bufferlen - 1) ;
	buffer [bufferlen - 1] = 0 ;
	
	return strlen (psf->strbuffer) ;
} /* sf_get_header_info */

/*------------------------------------------------------------------------------
*/

size_t	sf_get_lib_version	(char *buffer, size_t bufferlen)
{	if (! buffer || ! bufferlen)
		return 0 ;

	strncpy (buffer, PACKAGE, bufferlen - 1) ;
	buffer [bufferlen - 1] = 0 ;
	strncat (buffer, "-", bufferlen - strlen (buffer) - 1) ;
	strncat (buffer, VERSION, bufferlen - strlen (buffer) - 1) ;
	
	return strlen (buffer) ;
} /* sf_get_lib_version */

/*------------------------------------------------------------------------------
*/

double  sf_signal_max   (SNDFILE *sndfile)
{	SF_PRIVATE 		*psf ;
	off_t			position ;
	unsigned int	k, len, readcount ;
	double 			max = 0.0, *data, temp ;
	
	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	if (! psf->sf.seekable)
	{	psf->error = SFE_NOT_SEEKABLE ;
		return	0.0 ;
		} ;
		
	
	if (! psf->read_double)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	-1 ;
		} ;
		
	position = sf_seek (sndfile, 0, SEEK_CUR) ; /* Get current position in file */
	sf_seek (sndfile, 0, SEEK_SET) ;			/* Go to start of file. */
	
	len = psf->sf.channels * 1024 ;
	
	data = malloc (len * sizeof (double)) ;
	readcount = len ;
	while (readcount == len)
	{	readcount = psf->read_double (psf, data, len, 0) ;
		for (k = 0 ; k < len ; k++)
		{	temp = data [k] ;
			temp = temp < 0.0 ? -temp : temp ;
			max  = temp > max ? temp : max ;
			} ;
		} ;
	free (data) ;
	
	sf_seek (sndfile, position, SEEK_SET) ;		/* Return to original position. */
	
	return	max ;
} /* sf_signal_max */

/*------------------------------------------------------------------------------
*/

off_t	sf_seek	(SNDFILE *sndfile, off_t offset, int whence)
{	SF_PRIVATE 	*psf ;
	off_t		realseek, position ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	if (! psf->sf.seekable)
	{	psf->error = SFE_NOT_SEEKABLE ;
		return	((off_t) -1) ;
		} ;
	
	if (psf->seek_func)
		return	psf->seek_func (psf, offset, whence) ;
		
	if (! (psf->blockwidth && psf->datalength && psf->dataoffset))
	{	psf->error = SFE_BAD_SEEK ;
		return	((off_t) -1) ;
		} ;
	
	switch (whence)
	{	case SEEK_SET :
				if (offset < 0 || offset * psf->blockwidth > psf->datalength)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				realseek = psf->dataoffset + offset * psf->blockwidth ;
				fseek (psf->file, realseek, SEEK_SET) ;
				position = ftell (psf->file) ;
				break ;
				
		case SEEK_CUR :
				realseek = offset * psf->blockwidth ;
				position = ftell (psf->file) - psf->dataoffset ;
				if (position + realseek > psf->datalength || position + realseek < 0)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				fseek (psf->file, realseek, SEEK_CUR) ;
				position = ftell (psf->file) ;
				break ;
				
		case SEEK_END :
				if (offset > 0 || psf->sf.samples + offset < 0)
				{	psf->error = SFE_BAD_SEEK ;
					return	((off_t) -1) ;
					} ;
				realseek = (psf->sf.samples + offset) * psf->blockwidth + psf->dataoffset ;

				fseek (psf->file, realseek, SEEK_SET) ;
				position = ftell (psf->file) ;
				break ;
				
		default : 
				psf->error = SFE_BAD_SEEK ;
				return	((off_t) -1) ;
		} ;

	psf->current = (position - psf->dataoffset) / psf->blockwidth ;
	return psf->current ;
} /* sf_seek */

/*------------------------------------------------------------------------------
*/

size_t		sf_read_raw		(SNDFILE *sndfile, void *ptr, size_t bytes)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	if (psf->mode != SF_MODE_READ)
	{	psf->error = SFE_NOT_READMODE ;
		return	((size_t) -1) ;
		} ;
	
	if (psf->current >= psf->datalength)
	{	memset (ptr, 0, bytes) ;
		return 0 ;
		} ;
	
	if (bytes % (psf->sf.channels * psf->bytewidth))
	{	psf->error = SFE_BAD_READ_ALIGN ;
		return (size_t) -1 ;
		} ;
	
	count = fread (ptr, 1, bytes, psf->file) ;
		
	if (count < bytes)
		memset (((char*)ptr) + count, 0, bytes - count) ;

	psf->current += count / psf->blockwidth ;
	
	return count ;
} /* sf_read_raw */

/*------------------------------------------------------------------------------
*/

size_t	sf_read_short		(SNDFILE *sndfile, short *ptr, size_t len)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	if (psf->mode != SF_MODE_READ)
	{	psf->error = SFE_NOT_READMODE ;
		return (size_t) -1 ;
		} ;
	
	if (len % psf->sf.channels)
	{	psf->error = SFE_BAD_READ_ALIGN ;
		return (size_t) -1 ;
		} ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, len * sizeof (short)) ;
		return 0 ; /* End of file. */
		} ;

	if (! psf->read_short)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_short (psf, ptr, len) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra = len - count ;
		memset (ptr + count, 0, extra * sizeof (short)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_read_short */

size_t	sf_readf_short		(SNDFILE *sndfile, short *ptr, size_t frames)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;
	
	if (psf->mode != SF_MODE_READ)
	{	psf->error = SFE_NOT_READMODE ;
		return (size_t) -1 ;
		} ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, frames * psf->sf.channels * sizeof (short)) ;
		return 0 ; /* End of file. */
		} ;

	if (! psf->read_short)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_short (psf, ptr, frames * psf->sf.channels) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra =  frames * psf->sf.channels - count ;
		memset (ptr + count, 0, extra * sizeof (short)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_readf_short */

/*------------------------------------------------------------------------------
*/

size_t	sf_read_int		(SNDFILE *sndfile, int *ptr, size_t len)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF   (sndfile,psf) ;

	if (psf->mode != SF_MODE_READ)
		return SFE_NOT_READMODE ;
	
	if (len % psf->sf.channels)
		return	(psf->error = SFE_BAD_READ_ALIGN) ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, len * sizeof (int)) ;
		return 0 ;
		} ;

	if (! psf->read_int)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_int (psf, ptr, len) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra = len - count ;
		memset (ptr + count, 0, extra * sizeof (int)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_read_int */

size_t	sf_readf_int		(SNDFILE *sndfile, int *ptr, size_t frames)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF   (sndfile,psf) ;

	if (psf->mode != SF_MODE_READ)
		return SFE_NOT_READMODE ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, frames * psf->sf.channels * sizeof (int)) ;
		return 0 ;
		} ;

	if (! psf->read_int)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_int (psf, ptr, frames * psf->sf.channels) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra = frames * psf->sf.channels - count ;
		memset (ptr + count, 0, extra * sizeof (int)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_readf_int */

/*------------------------------------------------------------------------------
*/

size_t	sf_read_double	(SNDFILE *sndfile, double *ptr, size_t len, int normalize)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile, psf) ;
	
	if (psf->mode != SF_MODE_READ)
		return SFE_NOT_READMODE ;
	
	if (len % psf->sf.channels)
		return	(psf->error = SFE_BAD_READ_ALIGN) ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, len * sizeof (double)) ;
		return 0 ;
		} ;
		
	if (! psf->read_double)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_double (psf, ptr, len, normalize) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra = len - count ;
		memset (ptr + count, 0, extra * sizeof (double)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_read_double */

size_t	sf_readf_double	(SNDFILE *sndfile, double *ptr, size_t frames, int normalize)
{	SF_PRIVATE 	*psf ;
	size_t		count, extra ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile, psf) ;
	
	if (psf->mode != SF_MODE_READ)
		return SFE_NOT_READMODE ;
	
	if (psf->current >= psf->sf.samples)
	{	memset (ptr, 0, frames * psf->sf.channels * sizeof (double)) ;
		return 0 ;
		} ;
		
	if (! psf->read_double)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return	(size_t) -1 ;
		} ;
		
	count = psf->read_double (psf, ptr, frames * psf->sf.channels, normalize) ;
	
	if (psf->current + count / psf->sf.channels > psf->sf.samples)
	{	count = (psf->sf.samples - psf->current) * psf->sf.channels ;
		extra = frames * psf->sf.channels - count ;
		memset (ptr + count, 0, extra * sizeof (double)) ;
		psf->current = psf->sf.samples ;
		} ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_readf_double */

/*------------------------------------------------------------------------------
*/

size_t	sf_write_raw	(SNDFILE *sndfile, void *ptr, size_t len)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (len % (psf->sf.channels * psf->bytewidth))
	{	psf->error = SFE_BAD_WRITE_ALIGN ;
		return (size_t) -1 ;
		} ;
	
	count = fwrite (ptr, 1, len, psf->file) ;
	
	psf->current += count / psf->blockwidth ;
	
	return count ;
} /* sf_write_raw */

/*------------------------------------------------------------------------------
*/

size_t	sf_write_short		(SNDFILE *sndfile, short *ptr, size_t len)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (len % psf->sf.channels)
	{	psf->error = SFE_BAD_WRITE_ALIGN ;
		return (size_t) -1 ;
		} ;
	
	if (! psf->write_short)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_short (sndfile, ptr, len) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_write_short */

size_t	sf_writef_short		(SNDFILE *sndfile, short *ptr, size_t frames)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (! psf->write_short)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_short (sndfile, ptr, frames * psf->sf.channels) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_writef_short */

/*------------------------------------------------------------------------------
*/

size_t	sf_write_int		(SNDFILE *sndfile, int *ptr, size_t len)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (len % psf->sf.channels)
	{	psf->error = SFE_BAD_WRITE_ALIGN ;
		return (size_t) -1 ;
		} ;
	
	if (! psf->write_int)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_int (sndfile, ptr, len) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_write_int */

size_t	sf_writef_int		(SNDFILE *sndfile, int *ptr, size_t frames)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (! psf->write_int)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_int (sndfile, ptr, frames * psf->sf.channels) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_writef_int */

/*------------------------------------------------------------------------------
*/

size_t	sf_write_double		(SNDFILE *sndfile, double *ptr, size_t len, int normalize)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (len % psf->sf.channels)
	{	psf->error = SFE_BAD_WRITE_ALIGN ;
		return	(size_t) -1 ;
		} ;
		
	if (! psf->write_double)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_double (sndfile, ptr, len, normalize) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count ;
} /* sf_write_double */

size_t	sf_writef_double		(SNDFILE *sndfile, double *ptr, size_t frames, int normalize)
{	SF_PRIVATE 	*psf ;
	size_t		count ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->mode != SF_MODE_WRITE)
	{	psf->error = SFE_NOT_WRITEMODE ;
		return (size_t) -1 ;
		} ;
	
	if (! psf->write_double)
	{	psf->error = SFE_UNIMPLEMENTED ;
		return (size_t) -1 ;
		} ;
		
	count = psf->write_double (sndfile, ptr, frames * psf->sf.channels, normalize) ;
	
	psf->current += count / psf->sf.channels ;
	
	return count / psf->sf.channels ;
} /* sf_writef_double */

/*------------------------------------------------------------------------------
*/

int	sf_close	(SNDFILE *sndfile)
{	SF_PRIVATE  *psf ;
	int			error ;

	VALIDATE_SNDFILE_AND_ASSIGN_PSF (sndfile,psf) ;

	if (psf->close)
		error = psf->close (psf) ;
	
	fclose (psf->file) ;
	memset (psf, 0, sizeof (SF_PRIVATE)) ;
		
	free (psf) ;

	return 0 ;
} /* sf_close */


/*=========================================================================
** Private functions.
*/


