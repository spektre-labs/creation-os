/* ULTRA-9 — K_eff = (1 - σ) * K ; trend from dσ/dt (sign-only guard). */

#include "coherence.h"

#include <math.h>
#include <stdio.h>

float cos_ultra_coherence_k_eff(float sigma, float K) {
    float s = sigma;
    if (s != s) s = 1.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (K < 0.0f) K = 0.0f;
    return (1.0f - s) * K;
}

void cos_ultra_coherence_emit_report(FILE *fp, float sigma_mean_1h,
                                     float dsigma_dt_per_h, float K,
                                     float K_crit) {
    if (!fp) return;
    float ke = cos_ultra_coherence_k_eff(sigma_mean_1h, K);
    const char *st = "STABLE";
    if (ke < K_crit) st = "AT_RISK";
    if (dsigma_dt_per_h > 0.001f) st = "DRIFTING";
    fprintf(fp, "ULTRA-9 coherence conservation (Lagrangian-style scalars)\n");
    fprintf(fp, "  sigma_mean(1h):  %.3f  d_sigma/dt=%+.4f /h\n",
            (double)sigma_mean_1h, (double)dsigma_dt_per_h);
    fprintf(fp, "  K_eff: %.3f | K_crit: %.3f | Status: %s\n",
            (double)ke, (double)K_crit, st);
}

int cos_ultra_coherence_self_test(void) {
    float k = cos_ultra_coherence_k_eff(0.5f, 2.0f);
    if (fabsf(k - 1.0f) > 1e-4f) return 1;
    return 0;
}
