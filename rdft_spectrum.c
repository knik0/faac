
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

/* these are the two primordial constants of DSP */
#define SQRT2   1.414213562373095048801688724209698078569
#define PI      3.1415926535897932384626433832795

/* these six macros save source space and execution time */
#define SWAP(x, y)  { double tmp=x; x=y; y=tmp; }
#define SUMDIFF2(s, d)  { double tmp=s-d; s+=d; d=tmp; }
#define SUMDIFF4(a, b, s, d)  { s=a+b; d=a-b; }
#define CSQR4(a, b, u, v)  { u=a*a-b*b; v=a*b; v+=v; }
#define CMULT6(c, s, c1, s1, u, v)  { u=c1*c-s1*s; v=c1*s+s1*c; }

void rdft( double *fr, unsigned lg2n ) 
/* real discrete Fourier transform; not recursive */
{
    double *fi, *fn, *gi, tt;
    unsigned n=(1 << lg2n), m, j, p, k, k4;

    if (lg2n <= 1) /* degenerate cases */
    {
        if (lg2n == 1) SUMDIFF2(fr[0], fr[1]); /* two point rdft */

        return;
    }

    for ( m=1,j=0; m<n-1; m++ )
    {   
        for ( p=n>>1; (!((j^=p)&p)); p>>=1 );  /* butterfly */
        if (j>m) SWAP(fr[m], fr[j]);           /* shuffle */
    }

    k = lg2n & 1; /* is the size a power of 4? */

    if (k==0) /* yes a power of 4 */
    {
        for ( fi=fr,fn=fr+n; fi<fn; fi+=4 )
        {
            double f0, f1, f2, f3;

            SUMDIFF4(fi[0], fi[1], f0, f1);
            SUMDIFF4(fi[2], fi[3], f2, f3);

            SUMDIFF4(f0, f2, fi[0], fi[2]);
            SUMDIFF4(f1, f3, fi[1], fi[3]);
        }
    }
    else /* lg2n & 1 == 1, so n is not a power of 4 */
    {
        for ( fi=fr,fn=fr+n,gi=fi+1; fi<fn; fi+=8,gi+=8 )
        {
            double s1, c1, s2, c2, g0, f0, f1, g1;

            SUMDIFF4(fi[0], gi[0], s1, c1);
            SUMDIFF4(fi[2], gi[2], s2, c2);

            SUMDIFF4(s1, s2, f0, f1);
            SUMDIFF4(c1, c2, g0, g1);

            SUMDIFF4(fi[4], gi[4], s1, c1);
            SUMDIFF4(fi[6], gi[6], s2, c2);

            SUMDIFF2(s1, s2);

            c1 *= SQRT2;
            c2 *= SQRT2;

            SUMDIFF4(f0, s1, fi[0], fi[4]);
            SUMDIFF4(f1, s2, fi[2], fi[6]);
                
            SUMDIFF4(g0, c1, gi[0], gi[4]);
            SUMDIFF4(g1, c2, gi[2], gi[6]);
        }
    }

    if (n<16) return; /* base work was as much as 8-point */

    do
    {
        unsigned k1, k2, k3, kx, i;

        k += 2;
        k1 = 1  << k;
        k2 = k1 << 1;
        k4 = k2 << 1;
        k3 = k2 + k1;
        kx = k1 >> 1;
        fi = fr;
        gi = fi + kx;
        fn = fr + n;

        do
        {
            double f0, f1, f2, f3;

            SUMDIFF4(fi[0], fi[k1], f0, f1);
            SUMDIFF4(fi[k2], fi[k3], f2, f3);

            SUMDIFF4(f0, f2, fi[0], fi[k2]);
            SUMDIFF4(f1, f3, fi[k1], fi[k3]);

            SUMDIFF4(gi[0], gi[k1], f0, f1);

            f3 = SQRT2 * gi[k3];
            f2 = SQRT2 * gi[k2];

            SUMDIFF4(f0, f2, gi[0], gi[k2]);
            SUMDIFF4(f1, f3, gi[k1], gi[k3]);

            gi += k4;
            fi += k4;
        }
        while (fi<fn);

        tt = PI/4/kx;

        for ( i=1; i<kx; i++ )
        {
            double tti, cs1, sn1, cs2, sn2;

            tti = tt*i;
            sn1 = sin(tti); /* ideally, we should be using a sincos() primitive; */
            cs1 = cos(tti); /* but this can be faster by deriving cos from sin
                                   cos(tti) := +/- sqrt(1-sin(tti)^2) 
                                   the sign needs to use floor() and fmod(,):
                                   2.0*(floor(fmod(tti-PI/2, PI))-0.5) ??? */

            CSQR4(cs1, sn1, cs2, sn2);

            fn = fr + n;
            fi = fr + i;
            gi = fr + k1 - i;

            do
            {
                double a, b, g0, f0, f1, g1, f2, g2, f3, g3;

                CMULT6(sn2, cs2, fi[k1], gi[k1], b, a);
                SUMDIFF4(fi[0], a, f0, f1);
                SUMDIFF4(gi[0], b, g0, g1);

                CMULT6(sn2, cs2, fi[k3], gi[k3], b, a);
                SUMDIFF4(fi[k2], a, f2, f3);
                SUMDIFF4(gi[k2], b, g2, g3);

                CMULT6(sn1, cs1, f2, g3, b, a);
                SUMDIFF4(f0, a, fi[0], fi[k2]);
                SUMDIFF4(g1, b, gi[k1], gi[k3]);

                CMULT6(cs1, sn1, g2, f3, b, a);
                SUMDIFF4(g0, a, gi[0], gi[k2]);
                SUMDIFF4(f1, b, fi[k1], fi[k3]);

                gi += k4;
                fi += k4;
            }
            while (fi<fn);
        }
    }
    while (k4<n);
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
		f[j] = atan2(b, a); /* magnitude f[i] has phase f[n-i] */
    }

    f[0]=sqrt(f[0]*f[0]+f[n/2]*f[n/2]);  /* ill-defined bin */

    /* The corresponding phase, f[k], should really be set to 
       some kind of not-a-number value because it is completely 
       meaningless, instead of just tainted like f[0]. */
}

/* end of rdft_spectrum.c :jps 26 April 1999 */