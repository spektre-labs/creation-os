/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-reinforce primitive implementation + exhaustive self-test. */

#include "reinforce.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

const char *cos_sigma_reinforce_rethink_suffix(void) {
    /* The suffix is appended verbatim after a newline to the original
     * prompt.  Empirical CoT wording from arxiv 2602.08520 §3.2.    */
    return "\n\nYour previous answer had low confidence. "
           "Reconsider from first principles, show each step, and "
           "if any step is uncertain, say \"I do not know\".";
}

const char *cos_sigma_action_label(cos_sigma_action_t a) {
    switch (a) {
        case COS_SIGMA_ACTION_ACCEPT:  return "ACCEPT";
        case COS_SIGMA_ACTION_RETHINK: return "RETHINK";
        case COS_SIGMA_ACTION_ABSTAIN: return "ABSTAIN";
        default:                       return "UNKNOWN";
    }
}

/* --- self-test --------------------------------------------------------- */

/* Canonical reference table.  Pre-registered inputs → expected action. */
struct cos_v_row {
    float sigma;
    float tau_accept;
    float tau_rethink;
    cos_sigma_action_t expected;
};

static const struct cos_v_row canonical[] = {
    /* ACCEPT — σ well below τ_accept. */
    { 0.00f, 0.30f, 0.70f, COS_SIGMA_ACTION_ACCEPT  },
    { 0.10f, 0.30f, 0.70f, COS_SIGMA_ACTION_ACCEPT  },
    { 0.29f, 0.30f, 0.70f, COS_SIGMA_ACTION_ACCEPT  },
    /* RETHINK — σ in middle band. */
    { 0.30f, 0.30f, 0.70f, COS_SIGMA_ACTION_RETHINK },
    { 0.50f, 0.30f, 0.70f, COS_SIGMA_ACTION_RETHINK },
    { 0.69f, 0.30f, 0.70f, COS_SIGMA_ACTION_RETHINK },
    /* ABSTAIN — σ ≥ τ_rethink. */
    { 0.70f, 0.30f, 0.70f, COS_SIGMA_ACTION_ABSTAIN },
    { 0.99f, 0.30f, 0.70f, COS_SIGMA_ACTION_ABSTAIN },
    { 1.00f, 0.30f, 0.70f, COS_SIGMA_ACTION_ABSTAIN },
    /* Edge: τ_accept == τ_rethink collapses the middle band. */
    { 0.40f, 0.50f, 0.50f, COS_SIGMA_ACTION_ACCEPT  },
    { 0.50f, 0.50f, 0.50f, COS_SIGMA_ACTION_ABSTAIN },
    { 0.60f, 0.50f, 0.50f, COS_SIGMA_ACTION_ABSTAIN },
    /* Degenerate: τ_accept > τ_rethink → ABSTAIN (mis-configured). */
    { 0.10f, 0.80f, 0.30f, COS_SIGMA_ACTION_ABSTAIN },
    /* Safe default on NaN / negative σ. */
    { 0.0f / 0.0f, 0.30f, 0.70f, COS_SIGMA_ACTION_ABSTAIN },
    { -1.0f,       0.30f, 0.70f, COS_SIGMA_ACTION_ABSTAIN },
};

static int check_monotone_in_sigma(void) {
    /* For fixed τ_accept=0.30, τ_rethink=0.70, action rank must be
     * monotone non-decreasing in σ. */
    float sig = 0.0f;
    cos_sigma_action_t prev = cos_sigma_reinforce(0.0f, 0.30f, 0.70f);
    for (int i = 1; i <= 10000; ++i) {
        sig = (float)i / 10000.0f;
        cos_sigma_action_t cur = cos_sigma_reinforce(sig, 0.30f, 0.70f);
        if ((int)cur < (int)prev) return 10;
        prev = cur;
    }
    return 0;
}

static int check_round_budget(void) {
    /* RETHINK hard-falls to ABSTAIN on the last round. */
    cos_sigma_action_t a0 = cos_sigma_reinforce_round(0.50f, 0.30f, 0.70f, 0);
    cos_sigma_action_t a1 = cos_sigma_reinforce_round(0.50f, 0.30f, 0.70f, 1);
    cos_sigma_action_t a2 = cos_sigma_reinforce_round(0.50f, 0.30f, 0.70f, 2);
    if (a0 != COS_SIGMA_ACTION_RETHINK) return 20;
    if (a1 != COS_SIGMA_ACTION_RETHINK) return 21;
    if (a2 != COS_SIGMA_ACTION_ABSTAIN) return 22;
    /* ACCEPT and ABSTAIN are round-insensitive. */
    if (cos_sigma_reinforce_round(0.10f, 0.30f, 0.70f, 5)
        != COS_SIGMA_ACTION_ACCEPT)  return 23;
    if (cos_sigma_reinforce_round(0.90f, 0.30f, 0.70f, 0)
        != COS_SIGMA_ACTION_ABSTAIN) return 24;
    return 0;
}

static int check_lcg_grid(void) {
    /* 10^5 deterministic random (σ, τa, τr) triples.  Every call
     * must return a value in the 3-element enum range. */
    uint32_t st = 0xCAFEB0BAu;
    for (int i = 0; i < 100000; ++i) {
        st = st * 1664525u + 1013904223u;
        float sig = (float)((st >> 8) & 0xFFFF) / 65535.0f;
        st = st * 1664525u + 1013904223u;
        float ta  = (float)((st >> 8) & 0xFFFF) / 65535.0f;
        st = st * 1664525u + 1013904223u;
        float tr  = (float)((st >> 8) & 0xFFFF) / 65535.0f;
        cos_sigma_action_t a = cos_sigma_reinforce(sig, ta, tr);
        if ((int)a < 0 || (int)a > 2) return 30;
    }
    return 0;
}

int cos_sigma_reinforce_self_test(void) {
    const size_t n = sizeof canonical / sizeof canonical[0];
    for (size_t i = 0; i < n; ++i) {
        cos_sigma_action_t a = cos_sigma_reinforce(
            canonical[i].sigma, canonical[i].tau_accept,
            canonical[i].tau_rethink);
        if (a != canonical[i].expected) return 1 + (int)i;
    }
    int rc = check_monotone_in_sigma();
    if (rc) return rc;
    rc = check_round_budget();
    if (rc) return rc;
    rc = check_lcg_grid();
    if (rc) return rc;
    if (cos_sigma_reinforce_max_rounds() <= 0)          return 40;
    if (!cos_sigma_reinforce_rethink_suffix())          return 41;
    if (cos_sigma_reinforce_rethink_suffix()[0] == '\0') return 42;
    return 0;
}
