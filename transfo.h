#ifndef TRANSFORM_H
#define TRANSFORM_H 

/* Define this to use original FFT*/
//#define USE_ORIG_FFT

void MDCT(double* data, int N);
void FFT(double *data, int nn, int isign);

#ifndef USE_ORIG_FFT
#define c_re(c)  ((c).re)
#define c_im(c)  ((c).im)

typedef struct {
     double re, im;
} fftw_complex_d;

#define DEFINE_PFFTW(size)			\
 void pfftw_d_##size(fftw_complex_d *input);	\
 void pfftwi_d_##size(fftw_complex_d *input);	\
 int pfftw_d_permutation_##size(int i);		

DEFINE_PFFTW(16)
DEFINE_PFFTW(32)
DEFINE_PFFTW(64)
DEFINE_PFFTW(128)
DEFINE_PFFTW(512)

void MakeFFTOrder(void);

#endif

#endif