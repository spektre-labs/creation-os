/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v57 Verified Agent — composition data.
 *
 * The slot table below is the authoritative listing of which
 * Creation OS subsystem owns which agent-runtime slot, what
 * deterministic make target verifies it, and the tier that target
 * achieves today.
 *
 * Tier rules (mirrored from invariants.c):
 *   M = the make target runs a deterministic self-test that returns
 *       0 iff the slot's claim holds in this build
 *   F = a formal proof artifact discharges the slot's claim
 *   I = the slot is documented and reasoned about, but not yet
 *       checked mechanically
 *   P = the slot is planned; the make target is a placeholder
 */
#include "verified_agent.h"

#include <string.h>

static const v57_slot_t k_slots[] = {
    {
        .slot           = "execution_sandbox",
        .owner_versions = "v48",
        .make_target    = "make check-v48",
        .best_tier      = V57_TIER_M,
        .summary        = "Capability sandbox + 7-layer defense; "
                          "deny-by-default tool admission",
    },
    {
        .slot           = "sigma_kernel_surface",
        .owner_versions = "v47",
        .make_target    = "make verify-c",
        .best_tier      = V57_TIER_F,
        .summary        = "Frama-C / WP ACSL contracts on σ-kernel "
                          "(SKIPs honestly without Frama-C installed)",
    },
    {
        .slot           = "harness_loop",
        .owner_versions = "v53",
        .make_target    = "make check-v53",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-TAOR loop with σ-abstention and "
                          "creation.md invariants",
    },
    {
        .slot           = "multi_llm_routing",
        .owner_versions = "v54",
        .make_target    = "make check-v54",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-proconductor: σ-profiled subagents, "
                          "σ-triangulation, disagreement abstention",
    },
    {
        .slot           = "speculative_decode",
        .owner_versions = "v55",
        .make_target    = "make check-v55",
        .best_tier      = V57_TIER_M,
        .summary        = "σ₃-speculative: per-channel σ "
                          "decomposition + EARS commit policy",
    },
    {
        .slot           = "constitutional_self_modification",
        .owner_versions = "v56",
        .make_target    = "make check-v56",
        .best_tier      = V57_TIER_M,
        .summary        = "VPRM verifier + σ-gated IP-TTT + "
                          "grokking commutator-σ + ANE layout",
    },
    {
        .slot           = "do178c_assurance_pack",
        .owner_versions = "v49",
        .make_target    = "make certify",
        .best_tier      = V57_TIER_I,
        .summary        = "DO-178C-aligned HLR/LLR/PSAC/SCMP/SDD/SDP/"
                          "SQAP/SVP artifacts (NOT FAA/EASA "
                          "certification)",
    },
    {
        .slot           = "red_team_suite",
        .owner_versions = "v48 + v49",
        .make_target    = "make red-team",
        .best_tier      = V57_TIER_I,
        .summary        = "Garak + DeepTeam + σ-property red-team "
                          "(aggregator; SKIPs without tooling)",
    },
    {
        .slot           = "kv_cache_eviction",
        .owner_versions = "v58",
        .make_target    = "make check-v58",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Cache: σ-decomposed KV-cache eviction + "
                          "per-token precision tier + branchless NEON "
                          "kernel (deterministic self-test + microbench)",
    },
    {
        .slot           = "adaptive_compute_budget",
        .owner_versions = "v59",
        .make_target    = "make check-v59",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Budget: σ-decomposed adaptive test-time "
                          "compute budget controller + four-valued "
                          "CONTINUE/EARLY_EXIT/EXPAND/ABSTAIN kernel "
                          "(branchless + NEON 4-accum SoA; ≥60 tests)",
    },
    {
        .slot           = "runtime_security_kernel",
        .owner_versions = "v60",
        .make_target    = "make check-v60",
        .best_tier      = V57_TIER_M,
        .summary        = "σ-Shield: first cap-kernel with σ=(ε,α) "
                          "intent gate + TOCTOU-free arg-hash + "
                          "code-page integrity + sticky-deny "
                          "(constant-time; branchless; 81 tests; "
                          "refuses α-dominated intent regardless of caps)",
    },
};

static const int k_slot_count =
    (int)(sizeof(k_slots) / sizeof(k_slots[0]));

int v57_slot_count(void)
{
    return k_slot_count;
}

const v57_slot_t *v57_slot_get(int index)
{
    if (index < 0 || index >= k_slot_count) return NULL;
    return &k_slots[index];
}

const v57_slot_t *v57_slot_find(const char *slot)
{
    if (!slot) return NULL;
    for (int i = 0; i < k_slot_count; ++i) {
        const char *cand = k_slots[i].slot;
        if (cand && strcmp(cand, slot) == 0) return &k_slots[i];
    }
    return NULL;
}

void v57_report_build(v57_report_t *out)
{
    if (!out) return;

    for (int i = 0; i <= V57_TIER_N; ++i) {
        out->slot_counts[i]      = 0;
        out->invariant_counts[i] = 0;
    }

    for (int i = 0; i < k_slot_count; ++i) {
        v57_tier_t t = k_slots[i].best_tier;
        if (t >= 0 && t <= V57_TIER_N) out->slot_counts[t]++;
    }
    out->n_slots = k_slot_count;

    int inv_n = v57_invariant_count();
    for (int i = 0; i < inv_n; ++i) {
        v57_tier_t t = v57_invariant_best_tier(v57_invariant_get(i));
        if (t >= 0 && t <= V57_TIER_N) out->invariant_counts[t]++;
    }
    out->n_invariants = inv_n;
}
