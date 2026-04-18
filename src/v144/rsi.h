/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v144 σ-RSI — Recursive Self-Improvement loop.
 *
 * The ICLR 2026 RSI workshop categorises self-improvement along
 * five axes (what / when / how / where / safety).  Creation OS
 * already has every piece separately — continual learning weights
 * (v124), idle-trigger deploy-time updates (v124.idle), σ-DPO as
 * the reward signal without annotators (v125), agentic tool use
 * (v112/v113/v114) for the *where*, and v133 meta-dashboard +
 * v122 red-team for the safety axis.  v144 is the *orchestrator*
 * that closes the loop:
 *
 *     cycle():
 *       weaknesses = v133_meta.identify_weaknesses()
 *       curriculum = v141_curriculum.generate(weaknesses)
 *       data       = v120_distill(curriculum, big, small)
 *       adapter    = v125_dpo.train(data)
 *       v124_continual.hot_swap(adapter)
 *       score      = v143_benchmark.evaluate()
 *       if score < previous_score:
 *           v124_continual.rollback()
 *       else:
 *           previous_score = score
 *
 * v144.0 is the deterministic, dependency-free core of that loop:
 * the submit → accept-or-rollback → pause state machine, plus
 * the σ_rsi stability signal derived from the last 10 accepted
 * scores.  It is the *contract* a real RSI implementation has to
 * honour, tested end-to-end in the merge-gate.
 *
 * Contracts enforced by this kernel (all asserted in the
 * self-test and the benchmark shell gate):
 *
 *   C1  Candidate score < current accepted score ⇒ rollback
 *       (current_score unchanged, cycle counted as rolled_back).
 *   C2  Three consecutive regressions ⇒ paused = 1 and every
 *       subsequent submit short-circuits as "paused" without
 *       touching current_score.
 *   C3  A successful submit resets consecutive_regressions to 0,
 *       updates current_score, and pushes the new score onto the
 *       rolling 10-wide history window.
 *   C4  σ_rsi = variance(history) / mean(history) on the
 *       population variance of the last 10 accepted scores, with
 *       the mean clamped above a small epsilon.  Low σ_rsi =
 *       stable learning, high σ_rsi = unstable (the v148
 *       sovereign loop consumes this to throttle itself).
 *
 * v144.1 (deferred) wires the real v124/v125/v141/v143 stack into
 * cos_v144_cycle() via callbacks; v144.0 only takes a *candidate
 * score* because that is the single scalar a real run produces.
 */
#ifndef COS_V144_RSI_H
#define COS_V144_RSI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V144_HISTORY      10
#define COS_V144_PAUSE_AFTER   3   /* consecutive regressions */

typedef struct cos_v144_rsi {
    int    cycle_count;              /* submits attempted                */
    int    accepted_cycles;
    int    rolled_back_cycles;
    int    skipped_while_paused;
    int    consecutive_regressions;
    int    paused;                   /* 1 iff ≥ PAUSE_AFTER regressions */
    float  current_score;            /* accepted score                  */

    /* Rolling history of accepted scores (most recent at [history_len-1]
     * until we hit COS_V144_HISTORY then it's a true ring buffer with
     * head = write cursor). */
    int    history_len;              /* ≤ COS_V144_HISTORY              */
    int    history_head;             /* next write index                */
    float  history[COS_V144_HISTORY];
} cos_v144_rsi_t;

typedef struct cos_v144_cycle_report {
    int    cycle_index;              /* 0-based attempt number          */
    float  score_before;
    float  score_after;
    float  delta;                    /* candidate − previous            */
    int    accepted;                 /* 1 iff candidate ≥ previous      */
    int    rolled_back;              /* 1 iff candidate rejected        */
    int    paused_now;               /* 1 iff this submit triggered it  */
    int    skipped_paused;           /* 1 iff submit arrived while paused*/
    int    consecutive_regressions;
    float  sigma_rsi;                /* stability signal after update   */
} cos_v144_cycle_report_t;

/* Zero the state and seed current_score with a baseline.  The
 * baseline is immediately pushed onto the history ring so σ_rsi
 * is defined on the very first submit. */
void  cos_v144_init(cos_v144_rsi_t *r, float baseline_score);

/* Submit a candidate score from a just-finished improvement
 * attempt.  Applies contracts C1–C4.  Returns 0 on success and
 * fills *out; returns <0 only on argument errors. */
int   cos_v144_submit(cos_v144_rsi_t *r, float candidate_score,
                      cos_v144_cycle_report_t *out);

/* Manual resume after the user has reviewed a paused loop.
 * Clears paused and consecutive_regressions.  No-op if not
 * paused. */
void  cos_v144_resume(cos_v144_rsi_t *r);

/* σ_rsi on the current accepted-score history window.  Returns 0
 * if history_len < 2.  Population variance / mean.  Mean is
 * clamped above 1e-6 so σ_rsi is defined even at zero-score. */
float cos_v144_sigma_rsi(const cos_v144_rsi_t *r);

/* Canonical JSON shape.  Written to buf, returns bytes written or
 * -1 on overflow. */
int   cos_v144_cycle_to_json(const cos_v144_cycle_report_t *c,
                             const cos_v144_rsi_t *r,
                             char *buf, size_t cap);

/* Full-state snapshot as JSON (for v108 UI / v133 dashboard). */
int   cos_v144_state_to_json(const cos_v144_rsi_t *r,
                             char *buf, size_t cap);

/* Self-test exercising C1..C4. */
int   cos_v144_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
