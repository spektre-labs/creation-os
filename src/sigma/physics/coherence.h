/* ULTRA-9 — coherence / safety scalars from σ and nominal capacity K. */

#ifndef COS_ULTRA_COHERENCE_H
#define COS_ULTRA_COHERENCE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

float cos_ultra_coherence_k_eff(float sigma, float K);

void cos_ultra_coherence_emit_report(FILE *fp, float sigma_mean_1h,
                                     float dsigma_dt_per_h, float K,
                                     float K_crit);

int cos_ultra_coherence_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
