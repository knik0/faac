#include <math.h>
#include "all.h"
#include "transfo.h"

    float FFTarray [1024];    /* the array for in-place FFT */

#ifndef PI
#define PI 3.141592653589795
#endif

/*****************************
  Fast MDCT Code
*****************************/

void MDCT (double *data, int N) {

	static int init = 1;
    float tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    float freq = 2. * PI / N;
    float fac;
    int i, n;
	int isign = 1;
	int b = N >> 1;
	int N4 = N >> 2;
	int N2 = N >> 1;
    int a = N - b;
	int a2 = a >> 1;
	int a4 = a >> 2;
	int b4 = b >> 2;

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2.; /* 2 from MDCT inverse  to forward */

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);

    for (i = 0; i < N4; i++) {
		/* calculate real and imaginary parts of g(n) or G(p) */
		
		n = N / 2 - 1 - 2 * i;
		if (i < b4) {
			tempr = data [a2 + n] + data [N + a2 - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
		} else {
			tempr = data [a2 + n] - data [a2 - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */
		}
		n = 2 * i;
		if (i < a4) {
			tempi = data [a2 + n] - data [a2 - 1 - n]; /* use first form of e(n) for n=2i */
		} else {
			tempi = data [a2 + n] + data [N + a2 - 1 - n]; /* use second form of e(n) for n=2i*/
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
    FFT (FFTarray, N4, -1);

    /* prepare for recurrence relations in post-twiddle */
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);
    
    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N4; i++) {
		
		/* get post-twiddled FFT output  */
		/* Note: fac allocates 4/N factor from IFFT to forward and inverse */
		tempr = fac * (FFTarray [2 * i] * c + FFTarray [2 * i + 1] * s);
		tempi = fac * (FFTarray [2 * i + 1] * c - FFTarray [2 * i] * s);

		/* fill in output values */
		data [2 * i] = -tempr;   /* first half even */
		data [N2 - 1 - 2 * i] = tempi;  /* first half odd */
		data [N2 + 2 * i] = -tempi;  /* second half even */
		data [N - 1 - 2 * i] = tempr;  /* second half odd */
		
		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }
}

void FFT (float *data, int nn, int isign)  {
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
    static float wtemp, wr, wpr, wpi, wi, theta, wpin;
    static float tempr, tempi, datar, datai;
    static float data1r, data1i;

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
			tempr = (float) wr * (data1r = data [j]);
			tempi = (float) wi * (data1i = data [j + 1]);
			for (i = m; i < n - mmax * 2; i += mmax * 2) {
/* mixed precision not significantly more
 * accurate here; if removing float casts,
 * tempr and tempi should be double */
				tempr -= tempi;
				tempi = (float) wr *data1i + (float) wi *data1r;
/* don't expect compiler to analyze j > i + 1 */
				data1r = data [j + mmax * 2];
				data1i = data [j + mmax * 2 + 1];
				data [i] = (datar = data [i]) + tempr;
				data [i + 1] = (datai = data [i + 1]) + tempi;
				data [j] = datar - tempr;
				data [j + 1] = datai - tempi;
				tempr = (float) wr *data1r;
				tempi = (float) wi *data1i;
				j += mmax * 2;
				}
			tempr -= tempi;
			tempi = (float) wr * data1i + (float) wi *data1r;
			data [i] = (datar = data [i]) + tempr;
			data [i + 1] = (datai = data [i + 1]) + tempi;
			data [j] = datar - tempr;
			data [j + 1] = datai - tempi;
			wr = (wtemp = wr) * wpr - wi * wpi;
			wi = wtemp * wpi + wi * wpr;
			}
		}
}
