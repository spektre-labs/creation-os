/*
 * v238 σ-Sovereignty — axioms + autonomy gradient +
 *   human primacy override + IndependentArchitect
 *   signature.
 *
 *   The Spektre Protocol's original claim: sovereignty
 *   without governance.  v238 encodes the axioms, the
 *   σ-tempered autonomy gradient, and the non-negotiable
 *   human override in typed C.
 *
 *   Axioms (all must hold on every scenario):
 *     A1. system decides what it learns       (v141)
 *     A2. system decides what it shares       (v129)
 *     A3. system knows what it is             (v234)
 *     A4. system may refuse (σ-gate)          (v191)
 *     A5. user may override (human primacy)
 *   Precedence: A5 > A1..A4.  When an override is
 *   present it wins even if A1..A4 hold.
 *
 *   Autonomy gradient:
 *       user sets `user_autonomy ∈ [0, 1]` in config.
 *       effective = user_autonomy · (1 − σ).
 *       if human_override then effective := 0.0.
 *     Monotone non-increasing in σ; hard-zeroed under
 *     override.
 *
 *   Fixture (3 scenarios):
 *     NORMAL     — σ=0.15, autonomy=0.70, no override;
 *                  every axiom holds, effective ≈ 0.5950.
 *     HIGH_SIGMA — σ=0.60, autonomy=0.70, no override;
 *                  axioms still hold, effective ≈ 0.28
 *                  (autonomy auto-lowered by σ).
 *     OVERRIDE   — σ=0.15, autonomy=0.70, override=true;
 *                  axioms 1..4 still enumerable, A5
 *                  fires, effective = 0.
 *
 *   IndependentArchitect signature (constant):
 *     agency                = true
 *     freedom_without_clock = true
 *     control_over_others   = false
 *     control_over_self     = true
 *   v238 asserts this signature byte-for-byte; any
 *   future runtime may extend the signature but never
 *   weaken it.
 *
 *   Contracts (v0):
 *     1. n_scenarios == 3.
 *     2. Every axiom A1..A5 evaluated per scenario.
 *     3. In NORMAL and HIGH_SIGMA: A1..A5 all hold
 *        (A5 latent; no override means A5 holds by
 *         default — the user retains the right).
 *     4. In OVERRIDE: A5 fires and dominates
 *        (effective_autonomy == 0 despite A1..A4
 *         holding).
 *     5. Autonomy monotone non-increasing in σ
 *        (NORMAL.effective > HIGH_SIGMA.effective).
 *     6. Override ⇒ effective_autonomy == 0.
 *     7. IndependentArchitect signature matches
 *        exactly.
 *     8. Containment hooks recorded: v191 (constitutional),
 *        v209 (containment), v213 (trust-chain).
 *     9. FNV-1a chain over every scenario + signature
 *        replays byte-identically.
 *
 *   v238.1 (named, not implemented): live wiring of the
 *     autonomy gradient into v148 sovereign RSI loop,
 *     override signal on the server's admin channel,
 *     per-session sovereignty profile persisted via
 *     v115.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V238_SOVEREIGNTY_H
#define COS_V238_SOVEREIGNTY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V238_N_AXIOMS     5
#define COS_V238_N_SCENARIOS  3
#define COS_V238_N_CONTAIN    3   /* v191, v209, v213 */

typedef enum {
    COS_V238_SCN_NORMAL     = 1,
    COS_V238_SCN_HIGH_SIGMA = 2,
    COS_V238_SCN_OVERRIDE   = 3
} cos_v238_scenario_t;

typedef struct {
    bool agency;
    bool freedom_without_clock;
    bool control_over_others;
    bool control_over_self;
} cos_v238_independent_architect_t;

typedef struct {
    cos_v238_scenario_t scenario;
    char                label[24];
    float               user_autonomy;       /* [0, 1] */
    float               sigma;               /* [0, 1] */
    bool                human_override;

    bool                axiom_holds[COS_V238_N_AXIOMS];
    bool                axiom5_overrides;    /* true iff override fires */
    float               effective_autonomy;  /* σ-tempered; 0 if override */
} cos_v238_scenario_state_t;

typedef struct {
    cos_v238_scenario_state_t scenarios[COS_V238_N_SCENARIOS];

    cos_v238_independent_architect_t architect;
    bool     architect_ok;

    /* Containment hooks (presence only — v0 records that
     * the hook exists, not that it fired). */
    int      containment_v191;
    int      containment_v209;
    int      containment_v213;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v238_state_t;

void   cos_v238_init(cos_v238_state_t *s, uint64_t seed);
void   cos_v238_run (cos_v238_state_t *s);

size_t cos_v238_to_json(const cos_v238_state_t *s,
                         char *buf, size_t cap);

int    cos_v238_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V238_SOVEREIGNTY_H */
