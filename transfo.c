#include <math.h>
#include "transfo.h"

fftw_complex_d FFTarray[2048];    /* the array for in-place FFT */
#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923
#endif

/*****************************
  Fast MDCT Code
*****************************/

void MDCT (double *data, int N) {

    double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    double freq = 2.0 * M_PI / N;
    double fac,cosfreq8,sinfreq8;
    int i, n;
    int isign = 1;
    int b = N >> 1;
    int N4 = N >> 2;
    int N2 = N >> 1;
    int a = N - b;
    int a2 = a >> 1;
    int a4 = a >> 2;
    int b4 = b >> 2;
    int unscambled;
    
    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2.; /* 2 from MDCT inverse  to forward */

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    cosfreq8 = cos (freq * 0.125);
    sinfreq8 = sin (freq * 0.125);
    c = cosfreq8;
    s = sinfreq8;

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
		FFTarray [i].re = tempr * c + tempi * s;
		FFTarray [i].im = tempi * c - tempr * s;

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }

    /* Perform in-place complex FFT of length N/4 */
    switch (N) {
	case 256: pfftw_d_64(FFTarray);
		break;
	case 2048:pfftw_d_512(FFTarray);
	}

    /* prepare for recurrence relations in post-twiddle */
    c = cosfreq8;
    s = sinfreq8;

    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N4; i++) {

		/* get post-twiddled FFT output  */
		/* Note: fac allocates 4/N factor from IFFT to forward and inverse */
	switch (N) {
	case 256:
		unscambled = unscambled64[i];
		break;
	case 2048:
		unscambled = unscambled512[i];
	}
	tempr = fac * (FFTarray [unscambled].re * c + FFTarray [unscambled].im * s);
	tempi = fac * (FFTarray [unscambled].im * c - FFTarray [unscambled].re * s);


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

