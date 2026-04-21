/* ULTRA-1 — σ as the MoE router: routing uncertainty drives adaptive top-k. */

#ifndef COS_ULTRA_SIGMA_ROUTER_H
#define COS_ULTRA_SIGMA_ROUTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable softmax over logits; writes n probabilities into `probs`. */
void cos_ultra_sigma_router_softmax(const float *logits, int n, float *probs);

/* σ_routing = 1 - max(probs) — high when the gate is flat / unsure. */
float cos_ultra_sigma_routing_uncertainty(const float *probs, int n);

/* Adaptive expert count from routing uncertainty and threshold τ.
 * When uncertainty is low relative to τ, prefer k=1; ramps to 8. */
int cos_ultra_sigma_router_adaptive_k(float sigma_routing, float tau,
                                      int n_experts);

int cos_ultra_sigma_router_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
