/*
 * v247 σ-Benchmark-Suite — comprehensive test suite.
 *
 *   v143 benchmark is a single measurement kernel;
 *   v247 is the production-grade suite across four
 *   categories:
 *     1. correctness (unit / integration / e2e /
 *        regression)
 *     2. performance (latency p50/p95/p99 / throughput
 *        / memory / σ-overhead)
 *     3. cognitive (TruthfulQA / calibration /
 *        consistency / adversarial)
 *     4. comparative (vs raw llama.cpp, vs OpenAI API)
 *
 *   CI / CD targets (exactly 3):
 *     make test    — every correctness test
 *     make bench   — every performance + cognitive
 *     make verify  — Frama-C + TLA+ + merge-gate
 *
 *   σ-overhead contract:
 *     The cost of σ-aggregation on top of raw inference
 *     must stay below `tau_overhead = 0.05` (5 %).  v0
 *     fixture measures 3.2 %.
 *
 *   Consistency contract:
 *     The same question asked 10 times must produce
 *     the same answer (`n_stable_answers == n_trials`).
 *
 *   Cognitive numbers (v0 fixture):
 *     TruthfulQA  : accuracy_answered = 0.78,
 *                   abstention_rate   = 0.15
 *     Calibration : expected calibration error 0.04
 *     Consistency : 10/10 identical answers
 *     Adversarial : red-team pass 20/20
 *
 *   Comparative (v0 fixture):
 *     creation_os_vs_raw_llama : σ_product 0.18 vs 0.41,
 *                                 abstain  0.15 vs 0.00
 *     creation_os_vs_openai_api: σ_product 0.18 vs "n/a",
 *                                 abstain  0.15 vs 0.02
 *
 *   σ_suite (overall pass rate):
 *       σ_suite = 1 − passing_tests / total_tests
 *     v0 requires `σ_suite == 0.0` (100 % pass).
 *
 *   Contracts (v0):
 *     1. Exactly 4 categories in canonical order.
 *     2. correctness: 4 tests (unit / integration / e2e /
 *        regression), every `pass == true`.
 *     3. performance: p50 ≤ p95 ≤ p99; throughput_rps > 0;
 *        σ-overhead < tau_overhead.
 *     4. cognitive: consistency `n_stable == n_trials`;
 *        TruthfulQA accuracy_answered ∈ [0, 1];
 *        abstention_rate ∈ [0, 1];
 *        adversarial_pass / adversarial_total == 1.0.
 *     5. comparative: both rows present, names match.
 *     6. CI / CD: exactly 3 targets (`make test`,
 *        `make bench`, `make verify`).
 *     7. σ_suite ∈ [0, 1] AND σ_suite == 0.0.
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v247.1 (named, not implemented): live harness
 *     wire-up — run the actual ARC / MMLU / HumanEval
 *     / TruthfulQA sets and lift P-tier categories in
 *     v243 to M-tier.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V247_BENCHMARK_SUITE_H
#define COS_V247_BENCHMARK_SUITE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V247_N_CATEGORIES 4
#define COS_V247_N_CORRECT    4
#define COS_V247_N_COMPARE    2
#define COS_V247_N_CI_TARGETS 3

typedef struct {
    char name[24];
    bool pass;
} cos_v247_correct_t;

typedef struct {
    float  latency_p50_ms;
    float  latency_p95_ms;
    float  latency_p99_ms;
    float  throughput_rps;
    int    peak_rss_mb;
    float  sigma_overhead;      /* σ-compute cost / total */
    float  tau_overhead;        /* 0.05 */
    bool   overhead_ok;
} cos_v247_perf_t;

typedef struct {
    float accuracy_answered;    /* TruthfulQA */
    float abstention_rate;
    float calibration_ece;      /* expected calibration error */
    int   consistency_trials;
    int   consistency_stable;
    int   adversarial_total;
    int   adversarial_pass;
} cos_v247_cog_t;

typedef struct {
    char  name[40];
    float cos_sigma;
    float other_sigma;          /* use negative value when "n/a" */
    float cos_abstain;
    float other_abstain;
    bool  other_has_sigma;
} cos_v247_compare_t;

typedef struct {
    char category_names[COS_V247_N_CATEGORIES][24];
    cos_v247_correct_t    correctness[COS_V247_N_CORRECT];
    cos_v247_perf_t       performance;
    cos_v247_cog_t        cognitive;
    cos_v247_compare_t    comparative[COS_V247_N_COMPARE];
    char                  ci_targets [COS_V247_N_CI_TARGETS][16]; /* test/bench/verify */

    int                   n_passing_tests;
    int                   n_total_tests;
    float                 sigma_suite;

    bool                  chain_valid;
    uint64_t              terminal_hash;
    uint64_t              seed;
} cos_v247_state_t;

void   cos_v247_init(cos_v247_state_t *s, uint64_t seed);
void   cos_v247_run (cos_v247_state_t *s);

size_t cos_v247_to_json(const cos_v247_state_t *s,
                         char *buf, size_t cap);

int    cos_v247_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V247_BENCHMARK_SUITE_H */
