/* Test-only hooks into SBR internals.
 * Not installed; consumed by tests/qmf_test.c. */
#ifndef SBR_INTERNAL_H
#define SBR_INTERNAL_H

#include "sbr.h"

void qmf_analysis_slot_complex(const SBRInfo *sbr,
                               const faac_real *slot,
                               faac_real *ovl,
                               faac_real *W_re,
                               faac_real *W_im);

#endif
