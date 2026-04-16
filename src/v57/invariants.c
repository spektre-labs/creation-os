/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v57 Verified Agent — invariant registry data.
 *
 * Honesty discipline: every check below is tagged with the tier
 * it actually meets *today* in this repository, not the tier we
 * would like it to meet.  Promotions require shipping the proof
 * artifact or the deterministic check; they do not happen by
 * narrative alone.
 *
 * Each invariant carries up to four claims:
 *   - runtime    (M): a make / shell target that returns 0 iff the
 *                     deterministic check passes
 *   - formal     (F): a proof artifact path (Frama-C / TLA+ / sby /
 *                     Coq) that mechanically discharges the claim
 *   - documented (I): a doc path explaining why the invariant is
 *                     reasonable when M and F are not yet shipped
 *   - planned    (P): the next concrete step that would elevate
 *                     this invariant to M or F
 *
 * The table below is the *single source of truth* for what
 * Creation OS v57 claims about itself.  Don't paraphrase it in docs
 * without keeping the tiers in sync.
 */
#include "invariants.h"

#include <string.h>

static const v57_invariant_t k_invariants[] = {
    /* ---------------------------------------------------------------- */
    {
        .id              = "sigma_gated_actions",
        .statement       = "∀ action a executed by the agent: a is "
                           "issued only if the σ of the reasoning that "
                           "produced a is below the configured threshold "
                           "for the current task tier.",
        .rationale       = "Without a σ-gate the agent emits high-σ "
                           "actions indistinguishable from low-σ ones; "
                           "any downstream sandbox then sees the same "
                           "signature as a confident decision.  The "
                           "v53 σ-TAOR loop refuses to emit when σ "
                           "exceeds the per-tier threshold.",
        .owner_versions  = "v53 (σ-TAOR loop), v34 (σ decomposition)",
        .runtime = { 1, V57_TIER_M, "make check-v53",
                     "13/13 deterministic σ-TAOR self-tests in src/v53" },
        .formal  = { 1, V57_TIER_F, "src/v47/sigma_kernel_verified.c",
                     "ACSL contracts on σ-kernel surface "
                     "(make verify-c discharges via Frama-C/WP "
                     "when toolchain is present)" },
        .documented = { 1, V57_TIER_I, "docs/v53/ARCHITECTURE.md",
                        "σ-gate semantics and per-tier thresholds" },
        .planned    = { 0, V57_TIER_P, NULL, NULL },
    },
    /* ---------------------------------------------------------------- */
    {
        .id              = "bounded_side_effects",
        .statement       = "∀ tool execution e dispatched by the agent: "
                           "the set of side effects of e is a subset of "
                           "the capabilities declared in the active "
                           "sandbox policy at the moment e was admitted.",
        .rationale       = "An agent that can silently exceed its "
                           "declared capabilities cannot be reasoned "
                           "about by either the user or the operator. "
                           "The v48 sandbox policy is the run-time "
                           "subset check; it admits or denies tool "
                           "calls before they reach the host.",
        .owner_versions  = "v48 (sandbox + defense layers)",
        .runtime = { 1, V57_TIER_M, "make check-v48",
                     "v48 sandbox + defense_layers self-tests" },
        .formal  = { 0, V57_TIER_F, NULL, NULL },
        .documented = { 1, V57_TIER_I, "src/v48/sandbox.h",
                        "explicit capability set, deny-by-default" },
        .planned    = { 1, V57_TIER_P, NULL,
                        "TLA+ specification of the sandbox-policy "
                        "subset relation (proofs/v57/sandbox.tla)" },
    },
    /* ---------------------------------------------------------------- */
    {
        .id              = "no_unbounded_self_modification",
        .statement       = "∀ in-place test-time training update u "
                           "applied by the agent: u is gated by a σ "
                           "drift threshold, fits within a declared "
                           "byte budget, and is rolled back if the "
                           "post-update σ regresses.",
        .rationale       = "IP-TTT-style fast-weight updates are the "
                           "newest attack surface for agent runtimes — "
                           "they let the model edit itself between "
                           "tokens.  v56's σ-Constitutional controller "
                           "requires every update to strictly lower σ "
                           "or be rolled back.",
        .owner_versions  = "v56 (σ-gated IP-TTT budget)",
        .runtime = { 1, V57_TIER_M, "make check-v56",
                     "56/56 deterministic v56 self-tests, including "
                     "ipttt cooldown / drift / rollback policy" },
        .formal  = { 0, V57_TIER_F, NULL, NULL },
        .documented = { 1, V57_TIER_I, "docs/v56/ARCHITECTURE.md",
                        "σ-Constitutional invariant K_eff = (1-σ)·K "
                        "extended to weight-space updates" },
        .planned    = { 0, V57_TIER_P, NULL, NULL },
    },
    /* ---------------------------------------------------------------- */
    {
        .id              = "deterministic_verifier_floor",
        .statement       = "∀ multi-step reasoning chain c emitted by "
                           "the agent: c is graded by a rule-based, "
                           "deterministic verifier whose precision is "
                           "the agent's verifier-σ floor; high "
                           "verifier-σ blocks downstream commits.",
        .rationale       = "Neural process reward models hit a "
                           "false-positive ceiling.  v56's VPRM-style "
                           "rule verifier is deterministic, "
                           "zero-parameter, single-pass; its precision "
                           "is the floor under any learned PRM.",
        .owner_versions  = "v56 (rule-based process verifier)",
        .runtime = { 1, V57_TIER_M, "make check-v56",
                     "verifier rules + integration tests in v56 "
                     "self-test suite" },
        .formal  = { 0, V57_TIER_F, NULL, NULL },
        .documented = { 1, V57_TIER_I, "docs/v56/POSITIONING.md",
                        "rule-floor vs neural PRM ceiling" },
        .planned    = { 0, V57_TIER_P, NULL, NULL },
    },
    /* ---------------------------------------------------------------- */
    {
        .id              = "ensemble_disagreement_abstains",
        .statement       = "∀ high-stakes query q routed to the σ-"
                           "proconductor: if the σ-weighted ensemble "
                           "of subagents disagrees beyond the "
                           "configured threshold, the proconductor "
                           "abstains and surfaces the disagreement "
                           "rather than picking a side.",
        .rationale       = "Routing-only multi-LLM systems return a "
                           "single answer even when their members "
                           "disagree.  v54's σ-proconductor uses the "
                           "ensemble's disagreement as an additional "
                           "epistemic σ channel and abstains on "
                           "real disagreement.",
        .owner_versions  = "v54 (σ-proconductor)",
        .runtime = { 1, V57_TIER_M, "make check-v54",
                     "v54 σ-proconductor self-tests, including "
                     "abstention on disagreement" },
        .formal  = { 0, V57_TIER_F, NULL, NULL },
        .documented = { 1, V57_TIER_I, "docs/v54/ARCHITECTURE.md",
                        "σ-triangulated convergence and abstention" },
        .planned    = { 0, V57_TIER_P, NULL, NULL },
    },
};

static const int k_invariant_count =
    (int)(sizeof(k_invariants) / sizeof(k_invariants[0]));

int v57_invariant_count(void)
{
    return k_invariant_count;
}

const v57_invariant_t *v57_invariant_get(int index)
{
    if (index < 0 || index >= k_invariant_count) return NULL;
    return &k_invariants[index];
}

const v57_invariant_t *v57_invariant_find(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < k_invariant_count; ++i) {
        const char *cand = k_invariants[i].id;
        if (cand && strcmp(cand, id) == 0) return &k_invariants[i];
    }
    return NULL;
}

/* Branchless tier ordering: M > F > I > P > none.
 * Implementation uses simple comparisons (no allocation, no branches
 * beyond the trivial enable check). */
v57_tier_t v57_invariant_best_tier(const v57_invariant_t *inv)
{
    if (!inv) return V57_TIER_N;

    const v57_check_t *checks[4] = {
        &inv->runtime, &inv->formal, &inv->documented, &inv->planned
    };

    /* Preference order encoded explicitly: M, F, I, P. */
    const v57_tier_t pref[4] = { V57_TIER_M, V57_TIER_F,
                                  V57_TIER_I, V57_TIER_P };

    for (int p = 0; p < 4; ++p) {
        for (int c = 0; c < 4; ++c) {
            if (checks[c]->enabled && checks[c]->tier == pref[p]) {
                return pref[p];
            }
        }
    }
    return V57_TIER_N;
}

void v57_invariant_tier_histogram(int counts[V57_TIER_N + 1])
{
    if (!counts) return;
    for (int i = 0; i <= V57_TIER_N; ++i) counts[i] = 0;

    for (int i = 0; i < k_invariant_count; ++i) {
        v57_tier_t t = v57_invariant_best_tier(&k_invariants[i]);
        if (t >= 0 && t <= V57_TIER_N) counts[t]++;
    }
}

const char *v57_tier_tag(v57_tier_t t)
{
    switch (t) {
        case V57_TIER_M: return "M";
        case V57_TIER_F: return "F";
        case V57_TIER_I: return "I";
        case V57_TIER_P: return "P";
        default:         return "?";
    }
}
