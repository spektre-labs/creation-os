/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-TTT — test-time training gated by σ (v275.1 live).
 *
 * v275 declared the contract (σ_update gate + dual-track drift cap +
 * sliding-window eviction + citations) but did NOT execute real weight
 * updates.  This primitive is the live kernel: actual float arithmetic
 * over a fast/slow weight pair with a σ-gated admission rule, a Gödel-
 * aware drift cap, and a hard reset rule that prevents catastrophic
 * forgetting.
 *
 * Design (matches arxiv 2604.06169 "In-Place TTT" + arxiv 2604.09624
 * "SECL" entropy-gating, adapted to σ):
 *
 *     state := { fast[N], slow[N], lr, tau_sigma, tau_drift }
 *
 *     step(σ, grad):
 *         if σ < tau_sigma            : skip      # model is confident
 *         else                        : fast -= lr · grad
 *         drift := ||fast − slow|| / max(||slow||, eps)
 *         if drift > tau_drift        : fast := slow    # Gödel guard
 *
 * Semantic:
 *
 *   * High σ = model is uncertain about THIS prediction → spend a
 *     gradient step on it so next generation benefits.  This is the
 *     opposite convention from v275's manifest σ_update (which measures
 *     gradient quality, not prediction uncertainty); keep both
 *     conventions straight when integrating.
 *   * The σ-gate saves ~75% of gradient steps empirically (SECL
 *     Table 2), matching v275's stated target.
 *   * The drift cap is the Gödel-aware invariant: "I am a small model
 *     that admits I can be wrong" — if fast drifts too far from slow,
 *     we reset to slow.  Prevents runaway adaptation.
 *
 * Contracts (v0, discharged at runtime):
 *   1. step() with σ < tau_sigma is a no-op on fast[].
 *   2. step() with σ ≥ tau_sigma decreases fast by lr·grad (byte-ident.
 *      with plain C arithmetic; no SIMD hand-off).
 *   3. drift() returns a nonnegative float; returns 0 when
 *      fast[] == slow[] byte-identically.
 *   4. reset_if_drift() is idempotent: calling it twice in a row
 *      after a drift-trigger leaves state unchanged the second time.
 *   5. A sequence of N steps with alternating σ high / low keeps
 *      drift bounded by N/2 · lr · max|grad| / ||slow||.
 *   6. self_test over a canonical 10-step trace + 10^4 LCG-random
 *      (σ, grad) updates keeps fast[] finite and within drift cap OR
 *      hits exactly one reset event.
 *
 * This primitive is INTENTIONALLY model-agnostic: it operates on raw
 * float buffers so a BitNet integration (Python side) can wrap the
 * last-MLP-projection tail without this file knowing anything about
 * tokenizers, attention, or gguf layouts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TTT_H
#define COS_SIGMA_TTT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Backing buffers are caller-owned; this struct only holds pointers and
 * hyperparameters.  This keeps the primitive allocation-free. */
typedef struct {
    float *fast;         /* length = n; living weights (updated)   */
    const float *slow;   /* length = n; frozen reference weights   */
    int    n;            /* dimension                              */
    float  lr;           /* learning rate                          */
    float  tau_sigma;    /* σ gate: update iff σ ≥ tau_sigma       */
    float  tau_drift;    /* reset iff ||fast−slow||/||slow|| > tau */
    /* Counters for self-test / observability.  Not used in the gate. */
    int   n_steps_total;
    int   n_steps_updated;
    int   n_resets;
} cos_sigma_ttt_state_t;

/* Initialise the state.  Requires caller to provide already-populated
 * ``slow`` and a writable ``fast`` buffer of the same length; copies
 * slow → fast so step 0 starts synchronised.  Returns 0 on success,
 * negative on argument violation. */
int cos_sigma_ttt_init(cos_sigma_ttt_state_t *st,
                       float *fast, const float *slow, int n,
                       float lr, float tau_sigma, float tau_drift);

/* One σ-gated TTT step.
 *
 * If σ < tau_sigma: no update, returns 0.
 * Else: fast[i] -= lr · grad[i] for all i; increments n_steps_updated.
 * Always increments n_steps_total.
 *
 * Returns 1 if an update was applied, 0 if skipped, negative on error. */
int cos_sigma_ttt_step(cos_sigma_ttt_state_t *st, float sigma,
                       const float *grad);

/* ||fast − slow|| / max(||slow||, eps).  Pure; no side effects.
 * Returns NaN only if slow[] has zero norm AND fast[] differs (we avoid
 * returning that in practice — eps is 1e-12). */
float cos_sigma_ttt_drift(const cos_sigma_ttt_state_t *st);

/* If drift > tau_drift, set fast := slow and increment n_resets.
 * Returns 1 if a reset happened, 0 otherwise. */
int cos_sigma_ttt_reset_if_drift(cos_sigma_ttt_state_t *st);

/* One canonical self-test: synthetic 10-step alternating trace +
 * 10^4-point LCG-random trace; asserts update / skip / drift / reset
 * semantics.  Returns 0 on PASS, positive failure code otherwise. */
int cos_sigma_ttt_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_TTT_H */
