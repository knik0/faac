/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
"This software module was originally developed by 
AT&T, Dolby Laboratories, Fraunhofer Gesellschaft IIS 
and edited by Takashi Koike (Sony Corporation),
Yasuhiro Toguri(Sony Corporation) in the course of 
development of the MPEG-2 NBC/MPEG-4 Audio standard ISO/IEC 13818-7, 
14496-1,2 and 3. This software module is an implementation of a part of one or more 
MPEG-2 NBC/MPEG-4 Audio tools as specified by the MPEG-2 NBC/MPEG-4 
Audio standard. ISO/IEC  gives users of the MPEG-2 NBC/MPEG-4 Audio 
standards free license to this software module or modifications thereof for use in 
hardware or software products claiming conformance to the MPEG-2 NBC/MPEG-4
Audio  standards. Those intending to use this software module in hardware or 
software products are advised that this use may infringe existing patents. 
The original developer of this software module and his/her company, the subsequent 
editors and their companies, and ISO/IEC have no liability for use of this software 
module or modifications thereof in an implementation. Copyright is not released for 
non MPEG-2 NBC/MPEG-4 Audio conforming products.The original developer
retains full right to use the code for his/her  own purpose, assign or donate the 
code to a third party and to inhibit third party from using the code for non 
MPEG-2 NBC/MPEG-4 Audio conforming products. This copyright notice must
be included in all copies or derivative works." 
Copyright(c)1996.
 *                                                                           *
 ****************************************************************************/
#ifndef BLOCK_H
#define BLOCK_H 1

#include "dolby_def.h"


#define IN_DATATYPE  double
#define OUT_DATATYPE double

#define BLOCK_LEN_LONG	   1024
#define BLOCK_LEN_MEDIUM   512
#define BLOCK_LEN_SHORT    128
#define BLOCK_LEN_LONG_S   960
#define BLOCK_LEN_MEDIUM_S 480
#define BLOCK_LEN_SHORT_S  120
#define BLOCK_LEN_LONG_SSR	  256 /* 980129 added for SSR */
#define BLOCK_LEN_SHORT_SSR   32  /* 980129 added for SSR */

#define NWINLONG	(BLOCK_LEN_LONG)
#define ALFALONG	4.0
#define NWINSHORT	(BLOCK_LEN_SHORT)
#define ALFASHORT	7.0

#define	NWINFLAT	(NWINLONG)					/* flat params */
#define	NWINADV		(NWINLONG-NWINSHORT)		/* Advanced flat params */
#define NFLAT		((NWINFLAT-NWINSHORT)/2)
#define NADV0		((NWINADV-NWINSHORT)/2)


typedef enum {
    WS_FHG, WS_DOLBY, N_WINDOW_SHAPES
} 
Window_shape;

/* YT 970615 for Son_PP  */
typedef enum {
	MOVERLAPPED,
	MNON_OVERLAPPED
}
Mdct_in,Imdct_out;

typedef enum {
    WT_LONG, 
    WT_SHORT, 
    WT_FLAT, 
    WT_ADV,			/* Advanced flat window */
    N_WINDOW_TYPES
} 
WINDOW_TYPE_AAC; 

typedef enum {                  /* ADVanced transform types */
    LONG_BLOCK,
    START_BLOCK,
    SHORT_BLOCK,
    STOP_BLOCK,
    START_ADV_BLOCK,
    STOP_ADV_BLOCK,
    START_FLAT_BLOCK,
    STOP_FLAT_BLOCK,
    N_BLOCK_TYPES
} 
BLOCK_TYPE;

typedef enum {  		/* Advanced window sequence (frame) types */
    ONLY_LONG,
    LONG_START, 
    LONG_STOP,
    SHORT_START, 
    SHORT_STOP,
    EIGHT_SHORT, 
    SHORT_EXT_STOP,
    NINE_SHORT,
    OLD_START,
    OLD_STOP,
    N_WINDOW_SEQUENCES
} 
WINDOW_SEQUENCE;


#endif	/*	BLOCK_H	*/

