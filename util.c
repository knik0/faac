
#include "all.h"
#include "util.h"
#include <stdlib.h>


#ifndef MAXINT
#undef MAXINT
#define MAXINT	32767.
#endif

Float *NewFloat (int N) {
/* Allocate array of N Floats */

    Float *temp;

    temp = (Float *) malloc( N * sizeof (Float));
    return temp;
}

typedef Float *pFloat;

Float **NewFloatMatrix (int N, int M) {
/* Allocate NxM matrix of Floats */

    Float **temp;
    int i;

/* allocate N pointers to Float arrays */
    temp = (pFloat *) malloc( N * sizeof (pFloat));
    if (!temp) {
		return temp;
		}

/* Allocate a Float array M long for each of the N array pointers */

    for (i = 0; i < N; i++) {
		temp [i] = (Float *) malloc( M * sizeof (Float));
		if (! temp [i]) {
			return temp;
			}
		}
    return temp;
}

