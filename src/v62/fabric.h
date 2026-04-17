/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v62 — The Reasoning Fabric (alien-tier insight stack).
 *
 * The 2026 arXiv / Zenodo / OpenReview frontier converged on six
 * findings that, taken together, redraw the agent runtime:
 *
 *   1. Latent CoT (Coconut, arXiv:2412.06769; superposition theory in
 *      ICLR 2026 followups) — reasoning lives in a continuous space,
 *      not in language tokens.  Most CoT tokens are linguistic glue.
 *
 *   2. Energy-Based Transformers (Gladstone et al., arXiv:2507.02092,
 *      ICLR 2026) — predict by minimizing E(input, candidate); the
 *      verifier *is* the model.  +29 % on language System-2 tasks,
 *      99 % fewer forward passes vs DiT on image denoising.
 *
 *   3. Hierarchical Reasoning Model (Sapient, arXiv:2506.21734) —
 *      27 M-param model with a slow H-loop and fast L-loop reaches
 *      40 % ARC-AGI-1 with 1 000 examples and no CoT corpus.  ARC
 *      Prize 2026 analysis: the *outer refinement loop* dominates.
 *
 *   4. Native Sparse Attention (DeepSeek, arXiv:2502.11089; Flash
 *      Sparse Attention 2026) — three-branch hierarchical sparsity
 *      (compress + select + slide) is hardware-aligned and matches
 *      full attention quality at 64 k context.
 *
 *   5. Multi-Token Prediction (DeepSeek-V3, arXiv:2412.19437; LK-loss
 *      fine-tunes 2026) — MTP heads as speculative drafters give
 *      +220 % decode speed at temperature 0 with full causal chain.
 *
 *   6. Adaptive KV management (ARKV, arXiv:2603.08727; SAGE-KV) —
 *      per-token states {ORIG, QUANT, EVICT} cut KV memory 4× while
 *      preserving ~97 % accuracy on long-context tasks.
 *
 * Creation OS already protects *execution* (v60 σ-Shield runtime
 * security kernel + v61 Σ-Citadel BLP+Biba lattice + attestation).
 * v62 protects and accelerates *reasoning itself* — every loop
 * iteration is gated by σ-coherence and Σ-lattice; every speculative
 * draft is verified by an energy function, not by hope.
 *
 * Hardware discipline (`AGENTS.md`, `.cursorrules`, M4 invariants):
 *
 *   - All buffers `aligned_alloc(64, ...)`.  Never `malloc` on the
 *     hot path.  64-byte alignment = one M4 cache line per NEON load.
 *
 *   - Prefetch the next iteration in every weight / token walk
 *     (`__builtin_prefetch(p + 64, 0, 3)`).
 *
 *   - Branchless hot paths: select via 0/1 masks (`-(cond)`), never
 *     `if` inside the inner loop.  The CPU does not speculate
 *     branches that do not exist.
 *
 *   - Four NEON accumulators in parallel so the M4's wide decode
 *     stays full.  Apple SME path (compile-only stub today, gated
 *     by `COS_V62_SME=1`) targets the M4 matrix engine when the
 *     toolchain catches up.
 *
 *   - mmap large state.  Disk is memory.  No `fread`.
 *
 *   - Priority order (10. PRIORITY ORDER, .cursorrules):
 *       L0  lookup hit                → done
 *       L1  kernel logic              → done
 *       L2  energy-verified candidate → done
 *       L3  full transformer pass     → only then.
 *
 * Tier semantics (v57 dialect):
 *   M  this kernel runs and is checked at runtime by `make check-v62`.
 *   I  contracts that depend on external weights / runtimes are
 *      documented and exercised on synthetic inputs only.
 *   P  hardware paths (SME, Neural Engine wiring) are planned and
 *      compile-only.
 *
 * v62 ships M-tier for all six modules and the σ/Σ composition.
 * No silent downgrades.
 */

#ifndef COS_V62_FABRIC_H
#define COS_V62_FABRIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Shared types — alien-tier means: one vocabulary, six modules.
 * ==================================================================== */

/*  cos_v62_thought_t — a continuous "thought" vector (Coconut).
 *
 *  In Coconut and HRM the model's last hidden state is fed back
 *  directly as the next input embedding, with no detokenize→retokenize
 *  round trip.  We model that as a stable (dim, layer, step) record
 *  carrying the latent vector plus the σ-coherence and EBT energy.
 */
typedef struct {
    float    *vec;         /* aligned_alloc(64, dim*4) — owned        */
    uint32_t  dim;         /* vector dimensionality                    */
    uint32_t  layer;       /* logical layer of origin                  */
    uint32_t  step;        /* monotone step counter                    */
    float     sigma;       /* σ-coherence in [0, 1]; 1 = coherent      */
    float     energy;      /* EBT energy of this thought; lower=better */
} cos_v62_thought_t;

/*  cos_v62_decision_t — composed gate verdict for one reasoning step.
 *
 *  Composes three independent signals branchlessly:
 *      v60 σ-Shield action   — was the action allowed?
 *      v61 Σ-Citadel lattice — does the data flow honour BLP+Biba?
 *      v62 EBT verifier      — is the candidate energy below budget?
 *
 *  Each lane is 0/1; the byte is the 3-bit composition.  No branches
 *  on the hot path; downstream code can read `.allow` (single AND)
 *  or inspect the failing lane(s) for telemetry.
 */
typedef struct {
    uint8_t v60_ok;        /* 1 if σ-Shield permitted the action      */
    uint8_t v61_ok;        /* 1 if Σ-Citadel lattice permitted flow   */
    uint8_t v62_ok;        /* 1 if EBT energy ≤ budget                */
    uint8_t allow;         /* v60_ok & v61_ok & v62_ok                */
} cos_v62_decision_t;

/*  cos_v62_budget_t — explicit, observable test-time-compute budget.
 *
 *  Adaptive Rectification Sampling (arXiv:2504.01317) and the broader
 *  test-time-scaling literature (Zylos 2026 review) all converge on
 *  the same need: the agent must *see* its own budget, not consume it
 *  blindly.  Every v62 module accepts this struct and never silently
 *  exceeds it; exhaustion is reported as a SKIP, never as a FAIL.
 */
typedef struct {
    uint32_t max_thoughts;     /* hard cap on Coconut-loop steps      */
    uint32_t max_ebt_steps;    /* gradient-descent steps per verify   */
    uint32_t max_h_iters;      /* HRM outer (slow) loop budget        */
    uint32_t max_l_iters;      /* HRM inner (fast) loop budget        */
    uint32_t max_draft;        /* speculative-draft K                  */
    uint32_t max_kv_tokens;    /* ARKV cap on resident-original tokens */
} cos_v62_budget_t;

/* ====================================================================
 *  1.  Latent Chain-of-Thought  (Coconut-class)
 *      reasoning lives in a continuous space, not in tokens.
 * ==================================================================== */

/*  Allocate a thought vector with the M4 invariants from .cursorrules.
 *  Returns NULL on allocation failure; never partially-initialised.   */
cos_v62_thought_t *cos_v62_thought_new(uint32_t dim);
void               cos_v62_thought_free(cos_v62_thought_t *t);

/*  cos_v62_latent_step
 *
 *  One Coconut-style continuous step:
 *      thought_{t+1} = normalize( thought_t + alpha * gradient )
 *
 *  We model `gradient` as a caller-supplied vector (in production it
 *  is the model's last-hidden-state delta).  `alpha` is the step size.
 *  The σ-coherence is recomputed as 1 - clamp(||delta||, 0, 1).
 *
 *  Branchless:  no `if` inside the loop; the clamp is a min/max pair.
 *  NEON-aligned: 4 accumulators, prefetch +64 B ahead.
 *  Returns 0 on success.
 */
int cos_v62_latent_step(cos_v62_thought_t       *thought,
                        const float             *gradient,
                        float                    alpha);

/*  cos_v62_latent_loop
 *
 *  Run up to `budget->max_thoughts` continuous-CoT steps, halting
 *  early when σ-coherence ≥ `sigma_target` (BFS-like convergence in
 *  superposition; ICLR 2026 theoretical justification).  Caller
 *  supplies a callback that produces the next gradient given the
 *  current thought; the callback is allowed to return non-zero to
 *  request a hard stop (e.g. when the σ-Shield denies a tool call).
 */
typedef int (*cos_v62_grad_fn)(const cos_v62_thought_t *thought,
                               float                   *out_grad,
                               void                    *ctx);

int cos_v62_latent_loop(cos_v62_thought_t        *thought,
                        cos_v62_grad_fn           grad_fn,
                        void                     *grad_ctx,
                        const cos_v62_budget_t   *budget,
                        float                     alpha,
                        float                     sigma_target,
                        uint32_t                 *out_steps);

/* ====================================================================
 *  2.  Energy-Based Verifier  (EBT-class)
 *      "the verifier is the model."  arXiv:2507.02092.
 * ==================================================================== */

/*  cos_v62_energy
 *
 *  Default deterministic energy:
 *      E(x, y) = 0.5 * ||x - y||^2  +  beta * (1 - cos(x, y))
 *
 *  Two terms because EBTs need both a magnitude term (penalises
 *  large prediction-input mismatch) and a directional term (penalises
 *  semantic drift even at matched magnitude).  Branchless; runs in
 *  NEON 4-way; constant-time over equal-length vectors.
 */
float cos_v62_energy(const float *input,
                     const float *candidate,
                     uint32_t     dim,
                     float        beta);

/*  cos_v62_ebt_minimize
 *
 *  Branchless gradient-descent in candidate space:
 *      y_{k+1} = y_k - eta * dE/dy
 *
 *  We use the analytic gradient of the default energy
 *  (cheap on-device; no autograd).  Stops when energy delta < tol or
 *  budget->max_ebt_steps is reached.  Returns the final energy in
 *  *out_energy and the actual step count in *out_steps.
 */
int cos_v62_ebt_minimize(const float            *input,
                         float                  *candidate,
                         uint32_t                dim,
                         float                   beta,
                         float                   eta,
                         float                   tol,
                         const cos_v62_budget_t *budget,
                         float                  *out_energy,
                         uint32_t               *out_steps);

/* ====================================================================
 *  3.  Hierarchical Reasoning Loop  (HRM-class)
 *      H-slow + L-fast nested recurrence.  arXiv:2506.21734.
 * ==================================================================== */

/*  Run the HRM-style nested loop:
 *
 *      for h in 1..max_h_iters:
 *          for l in 1..max_l_iters:
 *              L = update_L(L, H)         // fast
 *          H = update_H(H, L)             // slow
 *
 *  Updates are NEON SAXPY (residual: x ← x + rate * (y - x)); both
 *  rates are caller-tunable.  The ARC-Prize 2026 analysis showed the
 *  *outer* refinement loop dominates performance, so the H-update
 *  actually carries most of the signal — we expose `out_h_iters` and
 *  `out_l_iters` so the caller can monitor both.
 *
 *  σ-coherence is recomputed after each H-update; the loop halts
 *  early once coherence ≥ sigma_target.
 */
int cos_v62_hrm_run(cos_v62_thought_t        *H,
                    cos_v62_thought_t        *L,
                    float                     h_rate,
                    float                     l_rate,
                    float                     sigma_target,
                    const cos_v62_budget_t   *budget,
                    uint32_t                 *out_h_iters,
                    uint32_t                 *out_l_iters);

/* ====================================================================
 *  4.  Native Sparse Attention
 *      three branches: compress + select + slide.  arXiv:2502.11089.
 * ==================================================================== */

/*  Compute the NSAttn gate weights and a fused attention output.
 *
 *  We expose the three-branch design as a single call:
 *      out = w_c * Compress(K, V)  +  w_s * Select_topK(K, V)
 *          + w_l * SlidingWindow(K, V)
 *
 *  Inputs:
 *      Q     [d]              query
 *      K, V  [n, d]            keys and values, mmap-friendly layout
 *      n      number of context tokens
 *      d      head dimension
 *      window  sliding-window size (local branch)
 *      topk    fine-grain selection (selected branch)
 *      block   coarse-grain block size (compress branch)
 *      gate[3] caller-supplied (w_c, w_s, w_l) — sum freely
 *      out  [d]               result
 *
 *  Branchless top-K via two-pointer partition (no `if` in inner loop);
 *  prefetch on K and V; 4-way NEON dot inside `Compress`.  Designed
 *  to align cleanly with mmap'd KV blocks (64-B row stride).
 */
int cos_v62_nsa_attend(const float *Q,
                       const float *K,
                       const float *V,
                       uint32_t     n,
                       uint32_t     d,
                       uint32_t     window,
                       uint32_t     topk,
                       uint32_t     block,
                       const float  gate[3],
                       float       *out);

/* ====================================================================
 *  5.  Multi-Token Predictor / Speculative Drafter  (MTP-class)
 *      DeepSeek-V3 MTP, arXiv:2412.19437; +220 % LK-loss tunes 2026.
 * ==================================================================== */

/*  cos_v62_mtp_draft
 *
 *  Given the current logits over a vocabulary, draft the next K
 *  tokens *with a complete causal chain* (DeepSeek-V3 innovation —
 *  not independent heads).  We keep the math hardware-trivial: each
 *  step picks argmax of (logits + bias_k) where bias_k is a
 *  caller-supplied draft head.  Branchless argmax in NEON.
 *
 *  The chain is returned together with a per-step confidence so the
 *  verifier can decide acceptance length without a second pass.
 */
int cos_v62_mtp_draft(const float *logits,
                      uint32_t     vocab,
                      const float *bias_heads,   /* [K, vocab]       */
                      uint32_t     K,
                      uint32_t    *out_tokens,    /* [K]              */
                      float       *out_conf);     /* [K]              */

/*  cos_v62_mtp_verify
 *
 *  Branchless verification of a K-step draft against the "true" next
 *  tokens (in production: the target model's argmax of its own
 *  logits at each accepted position).  Returns the acceptance length
 *  in [0, K].  No conditional branches in the loop body.
 */
uint32_t cos_v62_mtp_verify(const uint32_t *draft,
                            const uint32_t *truth,
                            uint32_t        K);

/* ====================================================================
 *  6.  Adaptive KV management  (ARKV-class)
 *      per-token state ∈ {ORIG, QUANT, EVICT}.  arXiv:2603.08727.
 * ==================================================================== */

enum {
    COS_V62_KV_ORIG  = 0,    /* full precision, hot                  */
    COS_V62_KV_QUANT = 1,    /* lossy, warm                          */
    COS_V62_KV_EVICT = 2,    /* dropped, cold                        */
};

typedef struct {
    uint8_t  *state;         /* [n] one byte per token                */
    float    *score;         /* [n] running attention weight          */
    uint32_t  n;             /* token count                           */
    uint32_t  cap_orig;      /* max tokens kept in ORIG               */
    uint32_t  cap_quant;     /* max tokens kept in QUANT              */
} cos_v62_arkv_t;

cos_v62_arkv_t *cos_v62_arkv_new(uint32_t n,
                                 uint32_t cap_orig,
                                 uint32_t cap_quant);
void            cos_v62_arkv_free(cos_v62_arkv_t *a);

/*  Update score with a new attention-weight vector and re-classify
 *  every token branchlessly into {ORIG, QUANT, EVICT}.  Returns the
 *  number of tokens currently held in COS_V62_KV_ORIG.
 */
uint32_t cos_v62_arkv_update(cos_v62_arkv_t *a,
                             const float    *new_attn_weights);

/* ====================================================================
 *  Composition with v60 σ-Shield + v61 Σ-Citadel
 * ==================================================================== */

/*  cos_v62_compose_decision
 *
 *  Branchless 3-lane composition for one reasoning step.  Returns 0
 *  iff `out` is non-NULL and was populated.  No allocations.
 */
int cos_v62_compose_decision(uint8_t              v60_ok,
                             uint8_t              v61_ok,
                             uint8_t              v62_ok,
                             cos_v62_decision_t  *out);

/* ====================================================================
 *  Misc.
 * ==================================================================== */

const char *cos_v62_version(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V62_FABRIC_H */
