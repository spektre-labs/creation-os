/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v120 σ-Distill — σ-targeted knowledge transfer from big→small.
 *
 * Classical knowledge distillation copies every (input, output) pair
 * from a teacher to a student.  That's wasteful: most pairs are
 * already easy for the student.  v120 uses σ to pick the *opportunity
 * rows* — records where the teacher is confident (σ_big < τ_low) and
 * the student is uncertain (σ_small > τ_high).  Those are the rows
 * where fine-tuning should pay off.
 *
 * v120.0 (this file) is a pure-C selector + manifest writer.  Input
 * is a line-delimited JSON corpus with one row per prompt:
 *
 *   {"id":"...", "prompt":"...",
 *    "big_response":"...", "small_response":"...",
 *    "sigma_big":0.12, "sigma_small":0.71}
 *
 * Output is a SFT dataset (again JSONL) keeping only the rows that
 * satisfy the σ-targeting rule, plus a one-line JSON manifest with
 * counts, means, and the σ thresholds used.  The manifest is what
 * `make check-v120` greps; the JSONL is what v120.1 feeds to MLX
 * `mlx_lm.lora` (see docs/v120/README.md).  No training in v120.0 —
 * that's deferred so the merge-gate stays weights-free.
 */
#ifndef COS_V120_DISTILL_H
#define COS_V120_DISTILL_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V120_TAU_BIG_LOW_DEFAULT    0.30f
#define COS_V120_TAU_SMALL_HIGH_DEFAULT 0.50f
#define COS_V120_MAX_FIELD              8192   /* per-string cap */

typedef struct cos_v120_config {
    float tau_big_low;     /* keep if σ_big  < tau_big_low */
    float tau_small_high;  /* keep if σ_small > tau_small_high */
    int   max_rows;        /* 0 = unlimited */
} cos_v120_config_t;

typedef struct cos_v120_stats {
    int rows_read;
    int rows_malformed;
    int rows_kept;
    int rows_rejected_sigma_big_too_high;
    int rows_rejected_sigma_small_too_low;
    int rows_rejected_both;
    double sum_sigma_big;       /* over kept rows */
    double sum_sigma_small;     /* over kept rows */
} cos_v120_stats_t;

/* Fill defaults (τ_big_low=0.30, τ_small_high=0.50, max_rows=0). */
void cos_v120_config_defaults(cos_v120_config_t *cfg);

/* Select rows from stdin-style JSONL streams.  Reads line-by-line
 * from `in_path`, writes kept rows as compact JSONL to `out_path`,
 * and fills `stats`.  A kept row is rewritten in SFT form:
 *
 *   {"id":"...","prompt":"...","response":"<big_response>",
 *    "sigma_big":X,"sigma_small":Y,"source":"v120-sigma-distill"}
 *
 * Returns 0 on success, nonzero on I/O error. */
int cos_v120_select(const cos_v120_config_t *cfg,
                    const char *in_path,
                    const char *out_path,
                    cos_v120_stats_t *stats);

/* One-line JSON manifest; returns bytes written (excluding NUL) or
 * negative on overflow. */
int cos_v120_stats_to_json(const cos_v120_config_t *cfg,
                           const cos_v120_stats_t *stats,
                           char *out, size_t cap);

/* Pure-C self-test.  Writes a synthetic 8-row JSONL corpus to a temp
 * file, runs the selector with defaults, verifies:
 *   - exactly the σ-targeted rows survive (3/8 by construction)
 *   - stats counts match
 *   - output JSONL is parseable and contains the teacher response
 *   - manifest JSON has required keys
 * Returns 0 on pass. */
int cos_v120_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
