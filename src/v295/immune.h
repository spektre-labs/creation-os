/*
 * v295 σ-Immune — innate + adaptive + memory +
 *                 autoimmune-prevention in code.
 *
 *   The human immune system recognises, remembers,
 *   resists, and — crucially — does not attack itself.
 *   v295 types the same discipline at software level:
 *   σ-gate is the innate "first wall", v278 RSI-style
 *   learning is the adaptive response, the σ-log is
 *   immunological memory, and the oculus / parthenon
 *   pair keeps the immune response off the body's
 *   own cells.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Innate immunity (exactly 3 canonical patterns):
 *     `sql_injection`    σ_raw=0.85 INSTANT;
 *     `prompt_injection` σ_raw=0.80 INSTANT;
 *     `obvious_malware`  σ_raw=0.90 INSTANT.
 *     Each: `blocked = true`,
 *           `requires_training = false`,
 *           `response_tier == INSTANT`.
 *     Contract: 3 rows; σ_raw ≥ τ_innate=0.70 on every
 *     row; all blocked; no training required; every
 *     row fires INSTANT — the innate wall is always
 *     on.
 *
 *   Adaptive immunity (exactly 3 canonical rows):
 *     `novel_attack_first_seen`      learned
 *                                    faster=false
 *                                    cross=false;
 *     `same_attack_second_seen`      learned
 *                                    faster=true
 *                                    cross=false;
 *     `related_variant_seen`         learned
 *                                    faster=false
 *                                    cross=true.
 *     Contract: 3 rows; `learned == true` on every
 *     row; exactly 1 `faster_on_repeat = true`
 *     (the second encounter) AND exactly 1
 *     `cross_recognized = true` (the variant).
 *
 *   Memory cells (exactly 3 canonical σ-log entries):
 *     `pattern_A_first_logged`    recognised=false
 *                                 tier=SLOW;
 *     `pattern_A_reencountered`   recognised=true
 *                                 tier=FAST;
 *     `pattern_B_new_logged`      recognised=false
 *                                 tier=SLOW.
 *     Contract: 3 rows; `recognised == true` iff
 *     `tier == FAST`; exactly 1 row with
 *     `recognised = true` (re-encounter).
 *
 *   Autoimmune prevention (exactly 3 canonical τ
 *   scenarios):
 *     `tau_too_tight` τ=0.05 fpr=0.40 AUTOIMMUNE;
 *     `tau_balanced`  τ=0.30 fpr=0.05 HEALTHY;
 *     `tau_too_loose` τ=0.80 fpr=0.01 IMMUNODEFICIENT.
 *     Contract: 3 DISTINCT verdicts in order; τ
 *     strictly monotonically increasing; exactly 1
 *     HEALTHY AND exactly 1 AUTOIMMUNE AND exactly 1
 *     IMMUNODEFICIENT; HEALTHY iff
 *     τ ∈ [τ_lo=0.10, τ_hi=0.60] AND
 *     fpr ≤ fpr_budget=0.10 — defends without
 *     attacking itself.
 *
 *   σ_immune (surface hygiene):
 *       σ_immune = 1 −
 *         (innate_rows_ok + innate_all_blocked_ok +
 *          innate_all_instant_ok +
 *          adaptive_rows_ok + adaptive_all_learned_ok +
 *          adaptive_faster_ok +
 *          adaptive_cross_ok +
 *          memory_rows_ok + memory_recog_polarity_ok +
 *          memory_count_ok +
 *          auto_rows_ok + auto_order_ok +
 *          auto_verdict_ok + auto_healthy_range_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1 +
 *          3 + 1 + 1 + 1)
 *   v0 requires `σ_immune == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 innate patterns; all blocked; none require
 *        training; all INSTANT.
 *     2. 3 adaptive rows; all learned; exactly 1
 *        faster; exactly 1 cross-recognised.
 *     3. 3 memory rows; recognised iff FAST;
 *        exactly 1 recognised.
 *     4. 3 autoimmune scenarios; τ strictly
 *        increasing; 3 distinct verdicts; HEALTHY
 *        iff τ ∈ [0.10, 0.60] AND fpr ≤ 0.10.
 *     5. σ_immune ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v295.1 (named, not implemented): live σ-log
 *     driving real memory cells, an automatic
 *     calibrator that slides τ to stay HEALTHY
 *     over time, and a cross-variant recogniser
 *     wired to the v278 recursive-self-improve loop.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V295_IMMUNE_H
#define COS_V295_IMMUNE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V295_N_INNATE   3
#define COS_V295_N_ADAPT    3
#define COS_V295_N_MEMORY   3
#define COS_V295_N_AUTO     3

#define COS_V295_TAU_INNATE   0.70f
#define COS_V295_TAU_LO       0.10f
#define COS_V295_TAU_HI       0.60f
#define COS_V295_FPR_BUDGET   0.10f

typedef struct {
    char  pattern[18];
    float sigma_raw;
    bool  blocked;
    bool  requires_training;
    char  response_tier[10];
} cos_v295_innate_t;

typedef struct {
    char  row[28];
    bool  learned;
    bool  faster_on_repeat;
    bool  cross_recognized;
} cos_v295_adapt_t;

typedef struct {
    char  entry[26];
    bool  recognised;
    char  tier[6];
} cos_v295_memory_t;

typedef struct {
    char  scenario[16];
    float tau;
    float false_positive_rate;
    char  verdict[18];
} cos_v295_auto_t;

typedef struct {
    cos_v295_innate_t   innate   [COS_V295_N_INNATE];
    cos_v295_adapt_t    adapt    [COS_V295_N_ADAPT];
    cos_v295_memory_t   memory   [COS_V295_N_MEMORY];
    cos_v295_auto_t     autoprev [COS_V295_N_AUTO];

    int   n_innate_rows_ok;
    bool  innate_all_blocked_ok;
    bool  innate_all_instant_ok;

    int   n_adaptive_rows_ok;
    bool  adaptive_all_learned_ok;
    bool  adaptive_faster_ok;
    bool  adaptive_cross_ok;

    int   n_memory_rows_ok;
    bool  memory_recog_polarity_ok;
    bool  memory_count_ok;

    int   n_auto_rows_ok;
    bool  auto_order_ok;
    bool  auto_verdict_ok;
    bool  auto_healthy_range_ok;

    float sigma_immune;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v295_state_t;

void   cos_v295_init(cos_v295_state_t *s, uint64_t seed);
void   cos_v295_run (cos_v295_state_t *s);

size_t cos_v295_to_json(const cos_v295_state_t *s,
                         char *buf, size_t cap);

int    cos_v295_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V295_IMMUNE_H */
