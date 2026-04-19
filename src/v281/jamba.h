/*
 * v281 σ-Jamba — hybrid Transformer · Mamba · MoE with
 *                per-layer σ adaptation.
 *
 *   AI21's Jamba composes Transformer, Mamba, and MoE
 *   layers in a fixed schedule and ships a 2.5×
 *   throughput win on long contexts.  Creation OS types
 *   the σ-layer on top:
 *     * layer types and complexity are a canonical
 *       contract;
 *     * per-context chosen architecture is σ-adaptive
 *       (easy → Mamba, hard → Transformer, factual →
 *       MoE, long → Mamba);
 *     * a 5-tier memory hierarchy (engram · mamba · ttt
 *       · transformer · moe) is typed in canonical slot
 *       order;
 *     * the unified bench predicts σ_jamba ≤
 *       σ_baseline on every measured dimension.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Layer types (exactly 3, canonical order):
 *     `mamba` (LINEAR) · `transformer` (QUADRATIC) ·
 *     `moe` (SPARSE), each name non-empty.
 *     Contract: names canonical, complexities canonical,
 *     all three distinct.
 *
 *   Adaptive mixing (exactly 4 contexts, canonical
 *   order `easy`, `hard`, `factual`, `long`):
 *     Each: `label`, `σ_difficulty ∈ [0, 1]`,
 *     `chosen ∈ {MAMBA, TRANSFORMER, MOE}`.
 *     Canonical mapping:
 *       easy    → MAMBA       (σ low, linear fast)
 *       hard    → TRANSFORMER (σ high, attention)
 *       factual → MOE         (dispatch to expert)
 *       long    → MAMBA       (linear for long ctx)
 *     Contract: ≥ 2 distinct archs chosen across
 *     contexts (a regression that always picks one
 *     arch fails).
 *
 *   Memory hierarchy (exactly 5 tiers, canonical
 *   order):
 *     `engram` · `mamba` · `ttt` · `transformer` ·
 *     `moe`, each `enabled == true`, `tier_slot ∈
 *     [1..5]` matching the canonical permutation.
 *     Contract: all enabled AND tier_slot permutation
 *     equals [1,2,3,4,5] exactly.
 *
 *   Unified bench (exactly 3 metrics, canonical
 *   order):
 *     `accuracy` · `latency` · `throughput`, each with
 *     `sigma_baseline ∈ [0, 1]` and `sigma_jamba ∈
 *     [0, 1]`.
 *     Contract: sigma_jamba ≤ sigma_baseline for every
 *     metric (σ-Jamba is at least as well-calibrated
 *     as plain Jamba on every dimension).
 *
 *   σ_jamba (surface hygiene):
 *       σ_jamba = 1 −
 *         (layer_rows_ok + layer_complexity_ok +
 *          layer_distinct_ok +
 *          mixing_rows_ok + mixing_distinct_ok +
 *          memory_rows_ok + memory_order_ok +
 *          bench_rows_ok + bench_monotone_ok) /
 *         (3 + 1 + 1 + 4 + 1 + 5 + 1 + 3 + 1)
 *   v0 requires `σ_jamba == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 layers canonical with canonical complexities
 *        all distinct.
 *     2. 4 mixing rows canonical with ≥ 2 distinct
 *        archs chosen.
 *     3. 5 memory tiers canonical with tier_slot
 *        permutation [1..5].
 *     4. 3 bench rows canonical with σ_jamba ≤
 *        σ_baseline per metric.
 *     5. σ_jamba ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v281.1 (named, not implemented): live Jamba-style
 *     hybrid engine wired into v262 with σ-adaptive
 *     layer routing per token, measured accuracy /
 *     latency / throughput vs the fixed-schedule
 *     baseline, and a unified σ-memory bus across all
 *     five tiers.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V281_JAMBA_H
#define COS_V281_JAMBA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V281_N_LAYER   3
#define COS_V281_N_MIXING  4
#define COS_V281_N_MEMORY  5
#define COS_V281_N_BENCH   3

typedef enum {
    COS_V281_CMP_LINEAR    = 1,
    COS_V281_CMP_QUADRATIC = 2,
    COS_V281_CMP_SPARSE    = 3
} cos_v281_cmp_t;

typedef enum {
    COS_V281_ARCH_MAMBA       = 1,
    COS_V281_ARCH_TRANSFORMER = 2,
    COS_V281_ARCH_MOE         = 3
} cos_v281_arch_t;

typedef struct {
    char            name[12];
    cos_v281_cmp_t  complexity;
} cos_v281_layer_t;

typedef struct {
    char             label[10];
    float            sigma_difficulty;
    cos_v281_arch_t  chosen;
} cos_v281_mixing_t;

typedef struct {
    char  name[12];
    bool  enabled;
    int   tier_slot;
} cos_v281_memory_t;

typedef struct {
    char   metric[12];
    float  sigma_baseline;
    float  sigma_jamba;
} cos_v281_bench_t;

typedef struct {
    cos_v281_layer_t    layer  [COS_V281_N_LAYER];
    cos_v281_mixing_t   mixing [COS_V281_N_MIXING];
    cos_v281_memory_t   memory [COS_V281_N_MEMORY];
    cos_v281_bench_t    bench  [COS_V281_N_BENCH];

    int   n_layer_rows_ok;
    bool  layer_complexity_ok;
    bool  layer_distinct_ok;

    int   n_mixing_rows_ok;
    int   n_distinct_arch;
    bool  mixing_distinct_ok;

    int   n_memory_rows_ok;
    bool  memory_order_ok;

    int   n_bench_rows_ok;
    bool  bench_monotone_ok;

    float sigma_jamba;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v281_state_t;

void   cos_v281_init(cos_v281_state_t *s, uint64_t seed);
void   cos_v281_run (cos_v281_state_t *s);

size_t cos_v281_to_json(const cos_v281_state_t *s,
                         char *buf, size_t cap);

int    cos_v281_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V281_JAMBA_H */
