/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Evolve — parameter evolution with σ-gate as the fitness function.
 *
 * Mutates the σ-ensemble weights + τ on a labeled calibration fixture,
 * measures the Brier score of σ_combined vs (1 - correct), and keeps
 * only mutations that drop Brier below the current champion by at
 * least `improvement_eps`.  Defaults converge to near-MMSE weights on
 * the shipped fixture, so the self-test is deterministic and
 * reproducible — no LLM, no GPU, no clock dependency.
 *
 * The same primitives drive the LLM-powered mutator path (when the
 * caller wires $COS_EVOLVE_MUTATOR to a script that shells out to
 * llama-server); σ-Evolve never cares where the candidate came from,
 * only whether its Brier clears the champion.
 *
 * Contracts:
 *   1. mutate_default() preserves w_i ∈ [0,1] and Σ w_i = 1 ± 1e-6.
 *   2. brier() returns NaN iff n_items == 0, else a value in [0,1].
 *   3. step() is pure: no I/O unless the caller opts in with hooks.
 *   4. The best-so-far Brier is monotonically non-increasing across
 *      a single run() call.
 *   5. accept() implies Brier strictly improved; revert() leaves
 *      state bit-identical to the pre-call snapshot.
 *
 * Everything above this docstring is testable and tested in
 * cos_sigma_evolve_self_test().
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EVOLVE_H
#define COS_SIGMA_EVOLVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The tunable genome.  Five floats; five. */
typedef struct {
    float w_logprob;
    float w_entropy;
    float w_perplexity;
    float w_consistency;
    float tau;               /* σ-gate accept threshold, in [0,1] */
} cos_evolve_params_t;

/* One labeled fixture item: raw four-signal σ + ground truth.
 * `correct == 1` means the generation was factually correct, so the
 * ideal σ_combined for this row is 0.  `correct == 0` → ideal σ = 1. */
typedef struct {
    float sigma_logprob;
    float sigma_entropy;
    float sigma_perplexity;
    float sigma_consistency;
    int   correct;
} cos_evolve_item_t;

typedef struct {
    float brier;             /* (1/N) Σ (σ_combined - (1-correct))^2 */
    float accuracy_accept;   /* correct fraction among σ ≤ τ rows     */
    float coverage;          /* n_accept / N                          */
    int   n_items;
    int   n_accept;
    int   n_correct_accept;
} cos_evolve_score_t;

/* Policy.  NULL → defaults (see cos_sigma_evolve_policy_default). */
typedef struct {
    int    generations;           /* max attempted mutations         */
    float  improvement_eps;       /* strict-improve threshold        */
    float  mutation_sigma;        /* Gaussian step on one weight     */
    uint64_t rng_seed;            /* deterministic PRNG              */
    int    log_every;             /* stdout progress cadence         */
} cos_evolve_policy_t;

typedef struct {
    int   iterations;
    int   n_accepted;
    int   n_reverted;
    cos_evolve_params_t best;
    cos_evolve_score_t  best_score;
    cos_evolve_params_t start;
    cos_evolve_score_t  start_score;
} cos_evolve_report_t;

/* -------- genome math (pure) -------- */

void cos_sigma_evolve_params_default(cos_evolve_params_t *p);

/* Normalise w_i ≥ 0, Σ w_i = 1.  τ clamped to (0,1).  Safe on NaN. */
void cos_sigma_evolve_params_normalise(cos_evolve_params_t *p);

/* Gaussian perturbation on one randomly-chosen weight (not τ by
 * default); renormalises.  PRNG state threaded via `*rng_state`. */
void cos_sigma_evolve_mutate_default(cos_evolve_params_t *p,
                                     float mutation_sigma,
                                     uint64_t *rng_state);

/* Mutate τ only (separate knob so we can evolve threshold without
 * touching weights; used by cos calibrate --auto).  Gaussian step. */
void cos_sigma_evolve_mutate_tau(cos_evolve_params_t *p,
                                 float mutation_sigma,
                                 uint64_t *rng_state);

/* -------- fitness (pure) -------- */

/* Score a parameter set on a fixture.  Returns 0 on success, -1 on
 * bad input.  brier/accuracy/coverage fields are set; a zero-length
 * fixture yields brier = NaN and everything else zero. */
int  cos_sigma_evolve_score(const cos_evolve_params_t *p,
                            const cos_evolve_item_t   *items,
                            int n_items,
                            cos_evolve_score_t *out);

/* -------- run loop -------- */

void cos_sigma_evolve_policy_default(cos_evolve_policy_t *p);

/* In-memory evolve: caller owns items[], initial params, and the
 * output report.  No I/O.  Returns 0 on success, -1 on bad input. */
int  cos_sigma_evolve_run(cos_evolve_params_t *inout_params,
                          const cos_evolve_item_t *items, int n_items,
                          const cos_evolve_policy_t *policy,
                          cos_evolve_report_t *out_report);

/* JSONL fixture loader.  One object per line, keys:
 *   sigma_logprob, sigma_entropy, sigma_perplexity, sigma_consistency,
 *   correct (0 or 1).
 * Unknown keys are ignored; missing numeric keys default to 0.0.
 * Returns items allocated with malloc() on success (caller frees).
 * *n_items is set.  Returns NULL on error; *err_msg may be set. */
cos_evolve_item_t *cos_sigma_evolve_load_fixture_jsonl(
        const char *path, int *n_items, char *err_msg, size_t err_cap);

/* Params JSON read/write.  Minimal parser — keys at top level only:
 * w_logprob, w_entropy, w_perplexity, w_consistency, tau.  Missing
 * keys retain the caller-supplied defaults.  Returns 0 on success. */
int  cos_sigma_evolve_params_load_json(const char *path,
                                       cos_evolve_params_t *out);
int  cos_sigma_evolve_params_save_json(const char *path,
                                       const cos_evolve_params_t *p);

int  cos_sigma_evolve_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_EVOLVE_H */
