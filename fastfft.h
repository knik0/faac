#include "transfo.h"

#define PFFTW(name)  CONCAT(pfftw_, name)
#define PFFTWI(name)  CONCAT(pfftwi_, name)
#define CONCAT_AUX(a, b) a ## b
#define CONCAT(a, b) CONCAT_AUX(a,b)
#define FFTW_KONST(x) ((fftw_real) x)

void PFFTW(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);
void PFFTWI(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);

