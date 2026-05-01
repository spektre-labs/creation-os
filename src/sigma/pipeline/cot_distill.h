/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-CoT-Distill — chain-of-thought distillation with per-step σ.
 *
 * σ-distill (NEXT-1) teaches the student the teacher's *answer*.
 * σ-CoT-distill teaches the student the teacher's *reasoning*:
 * the step sequence that produced the answer, annotated with a σ
 * per step.  The student learns not just "what" to answer but
 * when to pause, when to rethink, and why.
 *
 * Data model
 * ----------
 *   input
 *     raw prompt
 *   teacher_cot
 *     the full chain of thought as a single string
 *   steps[]
 *     parsed reasoning steps; each has body + σ
 *   rethink_points[]
 *     indices into steps[] where σ ≥ τ_rethink, indicating the
 *     teacher paused and reconsidered
 *   teacher_answer
 *     the final answer string (may be empty)
 *
 * Parsing
 * -------
 *   The parser splits the CoT on a small set of markers
 *   (newlines, "Step N:", "Thought:", "Wait,"), clip01s each
 *   step's σ if supplied, and marks every step whose σ ≥
 *   τ_rethink as a rethink point.  Unsupplied σ per step defaults
 *   to 0.0 (the teacher was confident at that step).
 *
 * Reinforces P1 RETHINK
 * ---------------------
 *   The student does not rethink at random; it rethinks at the
 *   same places the teacher rethought.  Training the student's
 *   τ-calibration on the teacher's σ-per-step sequence is what
 *   σ-CoT-distill records.  A σ_cot_value aggregates the signal:
 *
 *       σ_cot_value = mean(max(0, σ_step - τ_confident))
 *
 *   where τ_confident = 0.20.  A teacher whose reasoning σ stays
 *   near 0 everywhere contributes very little information (value
 *   near 0); a teacher with clear rethink spikes contributes a
 *   larger value.  This is *per-pair*; the module tracks stats
 *   across pairs just like σ-distill.
 *
 * Contracts (v0)
 * --------------
 *   1. Zero-copy after append: strings are copied on ingest; the
 *      caller may free its originals immediately.
 *   2. Determinism: parse_cot is a pure function of (cot, σ_seq,
 *      τ_rethink).
 *   3. σ bounds: every recorded σ is clip01'd.
 *   4. Bounded step count: at most COS_COT_MAX_STEPS per pair.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_COT_DISTILL_H
#define COS_SIGMA_COT_DISTILL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_COT_MAX_STEPS
#define COS_COT_MAX_STEPS   16
#endif

#ifndef COS_COT_RING_CAP
#define COS_COT_RING_CAP    256
#endif

#ifndef COS_COT_TAU_RETHINK_DEFAULT
#define COS_COT_TAU_RETHINK_DEFAULT 0.50f
#endif

#ifndef COS_COT_TAU_CONFIDENT
#define COS_COT_TAU_CONFIDENT       0.20f
#endif

enum cos_cot_status {
    COS_COT_OK          =  0,
    COS_COT_ERR_ARG     = -1,
    COS_COT_ERR_OOM     = -2,
    COS_COT_ERR_PARSE   = -3,
};

/* Caller input: optional explicit σ-per-step sequence.  Pass
 * sigma_seq = NULL to let the parser default every step's σ to
 * 0.0; pass n_sigma < parsed step count to default the tail. */
typedef struct {
    const char *input;
    const char *teacher_cot;
    const char *teacher_answer;
    const float *sigma_seq;  /* length n_sigma, may be NULL        */
    int         n_sigma;
    uint64_t    timestamp_ns;
} cos_sigma_cot_pair_t;

typedef struct {
    char  *body;         /* owned copy of the step's text          */
    float  sigma;        /* clip01'd σ at this step                */
    int    is_rethink;   /* σ ≥ τ_rethink                          */
} cos_sigma_cot_step_t;

typedef struct {
    char  *input;
    char  *teacher_cot;
    char  *teacher_answer;
    cos_sigma_cot_step_t steps[COS_COT_MAX_STEPS];
    int    n_steps;
    int    n_rethink;
    float  sigma_cot_value;     /* mean of max(0, σ_i − τ_conf)    */
    float  sigma_max_step;      /* max σ_i                          */
    uint64_t timestamp_ns;
    uint64_t insertion_order;
} cos_sigma_cot_record_t;

typedef struct {
    uint64_t total_seen;
    uint64_t evicted;
    uint64_t rethink_points_total;
    float    mean_cot_value;
    float    mean_max_step;
} cos_sigma_cot_stats_t;

typedef struct {
    cos_sigma_cot_record_t ring[COS_COT_RING_CAP];
    int      size;
    int      head;
    float    tau_rethink;
    float    tau_confident;
    uint64_t insertion_counter;
    cos_sigma_cot_stats_t stats;
} cos_sigma_cot_state_t;

/* -------- Lifecycle ------------------------------------------------ */
int  cos_sigma_cot_init(cos_sigma_cot_state_t *st,
                        float tau_rethink, float tau_confident);
void cos_sigma_cot_free(cos_sigma_cot_state_t *st);

/* -------- Pure helpers -------------------------------------------- */

/* Parse `cot` into steps under the module's default step markers.
 * `out_steps` is caller-owned of capacity `cap`; strings written
 * into each step must be freed by the caller with free().  `σ_seq`
 * supplies σ per step; missing entries default to 0.0.  Returns
 * the number of steps parsed (0..cap). */
int cos_sigma_cot_parse(const char *cot,
                        const float *sigma_seq, int n_sigma,
                        float tau_rethink,
                        cos_sigma_cot_step_t *out_steps,
                        int cap);

/* σ_cot_value aggregate. */
float cos_sigma_cot_value(const cos_sigma_cot_step_t *steps,
                          int n_steps, float tau_confident);

/* -------- Ingest -------------------------------------------------- */
int cos_sigma_cot_append(cos_sigma_cot_state_t *st,
                         const cos_sigma_cot_pair_t *pair);

/* -------- Read / stats -------------------------------------------- */
const cos_sigma_cot_record_t *
cos_sigma_cot_at(const cos_sigma_cot_state_t *st, int idx);

void cos_sigma_cot_stats(const cos_sigma_cot_state_t *st,
                         cos_sigma_cot_stats_t *out);

/* -------- Self-test ----------------------------------------------- */
int cos_sigma_cot_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_COT_DISTILL_H */
