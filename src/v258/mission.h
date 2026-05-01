/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v258 σ-Mission — the mission layer.
 *
 *   Why does Creation OS exist?  v258 encodes the
 *   mission in code, measures impact (σ before vs σ
 *   after), and gates kernels against mission drift.
 *   The anti-drift gate MUST accept at least one kernel
 *   and reject at least one, so both branches have
 *   teeth.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Mission statement (exact string, checked verbatim):
 *     "Reduce sigma in every system that touches human
 *      life. Make intelligence trustworthy."
 *
 *   Impact measurement (exactly 4 scopes, canonical order):
 *     per_user · per_domain · per_institution · per_society
 *   Each row:
 *     sigma_before ∈ [0, 1]
 *     sigma_after  ∈ [0, 1]
 *     delta_sigma  = sigma_before − sigma_after
 *     improved     = (delta_sigma > 0)
 *   v0 contract: every row `improved == true` (σ
 *   strictly reduced across every scope).
 *
 *   Anti-mission-drift (exactly 4 kernel evaluations):
 *     Each row:
 *       kernel_ref  (e.g. "v29", "v254", "v258_stub_marketing")
 *       sigma_mission ∈ [0, 1] (higher = more drift)
 *       decision   ∈ {ACCEPT, REJECT}
 *     Rule:
 *       sigma_mission ≤ τ_mission (0.30) → ACCEPT
 *       sigma_mission >  τ_mission        → REJECT
 *     v0 fixture: ≥ 1 ACCEPT AND ≥ 1 REJECT (both
 *     branches fire), and the decision field MUST match
 *     the σ vs τ comparison byte-for-byte.  v191
 *     constitutional is the upstream justifier.
 *
 *   Long-term vision (exactly 4 anchors, v203 + v233 +
 *   v238 + 1=1 identity):
 *     civilization_governance     (v203)
 *     wisdom_transfer_legacy      (v233)
 *     human_sovereignty           (v238)
 *     declared_eq_realized        (1=1 identity)
 *   Every anchor declared AND `anchor_ok == true`.
 *
 *   σ_mission (surface hygiene):
 *       σ_mission = 1 −
 *         (statement_ok + scopes_improved +
 *          mission_gate_ok + anchors_ok) /
 *         (1 + 4 + 4 + 4)
 *   v0 requires `σ_mission == 0.0`.
 *
 *   Contracts (v0):
 *     1. `mission_statement` matches the canonical
 *        string byte-for-byte AND `statement_ok == true`.
 *     2. Exactly 4 impact scopes in canonical order;
 *        every σ ∈ [0, 1]; every `improved == true`;
 *        delta_sigma byte-consistent.
 *     3. Exactly 4 anti-drift rows; decision matches
 *        σ_mission vs τ_mission=0.30; ≥ 1 ACCEPT AND
 *        ≥ 1 REJECT.
 *     4. Exactly 4 long-term anchors in canonical order
 *        with `anchor_ok == true`.
 *     5. σ_mission ∈ [0, 1] AND σ_mission == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v258.1 (named, not implemented): `cos impact`
 *     producing a signed report against live telemetry,
 *     v191 constitutional feeding a running anti-drift
 *     classifier, v203 / v233 / v238 binding to real
 *     governance / legacy / sovereignty subsystems.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V258_MISSION_H
#define COS_V258_MISSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V258_MISSION_STATEMENT \
    "Reduce sigma in every system that touches human life. " \
    "Make intelligence trustworthy."

#define COS_V258_N_SCOPES   4
#define COS_V258_N_DRIFT    4
#define COS_V258_N_ANCHORS  4

typedef enum {
    COS_V258_DRIFT_ACCEPT = 1,
    COS_V258_DRIFT_REJECT = 2
} cos_v258_drift_dec_t;

typedef struct {
    char  statement[192];
    bool  statement_ok;
} cos_v258_statement_t;

typedef struct {
    char  scope[24];
    float sigma_before;
    float sigma_after;
    float delta_sigma;
    bool  improved;
} cos_v258_impact_t;

typedef struct {
    char                  kernel_ref[32];
    float                 sigma_mission;
    cos_v258_drift_dec_t  decision;
} cos_v258_drift_t;

typedef struct {
    char  anchor[32];
    char  upstream[8];
    bool  anchor_ok;
} cos_v258_anchor_t;

typedef struct {
    cos_v258_statement_t mission;
    cos_v258_impact_t    scopes  [COS_V258_N_SCOPES];
    cos_v258_drift_t     drift   [COS_V258_N_DRIFT];
    cos_v258_anchor_t    anchors [COS_V258_N_ANCHORS];

    float tau_mission;

    int   n_scopes_improved;
    int   n_accept;
    int   n_reject;
    int   n_anchors_ok;

    float sigma_mission;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v258_state_t;

void   cos_v258_init(cos_v258_state_t *s, uint64_t seed);
void   cos_v258_run (cos_v258_state_t *s);

size_t cos_v258_to_json(const cos_v258_state_t *s,
                         char *buf, size_t cap);

int    cos_v258_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V258_MISSION_H */
