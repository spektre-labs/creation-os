/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v288 σ-Oculus — tunable aperture (the conscious
 *                  hole in the roof).
 *
 *   The Pantheon's oculus admits both light AND rain:
 *   a perfect seal is useless, a perfect opening is
 *   unsafe.  The σ-gate threshold τ is the same
 *   aperture: τ = 0 lets everything through (no
 *   protection), τ = 1 blocks everything (no
 *   utility), τ ≈ 0.3 passes signal and rejects
 *   noise.  v288 types the aperture discipline as
 *   four v0 manifests so the kernel cannot ship a
 *   hidden threshold.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Aperture cascade (exactly 3 canonical risk
 *   domains):
 *     `medical` → τ = 0.10, width = `TIGHT`;
 *     `code`    → τ = 0.30, width = `NORMAL`;
 *     `creative`→ τ = 0.60, width = `OPEN`.
 *     Contract: canonical names in order; 3 distinct
 *     widths; τ strictly increasing — higher allowed
 *     risk ⇒ wider aperture.
 *
 *   Aperture extremes (exactly 3 canonical fixtures):
 *     `closed`  → τ = 0.0 → useless = true,
 *                              dangerous = false;
 *     `open`    → τ = 1.0 → useless = false,
 *                              dangerous = true;
 *     `optimal` → τ = 0.3 → useless = false,
 *                              dangerous = false.
 *     Contract: closed fixture is useless AND NOT
 *     dangerous; open fixture is dangerous AND NOT
 *     useless; optimal is neither.
 *
 *   Adaptive aperture (exactly 3 canonical
 *   self-tuning steps):
 *     step 0: τ = 0.30, error_rate = 0.15, action
 *             = `TIGHTEN`;
 *     step 1: τ = 0.20, error_rate = 0.08, action
 *             = `TIGHTEN`;
 *     step 2: τ = 0.15, error_rate = 0.03, action
 *             = `STABLE`.
 *     Rule: error_rate > 0.05 → `TIGHTEN` else
 *     `STABLE`; when `TIGHTEN`, τ at step n+1 < τ
 *     at step n.
 *     Contract: 3 rows; error_rate strictly
 *     monotonically decreasing; action matches the
 *     rule on every row; at least one `TIGHTEN` AND
 *     at least one `STABLE` — v278-style recursive
 *     self-improvement applied to the aperture.
 *
 *   Transparency (exactly 3 canonical fields, all
 *   reported):
 *     `tau_declared` · `sigma_measured` ·
 *     `decision_visible`, each `reported == true`.
 *     Contract: 3 distinct names; every field
 *     reported — the aperture is always visible to
 *     the user, same way the Pantheon's oculus is
 *     visible from the floor.
 *
 *   σ_oculus (surface hygiene):
 *       σ_oculus = 1 −
 *         (cascade_rows_ok + cascade_width_distinct_ok +
 *          cascade_tau_monotone_ok + extreme_rows_ok +
 *          extreme_polarity_ok + adaptive_rows_ok +
 *          adaptive_monotone_ok + adaptive_action_rule_ok +
 *          adaptive_both_actions_ok + transparency_rows_ok +
 *          transparency_all_reported_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 1 + 3 + 1)
 *   v0 requires `σ_oculus == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 cascade rows canonical; 3 distinct widths;
 *        τ strictly increasing.
 *     2. 3 extreme fixtures canonical; closed is
 *        useless AND NOT dangerous; open is
 *        dangerous AND NOT useless; optimal is
 *        neither.
 *     3. 3 adaptive steps canonical; error_rate
 *        strictly decreasing; action matches rule;
 *        TIGHTEN and STABLE both fire.
 *     4. 3 transparency fields canonical; all
 *        reported.
 *     5. σ_oculus ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v288.1 (named, not implemented): a live
 *     session-level σ_budget tracker with per-
 *     context recalibration, a real TIGHTEN /
 *     LOOSEN feedback loop driven by downstream
 *     error telemetry, and a UI that shows the
 *     aperture + measured σ for every response
 *     alongside the user-visible decision.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V288_OCULUS_H
#define COS_V288_OCULUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V288_N_CASCADE      3
#define COS_V288_N_EXTREME      3
#define COS_V288_N_ADAPTIVE     3
#define COS_V288_N_TRANSPARENT  3

typedef enum {
    COS_V288_WIDTH_TIGHT   = 1,
    COS_V288_WIDTH_NORMAL  = 2,
    COS_V288_WIDTH_OPEN    = 3
} cos_v288_width_t;

typedef enum {
    COS_V288_ACTION_TIGHTEN = 1,
    COS_V288_ACTION_STABLE  = 2
} cos_v288_action_t;

typedef struct {
    char              domain[10];
    float             tau;
    cos_v288_width_t  width;
} cos_v288_cascade_t;

typedef struct {
    char   label[10];
    float  tau;
    bool   useless;
    bool   dangerous;
} cos_v288_extreme_t;

typedef struct {
    int                step;
    float              tau;
    float              error_rate;
    cos_v288_action_t  action;
} cos_v288_adaptive_t;

typedef struct {
    char  field[18];
    bool  reported;
} cos_v288_transparent_t;

typedef struct {
    cos_v288_cascade_t      cascade     [COS_V288_N_CASCADE];
    cos_v288_extreme_t      extreme     [COS_V288_N_EXTREME];
    cos_v288_adaptive_t     adaptive    [COS_V288_N_ADAPTIVE];
    cos_v288_transparent_t  transparent [COS_V288_N_TRANSPARENT];

    float threshold_error;  /* 0.05 */

    int   n_cascade_rows_ok;
    bool  cascade_width_distinct_ok;
    bool  cascade_tau_monotone_ok;

    int   n_extreme_rows_ok;
    bool  extreme_polarity_ok;

    int   n_adaptive_rows_ok;
    bool  adaptive_error_monotone_ok;
    bool  adaptive_action_rule_ok;
    int   n_action_tighten;
    int   n_action_stable;

    int   n_transparent_rows_ok;
    bool  transparency_all_reported_ok;

    float sigma_oculus;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v288_state_t;

void   cos_v288_init(cos_v288_state_t *s, uint64_t seed);
void   cos_v288_run (cos_v288_state_t *s);

size_t cos_v288_to_json(const cos_v288_state_t *s,
                         char *buf, size_t cap);

int    cos_v288_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V288_OCULUS_H */
