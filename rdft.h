#ifdef USE_FFTW
#include <rfftw.h>
rfftw_plan rdft_plan11;
rfftw_plan rdft_plan8;
#endif

void fftw_init();
void fftw_destroy();
