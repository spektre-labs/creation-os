/* ULTRA-4 — σ routes fast System-1 vs slow System-2 symbolic check. */

#ifndef COS_ULTRA_NEURO_SYM_H
#define COS_ULTRA_NEURO_SYM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_system1;
    float tau_symbolic;
    int   use_system2; /* 1 when symbolic path should engage */
} cos_ultra_neuro_sym_t;

void cos_ultra_neuro_sym_route(cos_ultra_neuro_sym_t *out,
                               float sigma_system1, float tau_symbolic);

int cos_ultra_neuro_sym_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
