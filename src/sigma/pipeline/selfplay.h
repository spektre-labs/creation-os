/*
 * σ-Selfplay — proposer/solver/verifier loop, σ is the verifier.
 *
 * SeRL (arxiv 2505.20347) and SPELL (arxiv 2509.23863) both solve
 * the "where does training data come from?" problem by making one
 * model play multiple roles: propose a question, solve it, verify
 * it.  Both rely on an external critic to close the loop.
 *
 * Creation OS folds that critic into the existing σ machinery:
 * σ-gate IS the verifier.  A round looks like
 *
 *     PROPOSER (σ_q) → SOLVER (σ_a) → VERIFIER (σ-gate on σ_a)
 *
 * σ_q  — how confident the proposer was in its question
 *        (low σ_q = well-formed question; high σ_q = nonsense)
 * σ_a  — how confident the solver was in its answer
 *
 * Difficulty targeting: the caller supplies a `difficulty_target`
 * in [0, 1].  The runtime labels a round as
 *
 *     TOO_EASY     σ_a < 0.20
 *     LEARNING     0.20 ≤ σ_a < 0.60    ← the useful band
 *     TOO_HARD     σ_a ≥ 0.60
 *
 * and exposes a `new_difficulty` knob the caller walks toward the
 * target.  No gradient — just a proportional controller over the
 * σ gap so it stays deterministic under stubs.
 *
 * Self-evaluation is not trusted blindly (the model critiquing
 * itself is Gödel-shaped).  σ-Selfplay supports a PROCONDUCTOR
 * variant: run the SOLVER with a second, independent generator
 * and flag the round OVERCONFIDENT iff σ_self << σ_other.
 *
 * Contracts (v0):
 *   1. init rejects invalid difficulty or NULL proposer/solver.
 *   2. round() runs PROPOSER → SOLVER → VERIFIER in order; returns
 *      the round record with label + new_difficulty.
 *   3. proconductor_check stamps `overconfident=1` iff σ_self is
 *      more than `margin` below σ_other on the same question.
 *   4. Self-test uses deterministic callbacks that trigger every
 *      verdict class.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SELFPLAY_H
#define COS_SIGMA_SELFPLAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_SP_TOO_EASY  = 0,
    COS_SP_LEARNING  = 1,
    COS_SP_TOO_HARD  = 2,
} cos_selfplay_label_t;

/* Proposer: given a seed topic + current difficulty ∈ [0,1], write
 * a question into `out_q` (caller-owned buffer) and stamp σ_q.
 * Returns 0 on success. */
typedef int (*cos_selfplay_propose_fn)(const char *topic,
                                       float difficulty,
                                       void *ctx,
                                       char *out_q, size_t cap_q,
                                       float *out_sigma_q);

/* Solver: given a question, write an answer + σ_a. */
typedef int (*cos_selfplay_solve_fn)(const char *question,
                                     void *ctx,
                                     char *out_a, size_t cap_a,
                                     float *out_sigma_a);

typedef struct {
    char                  question[512];
    char                  answer[1024];
    float                 sigma_question;
    float                 sigma_answer;
    float                 difficulty_in;      /* requested          */
    float                 new_difficulty;     /* controller output  */
    cos_selfplay_label_t  label;
    int                   verified;           /* σ_answer < τ_verify */
    int                   overconfident;      /* proconductor flag  */
    float                 sigma_answer_other; /* filled by proconductor */
} cos_selfplay_round_t;

typedef struct {
    float tau_verify;              /* σ_a below this = verified OK  */
    float difficulty_target;       /* [0,1]                         */
    float controller_gain;         /* 0..1 step toward target       */
    float band_low;                /* = 0.20 (TOO_EASY cutoff)      */
    float band_high;               /* = 0.60 (TOO_HARD cutoff)      */
    cos_selfplay_propose_fn proposer;
    void  *proposer_ctx;
    cos_selfplay_solve_fn   solver;
    void  *solver_ctx;
} cos_selfplay_config_t;

int cos_sigma_selfplay_init(cos_selfplay_config_t *cfg,
                            float tau_verify,
                            float difficulty_target,
                            float controller_gain,
                            cos_selfplay_propose_fn proposer,
                            void *proposer_ctx,
                            cos_selfplay_solve_fn solver,
                            void *solver_ctx);

int cos_sigma_selfplay_round(const cos_selfplay_config_t *cfg,
                             const char *topic,
                             float current_difficulty,
                             cos_selfplay_round_t *out);

/* Proconductor: rerun the same question with a SECOND solver and
 * compare σ_a.  If σ_a_other − σ_a_self > margin, the original
 * solver was overconfident. */
int cos_sigma_selfplay_proconductor(cos_selfplay_round_t *round,
                                    cos_selfplay_solve_fn other_solver,
                                    void *other_ctx,
                                    float overconfidence_margin);

/* Aggregate stats over N rounds. */
typedef struct {
    int   n_rounds;
    int   n_verified;
    int   n_too_easy;
    int   n_learning;
    int   n_too_hard;
    int   n_overconfident;
    float mean_sigma_q;
    float mean_sigma_a;
} cos_selfplay_stats_t;

void cos_sigma_selfplay_stats(const cos_selfplay_round_t *rounds, int n,
                              cos_selfplay_stats_t *out);

int  cos_sigma_selfplay_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SELFPLAY_H */
