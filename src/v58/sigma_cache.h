/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v58 — σ-Cache: σ-decomposed KV-cache eviction policy.
 *
 * Why this exists (Q2 2026 gap, honest framing):
 *
 *   The 2025–2026 KV-cache eviction literature (StreamingLLM,
 *   H2O, SnapKV, PyramidKV, KIVI, SAGE-KV, G-KV, EntropyCache,
 *   KV Policy / KVP, Attention-Gate) drives eviction with one of:
 *
 *     - attention scores (top-k by mass over a window)
 *     - one-channel entropy of the decoded distribution (EntropyCache)
 *     - a learned RL policy that ranks tokens by predicted utility
 *     - a recency / sink heuristic (StreamingLLM keeps positions [0,k))
 *
 *   None of these use a *decomposed* σ signal as the eviction
 *   driver.  Creation OS v34 already separates epistemic from
 *   aleatoric uncertainty per token (Dirichlet decomposition).
 *   v55 generalises further into σ_input / σ_knowledge /
 *   σ_decoding.  The natural next step the field has not yet
 *   taken: drive KV-cache eviction with the *epistemic
 *   contribution* of each cached entry, not with attention mass
 *   or single-channel entropy.
 *
 *   v58 codifies that step as a small, deterministic, hardware-
 *   realistic kernel: per-cached-token score → branchless
 *   precision tier → branchless decision write → branchless
 *   compaction.  The kernel does not invoke a model, allocate
 *   on the hot path, or open sockets; it operates on a flat
 *   array of `cos_v58_token_t` records that an inference loop
 *   would populate from per-token σ telemetry (v34 + v45).
 *
 *   Three explicit non-claims:
 *     - σ-Cache is NOT a transformer.  It owns no inference path.
 *     - σ-Cache does NOT prove perplexity recovery; it ships a
 *       reproducible eviction policy.  Quality measurements vs
 *       SnapKV / SAGE-KV / EntropyCache require an in-tree model
 *       harness which Creation OS does not embed (creation.md
 *       invariant: no weights, no network calls).
 *     - σ-Cache uses portable scalar arithmetic that the AArch64
 *       autovectoriser and an explicit NEON 4-accumulator path
 *       can target.  No SVE2 / SME assumption.  Apple SME is the
 *       documented future direction (see .cursor/rules), not a
 *       v58 requirement.
 *
 * Decision tier semantics (M / F / I / P, mirrored from v57):
 *   v58 ships at TIER M: deterministic self-test + microbench.
 *   The decision policy itself is interpreted (no formal proof
 *   yet that the policy minimises expected σ_total under the
 *   v34 decomposition); promotions to F (Frama-C contracts on
 *   the kernel) and to a measured A-tier (real-model perplexity
 *   delta vs SnapKV / EntropyCache) are listed in
 *   docs/v58/THE_SIGMA_CACHE.md as planned (P) work.
 *
 * Hardware discipline (from .cursorrules and .cursor/rules):
 *   - 64-byte aligned buffers (`cos_v58_alloc_tokens`)
 *   - branchless decision kernel (no `if` on the hot path)
 *   - 4 NEON accumulators on AArch64; portable scalar elsewhere
 *   - prefetch hint on the score scan
 *   - popcount-based summary aggregation
 *
 *   The kernel does not depend on Accelerate, Metal, Core ML,
 *   or any SME enablement.  It runs on every Creation OS host.
 */
#ifndef CREATION_OS_V58_SIGMA_CACHE_H
#define CREATION_OS_V58_SIGMA_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Per-token cache record                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    int32_t  seq_pos;          /* original token sequence index */
    float    epistemic_contrib;/* Dirichlet-style epistemic σ
                                * contribution of this token (v34) */
    float    aleatoric_signal; /* irreducible noise carried by token
                                * (v34); high values mean the entry is
                                * an unstable predictor and is cheaper
                                * to compress aggressively */
    float    attention_mass;   /* recent attention mass (sliding window
                                * sum); units are policy-dependent */
    uint8_t  is_sink;          /* StreamingLLM sink protection (1 ⇒
                                * always kept at full precision) */
    uint8_t  _reserved[3];     /* keep the struct 24-byte aligned */
} cos_v58_token_t;

/* ------------------------------------------------------------------ */
/* Eviction policy                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    int32_t capacity;          /* maximum kept entries (sinks count
                                * inside the budget; sinks are kept
                                * even if capacity is exceeded by them
                                * — see below) */
    int32_t sink_count;        /* deterministic sink positions
                                * [0, sink_count); kept regardless of
                                * score (StreamingLLM observation) */
    int32_t recency_window;    /* positions within `recency_window`
                                * of the current cursor receive a
                                * gamma_recency bonus to their score */

    float   alpha_epistemic;   /* score weight on epistemic_contrib */
    float   beta_attention;    /* score weight on attention_mass */
    float   gamma_recency;     /* score weight on recency bonus
                                * (1.0 inside the window, 0 outside) */
    float   delta_aleatoric;   /* score *penalty* on aleatoric_signal
                                * (subtracted from the sum) */

    float   tau_full;          /* score >= tau_full   ⇒ KEEP_FULL */
    float   tau_int8;          /* score >= tau_int8   ⇒ KEEP_INT8 */
    /* score >= keep threshold ⇒ KEEP_INT4
     * score <  keep threshold ⇒ EVICTED                 */
} cos_v58_policy_t;

/* Per-entry decision tags (mutually exclusive, byte-packed). */
enum {
    COS_V58_KEEP_FULL = 0,
    COS_V58_KEEP_INT8 = 1,
    COS_V58_KEEP_INT4 = 2,
    COS_V58_EVICTED   = 3
};

typedef struct {
    int32_t kept_full;
    int32_t kept_int8;
    int32_t kept_int4;
    int32_t evicted;
    int32_t sink_protected;
    int32_t kept_total;
    float   keep_threshold;    /* the K-th-largest score after
                                * sinks are subtracted from capacity */
} cos_v58_decision_summary_t;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/* Populate a balanced default policy.  All weights are positive,
 * tau_full > tau_int8, capacity = 512, sink_count = 4 (StreamingLLM
 * default), recency_window = 64. */
void cos_v58_policy_default(cos_v58_policy_t *out);

/* Score one token against the policy.  Pure function; no state. */
float cos_v58_score_token(const cos_v58_token_t *tok,
                          int32_t                current_pos,
                          const cos_v58_policy_t *p);

/* Score a batch.  When built on AArch64 with NEON, uses a
 * 4-accumulator inner loop; otherwise scalar.  `scores_out` must
 * point to at least `n` floats with 16-byte alignment recommended. */
void cos_v58_score_batch(const cos_v58_token_t  *tokens,
                         int32_t                 n,
                         int32_t                 current_pos,
                         const cos_v58_policy_t *p,
                         float                  *scores_out);

/* Branchless eviction kernel.
 *
 *   tokens       — input array (length n)
 *   n            — token count
 *   current_pos  — sequence cursor (used for recency)
 *   p            — policy
 *   decisions_out — output array (length n) filled with COS_V58_*
 *   summary_out  — populated counts + chosen keep_threshold
 *
 *   Returns the number of kept entries (== summary_out->kept_total).
 *   Returns -1 if any input pointer is NULL or n < 0.
 *
 * Sink discipline: tokens with `is_sink == 1` are always KEEP_FULL
 * regardless of capacity.  If capacity < sink_count, the kernel
 * still keeps every sink — the policy's capacity is a soft target
 * for the non-sink slice, mirroring StreamingLLM behaviour.
 *
 * Determinism: same inputs ⇒ identical decisions byte-for-byte.
 */
int cos_v58_decide(const cos_v58_token_t      *tokens,
                   int32_t                     n,
                   int32_t                     current_pos,
                   const cos_v58_policy_t     *p,
                   uint8_t                    *decisions_out,
                   cos_v58_decision_summary_t *summary_out);

/* Branchless compaction.  Walks `decisions` and emits the indices
 * of every entry that is NOT EVICTED into `kept_indices_out` in
 * ascending order.  Returns the kept count, or -1 on bad inputs. */
int cos_v58_compact(const uint8_t *decisions,
                    int32_t        n,
                    int32_t       *kept_indices_out);

/* Tag for a decision byte; returns "?" for unknown values. */
const char *cos_v58_decision_tag(uint8_t decision);

/* 64-byte aligned allocation helper for token arrays.  Returns
 * NULL on n <= 0 or allocation failure.  Free with `free`. */
cos_v58_token_t *cos_v58_alloc_tokens(int32_t n);

/* Version triple for the v58 kernel (major.minor.patch). */
typedef struct {
    int major;
    int minor;
    int patch;
} cos_v58_version_t;

cos_v58_version_t cos_v58_version(void);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V58_SIGMA_CACHE_H */
