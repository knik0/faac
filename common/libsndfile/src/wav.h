#ifndef WAV_H_INCLUDED
#define WAV_H_INCLUDED

#ifdef _WIN32
	#pragma pack(push,1)
#endif

typedef	struct
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
} MIN_WAV_FMT ;

typedef	struct 
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
	unsigned short	extrabytes ;
	unsigned short	dummy ;
} WAV_FMT_SIZE20 ;

typedef	struct
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
	unsigned short	extrabytes ;
	unsigned short	samplesperblock ;
	unsigned short	numcoeffs ;
	struct
	{	short	coeff1 ;
		short	coeff2 ;
	}	coeffs [7] ;
} MS_ADPCM_WAV_FMT ;

typedef	struct
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
	unsigned short	extrabytes ;
	unsigned short	samplesperblock ;
} IMA_ADPCM_WAV_FMT ;

typedef struct
{	unsigned int	esf_field1 ;
	unsigned short	esf_field2 ;
	unsigned short	esf_field3 ;
	unsigned char	esf_field4 [8] ;
} EXT_SUBFORMAT ;

typedef	struct
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
	unsigned short	extrabytes ;
	unsigned short	validbits ;
	unsigned int	channelmask ;
	EXT_SUBFORMAT	esf ;
} EXTENSIBLE_WAV_FMT ;

typedef	struct
{	unsigned short	format ;
	unsigned short	channels ;
	unsigned int	samplerate ;
	unsigned int	bytespersec ;
	unsigned short	blockalign ;
	unsigned short	bitwidth ;
	unsigned short	extrabytes ;
	unsigned short	samplesperblock ;
} GSM610_WAV_FMT ;

typedef union
{	unsigned short		format ;
	MIN_WAV_FMT			min ;
	IMA_ADPCM_WAV_FMT	ima ;
	MS_ADPCM_WAV_FMT	msadpcm ;
	EXTENSIBLE_WAV_FMT	ext ;
	GSM610_WAV_FMT		gsm610 ;
	WAV_FMT_SIZE20		size20 ;
	char				padding [512] ;
} WAV_FMT ;

typedef struct
{	unsigned int samples ;
} FACT_CHUNK ;

/*------------------------------------------------------------------------------------ 
**	Functions defined in wav.c
*/

int		wav_close	(SF_PRIVATE  *psf) ;

/*------------------------------------------------------------------------------------ 
**	Functions defined in wav_float.c
*/

int		wav_read_x86f2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		wav_read_x86f2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		wav_read_x86f2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

int		wav_write_s2x86f (SF_PRIVATE *psf, short *ptr, int len) ;
int		wav_write_i2x86f (SF_PRIVATE *psf, int *ptr, int len) ;
int		wav_write_d2x86f (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

/*------------------------------------------------------------------------------------ 
**	Functions defined in wav_ima_adpcm.c
*/

off_t	ima_seek   (SF_PRIVATE *psf, off_t offset, int whence) ;
int		wav_ima_close	(SF_PRIVATE  *psf) ;

int		wav_ima_reader_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		ima_read_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		ima_read_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		ima_read_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

int		wav_ima_writer_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		ima_write_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		ima_write_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		ima_write_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

/*------------------------------------------------------------------------------------ 
**	Functions defined in wav_ms_adpcm.c
*/

off_t	msadpcm_seek   (SF_PRIVATE *psf, off_t offset, int whence) ;
int		msadpcm_close	(SF_PRIVATE  *psf) ;

int		msadpcm_reader_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		msadpcm_read_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		msadpcm_read_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		msadpcm_read_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

int		msadpcm_writer_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		msadpcm_write_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		msadpcm_write_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		msadpcm_write_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;


/*------------------------------------------------------------------------------------ 
**	Functions defined in wav_gsm610.c
*/

/*
**	off_t	wav_gsm610_seek   (SF_PRIVATE *psf, off_t offset, int whence) ;
*/
int		wav_gsm610_close	(SF_PRIVATE  *psf) ;

int		wav_gsm610_reader_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		wav_gsm610_read_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		wav_gsm610_read_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		wav_gsm610_read_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

int		wav_gsm610_writer_init (SF_PRIVATE *psf, WAV_FMT *fmt) ;
int		wav_gsm610_write_s (SF_PRIVATE *psf, short *ptr, int len) ;
int		wav_gsm610_write_i (SF_PRIVATE *psf, int *ptr, int len) ;
int		wav_gsm610_write_d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;


#ifdef _WIN32
	#pragma pack(pop,1)
#endif

#endif
