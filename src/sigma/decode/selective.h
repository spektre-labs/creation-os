/* ULTRA-3 — event-driven decode: recompute only when σ moves enough. */

#ifndef COS_ULTRA_SELECTIVE_H
#define COS_ULTRA_SELECTIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if downstream should run a full decode step. */
int cos_ultra_selective_should_decode(float sigma_t, float sigma_prev,
                                      float epsilon);

int cos_ultra_selective_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
