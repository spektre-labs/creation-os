/* ULTRA-11 — recurrent depth: repeated transform with σ halting (not learned ACT). */

#ifndef COS_ULTRA_RECURRENT_DEPTH_H
#define COS_ULTRA_RECURRENT_DEPTH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_ULTRA_REC_STOP_MAX_ITER = 0,
    COS_ULTRA_REC_STOP_SIGMA_ACCEPT = 1,
    COS_ULTRA_REC_STOP_OVERTHINK = 2,
    COS_ULTRA_REC_STOP_SPECTRAL = 3,
} cos_ultra_recurrent_stop_t;

typedef struct {
    int   t_max;          /* maximum recurrent steps (>= 1) */
    float tau_accept;     /* halt when σ < τ (answer ready) */
    int   dim;            /* hidden state dimension (>= 1) */
    float h0_scale;       /* initial ||h|| scale */
    float transform_gain; /* per-step linear contraction (0<g<1), pattern 0 */
    int   pattern;        /* 0 = contractive demo, 1 = overthink demo */
} cos_ultra_recurrent_params_t;

/* One reference run: encode → T steps of transform + measure; σ halts.
 * `out_rho_last` receives last ratio ||h_t||/||h_{t-1}|| (spectral proxy). */
int cos_ultra_recurrent_depth_run(const cos_ultra_recurrent_params_t *p,
                                  int *out_steps,
                                  cos_ultra_recurrent_stop_t *out_reason,
                                  float *out_sigma_final,
                                  float *out_rho_last);

int cos_ultra_recurrent_depth_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
