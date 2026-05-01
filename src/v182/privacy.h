/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v182 σ-Privacy — differential privacy with σ-adaptive noise.
 *
 *   v129 federated adds DP to gradients.  v182 extends privacy
 *   across the stack:
 *
 *     input   — prompt is SHA-256-hashed before any logging;
 *               plaintext never persists past the session.
 *     memory  — every v115 entry carries a privacy tier:
 *                 public    — shareable in v129 federation
 *                 private   — NEVER leaves this node
 *                 ephemeral — dropped on session end
 *     DP      — noise σ_n = base_sigma · f(σ_product); high σ
 *               ⇒ more noise (protect the uncertain), low σ ⇒
 *               less noise (preserve utility).
 *     forget  — GDPR right-to-erasure:
 *                   · memory rows purged
 *                   · audit rows marked forgotten, hash kept
 *                   · adapter contribution subtracted
 *                   · federation unlearn request broadcast.
 *
 *   Exit invariants (merge-gate):
 *
 *     1. input_hash == SHA-256(prompt) and plaintext never
 *        appears in serialized state.
 *     2. Privacy tiers: exactly one of {public, private,
 *        ephemeral}; ephemeral rows are purged at end-of-
 *        session.
 *     3. Adaptive σ-DP gives strictly larger noise on the
 *        high-σ subset than on the low-σ subset.
 *     4. At equal *utility* (mean |query error| on low-σ
 *        subset ≤ fixed-ε baseline), σ-DP spends strictly
 *        less privacy budget on the low-σ subset than the
 *        fixed-ε baseline (ε_effective_low < ε_fixed).
 *     5. `forget(session_id)` removes matching memory rows,
 *        marks audit rows, and post-forget verify still
 *        passes for the (reduced) chain.
 *
 *   v182.1 (named, not shipped):
 *     - real v115 memory row API + AES-GCM at rest;
 *     - live v129 unlearn broadcast;
 *     - zk-proof verifier for external right-to-forget.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V182_PRIVACY_H
#define COS_V182_PRIVACY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V182_MAX_ROWS        256
#define COS_V182_STR_MAX          96

typedef enum {
    COS_V182_TIER_PUBLIC    = 0,
    COS_V182_TIER_PRIVATE   = 1,
    COS_V182_TIER_EPHEMERAL = 2,
} cos_v182_tier_t;

typedef struct {
    uint64_t id;
    char     session_id[COS_V182_STR_MAX];
    uint8_t  input_hash [32];                  /* SHA-256(prompt) */
    uint8_t  output_hash[32];                  /* SHA-256(response) */
    /* NO plaintext prompt field. */
    float    sigma_product;
    float    query_answer;                     /* scalar response */
    cos_v182_tier_t tier;
    bool     forgotten;                        /* set by cos_v182_forget */
} cos_v182_row_t;

typedef struct {
    int             n_rows;
    cos_v182_row_t  rows[COS_V182_MAX_ROWS];

    float           base_sigma;                 /* σ_n baseline */
    float           sigma_adaptive_k;           /* slope k in noise */
    float           fixed_epsilon;              /* baseline ε */
    uint64_t        seed;

    /* Populated by cos_v182_run_dp(). */
    float           mean_noise_high_sigma;      /* σ-adaptive */
    float           mean_noise_low_sigma;
    float           mean_err_fixed_low;         /* fixed-ε error */
    float           mean_err_adaptive_low;      /* σ-DP error */
    float           epsilon_effective_low;      /* σ-DP effective */
    float           epsilon_effective_high;

    /* Populated by cos_v182_forget(). */
    int             n_rows_before_forget;
    int             n_rows_after_forget;
    int             n_audit_marked;             /* rows with forgotten=true */
} cos_v182_state_t;

void cos_v182_init(cos_v182_state_t *s, uint64_t seed);

/* Insert a row; prompt/response are hashed on entry and never
 * stored in plaintext. */
int  cos_v182_ingest(cos_v182_state_t *s,
                      const char *session_id,
                      const char *prompt,
                      const char *response,
                      float sigma_product,
                      float scalar_answer,
                      cos_v182_tier_t tier);

/* Populate a deterministic fixture with mixed σ / tier values. */
void cos_v182_populate_fixture(cos_v182_state_t *s, int n);

/* Drop ephemeral rows (simulating end-of-session). */
void cos_v182_end_session(cos_v182_state_t *s);

/* Run the σ-adaptive DP vs fixed-ε comparison and record
 * aggregate metrics. */
void cos_v182_run_dp(cos_v182_state_t *s);

/* Apply right-to-erasure for `session_id`.  Memory rows are
 * removed; an audit-style `forgotten` flag is set on any
 * copies retained by callers that hold a prior cursor. */
int  cos_v182_forget(cos_v182_state_t *s, const char *session_id);

/* Return true iff no plaintext prompt string is reachable
 * through the public surface.  (v182.0 has no plaintext field
 * at all; this returns true by construction and is asserted
 * by the merge-gate.) */
bool cos_v182_serialized_has_no_plaintext(const cos_v182_state_t *s);

size_t cos_v182_to_json(const cos_v182_state_t *s, char *buf, size_t cap);

int cos_v182_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V182_PRIVACY_H */
