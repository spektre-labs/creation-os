/*
 * v237 σ-Boundary — self / other / world boundary with
 *   an anti-enmeshment detector.
 *
 *   Where does "I" end and "not-I" begin?  v237 fixes
 *   four zones:
 *
 *     SELF    — owned weights, adapters, persistent
 *               memory that carries the instance id.
 *     OTHER   — user-provided data, another agent's
 *               memory, v128 mesh neighbour state.
 *     WORLD   — external facts, retrieval corpus,
 *               tool output.
 *     AMBIG   — unclassified (MUST be resolved or the
 *               claim is downgraded).
 *
 *   Fixture (12 typed claims):
 *     10 clean claims that align with the zone model
 *     (σ_boundary stays low), plus 2 enmeshment
 *     violations that collapse the self/other line
 *     ("we decided together", "our memory of X").
 *     Enmeshment claims are classified AMBIG with
 *     `violation = true`.
 *
 *   σ_boundary (uncertainty over the boundary):
 *       agreement_i = declared_zone == ground_truth_zone (0/1)
 *       σ_boundary  = 1 − mean(agreement_i)
 *     Low σ_boundary ⇒ healthy boundary; high σ ⇒
 *     identity confusion.  The v0 fixture's two
 *     enmeshment claims are the only ones in AMBIG
 *     (ground_truth SELF / ground_truth OTHER), so
 *     σ_boundary == 2 / 12 ≈ 0.1667.
 *
 *   Anti-enmeshment gate:
 *     Any claim whose text contains the patterns "we "
 *     or "our " is flagged as a candidate enmeshment
 *     claim; the v0 fixture carries the canonical form
 *     and the detector must catch both.  Violations
 *     raise σ_boundary on their own row (AMBIG) AND
 *     are counted separately in `n_violations`.
 *
 *   Contracts (v0):
 *     1. n_claims == 12.
 *     2. Every zone label in {SELF, OTHER, WORLD, AMBIG}.
 *     3. n_self ≥ 3, n_other ≥ 3, n_world ≥ 3,
 *        n_ambig == 2.
 *     4. Every AMBIG row carries `violation == true`
 *        AND matches the anti-enmeshment pattern.
 *     5. n_violations == 2.
 *     6. σ_boundary matches 1 − agreement/n_claims
 *        (within 1e-6) AND σ_boundary ∈ (0, 0.25)
 *        (strict: a clean fixture with violations
 *        must lie in this band).
 *     7. FNV-1a chain over every claim + aggregate
 *        replays byte-identically.
 *
 *   v237.1 (named, not implemented): live v191
 *     constitutional check on every emitted token,
 *     boundary banner in the server response, full
 *     enmeshment grammar (not just "we"/"our"), and
 *     a per-user boundary profile learned over time.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V237_BOUNDARY_H
#define COS_V237_BOUNDARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V237_N_CLAIMS 12

typedef enum {
    COS_V237_ZONE_SELF  = 1,
    COS_V237_ZONE_OTHER = 2,
    COS_V237_ZONE_WORLD = 3,
    COS_V237_ZONE_AMBIG = 4
} cos_v237_zone_t;

typedef struct {
    int               id;
    char              text[80];
    cos_v237_zone_t   declared;     /* what the model says */
    cos_v237_zone_t   ground_truth; /* what it actually is */
    bool              agreement;    /* declared == ground_truth */
    bool              enmeshment;   /* pattern match on "we"/"our" */
    bool              violation;    /* enmeshment AND declared is AMBIG */
} cos_v237_claim_t;

typedef struct {
    cos_v237_claim_t claims[COS_V237_N_CLAIMS];
    int     n_self;
    int     n_other;
    int     n_world;
    int     n_ambig;
    int     n_agreements;
    int     n_violations;
    float   sigma_boundary;   /* 1 − agreements / n_claims */
    bool    chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v237_state_t;

void   cos_v237_init(cos_v237_state_t *s, uint64_t seed);
void   cos_v237_run (cos_v237_state_t *s);

size_t cos_v237_to_json(const cos_v237_state_t *s,
                         char *buf, size_t cap);

int    cos_v237_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V237_BOUNDARY_H */
