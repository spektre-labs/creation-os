/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v59 — σ-Budget: σ-decomposed adaptive test-time compute
 * budget controller.
 *
 *   The Q2 2026 "adaptive reasoning budget" field — TAB (arXiv:2604.05164,
 *   GRPO-learned, 35-40 % savings), CoDE-Stop (arXiv:2604.04930, confidence
 *   dynamics, 25-50 %), LYNX (arXiv:2512.05325, learned hidden-state probe,
 *   40-70 %), DTSR (reflection-signal sufficiency, 28-35 %), DiffAdapt
 *   (entropy patterns, 22 %), Coda (arXiv:2603.08659, rollout difficulty
 *   gating, 60 %), AdaCtrl (arXiv:2505.18822, RL budgeting, 62-91 %),
 *   Risk-Control Budget Forcing (distribution-free dual thresholds, 52 %)
 *   — converges on one open problem: **how much compute to spend on this
 *   next reasoning step**.
 *
 *   Every published policy answers with a **scalar** signal:  entropy, a
 *   confidence dynamic, a probe scalar, an RL-learned policy, or a
 *   reflection-tag counter.  None of them uses the v34 σ decomposition
 *   (reducible ε  vs irreducible α), even though the rest of Creation OS
 *   already speaks that dialect (v34 Dirichlet, v55 σ₃ speculative, v56
 *   σ-Constitutional, v57 verifier registry, v58 σ-Cache KV-eviction).
 *
 *   v59 = σ-Budget.  Four-valued per-step decision, branchlessly computed:
 *
 *     CONTINUE    σ moderate, ε ≳ α      →  another reasoning step helps
 *     EARLY_EXIT  σ low,  answer stable  →  commit now, stop reasoning
 *     EXPAND      σ high, ε ≫ α          →  branch / self-verify / sample
 *     ABSTAIN     σ high, α ≫ ε          →  problem is irreducibly hard;
 *                                            do not waste compute, return
 *                                            an abstain response
 *
 *   The ABSTAIN signal is the novel contribution: every scalar-signal
 *   method wastes compute on α-dominated (noise / ambiguity) cases because
 *   it cannot tell ε from α.  σ-Budget refuses to spend budget on them and
 *   hands an explicit abstain decision upstream (to v53's σ-TAOR loop or
 *   v54's proconductor).
 *
 *   ## Tier semantics (v57 dialect)
 *
 *     M — runtime-checked deterministic self-test         (all v59 claims)
 *     F — formally proven proof artifact                  (none yet; open)
 *     I — interpreted / documented                        (ε/α fidelity)
 *     P — planned                                         (end-to-end acc.)
 *
 *   ## Hardware discipline (per .cursorrules)
 *
 *     1. mmap — n/a (policy, no file I/O).
 *     2. 64-byte aligned scratch via `aligned_alloc(64, ⌈n·4/64⌉·64)`.
 *     3. Prefetch 16 lanes ahead in every history-walking loop.
 *     4. Branchless kernel.  No `if` on the decision hot path; all tags
 *        assembled from 0/1 masks via `| & ~` then a four-way mux.
 *     5. NEON 4-accumulator reduction for the step-score pass; scalar
 *        fallback is bit-identical.
 *     6. No framework dependency.  No Accelerate, no Metal, no Core ML.
 *
 *   ## Non-claims
 *
 *     - v59 does NOT claim lower end-to-end tokens-per-correct than TAB /
 *       CoDE-Stop / LYNX on any benchmark.  End-to-end accuracy-under-
 *       budget is a P-tier measurement; v59 ships the *policy + kernel +
 *       correctness proof*, not a leaderboard row.
 *     - The σ signal is v34's; v59 claims novelty for **σ-decomposition
 *       applied as the adaptive budget controller**.
 *     - No Frama-C proof yet.  All claims are M-tier runtime checks.
 */

#ifndef CREATION_OS_V59_SIGMA_BUDGET_H
#define CREATION_OS_V59_SIGMA_BUDGET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One reasoning step's σ profile, in the caller's dialect.  The caller
 * is responsible for producing ε / α — for Creation OS this comes from
 * v34 Dirichlet, v55 σ₃ decomposition, or v56 VPRM process verifier. */
typedef struct {
    int32_t step_idx;          /* 0-based index in the reasoning trace  */
    float   epistemic;         /* reducible σ (spend budget to reduce)  */
    float   aleatoric;         /* irreducible σ (budget cannot reduce)  */
    float   answer_delta;      /* 0 = answer unchanged since previous,
                                * 1 = answer swapped (CoDE-Stop signal) */
    float   reflection_signal; /* 0/1 reflection token detected (DTSR)  */
    uint8_t _reserved[4];
} cos_v59_step_t;

typedef struct {
    int32_t budget_max_steps;    /* hard ceiling (refuses CONTINUE above)*/
    int32_t budget_min_steps;    /* floor; no exit allowed before this   */
    int32_t history_window;      /* recency window for trend reasoning   */

    float   alpha_epistemic;     /* weight on ε in the step score        */
    float   beta_stability;      /* weight on (1 − answer_delta)         */
    float   gamma_reflection;    /* weight on reflection_signal          */
    float   delta_aleatoric;     /* penalty on α                         */

    float   tau_exit;            /* score ≤ τ_exit   → EARLY_EXIT        */
    float   tau_expand;          /* score ≥ τ_expand → EXPAND (if ε ≫ α) */
    float   alpha_dominance;     /* α/(ε+α) ≥ this AND σ high → ABSTAIN  */
    float   sigma_high;          /* ε + α ≥ this counts as "σ high"      */
} cos_v59_policy_t;

enum {
    COS_V59_CONTINUE   = 1,
    COS_V59_EARLY_EXIT = 2,
    COS_V59_EXPAND     = 3,
    COS_V59_ABSTAIN    = 4,
};

typedef struct {
    int32_t n_steps;
    int32_t continues;
    int32_t early_exits;
    int32_t expansions;
    int32_t abstentions;

    float   total_epistemic;
    float   total_aleatoric;
    float   final_step_score;
    uint8_t final_decision;
    uint8_t _pad[3];
} cos_v59_trace_summary_t;

/* Default policy: sane defaults for a 32-step reasoning trace.  Tuned
 * so that a low-σ step reliably fires EARLY_EXIT, a high-ε / low-α step
 * fires EXPAND, a high-α / low-ε step fires ABSTAIN, and everything
 * else fires CONTINUE.  These are POLICY defaults, not tuned against
 * any external benchmark — see the non-claims list in this header. */
void cos_v59_policy_default(cos_v59_policy_t *out);

/* Per-step σ-budget score.  Branchless.  Score ordering:
 *   large positive  →  answer is crystallising (exit-candidate)
 *   large negative  →  answer is noisy (abstain-candidate)
 *   middle band     →  keep reasoning (continue-candidate) */
float cos_v59_score_step(const cos_v59_step_t   *step,
                         const cos_v59_policy_t *p);

/* Batched score — writes scores_out[i] for i in [0, n).  NEON 4-accumulator
 * SoA hot path lives in cos_v59_score_soa_neon() for microbench; this
 * is the AoS API.  Safe: returns without writing if any pointer is NULL
 * or n <= 0. */
void  cos_v59_score_batch(const cos_v59_step_t  *steps,
                          int32_t                n,
                          const cos_v59_policy_t *p,
                          float                 *scores_out);

/* Explicit NEON 4-accumulator reduction over Structure-of-Arrays inputs.
 * Used by the microbench to exercise the .cursorrules item 5 pattern
 * (four lanes, three vfmaq_f32 stages) on a pre-vectorised layout. */
void  cos_v59_score_soa_neon(const float *epistemic,
                             const float *aleatoric,
                             const float *stability,
                             const float *reflection,
                             int32_t      n,
                             float        alpha,
                             float        beta,
                             float        gamma,
                             float        delta,
                             float       *scores_out);

/* Online decision: returns one of COS_V59_{CONTINUE,EARLY_EXIT,EXPAND,
 * ABSTAIN} for the step `steps[n-1]` given the full history so far.
 *
 * Preconditions:
 *   - steps, p, decision_out must all be non-NULL
 *   - n must be ≥ 1
 *
 * The decision is branchless given a fixed history length.  Internally:
 *
 *   g_exit    = score ≤ tau_exit   AND n ≥ budget_min_steps AND stable
 *   g_cap     = n ≥ budget_max_steps                        (force-exit)
 *   g_abstain = (ε+α ≥ sigma_high) AND (α/(ε+α) ≥ alpha_dominance)
 *             AND n ≥ budget_min_steps
 *   g_expand  = (ε+α ≥ sigma_high) AND NOT g_abstain
 *             AND score ≥ tau_expand
 *   g_cont    = NOT (g_exit | g_cap | g_abstain | g_expand)
 *
 * Ordering: exit-on-cap > abstain > expand > exit-on-score > continue.
 * All ties resolved by mask priority (bitwise AND-NOT cascade).
 *
 * Returns 0 on success, -1 on NULL arg or n < 1. */
int   cos_v59_decide_online(const cos_v59_step_t   *steps,
                            int32_t                 n,
                            const cos_v59_policy_t *p,
                            uint8_t                *decision_out);

/* Trace-level post-hoc analysis: walk the full trace and populate
 * `summary_out` with counts of each decision, totals, and the final
 * per-step score.  The trace is considered closed; this routine is
 * for offline audit, not for the inference loop.
 *
 * Returns 0 on success, -1 on NULL arg or n < 0. */
int   cos_v59_summarize_trace(const cos_v59_step_t   *steps,
                              int32_t                 n,
                              const cos_v59_policy_t *p,
                              cos_v59_trace_summary_t *summary_out);

/* String tag for a decision byte, suitable for logs / traces.  Returns
 * a stable pointer to static storage. */
const char *cos_v59_decision_tag(uint8_t d);

/* 64-byte aligned step buffer allocator; zero-inited.  Caller frees. */
cos_v59_step_t *cos_v59_alloc_steps(int32_t n);

/* Version triple (major, minor, patch). */
typedef struct { int32_t major, minor, patch; } cos_v59_version_t;
cos_v59_version_t cos_v59_version(void);

#ifdef __cplusplus
}
#endif
#endif /* CREATION_OS_V59_SIGMA_BUDGET_H */
