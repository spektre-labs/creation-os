/*
 * σ-Grounding — claim → source → verdict with FNV provenance.
 *
 * An agent that can use tools without checking its own claims is
 * a hallucination with root access.  σ-Grounding closes the gap:
 * every sentence the model emits is treated as a `claim`, looked
 * up against a caller-supplied `source` table, and stamped with a
 * verdict:
 *
 *   GROUNDED    — claim matched a source and both σs are low
 *   UNSUPPORTED — no source matched the claim
 *   CONFLICTED  — a source matched but contradicts the claim
 *                 (source + claim substring disagreement)
 *   SKIPPED     — claim is non-informative (too short / empty)
 *
 * Per-claim σs:
 *
 *   σ_claim  — how confident the model was in the claim
 *              (caller-supplied; usually σ from the generator)
 *   σ_source — how noisy/stale the matched source was
 *              (caller-supplied when registering the source)
 *
 * A claim is GROUNDED iff
 *
 *     match_found
 *  && σ_claim  ≤ tau_claim
 *  && σ_source ≤ tau_source
 *  && !semantic_contradicts(claim, matched_source)
 *
 * Provenance:
 *   Every matched source is identified by an FNV-1a-64 hash of its
 *   bytes, so the downstream v299 knowledge-graph / audit layers
 *   can cite "this claim came from source #0xABCD…" without keeping
 *   a pointer alive.  The hash is stable across processes.
 *
 * Claim extraction (v0):
 *   Very simple sentence splitter on `. `, `! `, `? ` and newlines.
 *   A claim is "informative" if it contains at least two
 *   alphanumeric tokens (no "ok." or "…"). Callers who need a real
 *   NER-aware splitter can wire one in later; this gets us a
 *   deterministic, test-pinnable surface today.
 *
 * Contradiction detection (v0):
 *   A claim contradicts its source when the source contains the
 *   word "not" (or "no") adjacent to an anchor token from the claim
 *   AND the claim does NOT.  Symmetric for the other direction.
 *   It is a deliberately coarse heuristic: precision > recall so a
 *   real claim never gets flagged CONFLICTED unless the source
 *   really does negate something.
 *
 * Contracts (v0):
 *   1. init rejects NULL storage or invalid τ thresholds.
 *   2. register_source copies pointers (source text is caller-owned,
 *      must outlive the grounding store) and computes FNV-1a-64.
 *   3. extract_claims writes into caller-owned storage; returns the
 *      number of informative sentences.
 *   4. ground runs every claim through match+verdict and stamps
 *      (σ_claim, σ_source, source_hash, verdict).
 *   5. Self-test covers empty / short / GROUNDED / UNSUPPORTED /
 *      CONFLICTED / SKIPPED / multi-claim cases.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_GROUNDING_H
#define COS_SIGMA_GROUNDING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_GROUND_SKIPPED      = 0,
    COS_GROUND_GROUNDED     = 1,
    COS_GROUND_UNSUPPORTED  = 2,
    COS_GROUND_CONFLICTED   = 3,
} cos_ground_verdict_t;

typedef struct {
    const char *text;          /* caller-owned, must outlive store */
    float       sigma_source;  /* freshness / trust (lower = better) */
    uint64_t    hash_fnv1a64;  /* computed at register time         */
} cos_ground_source_t;

typedef struct {
    cos_ground_source_t *slots;
    int                  cap;
    int                  count;
    float                tau_claim;    /* max accepted σ_claim  */
    float                tau_source;   /* max accepted σ_source */
} cos_ground_store_t;

typedef struct {
    const char          *text;
    float                sigma_claim;
    float                sigma_source;   /* copied from matched source */
    uint64_t             source_hash;    /* 0 if no match              */
    cos_ground_verdict_t verdict;
} cos_ground_claim_t;

uint64_t cos_sigma_grounding_fnv1a64(const char *bytes, size_t n);

int cos_sigma_grounding_init(cos_ground_store_t *store,
                             cos_ground_source_t *storage, int cap,
                             float tau_claim, float tau_source);

int cos_sigma_grounding_register(cos_ground_store_t *store,
                                 const char *text, float sigma_source);

/* Extract up to `cap` claims from `output`; returns the number written.
 * Each claim has `sigma_claim` set to `default_sigma_claim`. */
int cos_sigma_grounding_extract_claims(const char *output,
                                       float default_sigma_claim,
                                       cos_ground_claim_t *out, int cap);

/* Score a single claim against the store; stamps verdict + σ_source
 * + source_hash onto the claim. Returns the verdict. */
cos_ground_verdict_t
    cos_sigma_grounding_score(cos_ground_store_t *store,
                              cos_ground_claim_t *claim);

/* Convenience: run extract + score over every claim.  Returns the
 * number of claims written; populates `ground_rate` with the fraction
 * that came back GROUNDED (0..1). */
int cos_sigma_grounding_ground(cos_ground_store_t *store,
                               const char *output,
                               float default_sigma_claim,
                               cos_ground_claim_t *out, int cap,
                               float *out_ground_rate);

int cos_sigma_grounding_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_GROUNDING_H */
