/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
"This software module was originally developed by 
AT&T, Dolby Laboratories, Fraunhofer Gesellschaft IIS in the course of 
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

#ifndef	_all_h_
#define _all_h_

#include "interface.h"
#include "tns.h"
#include "bitstream.h"


typedef struct
{
    int is_present;	/* right channel uses intensiy stereo */
    int is_used[MAXBANDS];
    int sign[2*(MAXBANDS+1)];
    int fac[2*(MAXBANDS+1)];
    int bot[2*(MAXBANDS+1)];
    int top[2*(MAXBANDS+1)];
} IS_Info;

typedef struct
{
    int is_present;  
    int ms_used[MAXBANDS];
} MS_Info;

typedef struct
{
    int present;	/* channel present */
    int tag;		/* element tag */
    int cpe;		/* 0 if single channel or lfe, 1 if channel pair */ 
    int lfe;            /* 1 if lfe channel */             
    int	common_window;	/* 1 if common window for cpe */
    int	ch_is_left;	/* 1 if left channel of cpe */
    int	paired_ch;	/* index of paired channel in cpe */
    int widx;		/* window element index for this channel */
    IS_Info is_info;	/* IS information */
    MS_Info ms_info;    /* MS information */
} Ch_Info;

#endif	/* _all_h_ */

