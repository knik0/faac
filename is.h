/*******************************************************************************************
 *
 * IS stereo coding module
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 *
 * Changes:
 * 22-jan-98   CL   Initial revision.  Simple IS stereo support.
 ***************************************************************************************/

#ifndef IS_ENC
#define IS_ENC

#include "all.h"
#include "tf_main.h"
#include "aac_qc.h"
#include <math.h>

#define IS_MIN_BAND_L 0 //28
#define IS_MIN_BAND_S 0 //7

void ISEncode(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
	      Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
	      int sfb_offset_table[][MAX_SCFAC_BANDS+1],
	      enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
	      AACQuantInfo* quantInfo,
	      int numberOfChannels);                 /* Number of channels */

void ISReconstruct(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
		   Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
		   int sfb_offset_table[][MAX_SCFAC_BANDS+1],
		   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
		   AACQuantInfo* quantInfo,
		   int numberOfChannels);                 /* Number of channels */



#endif

