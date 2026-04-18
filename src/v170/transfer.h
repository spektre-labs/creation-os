/*
 * v170 σ-Transfer — cross-domain transfer learning with
 * σ-gated decisions + negative-transfer rollback.
 *
 * v141 curriculum learns one subject at a time.  v170 moves
 * acquired skill from a strong domain to a weak neighbour
 * *only* when the domain-embedding distance is small and the
 * σ of the source domain is meaningfully lower than the σ of
 * the target, then re-measures σ on the target and rolls the
 * transfer back (v124) if σ *rose*.
 *
 * v170.0 (this file) ships:
 *   - a deterministic 8-domain embedding space (math, physics,
 *     chemistry, biology, code, logic, history, poetry) with
 *     hand-picked coordinates in ℝ^4 so close-and-far pairs
 *     are robustly ordered (math↔physics near, math↔poetry
 *     far)
 *   - σ per domain (the weak-domain identifier)
 *   - a transfer decision:
 *       source = closest domain with σ_source < σ_target - δ
 *       go = distance ≤ d_max ∧ σ_source ≤ τ_src
 *   - a post-transfer outcome model whose σ_delta is
 *     deterministically derived from the source/target pair,
 *     so "chemistry → biology" lands (σ drops), "math →
 *     poetry" fails (σ rises) and gets rolled back
 *   - a negative-transfer detector that emits a rollback
 *     event + reason
 *   - a zero-shot adapter: if the target domain has σ = 1.0
 *     ("never seen"), the k=2 nearest known domains contribute
 *     an ensemble initial σ with a σ_ensemble
 *
 * v170.1 (named, not shipped):
 *   - real LoRA-adapter composition
 *   - real v141 curriculum hook
 *   - v115 memory-backed history of transfer attempts
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V170_TRANSFER_H
#define COS_V170_TRANSFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V170_N_DOMAINS 8
#define COS_V170_EMBED_DIM 4
#define COS_V170_MAX_NAME  32

typedef struct {
    char  name[COS_V170_MAX_NAME];
    float embed[COS_V170_EMBED_DIM];
    float sigma;                          /* current weakness, ∈ [0,1]; 1.0 = unseen */
} cos_v170_domain_t;

typedef struct {
    bool  go;                             /* transfer allowed */
    int   source_idx;
    int   target_idx;
    float distance;                       /* ‖embed(src) − embed(tgt)‖₂ */
    float sigma_source;
    float sigma_target;
    float d_max;                          /* gate */
    float tau_src;                        /* gate */
    char  reason[96];
} cos_v170_decision_t;

typedef struct {
    int   source_idx;
    int   target_idx;
    float sigma_before;                   /* σ_target prior to transfer */
    float sigma_after;                    /* σ_target after transfer   */
    float delta;                          /* after − before (negative = good) */
    bool  rollback;                       /* true iff delta > 0 (made it worse) */
    float sigma_rollback;                 /* effective σ after rollback
                                             (== before if rollback) */
    char  note[128];
} cos_v170_outcome_t;

typedef struct {
    float    sigma_gap_delta;             /* δ: source must be ≥ this better */
    float    d_max;                       /* max transferable distance */
    float    tau_src;                     /* max σ for a usable source */
    uint64_t seed;
    cos_v170_domain_t domains[COS_V170_N_DOMAINS];
} cos_v170_state_t;

void  cos_v170_init(cos_v170_state_t *s, uint64_t seed);

int   cos_v170_find_domain(const cos_v170_state_t *s, const char *name);

float cos_v170_distance(const cos_v170_state_t *s, int a, int b);

cos_v170_decision_t
      cos_v170_decide(const cos_v170_state_t *s, int target_idx);

cos_v170_outcome_t
      cos_v170_apply(cos_v170_state_t *s, cos_v170_decision_t d);

/* Zero-shot: for an unseen target (σ=1.0), ensemble the k=2
 * nearest known domains (each σ < tau_src) into an initial σ.
 * Returns σ_ensemble; sets `*out_k` to how many sources were
 * usable (0..2). */
float cos_v170_zero_shot(const cos_v170_state_t *s,
                         int target_idx, int *out_k);

/* Full report of one decide/apply cycle for the "biology"
 * target (see fixture) in deterministic JSON form. */
size_t cos_v170_report_json(cos_v170_state_t *s,
                            const char *target_name,
                            char *buf, size_t cap);

size_t cos_v170_state_json(const cos_v170_state_t *s,
                           char *buf, size_t cap);

int cos_v170_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V170_TRANSFER_H */
