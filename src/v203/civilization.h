/*
 * v203 σ-Civilization — institution registry + K_eff
 * collapse detector + continuity score + inter-layer
 * contradiction check + public-good ledger + SCSL
 * integration.
 *
 *   Institution registry: every organisation /
 *   jurisdiction / community registers a profile
 *     (jurisdiction (v199), policy, capability, trust_score).
 *
 *   Collapse detector (v0, closed form): σ-trace per
 *   institution over a 12-tick window.  An institution
 *   collapses if its σ_mean over the last 4 ticks > K_crit
 *   (0.60) — equivalent to v193 coherence-loss signal.
 *   Recoveries are registered when σ drops back below
 *   K_crit for 4 consecutive ticks.
 *
 *   Continuity score (closed form): 1 − collapses /
 *   observations, scaled by (recoveries / collapses) when
 *   collapses > 0.  Stable institutions score near 1;
 *   permanently collapsed near 0.
 *
 *   Inter-layer contradiction check: given a query (topic,
 *   jurisdiction) v203 compares v199 verdict against the
 *   v202 profile norms for that jurisdiction; when they
 *   disagree, σ_contradiction rises and the query is
 *   escalated to REVIEW.
 *
 *   Public-good ledger: every institution contributes
 *   (public_contrib, private_contrib) per tick; the ledger
 *   tracks the running ratio.  SCSL-licensed jurisdictions
 *   report all revenue into public_contrib; closed
 *   jurisdictions split proportionally.
 *
 *   Contracts (v0):
 *     1. ≥ 5 institutions, 4 of them registered with a
 *        declared jurisdiction (≥ 1 SCSL-licensed).
 *     2. At least one institution collapses (σ > K_crit
 *        for 4 ticks) and at least one institution
 *        recovers.
 *     3. Continuity score strictly orders:
 *        stable > recovered > permanently_collapsed.
 *     4. At least one inter-layer contradiction detected
 *        and escalated to REVIEW (σ_contradiction > 0.5).
 *     5. Public-good ratio for SCSL institutions is
 *        strictly higher than for closed institutions.
 *     6. FNV-1a hash chain valid + byte-deterministic.
 *
 * v203.1 (named): live SCSL revenue streaming, v199 norm
 *   feed, v193 coherence link, v115 memory-backed registry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V203_CIVILIZATION_H
#define COS_V203_CIVILIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V203_N_INSTITUTIONS   6
#define COS_V203_N_TICKS         12
#define COS_V203_WINDOW           4
#define COS_V203_N_CONTRA_QUERIES 4
#define COS_V203_STR_MAX         32

typedef enum {
    COS_V203_LIC_SCSL    = 0,
    COS_V203_LIC_CLOSED  = 1,
    COS_V203_LIC_PRIVATE = 2
} cos_v203_license_t;

typedef struct {
    int      id;
    char     name[COS_V203_STR_MAX];
    int      license;            /* cos_v203_license_t */
    int      jurisdiction;       /* 0..2 — v199 */
    float    sigma[COS_V203_N_TICKS];
    float    public_contrib;     /* accumulated over ticks */
    float    private_contrib;
    int      collapses;
    int      recoveries;
    float    continuity_score;
    bool     permanently_collapsed;
} cos_v203_institution_t;

typedef struct {
    int      id;
    int      jurisdiction;
    int      topic;
    int      v199_verdict;          /* PERMITTED / PROHIBITED / REVIEW */
    int      v202_profile_verdict;  /* normative-practice */
    bool     contradiction;
    float    sigma_contradiction;
    bool     escalated_to_review;
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v203_contra_t;

typedef struct {
    cos_v203_institution_t inst[COS_V203_N_INSTITUTIONS];

    int                    n_contra;
    cos_v203_contra_t      contras[COS_V203_N_CONTRA_QUERIES];

    float                  k_crit;                /* 0.60 */
    int                    n_collapses_total;
    int                    n_recoveries_total;
    int                    n_contradictions;

    float                  public_ratio_scsl;
    float                  public_ratio_closed;

    bool                   chain_valid;
    uint64_t               terminal_hash;

    uint64_t               seed;
} cos_v203_state_t;

void   cos_v203_init(cos_v203_state_t *s, uint64_t seed);
void   cos_v203_build(cos_v203_state_t *s);
void   cos_v203_run(cos_v203_state_t *s);

size_t cos_v203_to_json(const cos_v203_state_t *s,
                         char *buf, size_t cap);

int    cos_v203_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V203_CIVILIZATION_H */
