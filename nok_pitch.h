/**************************************************************************

This software module was originally developed by
Nokia in the course of development of the MPEG-2 AAC/MPEG-4 
Audio standard ISO/IEC13818-7, 14496-1, 2 and 3.
This software module is an implementation of a part
of one or more MPEG-2 AAC/MPEG-4 Audio tools as specified by the
MPEG-2 aac/MPEG-4 Audio standard. ISO/IEC  gives users of the
MPEG-2aac/MPEG-4 Audio standards free license to this software module
or modifications thereof for use in hardware or software products
claiming conformance to the MPEG-2 aac/MPEG-4 Audio  standards. Those
intending to use this software module in hardware or software products
are advised that this use may infringe existing patents. The original
developer of this software module, the subsequent
editors and their companies, and ISO/IEC have no liability for use of
this software module or modifications thereof in an
implementation. Copyright is not released for non MPEG-2 aac/MPEG-4
Audio conforming products. The original developer retains full right to
use the code for the developer's own purpose, assign or donate the code to a
third party and to inhibit third party from using the code for non
MPEG-2 aac/MPEG-4 Audio conforming products. This copyright notice
must be included in all copies or derivative works.
Copyright (c)1997.  

***************************************************************************/
#ifndef NOK_PITCH_H_
#define NOK_PITCH_H_


extern double snr_pred (double *mdct_in, double *mdct_pred, int *sfb_flag,
                        int *sfb_offset, enum WINDOW_TYPE block_type, int side_info,
                        int num_of_sfb);

extern void prediction (short *buffer, double *predicted_samples, double *weight,
                        int delay, int flen);

//extern int estimate_delay (double *sb_samples, short *x_buffer, int flen);
extern int estimate_delay (double *sb_samples, short *x_buffer);

extern void pitch (double *sb_samples, double *sb_samples_pred, short *x_buffer,
                   int *ltp_coef, int delay, int flen);


#endif
