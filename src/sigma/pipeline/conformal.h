/*
 * σ-pipeline: conformal selective-prediction calibration (SCI-1 / SCI-2).
 *
 * Given a calibration set {(σ_i, correct_i)}_{i=1..N} drawn from the
 * same distribution as the deployment stream, find the largest
 * threshold τ such that accepting "σ_new ≤ τ" yields
 *
 *     P(wrong | σ_new ≤ τ)  ≤  α
 *
 * with probability at least 1 − δ over the calibration draw.
 * This is Angelopoulos-Bates "Learn-then-Test" risk control
 * (arXiv:2110.01052) specialized to binary selective prediction:
 *
 *   1. Candidate τ's are the sorted unique σ values.
 *   2. For each τ:
 *        n_accept(τ) = #{i : σ_i ≤ τ}
 *        n_wrong(τ)  = #{i : σ_i ≤ τ  ∧  correct_i = 0}
 *        p̂(τ)        = n_wrong / n_accept
 *        UCB(τ)      = Hoeffding upper bound on p̂ at level 1 − δ
 *                    = p̂ + √( log(1/δ) / (2 n_accept) )
 *   3. Pick the largest τ with UCB(τ) ≤ α  AND  n_accept ≥ min_support.
 *
 * Hoeffding is strictly conservative (Clopper-Pearson is tighter but
 * requires the inverse regularized incomplete beta); the kernel is a
 * C11 leaf so we pay the conservatism to keep the dep graph at libm.
 *
 * Per-domain τ (SCI-2) is implemented on top of the same primitive:
 * partition samples by domain tag, run `cos_conformal_calibrate` per
 * partition, report {domain -> τ}.  See `cos_conformal_per_domain`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CONFORMAL_H
#define COS_SIGMA_CONFORMAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_CONFORMAL_DOMAIN_MAX 16
#define COS_CONFORMAL_NAME_MAX   32
#define COS_CONFORMAL_MIN_SUPPORT 10

typedef struct {
    float sigma;
    int   correct;   /* 1=correct, 0=wrong, -1=unscored (skipped)        */
    char  domain[COS_CONFORMAL_NAME_MAX]; /* empty string = "all"        */
} cos_conformal_sample_t;

typedef struct {
    char  domain[COS_CONFORMAL_NAME_MAX]; /* "all" for global            */
    float tau;                 /* calibrated threshold                   */
    float alpha;               /* target error rate among accepted       */
    float delta;               /* confidence parameter (tail prob.)      */
    int   n_total;             /* all samples in this partition          */
    int   n_scored;            /* with correct != -1                     */
    int   n_accepted;          /* σ ≤ τ                                  */
    int   n_errors_accepted;   /* wrong AND σ ≤ τ                        */
    float empirical_risk;      /* n_errors_accepted / n_accepted         */
    float risk_ucb;            /* Hoeffding UCB at this τ                */
    float coverage;            /* n_accepted / n_scored                  */
    int   valid;               /* 1 if UCB ≤ α found, 0 otherwise        */
} cos_conformal_report_t;

typedef struct {
    int n_reports;
    cos_conformal_report_t reports[COS_CONFORMAL_DOMAIN_MAX];
} cos_conformal_bundle_t;

/* Core calibration — SCI-1.
 *
 * samples[] may include unscored rows (correct == -1); those are
 * ignored.  `alpha` ∈ (0,1) is the target error rate; `delta` ∈ (0,1)
 * is the confidence parameter (smaller δ = tighter bound, needs more
 * samples).  On success returns 0 and fills `out`.  If no τ passes
 * the UCB test, returns 0 but sets out->valid = 0 and out->tau to
 * the smallest σ observed (fully conservative: abstain-by-default).
 *
 * Returns -1 on invalid input (samples == NULL, n <= 0, α or δ out of
 * range, `out` == NULL).
 */
int cos_conformal_calibrate(const cos_conformal_sample_t *samples, int n,
                            float alpha, float delta,
                            cos_conformal_report_t *out);

/* Per-domain τ — SCI-2.
 *
 * Partitions samples by their `domain` tag (empty string = "all");
 * runs `cos_conformal_calibrate` on each partition that has at least
 * COS_CONFORMAL_MIN_SUPPORT scored rows, plus one global "all" report
 * over every scored sample.  Reports written in `bundle->reports[0..N]`
 * with bundle->reports[0] being "all".
 *
 * Returns 0 on success, -1 on invalid input.
 */
int cos_conformal_per_domain(const cos_conformal_sample_t *samples, int n,
                             float alpha, float delta,
                             cos_conformal_bundle_t *bundle);

/* Evaluate UCB at an arbitrary τ — used by coverage-curve (SCI-3).
 * Returns 0 on success, -1 on invalid input.  Fills out with the
 * empirical selective risk at this τ (no validity check, for plotting).
 */
int cos_conformal_eval_at(const cos_conformal_sample_t *samples, int n,
                          float tau, float delta,
                          cos_conformal_report_t *out);

/* JSON I/O.  cos_conformal_bundle_t is serialized as
 *   { "alpha":..., "delta":..., "per_domain":[ {...report...}, ... ] }
 * which matches ~/.cos/calibration.json. */
int cos_conformal_write_bundle_json(const char *path,
                                    const cos_conformal_bundle_t *b);
int cos_conformal_read_bundle_json (const char *path,
                                    cos_conformal_bundle_t *b);

/* Load samples from a truthfulqa_817_detail.jsonl-style file.
 *
 * Each line is one JSON object; required fields:
 *   "sigma"    : number
 *   "correct"  : bool | null
 *   "mode"     : string (optional; filters when `mode_filter` is non-NULL)
 *   "category" : string (optional; copied into sample.domain)
 *   "prompt"   : string (optional; consumed only when classify == 1)
 *
 * Rows with correct == null are loaded with correct = -1 so they do
 * not contribute to the calibration numerator or denominator.
 *
 * When `classify` is non-zero the loader overrides sample.domain
 * with cos_conformal_classify_prompt(row["prompt"]), yielding one of
 * {"factual","code","reasoning","other"} regardless of the row's
 * dataset-native category.  This is the SCI-2 per-domain τ surface.
 *
 * Returns the number of samples loaded on success (≤ cap), -1 on io.
 */
int cos_conformal_load_jsonl(const char *path, const char *mode_filter,
                             cos_conformal_sample_t *out, int cap);
int cos_conformal_load_jsonl_ex(const char *path, const char *mode_filter,
                                int classify,
                                cos_conformal_sample_t *out, int cap);

/* --------------------------------------------------------------- *
 * SCI-2: adaptive τ per domain + online update.
 * --------------------------------------------------------------- *
 *
 * (a) Prompt classifier.  Keyword-heuristic, returns one of the
 *     fixed category tags {"factual","code","reasoning","other"}.
 *     Strictly deterministic; no model calls.  The output pointer
 *     is a compile-time literal owned by the library.
 */
const char *cos_conformal_classify_prompt(const char *prompt);

/*
 * (b) Online adaptive τ.  Gibbs-Candès "Adaptive Conformal Inference
 *     under Distribution Shift" (arXiv:2106.00170) update rule:
 *
 *         τ_{t+1} = clamp(τ_t + η · (1{σ_t > τ_t} − α_target), 0, 1)
 *
 *     Interpretation: if recent rejection rate exceeds α_target, τ
 *     grows (we accept more); if it undershoots, τ shrinks.  This is
 *     the simplest adaptive rule that preserves a marginal coverage
 *     guarantee under arbitrary distribution drift.
 *
 *     `sigma` is the σ of the *latest* observation; `tau_in` is the
 *     current threshold; `alpha_target` is the desired rejection
 *     rate; `eta` is the learning rate (typical 0.05).  Returns the
 *     updated τ clipped to [0,1].  Pure function, no state.         */
float cos_conformal_aci_update(float sigma, float tau_in,
                               float alpha_target, float eta);

/* Self-test: synthetic calibration on a hand-constructed dataset; checks
 * τ monotonicity in α, UCB shape, and ACI convergence behaviour.
 * Returns 0 on success. */
int cos_conformal_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CONFORMAL_H */
