#ifndef TRANSFORM_H
#define TRANSFORM_H 

void MDCT(double* data, int N);
void IMDCT(double* data, int N);
void FFT(double *data, int nn, int isign);

#define c_re(c)  ((c).re)
#define c_im(c)  ((c).im)

typedef struct {
     double re, im;
} fftw_complex_d;

#define DEFINE_PFFTW(size)			\
 void pfftw_d_##size(fftw_complex_d *input);	\
 int pfftw_d_permutation_##size(int i);		

DEFINE_PFFTW(16)
DEFINE_PFFTW(32)
DEFINE_PFFTW(64)
DEFINE_PFFTW(128)
DEFINE_PFFTW(256)
DEFINE_PFFTW(512)
DEFINE_PFFTW(2048)

void MakeFFTOrder(void);
void MakeFFT2Order(void);
extern int unscambled64[64];    /* the permutation array for FFT64*/
extern int unscambled256[256];    /* the permutation array for FFT256*/
extern int unscambled512[512];  /* the permutation array for FFT512*/
extern int unscambled2048[2048];    /* the permutation array for FFT2048*/
extern fftw_complex_d FFTarray[2048];    /* the array for in-place FFT */

#endif
