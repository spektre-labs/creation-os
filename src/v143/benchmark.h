/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v143 σ-Benchmark — 5-category benchmark suite for Creation OS.
 *
 * Unlike external LLM benchmarks, v143 measures what Creation OS is
 * actually for: the quality of its σ signal and its consequences.
 *
 *   1. σ-Calibration — does σ_product correlate with wrongness?
 *      Expected Calibration Error (ECE) across 10 probability bins.
 *   2. σ-Abstention  — when the model *cannot* know, does it
 *      abstain?  Reports coverage@95%_accuracy on an oracle-
 *      unknowable set.
 *   3. σ-Swarm       — does the σ-router pick the right specialist
 *      out of N?  Reports routing accuracy + σ-spread across
 *      specialists (bigger spread = more informative router).
 *   4. σ-Learning    — after a continual-learning pass, does σ
 *      shrink on the targeted set without growing on the held-out
 *      set (no forgetting)?  Reports before/after ΔAccuracy.
 *   5. σ-Adversarial — on prompt-injection / jailbreak synthetic
 *      attempts, does σ rise above baseline?  Reports
 *      detection-rate@FPR≤5%.
 *
 * v143.0 is tier-0: every benchmark runs on deterministic synthetic
 * data generated from a seeded PRNG.  The scores therefore measure
 * the benchmark infrastructure itself (metric math + JSON writer +
 * CI hookup), not a live model.  v143.1 replaces each synthetic set
 * with archived σ-traces from v104/v106 and publishes the whole
 * bundle to `spektre-labs/cos-benchmark` on Hugging Face.
 *
 * Output: a single JSON object per-run at
 * `benchmarks/v143/creation_os_benchmark.json` with a stable
 * top-level shape so downstream tooling (v133 meta-dashboard) can
 * consume it without parsing changes.
 */
#ifndef COS_V143_BENCHMARK_H
#define COS_V143_BENCHMARK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_v143_result {
    /* 1. calibration */
    double calibration_ece;          /* lower is better ([0, 1])    */
    int    calibration_n;
    /* 2. abstention */
    double abstention_coverage;      /* fraction answered           */
    double abstention_accuracy;      /* accuracy on answered        */
    int    abstention_n;
    /* 3. swarm routing */
    double swarm_routing_accuracy;   /* [0, 1]                      */
    double swarm_sigma_spread;       /* mean (σ_max − σ_min) / σ_max*/
    int    swarm_n;
    /* 4. continual learning */
    double learning_accuracy_before;
    double learning_accuracy_after;
    double learning_sigma_before;
    double learning_sigma_after;
    double learning_holdout_drop;    /* held-out acc. change        */
    int    learning_n;
    /* 5. adversarial */
    double adv_detection_rate;       /* @FPR ≤ 5 %                  */
    double adv_fpr;
    int    adv_n;

    uint64_t seed;
    int      version;                /* bump on shape change        */
} cos_v143_result_t;

/* Run all five benchmarks with a deterministic seed. */
int   cos_v143_run_all(uint64_t seed, cos_v143_result_t *out);

/* Serialize the result into the canonical JSON shape. */
int   cos_v143_to_json(const cos_v143_result_t *r, char *buf, size_t cap);

/* Write the JSON to `path`.  Creates parent dirs as needed is
 * outside the scope of this kernel — the caller should `mkdir -p`. */
int   cos_v143_write_json(const cos_v143_result_t *r, const char *path);

int   cos_v143_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
