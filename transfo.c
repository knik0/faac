
#define d2PI  6.283185307179586

#define SWAP(a,b) tempr=a;a=b;b=tempr   

#include "all.h"
#include "transfo.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>
#ifndef PI
#define PI 3.141592653589795
#endif


/*****************************
  Fast MDCT Code
*****************************/

void MDCT (double *data, int N) {

    static Float *FFTarray = 0;    /* the array for in-place FFT */
	static int init = 1;
    Float tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    Float freq = 2. * PI / N;
    Float fac;
    int i, n;
	int isign = 1;
	int b = N >> 1;
    int a = N - b;

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2.; /* 2 from MDCT inverse  to forward */

	if (init) {
		init = 0;
		FFTarray = NewFloat (1024);
	}

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);

    for (i = 0; i < N / 4; i++) {
		/* calculate real and imaginary parts of g(n) or G(p) */
		
		n = N / 2 - 1 - 2 * i;
		if (i < b / 4) {
			tempr = data [a / 2 + n] + data [N + a / 2 - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
		} else {
			tempr = data [a / 2 + n] - data [a / 2 - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */
		}
		n = 2 * i;
		if (i < a / 4) {
			tempi = data [a / 2 + n] - data [a / 2 - 1 - n]; /* use first form of e(n) for n=2i */
		} else {
			tempi = data [a / 2 + n] + data [N + a / 2 - 1 - n]; /* use second form of e(n) for n=2i*/
		}

		/* calculate pre-twiddled FFT input */
		FFTarray [2 * i] = tempr * c + tempi * s;
		FFTarray [2 * i + 1] = tempi * c - tempr * s;
		
		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }
    
    /* Perform in-place complex FFT of length N/4 */
    CompFFT (FFTarray, N / 4, -1);

    /* prepare for recurrence relations in post-twiddle */
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);
    
    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N / 4; i++) {
		
		/* get post-twiddled FFT output  */
		/* Note: fac allocates 4/N factor from IFFT to forward and inverse */
		tempr = fac * (FFTarray [2 * i] * c + FFTarray [2 * i + 1] * s);
		tempi = fac * (FFTarray [2 * i + 1] * c - FFTarray [2 * i] * s);
		
		/* fill in output values */
		data [2 * i] = -tempr;   /* first half even */
		data [N / 2 - 1 - 2 * i] = tempi;  /* first half odd */
		data [N / 2 + 2 * i] = -tempi;  /* second half even */
		data [N - 1 - 2 * i] = tempr;  /* second half odd */
		
		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }
}


void CompFFT (Float *data, int nn, int isign) {

    static int i, j, k, kk;
    static int p1, q1;
    static int m, n, logq1;
    static Float **intermed = 0;
    static Float ar, ai;
    static Float d2pn;
    static Float ca, sa, curcos, cursin, oldcos, oldsin;
    static Float ca1, sa1, curcos1, cursin1, oldcos1, oldsin1;


/* Factorize n;  n = p1*q1 where q1 is a power of 2.
    For n = 1152, p1 = 9, q1 = 128.		*/

    n = nn;
    logq1 = 0;

    for (;;) {
		m = n >> 1;  /* shift right by one*/
		if ((m << 1) == n) {
		    logq1++;
		    n = m;
			} 
		else {
			break;
			}
		}

    p1 = n;
    q1 = 1;
    q1 <<= logq1;

    d2pn = d2PI / nn;

{static int oldp1 = 0, oldq1 = 0;

	if ((oldp1 < p1) || (oldq1 < q1)) {
		if (intermed) {
			free (intermed);
			intermed = 0;
			}
		if (oldp1 < p1) oldp1 = p1;
		if (oldq1 < q1) oldq1 = q1;;
		}
		
	if (!intermed)
		intermed = NewFloatMatrix (oldp1, 2 * oldq1);
}

/* Sort the p1 sequences */

    for	(i = 0; i < p1; i++) {
		for (j = 0; j < q1; j++){
		    intermed [i] [2 * j] = data [2 * (p1 * j + i)];
			intermed [i] [2 * j + 1] = data [2 * (p1 * j + i) + 1];
			}
		}

/* compute the power of two fft of the p1 sequences of length q1 */

    for (i = 0; i < p1; i++) {
/* Forward FFT in place for n complex items */
		FFT (intermed [i], q1, isign);
		}

/* combine the FFT results into one seqquence of length N */

    ca1 = cos (d2pn);
    sa1 = sin (d2pn);
    curcos1 = 1.;
    cursin1 = 0.;

    for (k = 0; k < nn; k++) {
		data [2 * k] = 0.;
		data [2 * k + 1] = 0.;
		kk = k % q1;

		ca = curcos1;
		sa = cursin1;
		curcos = 1.;
		cursin = 0.;

		for (j = 0; j < p1; j++) {
			ar = curcos;
			ai = isign * cursin;
			data [2 * k] += intermed [j] [2 * kk] * ar - intermed [j] [2 * kk + 1] * ai;
			data [2 * k + 1] += intermed [j] [2 * kk] * ai + intermed [j] [2 * kk + 1] * ar;

		    oldcos = curcos;
		    oldsin = cursin;
			curcos = oldcos * ca - oldsin * sa;
			cursin = oldcos * sa + oldsin * ca;
			}
		oldcos1 = curcos1;
		oldsin1 = cursin1;
		curcos1 = oldcos1 * ca1 - oldsin1 * sa1;
		cursin1 = oldcos1 * sa1 + oldsin1 * ca1;
		}

}


void FFT (Float *data, int nn, int isign)  {
/* Varient of Numerical Recipes code from off the internet.  It takes nn
interleaved complex input data samples in the array data and returns nn interleaved
complex data samples in place where the output is the FFT of input if isign==1 and it
is nn times the IFFT of the input if isign==-1. 
(Note: it doesn't renormalize by 1/N when doing the inverse transform!!!)
(Note: this follows physicists convention of +i, not EE of -j in forward
transform!!!!)
 */

/* Press, Flannery, Teukolsky, Vettering "Numerical
 * Recipes in C" tuned up ; Code works only when nn is
 * a power of 2  */

    static int n, mmax, m, j, i;
    static Float wtemp, wr, wpr, wpi, wi, theta, wpin;
    static Float tempr, tempi, datar, datai;
    static Float data1r, data1i;

    n = nn * 2;

/* bit reversal */

    j = 0;
    for (i = 0; i < n; i += 2) {
		if (j > i) {  /* could use j>i+1 to help compiler analysis */
			SWAP (data [j], data [i]);
			SWAP (data [j + 1], data [i + 1]);
			}
		m = nn;
		while (m >= 2 && j >= m) {
			j -= m;
			m >>= 1;
			}
		j += m;
		}

    theta = 3.141592653589795 * .5;
    if (isign < 0)
		theta = -theta;
    wpin = 0;   /* sin(+-PI) */
    for (mmax = 2; n > mmax; mmax *= 2) {
		wpi = wpin;
		wpin = sin (theta);
		wpr = 1 - wpin * wpin - wpin * wpin; /* cos(theta*2) */
		theta *= .5;
		wr = 1;
		wi = 0;
		for (m = 0; m < mmax; m += 2) {
			j = m + mmax;
			tempr = (Float) wr * (data1r = data [j]);
			tempi = (Float) wi * (data1i = data [j + 1]);
			for (i = m; i < n - mmax * 2; i += mmax * 2) {
/* mixed precision not significantly more
 * accurate here; if removing float casts,
 * tempr and tempi should be double */
				tempr -= tempi;
				tempi = (Float) wr *data1i + (Float) wi *data1r;
/* don't expect compiler to analyze j > i + 1 */
				data1r = data [j + mmax * 2];
				data1i = data [j + mmax * 2 + 1];
				data [i] = (datar = data [i]) + tempr;
				data [i + 1] = (datai = data [i + 1]) + tempi;
				data [j] = datar - tempr;
				data [j + 1] = datai - tempi;
				tempr = (Float) wr *data1r;
				tempi = (Float) wi *data1i;
				j += mmax * 2;
				}
			tempr -= tempi;
			tempi = (Float) wr * data1i + (Float) wi *data1r;
			data [i] = (datar = data [i]) + tempr;
			data [i + 1] = (datai = data [i + 1]) + tempi;
			data [j] = datar - tempr;
			data [j + 1] = datai - tempi;
			wr = (wtemp = wr) * wpr - wi * wpi;
			wi = wtemp * wpi + wi * wpr;
			}
		}
}

