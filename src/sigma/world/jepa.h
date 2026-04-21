/* ULTRA-2 — JEPA-style latent dynamics; σ_world = prediction error norm. */

#ifndef COS_ULTRA_JEPA_H
#define COS_ULTRA_JEPA_H

#ifdef __cplusplus
extern "C" {
#endif

/* L2 distance ||z_hat - z_next|| / (||z_next|| + eps) in [0, ~sqrt(dim)]. */
float cos_ultra_jepa_sigma_world(const float *z_hat, const float *z_next,
                                 int dim, float eps);

int cos_ultra_jepa_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
