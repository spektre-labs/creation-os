/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v141 σ-Curriculum — self-directed learning scheduler.
 *
 * v124 `continual` learns from actual usage.  v141 learns
 * *strategically*: the model inspects its own σ-per-topic
 * histogram, identifies the weakest topic, generates targeted
 * training pairs (via a synthetic answer function for v141.0,
 * via v114 swarm in v141.1), simulates one "micro-tune" step,
 * and re-measures σ on the same topic to prove improvement.
 *
 * v141.0 is deliberately minimal:
 *
 *   - Topic set is a fixed small roster (math, code, history, …).
 *   - σ per topic is a single scalar in (0, 1].
 *   - Weakness detection = argmax σ.
 *   - "Training" is a deterministic exponential decay of σ on the
 *     target topic with a configurable improvement-rate α:
 *        σ_new = σ_old · (1 − α)   (clamped to [eps, 1])
 *     Strong topics (low σ) are NOT retrained, so they suffer no
 *     forgetting (their σ stays exactly constant — a core v141
 *     contract tested in the merge-gate).
 *   - Full cycle: identify → (pretend to) generate data → train →
 *     measure → report.
 *
 * v141.1 (deferred): wire v114 swarm for data generation, v124
 * MLX LoRA for real micro-tuning, v125 DPO pair-labelling, and a
 * long-horizon curriculum scheduler that tracks per-topic σ
 * trajectories in v133 meta-dashboard.
 */
#ifndef COS_V141_CURRICULUM_H
#define COS_V141_CURRICULUM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V141_MAX_TOPICS  16
#define COS_V141_NAME_CAP    32

typedef struct cos_v141_topic {
    char   name[COS_V141_NAME_CAP];
    float  sigma;        /* current σ for the topic, [0, 1]          */
    int    n_measured;   /* observation count (for honest reporting) */
} cos_v141_topic_t;

typedef struct cos_v141_state {
    int              n_topics;
    cos_v141_topic_t topics[COS_V141_MAX_TOPICS];
    /* snapshot of σ at the start of the most recent cycle (for
     * before/after comparison), parallel to topics[].           */
    float            sigma_before[COS_V141_MAX_TOPICS];
} cos_v141_state_t;

typedef struct cos_v141_cycle_report {
    int    weakest_index;
    char   weakest_name[COS_V141_NAME_CAP];
    float  sigma_before;
    float  sigma_after;
    float  improvement;     /* sigma_before − sigma_after            */
    int    strong_topics_stable;  /* 1 iff no forgetting detected    */
    float  max_forgetting;        /* max σ-increase on strong topics */
} cos_v141_cycle_report_t;

/* Initialise state with a simple five-topic roster and baseline σ
 * values.  Used by the self-test and CLI demo. */
void  cos_v141_init_default(cos_v141_state_t *s);

/* Replace the topic roster (used by the CLI `--topics`).  Each
 * name is copied up to 31 chars + NUL, and σ is clamped to
 * (0, 1]. */
int   cos_v141_load(cos_v141_state_t *s,
                    const char *const *names,
                    const float *sigmas, int n);

/* Identify the current weakness (topic with max σ).  Returns
 * -1 if state is empty. */
int   cos_v141_weakest(const cos_v141_state_t *s);

/* Run a single curriculum cycle:
 *   1) identify weakest topic,
 *   2) snapshot all σ into sigma_before,
 *   3) "train" by decaying the weakest topic's σ by factor α,
 *   4) assert strong topics' σ did not rise (no forgetting),
 *   5) fill the report.
 * Returns 0 on success, <0 on error. */
int   cos_v141_cycle(cos_v141_state_t *s,
                     float improvement_rate /* α, 0 < α ≤ 1 */,
                     cos_v141_cycle_report_t *out);

/* JSON report of a cycle. */
int   cos_v141_cycle_to_json(const cos_v141_cycle_report_t *r,
                             const cos_v141_state_t *s,
                             char *buf, size_t cap);

int   cos_v141_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
