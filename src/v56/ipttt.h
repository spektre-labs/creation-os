/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v56 IP-TTT policy — σ-gated In-Place Test-Time Training budget
 * controller.
 *
 * Framing: "In-Place Test-Time Training" (arXiv:2604.06169,
 * April 2026) reports a 4B model matching much larger baselines on
 * 128K-context tasks by updating MLP-projection "fast weights" at
 * inference time.  The open problem: when should an inference
 * server allow such an update?  Uncontrolled TTT burns memory and
 * risks catastrophic forgetting; pure static weights leave
 * long-context competence on the table.
 *
 * v56 gives the answer that is native to Creation OS: update only
 * when σ drift provides evidence the current weights are
 * mis-specified for the running context, within a **bounded
 * buffer**, and roll back any update that does not measurably
 * reduce σ.  This is the inference-side dual of v40's threshold
 * theorem (K_eff = (1 − σ) · K): spend the TTT budget iff it
 * strictly raises K_eff on the next observation.
 *
 * Scope (tier C): policy layer only.  No tensors allocated, no
 * weights updated — we return the ALLOW / DENY decision, the
 * remaining budget, and the proposed update magnitude.  Caller
 * (model runtime) brings the actual SGD step.
 */
#ifndef CREATION_OS_V56_IPTTT_H
#define CREATION_OS_V56_IPTTT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   update_budget_bytes;    /* hard cap on TTT buffer, e.g. 4 MiB */
    float sigma_drift_threshold;  /* trigger: σ_recent - σ_baseline > this */
    float alpha_base;             /* base update-rate scale (e.g. 0.01) */
    float rollback_shrink;        /* on regression, new_alpha *= shrink (e.g. 0.5) */
    int   max_fast_weight_layers; /* how many MLP projections may be touched */
    int   min_steps_between;      /* min inference steps between updates */
} v56_ipttt_params_t;

typedef struct {
    float sigma_recent;           /* EWMA of recent σ */
    float sigma_baseline;         /* long-running baseline σ */
    float alpha_current;          /* current effective update-rate */
    int   bytes_used;             /* bytes of TTT buffer committed so far */
    int   updates_applied;        /* total updates accepted */
    int   updates_rolled_back;    /* total updates rolled back (σ regression) */
    int   steps_since_last;       /* inference steps since last accepted update */
    int   last_decision_allow;    /* last v56_ipttt_decide result (1/0) */
} v56_ipttt_state_t;

typedef enum {
    V56_IPTTT_DENY_BUDGET      = 1,  /* would exceed byte budget */
    V56_IPTTT_DENY_COOLDOWN    = 2,  /* too soon since last update */
    V56_IPTTT_DENY_NO_DRIFT    = 3,  /* σ drift below threshold */
    V56_IPTTT_DENY_INVALID     = 4,  /* bad inputs */
    V56_IPTTT_ALLOW            = 100
} v56_ipttt_reason_t;

typedef struct {
    int                   allow;               /* 1 = allow update, 0 = deny */
    v56_ipttt_reason_t    reason;
    float                 proposed_alpha;      /* 0 if denied */
    int                   bytes_reserved;      /* 0 if denied */
    float                 observed_drift;
} v56_ipttt_decision_t;

void v56_ipttt_init(const v56_ipttt_params_t *p, v56_ipttt_state_t *s);

/* Feed an observed σ measurement (e.g. from v55 sigma3, or any
 * token-level σ channel).  Updates the EWMA baseline/recent bands. */
void v56_ipttt_observe_sigma(v56_ipttt_state_t *s, float observed_sigma);

/* Request a fast-weight update of bytes_needed bytes.  Returns
 * ALLOW/DENY + reason + proposed α.  Branchless on the hot path. */
void v56_ipttt_decide(const v56_ipttt_params_t *p,
                      v56_ipttt_state_t *s,
                      int bytes_needed,
                      v56_ipttt_decision_t *out);

/* Called by the caller after it has applied (or not) an allowed
 * update: post_update_sigma is a fresh σ measurement on the same
 * context.  If σ regressed we shrink α and count a rollback. */
void v56_ipttt_commit_or_rollback(const v56_ipttt_params_t *p,
                                  v56_ipttt_state_t *s,
                                  int bytes_committed,
                                  float post_update_sigma);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V56_IPTTT_H */
