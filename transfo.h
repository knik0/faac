#ifndef TRANSFORM_H
#define TRANSFORM_H 

#define SWAP(a,b) tempr=a;a=b;b=tempr   

void MDCT(double* data, int N);
void FFT(float *data, int nn, int isign);

#endif