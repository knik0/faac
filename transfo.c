#include <math.h>
#include "transfo.h"

fftw_complex_d FFTarray[2048];    /* the array for in-place FFT */
extern int unscambled64[64];    /* the permutation array for FFT64*/
extern int unscambled512[512];  /* the permutation array for FFT512*/

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


/*****************************
  Fast MDCT Code
*****************************/

#define SWAP(a,b) tempr=a;a=b;b=tempr
void CompFFT (double *data, int nn, int isign);
void FFT (double *data, int nn, int isign);

double *NewFloat (int N) {
/* Allocate array of N Floats */

    double *temp;

    temp = (double *) malloc (N * sizeof (double));
    if (!temp) {
		exit (1);
		}
    return temp;
}

typedef double *pFloat;

double **NewFloatMatrix (int N, int M) {
/* Allocate NxM matrix of Floats */

    double **temp;
    int i;

/* allocate N pointers to double arrays */
    temp = (pFloat *) malloc (N * sizeof (pFloat));
    if (!temp) {
		exit (1);
		}

/* Allocate a double array M long for each of the N array pointers */

    for (i = 0; i < N; i++) {
		temp [i] = (double *) malloc (M * sizeof (double));
		if (! temp [i]) {
			exit (1);
			}
		}
    return temp;
}


void IMDCT (double *data, int N) {

    static double *FFTarray = 0;    /* the array for in-place FFT */
    static double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    double freq = 2. * M_PI / N;
    static double fac;
    static int i, n;
	int isign = -1;
	int b = N/2;
    int a = N - b;

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    if (isign == 1) {
      fac = 2.; /* 2 from MDCT inverse  to forward */
    } 
    else {
      fac = 2. / N; /* remaining 2/N from 4/N IFFT factor */
    }
    
    
    {
      static int oldN = 0;
      if (N > oldN) {
	if (FFTarray) {
	  free (FFTarray);
	  FFTarray = 0;
	}
	oldN = N;
      }
      
      if (! FFTarray) 
	FFTarray = NewFloat (N / 2);  /* holds N/4 complex values */
    }
    
    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);

    for (i = 0; i < N / 4; i++) {
      
      /* calculate real and imaginary parts of g(n) or G(p) */
      
      if (isign == 1) { /* Forward Transform */
	n = N / 2 - 1 - 2 * i;
	if (i < b / 4) {
	  tempr = data [a / 2 + n] + data [N + a / 2 - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
	} 
	else {
	  tempr = data [a / 2 + n] - data [a / 2 - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */
	}
	n = 2 * i;
	if (i < a / 4) {
	  tempi = data [a / 2 + n] - data [a / 2 - 1 - n]; /* use first form of e(n) for n=2i */
	} 
	else {
	  tempi = data [a / 2 + n] + data [N + a / 2 - 1 - n]; /* use second form of e(n) for n=2i*/
	}
      } 
      else { /* Inverse Transform */
	tempr = -data [2 * i];
	tempi = data [N / 2 - 1 - 2 * i];
      }
      
      /* calculate pre-twiddled FFT input */
      FFTarray [2 * i] = tempr * c + isign * tempi * s;
      FFTarray [2 * i + 1] = tempi * c - isign * tempr * s;
      
      /* use recurrence to prepare cosine and sine for next value of i */
      cold = c;
      c = c * cfreq - s * sfreq;
      s = s * cfreq + cold * sfreq;
    }
    
    /* Perform in-place complex FFT (or IFFT) of length N/4 */
    /* Note: FFT has physics (opposite) sign convention and doesn't do 1/N factor */
    CompFFT (FFTarray, N / 4, -isign);
    
    /* prepare for recurrence relations in post-twiddle */
    c = cos (freq * 0.125);
    s = sin (freq * 0.125);
    
    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N / 4; i++) {
      
      /* get post-twiddled FFT output  */
      /* Note: fac allocates 4/N factor from IFFT to forward and inverse */
      tempr = fac * (FFTarray [2 * i] * c + isign * FFTarray [2 * i + 1] * s);
      tempi = fac * (FFTarray [2 * i + 1] * c - isign * FFTarray [2 * i] * s);
      
      /* fill in output values */
      if (isign == 1) { /* Forward Transform */
	data [2 * i] = -tempr;   /* first half even */
	data [N / 2 - 1 - 2 * i] = tempi;  /* first half odd */
	data [N / 2 + 2 * i] = -tempi;  /* second half even */
	data [N - 1 - 2 * i] = tempr;  /* second half odd */
      } 
      else { /* Inverse Transform */
	data [N / 2 + a / 2 - 1 - 2 * i] = tempr;
	if (i < b / 4) {
	  data [N / 2 + a / 2 + 2 * i] = tempr;
	} 
	else {
	  data [2 * i - b / 2] = -tempr;
	}
	data [a / 2 + 2 * i] = tempi;
	if (i < a / 4) {
	  data [a / 2 - 1 - 2 * i] = -tempi;
	} 
	else {
	  data [a / 2 + N - 1 - 2*i] = tempi;
	}
      }
      
      /* use recurrence to prepare cosine and sine for next value of i */
      cold = c;
      c = c * cfreq - s * sfreq;
      s = s * cfreq + cold * sfreq;
    }
    
    /*    DeleteFloat (FFTarray);   */
    
}


void CompFFT (double *data, int nn, int isign) {

    static int i, j, k, kk;
    static int p1, q1;
    static int m, n, logq1;
    static double **intermed = 0;
    static double ar, ai;
    static double d2pn;
    static double ca, sa, curcos, cursin, oldcos, oldsin;
    static double ca1, sa1, curcos1, cursin1, oldcos1, oldsin1;


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

    d2pn = 2*M_PI / nn;

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


void FFT (double *data, int nn, int isign)  {
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
    static double wtemp, wr, wpr, wpi, wi, theta, wpin;
    static double tempr, tempi, datar, datai;
    static double data1r, data1i;

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
			tempr = (double) wr * (data1r = data [j]);
			tempi = (double) wi * (data1i = data [j + 1]);
			for (i = m; i < n - mmax * 2; i += mmax * 2) {
/* mixed precision not significantly more
 * accurate here; if removing double casts,
 * tempr and tempi should be double */
				tempr -= tempi;
				tempi = (double) wr *data1i + (double) wi *data1r;
/* don't expect compiler to analyze j > i + 1 */
				data1r = data [j + mmax * 2];
				data1i = data [j + mmax * 2 + 1];
				data [i] = (datar = data [i]) + tempr;
				data [i + 1] = (datai = data [i + 1]) + tempi;
				data [j] = datar - tempr;
				data [j + 1] = datai - tempi;
				tempr = (double) wr *data1r;
				tempi = (double) wi *data1i;
				j += mmax * 2;
				}
			tempr -= tempi;
			tempi = (double) wr * data1i + (double) wi *data1r;
			data [i] = (datar = data [i]) + tempr;
			data [i + 1] = (datai = data [i + 1]) + tempi;
			data [j] = datar - tempr;
			data [j + 1] = datai - tempi;
			wr = (wtemp = wr) * wpr - wi * wpi;
			wi = wtemp * wpi + wi * wpr;
			}
		}
}

