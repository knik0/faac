
/* rdft_spectrum.c by James Salsman 25 April 1999 for public domain */

/* spectrum( double *f, unsigned lg2n ):
   input f[0...2^lg2n-1] amplitudes, modified in-place;
   output f[0...2^(lg2n-1)-1] magnitudes (scaling depends on lg2n;
                                          f[0] ill-defined);
   complspectrum( double *f, unsigned lg2n ): same as above but
   f[2^(lg2n-1)+1...2^lg2n-1] are also phases in radians 
                                         (f[2^(lg2n-1)] meaningless.) */

/* computes a power spectrum using a real discrete Fourier transform,
   faster than a full FFT but not exactly a Hartley transform.  */

/* adapted from Joerg Arndt's code found around 
     http://www.spektracom.de/~arndt/joerg.html 
   in turn from Ron Mayer's 1988 Stanford project, and Ron credits 
   Bracewell, Hartley, Gauss, and Euler.  Don't forget Pappus, 
   Pythagoras, and Euclid! */

#include <math.h>  /* we need sqrt(), sin(), cos(), and atan2(),
   and we want the sincos(x,*cos) FP primitive, but if we can't 
   have that, we should use floor() and fmod(,) instead of cos() */

#include "rdft.h"
#include <rfftw.h>


void fftw_init(){
rdft_plan11=rfftw_create_plan(11,FFTW_REAL_TO_COMPLEX,FFTW_MEASURE);
rdft_plan8=rfftw_create_plan(8,FFTW_REAL_TO_COMPLEX,FFTW_MEASURE);
}

void fftw_destroy(){
rfftw_destroy_plan(rdft_plan11);
rfftw_destroy_plan(rdft_plan8);
}

void rdft( double *fr, unsigned lg2n ) 
/* real discrete Fourier transform; not recursive */
{
rfftw_plan rdft_plan;
double fo[lg2n];
switch(lg2n) {
    case 11: rfftw_one(rdft_plan11,fr,fo);
	break;
    case 8: rfftw_one(rdft_plan8,fr,fo);
	break;
    default: rdft_plan=rfftw_create_plan(lg2n,FFTW_REAL_TO_COMPLEX, 
		    FFTW_ESTIMATE);
	rfftw_one(rdft_plan,fr,fo);
	rfftw_destroy_plan(rdft_plan);
	printf("ERROR: rdft with size %i",lg2n);
}
	memcpy(fr,fo,sizeof(fr));
}

void spectrum( double *f, unsigned lg2n )
/* magnitude power spectrum from rdft */
{
    const int n=(1<<lg2n), k=(n>>1); /* k=n/2 */
    int i, j;

    rdft(f, lg2n);
    
    for ( i=1,j=n-1; i<k; i++,j-- )
    {
        double a, b;

        a = f[i] + f[j];  /* real part */
        b = f[i] - f[j];  /* imag part */

        f[i] = sqrt(a*a + b*b); /* magnitude -- please note that the
                                         scaling depends on size n */
    }

    f[0]=sqrt(f[0]*f[0]+f[n/2]*f[n/2]);  /* ill-defined bin */
        /* Reciprocated division by zero precludes any meaning 
           of "0 Hz" so please avoid using the f[0] output! */
}

void complspectrum( double *f, unsigned lg2n )
/* polar complex power spectrum from rdft */
{
    const int n=(1<<lg2n), k=(n>>1); /* k=n/2 */
    int i, j;

    rdft(f, lg2n);
    
    for ( i=1,j=n-1; i<k; i++,j-- )
    {
        double a, b;

        a = f[i] + f[j];  /* real part */
        b = f[i] - f[j];  /* imag part */ 

        f[i] = sqrt(a*a + b*b); /* magnitude -- please note that the
                                         scaling depends on size n */
        /* complex part: phase */

		if (a != 0.0) f[j] = atan2(b, a); /* magnitude f[i] has phase f[n-i] */
                   else f[j] = 0.0;
    }

    f[0]=sqrt(f[0]*f[0]+f[n/2]*f[n/2]);  /* ill-defined bin */

    /* The corresponding phase, f[k], should really be set to 
       some kind of not-a-number value because it is completely 
       meaningless, instead of just tainted like f[0]. */
}

/* end of rdft_spectrum.c :jps 26 April 1999 */#include <rfftw.h>
