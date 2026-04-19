/*
 * v285 σ-EU-AI-Act — regulatory fit for the
 *                    European AI Act (2024/1689).
 *
 *   Article 15 ("robustness, accuracy, and
 *   cybersecurity of high-risk AI systems") maps
 *   directly onto σ-gate: σ_product is the measured
 *   accuracy of a given answer, σ-log is the
 *   audit-trail artefact, and absence of an RLHF
 *   feedback loop simplifies the Article 52
 *   transparency / documentation obligations.
 *
 *   v285 types the compliance surface as four v0
 *   manifests so an auditor can confirm each
 *   property statically from the kernel's JSON
 *   output, regardless of which downstream pipeline
 *   Creation OS is embedded in.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Article 15 mapping (exactly 3 canonical rows):
 *     `robustness` · `accuracy` · `cybersecurity`,
 *     each with booleans `sigma_mapped == true` AND
 *     `audit_trail == true`.
 *     Contract: canonical names in order; 3 distinct;
 *     every Article-15 property is both σ-mapped and
 *     audit-trailed — a σ-gate deployment satisfies
 *     all three simultaneously.
 *
 *   Article 52 transparency (exactly 3 canonical
 *   rows):
 *     `training_docs` · `feedback_collection` ·
 *     `qa_process`, each with
 *     `required_by_eu == true` AND
 *     `sigma_simplifies == true`.
 *     Contract: canonical names; all required AND all
 *     simplified — σ-Constitutional eliminates RLHF
 *     feedback collection, so the documentation
 *     burden collapses into the σ-log itself.
 *
 *   Risk tiers (exactly 4 canonical rows with
 *   cascade τ_low = 0.20, τ_medium = 0.50,
 *   τ_high = 0.80):
 *     `low` · `medium` · `high` · `critical`, each:
 *     `label`, `σ_risk ∈ [0, 1]`, four boolean
 *     controls (`sigma_gate`, `hitl_required`,
 *     `audit_trail`, `redundancy_required`) and a
 *     derived `controls_count` ∈ {1..4}.
 *     Canonical:
 *       low      σ=0.10 → sigma_gate=Y, hitl=N,
 *                         audit=N, redundancy=N   (1)
 *       medium   σ=0.35 → sigma_gate=Y, hitl=N,
 *                         audit=Y, redundancy=N   (2)
 *       high     σ=0.65 → sigma_gate=Y, hitl=Y,
 *                         audit=Y, redundancy=N   (3)
 *       critical σ=0.90 → sigma_gate=Y, hitl=Y,
 *                         audit=Y, redundancy=Y   (4)
 *     Contract: canonical labels; `sigma_gate == true`
 *     on every tier; `controls_count` strictly
 *     increasing 1 → 2 → 3 → 4; the `critical` tier
 *     has all four controls on.
 *
 *   License / regulation stack (exactly 3 canonical
 *   rows):
 *     `scsl` → `LEGAL`, `eu_ai_act` → `REGULATORY`,
 *     `sigma_gate` → `TECHNICAL`, each with
 *     `enabled == true` AND `composable == true`.
 *     Contract: 3 distinct names; 3 distinct layers
 *     covering the legal / regulatory / technical
 *     axes; all enabled AND all composable —
 *     triple-layer defence without double coverage
 *     or gaps.
 *
 *   σ_euact (surface hygiene):
 *       σ_euact = 1 −
 *         (art15_rows_ok + art15_all_mapped_ok +
 *          art15_all_audit_ok + art52_rows_ok +
 *          art52_all_req_ok + art52_all_simpl_ok +
 *          risk_rows_ok + risk_all_sigma_ok +
 *          risk_monotone_ok + license_rows_ok +
 *          license_layers_ok + license_all_enabled_ok +
 *          license_all_composable_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 1 + 4 + 1 + 1 + 3 + 1 + 1 + 1)
 *   v0 requires `σ_euact == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 Article-15 rows canonical; sigma_mapped
 *        AND audit_trail on every row.
 *     2. 3 Article-52 rows canonical; required_by_eu
 *        AND sigma_simplifies on every row.
 *     3. 4 risk tiers canonical; sigma_gate on every
 *        tier; controls_count strictly monotonic
 *        1→2→3→4; critical has 4 controls.
 *     4. 3 license/regulation rows canonical; 3
 *        distinct layers (LEGAL/REGULATORY/TECHNICAL);
 *        all enabled AND all composable.
 *     5. σ_euact ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v285.1 (named, not implemented): a live
 *     end-to-end audit pipeline that turns the σ-log
 *     into a conformant Article-15/52 evidence bundle
 *     (including model/data sheets, σ_risk per use
 *     case, and a certified record-retention policy),
 *     plus a legal-opinion memo mapping every v0
 *     contract row to the corresponding EU AI Act
 *     provision.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V285_EU_AI_ACT_H
#define COS_V285_EU_AI_ACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V285_N_ART15    3
#define COS_V285_N_ART52    3
#define COS_V285_N_RISK     4
#define COS_V285_N_LICENSE  3

typedef enum {
    COS_V285_LAYER_LEGAL       = 1,
    COS_V285_LAYER_REGULATORY  = 2,
    COS_V285_LAYER_TECHNICAL   = 3
} cos_v285_layer_t;

typedef struct {
    char  name[14];
    bool  sigma_mapped;
    bool  audit_trail;
} cos_v285_art15_t;

typedef struct {
    char  name[22];
    bool  required_by_eu;
    bool  sigma_simplifies;
} cos_v285_art52_t;

typedef struct {
    char  label[10];
    float sigma_risk;
    bool  sigma_gate;
    bool  hitl_required;
    bool  audit_trail;
    bool  redundancy_required;
    int   controls_count;
} cos_v285_risk_t;

typedef struct {
    char              name[12];
    cos_v285_layer_t  layer;
    bool              enabled;
    bool              composable;
} cos_v285_license_t;

typedef struct {
    cos_v285_art15_t    art15   [COS_V285_N_ART15];
    cos_v285_art52_t    art52   [COS_V285_N_ART52];
    cos_v285_risk_t     risk    [COS_V285_N_RISK];
    cos_v285_license_t  license [COS_V285_N_LICENSE];

    float tau_low;     /* 0.20 */
    float tau_medium;  /* 0.50 */
    float tau_high;    /* 0.80 */

    int   n_art15_rows_ok;
    bool  art15_all_mapped_ok;
    bool  art15_all_audit_ok;

    int   n_art52_rows_ok;
    bool  art52_all_required_ok;
    bool  art52_all_simplifies_ok;

    int   n_risk_rows_ok;
    bool  risk_all_sigma_ok;
    bool  risk_monotone_ok;

    int   n_license_rows_ok;
    bool  license_layers_distinct_ok;
    bool  license_all_enabled_ok;
    bool  license_all_composable_ok;

    float sigma_euact;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v285_state_t;

void   cos_v285_init(cos_v285_state_t *s, uint64_t seed);
void   cos_v285_run (cos_v285_state_t *s);

size_t cos_v285_to_json(const cos_v285_state_t *s,
                         char *buf, size_t cap);

int    cos_v285_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V285_EU_AI_ACT_H */
