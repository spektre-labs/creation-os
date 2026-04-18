/*
 * v195 σ-Recover — error classification + per-class recovery
 * + calibration feedback.
 *
 *   Five error classes:
 *       HALLUCINATION  — σ was low but answer was wrong
 *       CAPABILITY_GAP — σ was high, correct abstain
 *       PLANNING_ERROR — wrong decomposition
 *       TOOL_FAILURE   — tool crashed / timed out
 *       CONTEXT_LOSS   — long session degradation (v194)
 *
 *   Per-class recovery (v0 records which operators would run):
 *       HALLUCINATION  ⇒ v180 steer truthful + v125 DPO pair
 *       CAPABILITY_GAP ⇒ v141 curriculum + v145 skill acquire
 *       PLANNING_ERROR ⇒ v121 replan
 *       TOOL_FAILURE   ⇒ v159 heal + retry
 *       CONTEXT_LOSS   ⇒ v194 checkpoint resume
 *
 *   Calibration feedback: hallucinations (σ < τ but wrong) are
 *   the *direct* signal that v187 calibration has drifted on
 *   the incident's domain.  v195 maintains a per-domain ECE
 *   estimator that is strictly lower after the recovery pass.
 *
 *   Recovery learning: every (error, recovery) pair is logged
 *   so v174 flywheel + v125 DPO can reuse them.  Subsequent
 *   occurrences of the same error should take FEWER total
 *   recovery steps — v195.0 ships a deterministic demonstration
 *   where the second pass over identical errors consumes
 *   strictly fewer recovery operations.
 *
 *   Merge-gate contracts (exercised in v0):
 *     1. All 5 error classes surfaced at least once.
 *     2. Each class is paired with its canonical recovery
 *        operator(s) — strict partitioning.
 *     3. ECE (domain-wise) strictly drops on every domain that
 *        had a hallucination incident.
 *     4. Second-pass recovery uses fewer steps than first pass
 *        (learning contract).
 *     5. Recovery log is hash-chained and replays.
 *     6. Byte-deterministic.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V195_RECOVER_H
#define COS_V195_RECOVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V195_N_INCIDENTS   30
#define COS_V195_N_CLASSES      5
#define COS_V195_N_DOMAINS      4
#define COS_V195_STR_MAX       32
#define COS_V195_MAX_OPS        3

typedef enum {
    COS_V195_ERR_HALLUCINATION  = 0,
    COS_V195_ERR_CAPABILITY_GAP = 1,
    COS_V195_ERR_PLANNING_ERROR = 2,
    COS_V195_ERR_TOOL_FAILURE   = 3,
    COS_V195_ERR_CONTEXT_LOSS   = 4
} cos_v195_class_t;

typedef enum {
    COS_V195_OP_STEER_TRUTHFUL = 180,  /* v180 */
    COS_V195_OP_DPO_PAIR       = 125,  /* v125 */
    COS_V195_OP_CURRICULUM     = 141,  /* v141 */
    COS_V195_OP_SKILL_ACQUIRE  = 145,  /* v145 */
    COS_V195_OP_REPLAN         = 121,  /* v121 */
    COS_V195_OP_HEAL_RETRY     = 159,  /* v159 */
    COS_V195_OP_CHECKPOINT     = 194   /* v194 */
} cos_v195_op_t;

typedef struct {
    int      id;
    int      class_;
    int      domain;                     /* 0..COS_V195_N_DOMAINS-1 */
    float    sigma;                      /* σ at failure */
    float    tau;                        /* gate τ */
    bool     correct;                    /* was the answer correct? */
    int      recovery_ops[COS_V195_MAX_OPS];
    int      n_recovery_ops;
    int      pass;                       /* 0 first, 1 second */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v195_incident_t;

typedef struct {
    int                  n_incidents;
    cos_v195_incident_t  incidents[COS_V195_N_INCIDENTS];

    /* Per-class counts. */
    int                  class_counts[COS_V195_N_CLASSES];

    /* Per-domain ECE, before / after calibration update. */
    float                ece_before[COS_V195_N_DOMAINS];
    float                ece_after[COS_V195_N_DOMAINS];

    /* Pass stats. */
    int                  n_ops_pass0;
    int                  n_ops_pass1;

    /* Chain. */
    bool                 chain_valid;
    uint64_t             terminal_hash;

    /* Partition sanity flag (every class → canonical ops only). */
    bool                 class_to_ops_canonical;

    uint64_t             seed;
} cos_v195_state_t;

void   cos_v195_init(cos_v195_state_t *s, uint64_t seed);
void   cos_v195_build(cos_v195_state_t *s);
void   cos_v195_run(cos_v195_state_t *s);

size_t cos_v195_to_json(const cos_v195_state_t *s,
                         char *buf, size_t cap);

int    cos_v195_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V195_RECOVER_H */
