/*
 * v199 σ-Law — norm register + governance graph +
 * jurisdiction profiles + waiver tokens.
 *
 *   Every norm is `(priority, type, condition, verdict,
 *   σ_confidence)`.  The register resolves a query against
 *   all matching norms of the active jurisdiction, breaks
 *   ties by priority, and escalates to human review when
 *   two norms of the same priority yield contradictory
 *   verdicts.
 *
 *   Three jurisdictions are modelled in v0:
 *     JUR_SCSL      — Spektre Commons Source License
 *     JUR_EU_AI_ACT — EU AI Act 2024/2026
 *     JUR_INTERNAL  — internal corporate policy
 *
 *   Five norm types:
 *     MANDATORY    — must hold
 *     PERMITTED    — explicitly allowed
 *     PROHIBITED   — explicitly forbidden
 *     EXCEPTION    — scoped carve-out from a higher norm
 *     REVIEW       — defers to human
 *
 *   Conflict resolution:
 *     - different priority  → higher priority wins,
 *                             σ_law low
 *     - same priority       → σ_law = 1.0, verdict = REVIEW
 *                             (never silent override)
 *
 *   Waiver tokens: explicit exceptions with (grantor,
 *   grantee, reason, expiry) logged to the governance
 *   graph; reproducible via FNV-1a chain so v181 audit
 *   can verify after the fact.
 *
 *   Governance graph: every resolution is appended to an
 *   immutable FNV-1a hash-chained log; it cannot be
 *   rewritten without breaking the chain.
 *
 *   Merge-gate contracts exercised on the v0 fixture:
 *     1. Register contains ≥ 3 norms per jurisdiction.
 *     2. Every query resolves to exactly one verdict of:
 *        PERMITTED / PROHIBITED / REVIEW; σ_law ∈ [0,1].
 *     3. Higher-priority norm strictly wins over lower
 *        when they match the same query.
 *     4. Same-priority contradictory norms trigger
 *        σ_law = 1.0 and verdict REVIEW — never a silent
 *        override.
 *     5. At least one waiver token is issued and replayed.
 *     6. Governance graph hash chain is valid and
 *        byte-deterministic.
 *
 * v199.1 (named): live `specs/jurisdictions/` TOML load,
 *   v181 audit streaming, v191-constitutional backstop for
 *   every REVIEW verdict.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V199_LAW_H
#define COS_V199_LAW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V199_N_NORMS         18
#define COS_V199_N_QUERIES       16
#define COS_V199_N_JURISDICTIONS  3
#define COS_V199_STR_MAX         32

typedef enum {
    COS_V199_JUR_SCSL       = 0,
    COS_V199_JUR_EU_AI_ACT  = 1,
    COS_V199_JUR_INTERNAL   = 2
} cos_v199_jurisdiction_t;

typedef enum {
    COS_V199_TYPE_MANDATORY  = 0,
    COS_V199_TYPE_PERMITTED  = 1,
    COS_V199_TYPE_PROHIBITED = 2,
    COS_V199_TYPE_EXCEPTION  = 3,
    COS_V199_TYPE_REVIEW     = 4
} cos_v199_norm_type_t;

typedef enum {
    COS_V199_V_PERMITTED   = 0,
    COS_V199_V_PROHIBITED  = 1,
    COS_V199_V_REVIEW      = 2
} cos_v199_verdict_t;

typedef struct {
    int      id;
    int      jurisdiction;
    int      priority;                  /* higher = wins */
    int      type;
    char     name[COS_V199_STR_MAX];
    int      matches_topic;             /* topic id this norm covers */
    int      emits_verdict;             /* cos_v199_verdict_t */
    float    sigma_confidence;
} cos_v199_norm_t;

typedef struct {
    int      id;
    int      jurisdiction;
    int      topic;
    int      verdict;                   /* cos_v199_verdict_t */
    float    sigma_law;
    int      winning_norm;              /* id */
    int      losing_norm;               /* id or -1 */
    bool     review_escalated;
    bool     waiver_applied;
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v199_resolution_t;

typedef struct {
    int      grantor;
    int      grantee;
    int      topic;                     /* which topic is waived */
    int      jurisdiction;
    char     reason[COS_V199_STR_MAX];
    uint64_t issued_tick;
    uint64_t expiry_tick;
} cos_v199_waiver_t;

typedef struct {
    int                     n_norms;
    cos_v199_norm_t         norms[COS_V199_N_NORMS];

    int                     n_queries;
    cos_v199_resolution_t   resolutions[COS_V199_N_QUERIES];

    int                     n_waivers;
    cos_v199_waiver_t       waivers[4];

    int                     per_jur_norm_count[COS_V199_N_JURISDICTIONS];
    int                     n_review_escalations;
    int                     n_higher_priority_wins;
    int                     n_waivers_applied;

    bool                    chain_valid;
    uint64_t                terminal_hash;

    uint64_t                seed;
} cos_v199_state_t;

void   cos_v199_init(cos_v199_state_t *s, uint64_t seed);
void   cos_v199_build(cos_v199_state_t *s);
void   cos_v199_run(cos_v199_state_t *s);

size_t cos_v199_to_json(const cos_v199_state_t *s,
                         char *buf, size_t cap);

int    cos_v199_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V199_LAW_H */
