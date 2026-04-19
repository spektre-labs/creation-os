/*
 * v276 σ-Gated-DeltaNet — linear attention with a
 *                         σ-gate on every gate.
 *
 *   Gated DeltaNet is a linear-attention kernel that
 *   competes with TTT and Mamba on long contexts.  v276
 *   types the σ-layer:
 *     * 2 canonical backends in the hybrid set
 *       (`deltanet` linear · `transformer` quadratic);
 *     * 4 σ-gate fixtures where `σ_gate > τ_gate`
 *       triggers a full-attention fallback for that
 *       token;
 *     * a 3-component combo stack (`deltanet` · `ttt`
 *       · `sigma_gate`) every enabled == true;
 *     * 3-task benchmark vs v267 σ-Mamba with
 *       `σ_chosen ≤ σ_rival` each AND ≥ 2 distinct
 *       chosen backends across tasks.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Backends (exactly 2, canonical order):
 *     `deltanet` — exponent 1, gate_mechanism true,
 *                  throughput_rel > transformer.
 *     `transformer` — exponent 2, gate_mechanism false.
 *     Contract: deltanet.throughput_rel >
 *     transformer.throughput_rel AND exponents typed.
 *
 *   σ-gate fallback (exactly 4 fixtures, τ_gate = 0.35):
 *     Each: `token_id`, `σ_gate ∈ [0, 1]`,
 *     `decision ∈ {LINEAR, FALLBACK_FULL}`,
 *     rule: `σ_gate ≤ τ_gate → LINEAR` else
 *     `FALLBACK_FULL`.
 *     Contract: ≥ 1 LINEAR AND ≥ 1 FALLBACK_FULL.
 *
 *   Combo stack (exactly 3 components, canonical order):
 *     `deltanet` · `ttt` · `sigma_gate`, each
 *     `enabled == true`, `layer_slot ∈ [1..3]`
 *     matching the canonical order (deltanet = slot 1,
 *     ttt = slot 2, sigma_gate = slot 3).
 *     Contract: all 3 enabled AND layer_slot permutation
 *     matches canonical order exactly.
 *
 *   Tri-backend tasks (exactly 3 fixtures, canonical
 *   order):
 *     Each task exposes one σ per backend
 *     (`sigma_mamba`, `sigma_deltanet`, `sigma_ttt`),
 *     `chosen ∈ {mamba, deltanet, ttt}`
 *     = `argmin(σ_backend)` for that task,
 *     `sigma_chosen ∈ [0, 1]`, and `sigma_rival` =
 *     min σ over the two non-chosen backends.
 *     Contract: `sigma_chosen ≤ sigma_rival` for each
 *     task AND ≥ 2 distinct backends chosen across the
 *     3 tasks (so a regression that always picks one
 *     backend fails).
 *
 *   σ_deltanet (surface hygiene):
 *       σ_deltanet = 1 −
 *         (backend_rows_ok + backend_exponents_ok +
 *          backend_throughput_ok +
 *          gate_rows_ok + gate_both_ok +
 *          combo_rows_ok + combo_order_ok +
 *          task_rows_ok + task_chosen_ok +
 *          task_diversity_ok) /
 *         (2 + 1 + 1 + 4 + 1 + 3 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_deltanet == 0.0`.
 *
 *   Contracts (v0):
 *     1. 2 backends typed; exponents canonical;
 *        deltanet throughput > transformer.
 *     2. 4 gate rows; decision matches σ vs τ_gate;
 *        both branches fire.
 *     3. 3 combo components enabled with canonical
 *        layer_slot order.
 *     4. 3 tri-backend tasks; chosen == argmin(σ);
 *        sigma_chosen ≤ sigma_rival; ≥ 2 distinct
 *        chosen backends.
 *     5. σ_deltanet ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v276.1 (named, not implemented): live Gated
 *     DeltaNet kernel wired into v262, per-token σ
 *     from the gate, live fallback to full attention,
 *     measured AURCC vs v267 σ-Mamba and v275 σ-TTT
 *     on matched workloads.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V276_GATED_DELTANET_H
#define COS_V276_GATED_DELTANET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V276_N_BACKENDS 2
#define COS_V276_N_GATE     4
#define COS_V276_N_COMBO    3
#define COS_V276_N_TASKS    3

typedef enum {
    COS_V276_DEC_LINEAR        = 1,
    COS_V276_DEC_FALLBACK_FULL = 2
} cos_v276_dec_t;

typedef enum {
    COS_V276_BACK_MAMBA    = 1,
    COS_V276_BACK_DELTANET = 2,
    COS_V276_BACK_TTT      = 3
} cos_v276_back_t;

typedef struct {
    char  name[14];
    int   exponent;
    bool  gate_mechanism;
    float throughput_rel;
} cos_v276_backend_t;

typedef struct {
    int             token_id;
    float           sigma_gate;
    cos_v276_dec_t  decision;
} cos_v276_gate_t;

typedef struct {
    char  component[14];
    bool  enabled;
    int   layer_slot;
} cos_v276_combo_t;

typedef struct {
    char             label[16];
    float            sigma_mamba;
    float            sigma_deltanet;
    float            sigma_ttt;
    cos_v276_back_t  chosen;
    float            sigma_chosen;
    float            sigma_rival;
} cos_v276_task_t;

typedef struct {
    cos_v276_backend_t  backends [COS_V276_N_BACKENDS];
    cos_v276_gate_t     gate     [COS_V276_N_GATE];
    cos_v276_combo_t    combo    [COS_V276_N_COMBO];
    cos_v276_task_t     tasks    [COS_V276_N_TASKS];

    float tau_gate;   /* 0.35 */

    int   n_backend_rows_ok;
    bool  backend_exponents_ok;
    bool  backend_throughput_ok;

    int   n_gate_rows_ok;
    int   n_gate_linear;
    int   n_gate_fallback;

    int   n_combo_rows_ok;
    bool  combo_order_ok;

    int   n_task_rows_ok;
    int   n_task_chosen_ok;
    int   n_distinct_chosen;

    float sigma_deltanet;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v276_state_t;

void   cos_v276_init(cos_v276_state_t *s, uint64_t seed);
void   cos_v276_run (cos_v276_state_t *s);

size_t cos_v276_to_json(const cos_v276_state_t *s,
                         char *buf, size_t cap);

int    cos_v276_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V276_GATED_DELTANET_H */
