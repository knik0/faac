/**********************************************************************
Header file: enc.h

$Id: enc.h,v 1.5 2000/02/18 09:21:05 lenox Exp $

Authors:
HP    Heiko Purnhagen, Uni Hannover <purnhage@tnt.uni-hannover.de>
RG    Ralf Geiger, FhG/IIS

Changes:
14-jun-96   HP    first version
18-jun-96   HP    added bit reservoir handling
04-jul-96   HP    joined with t/f code by BG (check "DISABLE_TF")
09-aug-96   HP    added EncXxxInfo(), EncXxxFree()
15-aug-96   HP    changed EncXxxInit(), EncXxxFrame() interfaces to
                  enable multichannel signals / float fSample, bitRate
26-aug-96   HP    CVS
19-feb-97   HP    added include <stdio.h>
07-apr-98   RG    added argument lfePresent in EncTfFrame()
**********************************************************************/


#ifndef _enc_h_
#define _enc_h_

#include <stdio.h>              /* typedef FILE */

#include "bitstream.h"		/* bit stream module */

/* ---------- functions ---------- */

#ifdef __cplusplus
extern "C" {
#endif


/* EncTfInit() */
/* Init t/f-based encoder core. */

void EncTfInit (faacAACStream *as);



/* EncTfFrame() */
/* Encode one audio frame into one bit stream frame with */
/* t/f-based encoder core. */

int EncTfFrame (faacAACStream *as, BsBitStream *bitBuf);


/* EncTfFree() */
/* Free memory allocated by t/f-based encoder core. */

void EncTfFree (void);


#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _enc_h_ */

/* end of enc.h */

