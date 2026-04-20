/*
 * σ-Split — distributed inference across a layer pipeline with
 * σ-gated early exit.
 *
 * A large model's forward pass is partitioned into contiguous
 * layer ranges and mapped to nodes:
 *
 *     Stage 0  layers  0..10   → node A   (embedding + early)
 *     Stage 1  layers 11..20   → node B   (middle)
 *     Stage 2  layers 21..32   → node C   (late + head)
 *
 * A token's hidden state travels A → B → C.  Each stage runs its
 * layers, then stamps a σ_layer on the partial answer.  If the
 * accumulated σ drops below τ_exit at or after `min_stages`, the
 * pipeline exits early: B and C never run, the round-trip count
 * drops with it, and the caller gets the confident answer straight
 * from the early stage.
 *
 * Contracts (v0):
 *   1. init rejects NULL storage / cap < 1 / malformed thresholds.
 *   2. assign() enforces ordering: layer_start ≤ layer_end, and
 *      each new stage's layer_start == previous layer_end + 1
 *      (or 0 for the first stage).  Duplicate ranges → −4.
 *   3. run() advances stages in order; early-exit only fires when
 *      `stages_run > min_stages` (i.e. at least min_stages+1 have
 *      produced a σ) AND current σ < τ_exit.  `min_stages = 0`
 *      lets the very first stage short-circuit.
 *   4. stage_fn returning non-zero aborts the pipeline; report
 *      captures the last completed stage and marks error=1.
 *
 * Why this works: σ is a reliability signal, not a logit.  If the
 * early layers have already committed to an answer with σ below
 * the ABSTAIN floor, running the deeper layers cannot legitimately
 * change that answer — only wallclock and network are at stake.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SPLIT_H
#define COS_SIGMA_SPLIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SPLIT_NODE_ID_CAP 32

typedef struct {
    int   stage_idx;
    int   layer_start;
    int   layer_end;
    char  node_id[COS_SPLIT_NODE_ID_CAP];
    float sigma_layer;     /* σ stamped after this stage (0..1) */
    float latency_ms;
    int   executed;
} cos_split_stage_t;

/* Stage callback.  Signals the kernel to run layers
 * [layer_start, layer_end] on `node_id`.  The callback returns
 * the σ for the partial answer in *out_sigma and the wall time
 * in *out_latency_ms.  Return 0 on success, non-zero on transport
 * failure — the pipeline aborts and report.error is raised. */
typedef int (*cos_split_stage_fn)(int   stage_idx,
                                  int   layer_start,
                                  int   layer_end,
                                  const char *node_id,
                                  void *ctx,
                                  float *out_sigma,
                                  float *out_latency_ms);

typedef struct {
    float              tau_exit;     /* σ < τ_exit → exit        */
    int                min_stages;   /* #completed stages before
                                      * early exit may fire       */
    cos_split_stage_fn stage_fn;
    void              *ctx;
} cos_split_config_t;

typedef struct {
    int   stages_run;
    int   early_exit;        /* 1 if we short-circuited           */
    int   exit_stage_idx;    /* index of the stage that ended the
                              * pipeline (last stage in either
                              * path)                              */
    float final_sigma;
    float total_latency_ms;
    int   error;
} cos_split_report_t;

/* Pre-wire a set of stages.  `stages` storage is caller-owned and
 * remains valid for the lifetime of the pipeline. */
int   cos_sigma_split_init(cos_split_stage_t *stages,
                           int cap,
                           int *out_count /* set to 0 on success */);

int   cos_sigma_split_assign(cos_split_stage_t *stages, int cap, int *count,
                             const char *node_id,
                             int layer_start, int layer_end);

/* Execute the pipeline in order, respecting τ_exit.  Returns 0 on
 * success (including early exit), <0 on argument error, and >0 on
 * stage_fn abort (the same rc returned from stage_fn). */
int   cos_sigma_split_run(const cos_split_config_t *cfg,
                          cos_split_stage_t *stages,
                          int n_stages,
                          cos_split_report_t *out_report);

int   cos_sigma_split_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SPLIT_H */
