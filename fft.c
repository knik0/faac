/*
** FFT and FHT routines
**  Copyright 1988, 1993; Ron Mayer
**  
**  fht(fz,n);
**      Does a hartley transform of "n" points in the array "fz".
**      
** NOTE: This routine uses at least 2 patented algorithms, and may be
**       under the restrictions of a bunch of different organizations.
**       Although I wrote it completely myself; it is kind of a derivative
**       of a routine I once authored and released under the GPL, so it
**       may fall under the free software foundation's restrictions;
**       it was worked on as a Stanford Univ project, so they claim
**       some rights to it; it was further optimized at work here, so
**       I think this company claims parts of it.  The patents are
**       held by R. Bracewell (the FHT algorithm) and O. Buneman (the
**       trig generator), both at Stanford Univ.
**       If it were up to me, I'd say go do whatever you want with it;
**       but it would be polite to give credit to the following people
**       if you use this anywhere:
**           Euler     - probable inventor of the fourier transform.
**           Gauss     - probable inventor of the FFT.
**           Hartley   - probable inventor of the hartley transform.
**           Buneman   - for a really cool trig generator
**           Mayer(me) - for authoring this particular version and
**                       including all the optimizations in one package.
**       Thanks,
**       Ron Mayer; mayer@acuson.com
** and added some optimization by
**           Mather    - idea of using lookup table
**           Takehiro  - some dirty hack for speed up
*/

#include <math.h>

#define SQRT2 sqrt(2)

static float costab[20]=
    {
     .00000000000000000000000000000000000000000000000000,
     .70710678118654752440084436210484903928483593768847,
     .92387953251128675612818318939678828682241662586364,
     .98078528040323044912618223613423903697393373089333,
     .99518472667219688624483695310947992157547486872985,
     .99879545620517239271477160475910069444320361470461,
     .99969881869620422011576564966617219685006108125772,
     .99992470183914454092164649119638322435060646880221,
     .99998117528260114265699043772856771617391725094433,
     .99999529380957617151158012570011989955298763362218,
     .99999882345170190992902571017152601904826792288976,
     .99999970586288221916022821773876567711626389934930,
     .99999992646571785114473148070738785694820115568892,
     .99999998161642929380834691540290971450507605124278,
     .99999999540410731289097193313960614895889430318945,
     .99999999885102682756267330779455410840053741619428
    };

static void fht(float *fz, int n)
{
    int i,k1,k2,k3,k4;
    float *fi, *fn, *gi;
    float *tri;

    fn = fi = fz + n;
    do {
	float f0,f1,f2,f3;
	fi -= 4;
	f1    = fi[0]-fi[1];
	f0    = fi[0]+fi[1];
	f3    = fi[2]-fi[3];
	f2    = fi[2]+fi[3];
	fi[2] = (f0-f2);
	fi[0] = (f0+f2);
	fi[3] = (f1-f3);
	fi[1] = (f1+f3);
    } while (fi != fz);

    tri = &costab[0];
    k1 = 1;
    do {
	float s1, c1;
	int kx;
	k1  *= 4;
	k2  = k1 << 1;
	kx  = k1 >> 1;
	k4  = k2 << 1;
	k3  = k2 + k1;
	fi  = fz;
	gi  = fi + kx;
	do {
	    float f0,f1,f2,f3;
	    f1      = fi[0]  - fi[k1];
	    f0      = fi[0]  + fi[k1];
	    f3      = fi[k2] - fi[k3];
	    f2      = fi[k2] + fi[k3];
	    fi[k2]  = f0     - f2;
	    fi[0 ]  = f0     + f2;
	    fi[k3]  = f1     - f3;
	    fi[k1]  = f1     + f3;
	    f1      = gi[0]  - gi[k1];
	    f0      = gi[0]  + gi[k1];
	    f3      = SQRT2  * gi[k3];
	    f2      = SQRT2  * gi[k2];
	    gi[k2]  = f0     - f2;
	    gi[0 ]  = f0     + f2;
	    gi[k3]  = f1     - f3;
	    gi[k1]  = f1     + f3;
	    gi     += k4;
	    fi     += k4;
	} while (fi<fn);
	c1 = tri[0];
	s1 = tri[1];
	for (i = 1; i < kx; i++) {
	    float c2,s2;
	    c2 = 1.0 - 2.0*s1*s1;
	    s2 = 2.0*s1*c1;
	    fi = fz + i;
	    gi = fz + k1 - i;
	    do {
		float a,b,g0,f0,f1,g1,f2,g2,f3,g3;
		b       = s2*fi[k1] - c2*gi[k1];
		a       = c2*fi[k1] + s2*gi[k1];
		f1      = fi[0 ]    - a;
		f0      = fi[0 ]    + a;
		g1      = gi[0 ]    - b;
		g0      = gi[0 ]    + b;
		b       = s2*fi[k3] - c2*gi[k3];
		a       = c2*fi[k3] + s2*gi[k3];
		f3      = fi[k2]    - a;
		f2      = fi[k2]    + a;
		g3      = gi[k2]    - b;
		g2      = gi[k2]    + b;
		b       = s1*f2     - c1*g3;
		a       = c1*f2     + s1*g3;
		fi[k2]  = f0        - a;
		fi[0 ]  = f0        + a;
		gi[k3]  = g1        - b;
		gi[k1]  = g1        + b;
		b       = c1*g2     - s1*f3;
		a       = s1*g2     + c1*f3;
		gi[k2]  = g0        - a;
		gi[0 ]  = g0        + a;
		fi[k3]  = f1        - b;
		fi[k1]  = f1        + b;
		gi     += k4;
		fi     += k4;
	    } while (fi<fn);
	    c2 = c1;
	    c1 = c2 * tri[0] - s1 * tri[1];
	    s1 = c2 * tri[1] + s1 * tri[0];
        }
	tri += 2;
    } while (k4<n);
}

void fft(float *x_real, float *energy, int N)
{
	float a,b;
	int i,j;

	fht(x_real,N);

	energy[0] = x_real[0] * x_real[0];
	for (i=1,j=N-1;i<N/2;i++,j--) {
		a = x_real[i];
		b = x_real[j];
		energy[i]=(a*a + b*b)/2;
		
		if (energy[i] < 0.0005f) {
			energy[i] = 0.0005f;
		}
	}
	energy[N/2] = x_real[N/2] * x_real[N/2];
}

void fft_side(float x_real0[],float x_real1[], float *x_real, float *energy, int N, int sign)
/*
x_real0 = x_real from channel 0
x_real1 = x_real from channel 1
sign = 1:    compute mid channel energy, ax, bx
sign =-1:    compute side channel energy, ax, bx
*/
{
	float a,b;
	int i,j;

#define XREAL(j) ((x_real0[j]+sign*x_real1[j])/SQRT2)

	energy[0] = XREAL(0) * XREAL(0);
	x_real[0] = XREAL(0);

	for (i=1,j=N-1;i<N/2;i++,j--) {
		a = x_real[i] = XREAL(i);
		b = x_real[j] = XREAL(j);
		energy[i]=(a*a + b*b)/2;
		
		if (energy[i] < 0.0005) {
			energy[i] = 0.0005;
			x_real[i] = x_real[j] = 0.0223606797749978970790696308768019662239; /* was sqrt(0.0005) */
		}
	}
	energy[N/2] = XREAL(N/2) * XREAL(N/2);
}
