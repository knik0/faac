#ifndef TRANSFORM_H
#define TRANSFORM_H

// Use this for decoder - single precision
//typedef float fftw_real;

// Use this for encoder - double precision
typedef double fftw_real;

typedef struct {
     fftw_real re, im;
} fftw_complex;

#define c_re(c)  ((c).re)
#define c_im(c)  ((c).im)

#define DEFINE_PFFTW(size)			\
 void pfftwi_##size(fftw_complex *input);	\
 void pfftw_##size(fftw_complex *input);	\
 int  pfftw_permutation_##size(int i);

DEFINE_PFFTW(16)
DEFINE_PFFTW(32)
DEFINE_PFFTW(64)
DEFINE_PFFTW(128)
DEFINE_PFFTW(512)

void make_FFT_order(void);
void make_MDCT_windows(void);
void IMDCT(fftw_real *data, int N);
void MDCT(fftw_real *data, int N);

extern int unscambled64[64];    /* the permutation array for FFT64*/
extern int unscambled512[512];  /* the permutation array for FFT512*/
extern fftw_complex FFTarray[512];    /* the array for in-place FFT */

#endif	  /*	TRANSFORM_H		*/
