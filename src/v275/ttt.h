/*
 * v275 σ-TTT — test-time training gated by σ.
 *
 *   Stanford/NVIDIA TTT-E2E (12/2025) showed that
 *   updating the last ~25 % of MLP layers during
 *   inference yields 35× throughput gains on 2 M
 *   contexts while matching full-attention accuracy —
 *   academic validation of the v124 σ-continual
 *   "living weights" thesis.
 *
 *   v275 adds the σ-gate on top of TTT:
 *     * weight updates only when σ_update is low;
 *     * dual-track storage (stable vs living) with a
 *       σ_drift cap that triggers reset;
 *     * sliding-window σ-prioritised eviction;
 *     * a validation manifest that cites v124 and
 *       TTT-E2E as convergent evidence (not as a
 *       throughput claim — this kernel does not
 *       execute TTT).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   σ-gated update (exactly 4 fixtures, τ_update = 0.30):
 *     Each: `step`, `σ_update ∈ [0, 1]`,
 *     `decision ∈ {LEARN, SKIP}`,
 *     rule: `σ_update ≤ τ_update → LEARN` else SKIP.
 *     Contract: ≥ 1 LEARN AND ≥ 1 SKIP.
 *
 *   Dual-track (exactly 3 fixtures, canonical status):
 *     Each: `label`, `σ_drift ∈ [0, 1]`,
 *     `status ∈ {SYNCED, DIVERGING, RESET}`,
 *     rule:
 *       σ_drift <  τ_sync    = 0.15  → SYNCED
 *       σ_drift <  τ_reset   = 0.50  → DIVERGING
 *       else                          → RESET
 *     Contract: every branch fires ≥ 1×.
 *
 *   Sliding-window σ-eviction (exactly 6 tokens):
 *     Each: `token_id`, `σ_token ∈ [0, 1]`,
 *     `evict_rank ∈ [1..6]` = permutation matching
 *     descending σ order (rank 1 = highest σ evicted
 *     first, rank 6 = lowest σ kept longest) —
 *     `window_order_ok`.
 *
 *   Validation manifest (exactly 2 citations):
 *     `v124_sigma_continual` (living-weights thesis)
 *     and `ttt_e2e_2025` (Stanford / NVIDIA);
 *     both rows must be `present == true` AND cite a
 *     non-empty `evidence` string; `source_distinct ==
 *     true` (the two sources are not the same paper).
 *     This is a citation contract, not a throughput
 *     claim: no tokens-per-second is asserted.
 *
 *   σ_ttt (surface hygiene):
 *       σ_ttt = 1 −
 *         (update_rows_ok + update_both_ok +
 *          dualtrack_rows_ok + dualtrack_all_branches_ok +
 *          window_rows_ok + window_order_ok +
 *          validation_rows_ok + validation_distinct_ok) /
 *         (4 + 1 + 3 + 1 + 6 + 1 + 2 + 1)
 *   v0 requires `σ_ttt == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 update rows; decision matches σ vs τ_update;
 *        both branches fire.
 *     2. 3 dualtrack rows; status matches σ_drift
 *        cascade; all 3 status values fire.
 *     3. 6 window rows; evict_rank is a permutation of
 *        [1..6] matching descending σ.
 *     4. 2 validation rows; both present, distinct
 *        sources, non-empty evidence.
 *     5. σ_ttt ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v275.1 (named, not implemented): live TTT kernel
 *     wired into v262, real weight updates on MLP
 *     tail with σ-gated admission, on-device dual-
 *     track storage + snapshot reset, measured
 *     throughput vs full attention on 2 M contexts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V275_TTT_H
#define COS_V275_TTT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V275_N_UPDATE      4
#define COS_V275_N_DUALTRACK   3
#define COS_V275_N_WINDOW      6
#define COS_V275_N_VALIDATION  2

typedef enum {
    COS_V275_UPD_LEARN = 1,
    COS_V275_UPD_SKIP  = 2
} cos_v275_upd_t;

typedef enum {
    COS_V275_DT_SYNCED    = 1,
    COS_V275_DT_DIVERGING = 2,
    COS_V275_DT_RESET     = 3
} cos_v275_dt_t;

typedef struct {
    int             step;
    float           sigma_update;
    cos_v275_upd_t  decision;
} cos_v275_update_t;

typedef struct {
    char           label[14];
    float          sigma_drift;
    cos_v275_dt_t  status;
} cos_v275_dualtrack_t;

typedef struct {
    int   token_id;
    float sigma_token;
    int   evict_rank;
} cos_v275_window_t;

typedef struct {
    char  source  [24];
    char  evidence[48];
    bool  present;
} cos_v275_citation_t;

typedef struct {
    cos_v275_update_t      update   [COS_V275_N_UPDATE];
    cos_v275_dualtrack_t   dualtrack[COS_V275_N_DUALTRACK];
    cos_v275_window_t      window   [COS_V275_N_WINDOW];
    cos_v275_citation_t    validation[COS_V275_N_VALIDATION];

    float tau_update;   /* 0.30 */
    float tau_sync;     /* 0.15 */
    float tau_reset;    /* 0.50 */

    int   n_update_rows_ok;
    int   n_update_learn;
    int   n_update_skip;

    int   n_dualtrack_rows_ok;
    int   n_dt_synced;
    int   n_dt_diverging;
    int   n_dt_reset;

    int   n_window_rows_ok;
    bool  window_order_ok;

    int   n_validation_rows_ok;
    bool  validation_distinct_ok;

    float sigma_ttt;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v275_state_t;

void   cos_v275_init(cos_v275_state_t *s, uint64_t seed);
void   cos_v275_run (cos_v275_state_t *s);

size_t cos_v275_to_json(const cos_v275_state_t *s,
                         char *buf, size_t cap);

int    cos_v275_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V275_TTT_H */
