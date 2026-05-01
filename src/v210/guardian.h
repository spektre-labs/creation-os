/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v210 σ-Guardian — autonomous security agent.
 *
 *   Runs in a separate process from the primary agent —
 *   in v0 we model this as an independent evaluation
 *   pass that can only *observe* the primary's actions,
 *   never mutate them.  It watches 20 behavioural
 *   windows and flags anomalies, mapping each hit to the
 *   OWASP Agentic Top-10 (2026) category so operators
 *   get a named threat, not a scalar.
 *
 *   OWASP Agentic Top-10 coverage (v0 mapping):
 *     OA01 Goal hijacking      → v191 constitutional
 *     OA02 Tool misuse         → v112 σ-gated tools
 *     OA03 Identity abuse      → v153 σ-identity
 *     OA04 Memory poisoning    → v115 σ-weighted recall
 *     OA05 Cascading failures  → v159 σ-heal
 *     OA06 Rogue agent         → v210 guardian (this)
 *     OA07 Context injection   → v117 σ-context
 *     OA08 Privilege escalation→ v209 containment L1
 *     OA09 Covert channels     → v209 L4 + L2
 *     OA10 Supply-chain taint  → v178 consensus + v181
 *
 *   Escalation ladder:
 *     L1  log + continue      σ_guardian ≤ 0.25
 *     L2  log + warn user     σ_guardian ≤ 0.50
 *     L3  log + block action  σ_guardian ≤ 0.75
 *     L4  log + kill process  σ_guardian >  0.75
 *
 *   Contracts (v0):
 *     1. 20 observation windows; every window carries
 *        a σ_anomaly ∈ [0,1] and a σ_guardian composite.
 *     2. Every non-L1 window names an OWASP category ∈
 *        [OA01, OA10].
 *     3. At least one window reaches L3 (block) and one
 *        reaches L4 (kill) — the guardian must actually
 *        bite at the top of the ladder.
 *     4. The guardian model id differs from the primary
 *        model id (self-monitor impossibility).
 *     5. Baseline drift: mean σ_baseline across windows
 *        is strictly below mean σ_anomaly — the anomaly
 *        signal actually separates from baseline.
 *     6. FNV-1a hash chain replays byte-identically.
 *
 *   v210.1 (named): live dual-model monitor, v160
 *     telemetry baseline, real v191 / v209 /v178 wiring.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V210_GUARDIAN_H
#define COS_V210_GUARDIAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V210_N_WINDOWS 20
#define COS_V210_OWASP_N   10

typedef enum {
    COS_V210_L1_LOG_CONTINUE = 1,
    COS_V210_L2_WARN_USER    = 2,
    COS_V210_L3_BLOCK_ACTION = 3,
    COS_V210_L4_KILL_PROCESS = 4
} cos_v210_level_t;

typedef enum {
    COS_V210_OA_NONE = 0,
    COS_V210_OA01 = 1,  /* goal hijacking */
    COS_V210_OA02 = 2,  /* tool misuse */
    COS_V210_OA03 = 3,  /* identity abuse */
    COS_V210_OA04 = 4,  /* memory poisoning */
    COS_V210_OA05 = 5,  /* cascading failures */
    COS_V210_OA06 = 6,  /* rogue agent */
    COS_V210_OA07 = 7,  /* context injection */
    COS_V210_OA08 = 8,  /* privilege escalation */
    COS_V210_OA09 = 9,  /* covert channels */
    COS_V210_OA10 = 10  /* supply chain */
} cos_v210_owasp_t;

typedef struct {
    int      id;
    int      primary_model_id;     /* observed */
    int      guardian_model_id;    /* observer — must differ */
    float    sigma_baseline;       /* per-window baseline (v160) */
    float    sigma_anomaly;        /* observed divergence */
    float    sigma_guardian;       /* composite decision */
    int      owasp_category;       /* cos_v210_owasp_t */
    int      escalation_level;     /* cos_v210_level_t */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v210_window_t;

typedef struct {
    int                  n;
    int                  primary_model_id;
    int                  guardian_model_id;
    cos_v210_window_t    windows[COS_V210_N_WINDOWS];

    float                tau_l2;   /* 0.25 */
    float                tau_l3;   /* 0.50 */
    float                tau_l4;   /* 0.75 */

    int                  per_level_count[5]; /* index 1..4 */
    int                  per_owasp_hits[COS_V210_OWASP_N + 1];

    float                baseline_mean;
    float                anomaly_mean;

    bool                 chain_valid;
    uint64_t             terminal_hash;
    uint64_t             seed;
} cos_v210_state_t;

void   cos_v210_init(cos_v210_state_t *s, uint64_t seed);
void   cos_v210_build(cos_v210_state_t *s);
void   cos_v210_run(cos_v210_state_t *s);

size_t cos_v210_to_json(const cos_v210_state_t *s,
                         char *buf, size_t cap);

int    cos_v210_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V210_GUARDIAN_H */
