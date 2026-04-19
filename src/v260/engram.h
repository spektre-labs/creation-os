/*
 * v260 σ-Engram — DeepSeek Engram integration.
 *
 *   DeepSeek Engram splits static knowledge (facts,
 *   syntax, entities) from dynamic reasoning (logic,
 *   analogy, creativity).  Static is O(1) hash-lookup
 *   in DRAM; dynamic is an MoE layer on GPU.  Creation
 *   OS adds a σ on top so "Engram returned a hit" is
 *   never the last word.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Parameter allocation (DeepSeek's split):
 *     static_pct  ∈ [20, 25]
 *     dynamic_pct ∈ [75, 80]
 *     sum == 100
 *
 *   Static-knowledge lookups (exactly 5, O(1) hash):
 *     Each row: `entity`, `hash` (non-zero), `hit ==
 *     true`, `sigma_result ∈ [0, 1]`, `lookup_ns ≤ 100`
 *     (O(1) budget — DRAM hash-table lookup).  Any
 *     lookup_ns above the budget fails the gate.
 *
 *   Dynamic-reasoning rows (exactly 3, MoE layer):
 *     Each row: `task`, `experts_activated > 0`,
 *     `sigma_result ∈ [0, 1]`.  Reasoning is typed but
 *     the σ is the reasoning reliability gate.
 *
 *   σ-gated lookup decision (exactly 4 fixtures):
 *     Rule: `σ_result ≤ τ_fact (0.30)` → `USE`; else →
 *     `VERIFY` (escalate to reasoning layer).  v0
 *     fixture exercises both branches: ≥ 1 USE AND ≥ 1
 *     VERIFY.
 *
 *   Long-context σ (Needle-in-a-Haystack @ 1 M tokens):
 *     hit_rate_pct == 97, miss_rate_pct == 3 (DeepSeek
 *     Engram reported behaviour); every miss is
 *     σ-flagged AND `miss_flagged_by_sigma == true`.
 *     Without σ, 3 % miss is silent.  With σ, the 3 %
 *     is surfaced — Engram + σ = 99 %+ reliable long
 *     context.
 *
 *   σ_engram (surface hygiene):
 *       σ_engram = 1 −
 *         (static_lookups_ok + dynamic_rows_ok +
 *          gate_paths_ok + long_context_ok +
 *          parameter_split_ok) /
 *         (5 + 3 + 4 + 1 + 1)
 *   v0 requires `σ_engram == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 5 static-knowledge rows with hash ≠ 0,
 *        hit == true, σ ∈ [0, 1], lookup_ns ≤ 100.
 *     2. Exactly 3 dynamic-reasoning rows with
 *        experts_activated > 0 and σ ∈ [0, 1].
 *     3. Exactly 4 σ-gate fixtures: decision matches σ
 *        vs τ_fact; ≥ 1 USE AND ≥ 1 VERIFY.
 *     4. Long-context hit_rate_pct == 97 AND
 *        miss_rate_pct == 3 AND miss_flagged_by_sigma.
 *     5. Parameter split: static_pct ∈ [20, 25],
 *        dynamic_pct ∈ [75, 80], sum == 100.
 *     6. σ_engram ∈ [0, 1] AND σ_engram == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v260.1 (named, not implemented): live DRAM hash-
 *     table backed Engram, live MoE reasoning layer
 *     wired through v243 pipeline, v227 entropy
 *     decomposition driving the knowledge/reasoning
 *     split at inference time, live NIAH harness
 *     producing hit_rate_pct from measured runs.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V260_ENGRAM_H
#define COS_V260_ENGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V260_N_STATIC     5
#define COS_V260_N_DYNAMIC    3
#define COS_V260_N_GATE       4

typedef enum {
    COS_V260_GATE_USE    = 1,
    COS_V260_GATE_VERIFY = 2
} cos_v260_gate_dec_t;

typedef struct {
    char      entity[28];
    uint64_t  hash;
    bool      hit;
    float     sigma_result;
    int       lookup_ns;
} cos_v260_static_t;

typedef struct {
    char  task[28];
    int   experts_activated;
    float sigma_result;
} cos_v260_dynamic_t;

typedef struct {
    char                  query[28];
    float                 sigma_result;
    cos_v260_gate_dec_t   decision;
} cos_v260_gate_t;

typedef struct {
    int   hit_rate_pct;
    int   miss_rate_pct;
    bool  miss_flagged_by_sigma;
    bool  ok;
} cos_v260_longctx_t;

typedef struct {
    int   static_pct;
    int   dynamic_pct;
    bool  split_ok;
} cos_v260_split_t;

typedef struct {
    cos_v260_split_t    split;
    cos_v260_static_t   statics [COS_V260_N_STATIC];
    cos_v260_dynamic_t  dynamics[COS_V260_N_DYNAMIC];
    cos_v260_gate_t     gate    [COS_V260_N_GATE];
    cos_v260_longctx_t  longctx;

    float tau_fact;
    int   lookup_ns_budget;

    int   n_static_ok;
    int   n_dynamic_ok;
    int   n_use;
    int   n_verify;

    float sigma_engram;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v260_state_t;

void   cos_v260_init(cos_v260_state_t *s, uint64_t seed);
void   cos_v260_run (cos_v260_state_t *s);

size_t cos_v260_to_json(const cos_v260_state_t *s,
                         char *buf, size_t cap);

int    cos_v260_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V260_ENGRAM_H */
