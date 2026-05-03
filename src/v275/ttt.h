/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v275 σ-TTT — test-time training gated by σ (merge-gate manifest).
 *
 * See docs/SURFACE_VERSIONS.md (v275 row) for the counting contract.
 */

#ifndef COS_V275_TTT_H
#define COS_V275_TTT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V275_N_UPDATE    4
#define COS_V275_N_DRIFT     3
#define COS_V275_N_WINDOW    6
#define COS_V275_N_CITE      2
#define COS_V275_DENOMINATOR 19

typedef enum {
    COS_V275_LEARN = 1,
    COS_V275_SKIP  = 2,
} cos_v275_update_decision_t;

typedef enum {
    COS_V275_SYNCED    = 1,
    COS_V275_DIVERGING = 2,
    COS_V275_RESET     = 3,
} cos_v275_drift_t;

typedef struct {
    char                        tag[8];
    float                       sigma_update;
    cos_v275_update_decision_t decision;
    bool                        decision_ok;
} cos_v275_update_row_t;

typedef struct {
    char              tag[8];
    float             sigma_drift;
    cos_v275_drift_t  state;
    bool              state_ok;
} cos_v275_drift_row_t;

typedef struct {
    char  id[8];
    float sigma;
    int   evict_rank;
    bool  rank_ok;
} cos_v275_window_token_t;

typedef struct {
    cos_v275_update_row_t   updates[COS_V275_N_UPDATE];
    cos_v275_drift_row_t    drift[COS_V275_N_DRIFT];
    cos_v275_window_token_t window[COS_V275_N_WINDOW];

    float tau_update;
    float tau_sync;
    float tau_reset;

    int   n_update_row_ok;
    bool  update_branches_ok;

    int   n_drift_row_ok;
    bool  drift_branches_ok;

    int   n_window_rank_ok;
    bool  window_permutation_ok;

    bool  cite_v124_ok;
    bool  cite_ttt_ok;

    int   passing;
    float sigma_ttt;

    bool  manifest_closed;

    uint64_t chain_hash;
} cos_v275_state_t;

cos_v275_update_decision_t cos_v275_update_decide(float sigma_update, float tau_update);
cos_v275_drift_t          cos_v275_drift_classify(float sigma_drift, float tau_sync, float tau_reset);

void cos_v275_init(cos_v275_state_t *s, uint64_t seed);
void cos_v275_run(cos_v275_state_t *s);

/** Serialize manifest to JSON (no trailing newline). Returns bytes written or 0 on overflow. */
size_t cos_v275_to_json(const cos_v275_state_t *s, char *buf, size_t cap);

/** Returns 0 if all v275 merge-gate predicates hold inside this TU. */
int cos_v275_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V275_TTT_H */
