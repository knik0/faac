/**********************************************************************
audio sample rate converter

$Id: rateconv.h,v 1.2 2000/02/18 09:17:28 lenox Exp $

Header file: rateconv.h

Authors:
NM    Nikolaus Meine, Uni Hannover (c/o Heiko Purnhagen)
HP    Heiko Purnhagen, Uni Hannover <purnhage@tnt.uni-hannover.de>

Changes:
xx-jun-98   NM      resamp.c
18-sep-98   HP      converted into module
04-nov-99   NM/HP   double ratio
**********************************************************************/

/*
 * Sample-rate converter
 *
 * Realized in three steps:
 *
 * 1. Upsampling by factor two while doing appropriate lowpass-filtering.
 *    This is done by using an FFT-based convolution algorithm with a multi-tap
 *    Kaiser-windowed lowpass filter.
 *    If the cotoff-frequency is less than 0.5, only lowpass-filtering without
 *    the upsampling is done.
 * 2. Upsampling by factor 128 using a 15 tap Kaiser-windowed lowpass filter
 *    (alpha=12.5) and conventional convolution algorithm.
 *    Two values (the next neighbours) are computed for every sample needed.
 * 3. Linear interpolation between the two bounding values.
 *
 * Stereo and mono data is supported.
 * Up- and downsampling of any ratio is possible.
 * Input and output file format is Sun-audio.
 *
 * Written by N.Meine, 1998
 *
 */

/* Multi channel data is interleaved: l0 r0 l1 r1 ... */
/* Total number of samples (over all channels) is used. */


#ifndef _rateconv_h_
#define _rateconv_h_


/* ---------- declarations ---------- */



/* ---------- functions ---------- */

#ifdef __cplusplus
extern "C" {
#endif


/* RateConvInit() */
/* Init audio sample rate conversion. */

RCBuf *RateConvInit (
  int debugLevel,		/* in: debug level */
				/*     0=off  1=basic  2=full */
  double ratio,			/* in: outputRate / inputRate */
  int numChannel,		/* in: number of channels */
  int htaps1,			/* in: num taps */
				/*      -1 = auto */
  float a1,			/* in: alpha for Kaiser window */
				/*      -1 = auto */
  float fc,			/* in: 6dB cutoff freq / input bandwidth */
				/*      -1 = auto */
  float fd,			/* in: 100dB cutoff freq / input bandwidth */
				/*      -1 = auto */
  int *numSampleIn);		/* out: num input samples / frame */
				/* returns: */
				/*  buffer (handle) */
				/*  or NULL if error */


/* RateConv() */
/* Convert sample rate for one frame of audio data. */

long RateConv (
  RCBuf *buf,			/* in: buffer (handle) */
  short *dataIn,		/* in: input data[] */
  long numSampleIn,		/* in: number of input samples */
  float **dataOut);		/* out: output data[] */
				/* returns: */
				/*  numSampleOut */


/* RateConvFree() */
/* Free RateConv buffers. */

void RateConvFree (
  RCBuf *buf);			/* in: buffer (handle) */


#ifdef __cplusplus
}
#endif

#endif	/* #ifndef _rateconv_h_ */

/* end of rateconv.h */
