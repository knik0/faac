#define fftw_real double
#define fftw_complex fftw_complex_d

#define CONCAT_AUX(a, b) a ## b
#define CONCAT(a, b) CONCAT_AUX(a,b)

#define PFFTW(name)  CONCAT(pfftw_d_, name)
#define PFFTWI(name)  CONCAT(pfftwi_d_, name)

#define FFTW_KONST(x) ((fftw_real) x)

void PFFTW(twiddle_2)(fftw_complex *A, const fftw_complex *W, int iostride);
void PFFTW(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);
void PFFTWI(twiddle_2)(fftw_complex *A, const fftw_complex *W, int iostride);
void PFFTWI(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);
