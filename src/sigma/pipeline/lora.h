/*
 * σ-LoRA — on-device fine-tuning via low-rank adaptation.
 *
 * P6 TTT flips fast weights for a single turn and throws them
 * away.  σ-LoRA makes the update durable: a rank-r delta
 * ΔW = α/r · B · A that lives beside the base matrix and
 * persists across sessions.
 *
 *     y = base(x) + (α/r) · B·(A·x)
 *
 * Shapes:
 *     A : rank × in_dim       (random init, small)
 *     B : out_dim × rank      (zero init — adapter starts as I)
 *
 * Training (minimal SGD, MSE loss):
 *     z = A·x                          (rank)
 *     h = B·z                          (out_dim, pre-scale)
 *     y = base + s·h           where s = α/r
 *     err = y - t
 *     ∂L/∂B = s · (err ⊗ z)
 *     ∂L/∂A = s · (Bᵀ · err) ⊗ x
 *     A -= lr · ∂L/∂A
 *     B -= lr · ∂L/∂B
 *
 * Only A and B move; the base weights stay frozen.
 *
 * σ-guidance:
 *     σ_adapter = mean_i clip01(|y_i - target_i|)
 *   computed before/after and stored in the adapter.  Callers
 *   reject adapters where σ_after ≥ σ_before.
 *
 * Determinism is paramount: the kernel uses a seeded LCG for
 * A's initialisation so two fresh adapters with the same seed
 * + rank + shape are byte-identical.
 *
 * This kernel is a reference implementation — real production
 * LoRA sits on top of BitNet 2B via the σ-Plugin ABI (NEXT-5);
 * here we ship the pure math + the adapter struct + the
 * lifecycle so everything above (multi-adapter routing,
 * sig-checked export, watchdog) is well-typed.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_LORA_H
#define COS_SIGMA_LORA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_LORA_MAX_NAME       32
#define COS_LORA_MAX_IN_DIM   1024
#define COS_LORA_MAX_OUT_DIM  1024
#define COS_LORA_MAX_RANK       64

enum cos_lora_status {
    COS_LORA_OK        =  0,
    COS_LORA_ERR_ARG   = -1,
    COS_LORA_ERR_OOM   = -2,
    COS_LORA_ERR_STATE = -3,
};

typedef struct {
    char     name[COS_LORA_MAX_NAME];
    int      in_dim;
    int      out_dim;
    int      rank;
    float    alpha;
    float   *A;              /* rank × in_dim, row-major          */
    float   *B;              /* out_dim × rank, row-major         */
    uint64_t seed;           /* reproducible init                 */
    /* Bookkeeping for σ-guided acceptance. */
    float    sigma_before;   /* σ before training (clip01)        */
    float    sigma_after;    /* σ after last training call        */
    int      training_steps;
    int      frozen;         /* 1 → no further updates            */
} cos_lora_adapter_t;

/* ==================================================================
 * Lifecycle
 * ================================================================== */

/* Allocate a fresh adapter.  A is filled from a 64-bit LCG
 * seeded with `seed`, scaled to the LoRA-PEFT default of
 * 1/sqrt(in_dim).  B starts at zero so the adapter is the
 * identity at construction. */
int  cos_lora_init(cos_lora_adapter_t *a,
                   const char *name,
                   int in_dim, int out_dim, int rank,
                   float alpha, uint64_t seed);
void cos_lora_free(cos_lora_adapter_t *a);

/* Zero B and rewind training counters (leave A alone). */
void cos_lora_reset(cos_lora_adapter_t *a);

/* ==================================================================
 * Forward
 * ================================================================== */

/* y = base + (α/rank) · B·(A·x)
 * - base may be NULL → treated as zeros
 * - y, x, base live in caller memory; shapes follow the adapter */
int cos_lora_forward(const cos_lora_adapter_t *a,
                     const float *x,
                     const float *base,
                     float *y);

/* ==================================================================
 * Training
 * ================================================================== */

/* Single MSE SGD step on (x, target).  `out` (if non-NULL)
 * receives the post-update prediction for measurement.  Updates
 * `sigma_after` and `training_steps`.
 */
int cos_lora_train_step(cos_lora_adapter_t *a,
                        const float *x,
                        const float *target,
                        const float *base,
                        float lr,
                        float *out);

/* σ helper: mean_i clip01(|y_i - t_i|). */
float cos_lora_sigma(const float *y, const float *t, int n);

/* ==================================================================
 * Introspection
 * ================================================================== */

typedef struct {
    size_t bytes_weights;
    float  sigma_before;
    float  sigma_after;
    float  sigma_improvement;  /* = before - after   */
    int    steps;
    int    frozen;
} cos_lora_stats_t;

void cos_lora_stats(const cos_lora_adapter_t *a, cos_lora_stats_t *s);

/* ==================================================================
 * Routing (multi-adapter)
 * ================================================================== */

#define COS_LORA_REGISTRY_CAP 16

typedef struct {
    const cos_lora_adapter_t *adapters[COS_LORA_REGISTRY_CAP];
    char                      roles[COS_LORA_REGISTRY_CAP][COS_LORA_MAX_NAME];
    int n;
} cos_lora_registry_t;

void cos_lora_registry_init(cos_lora_registry_t *reg);
int  cos_lora_registry_bind(cos_lora_registry_t *reg,
                            const char *role,
                            const cos_lora_adapter_t *adapter);

/* Pick an adapter by role ("factual", "code", ...); returns NULL
 * when no binding exists so the caller can fall back to the
 * frozen base model. */
const cos_lora_adapter_t *cos_lora_route(const cos_lora_registry_t *reg,
                                         const char *role);

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_lora_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_LORA_H */
