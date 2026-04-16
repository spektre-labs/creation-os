/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v57 Verified Agent — composition surface.
 *
 * The "Verified Agent" is not a new C runtime.  It is a small,
 * deterministic data structure that names the Creation OS
 * subsystems an agent invocation depends on, and reports — per
 * subsystem — the tier at which that subsystem is verified today.
 *
 * Operationally, the verified agent is what `make verify-agent`
 * walks: for each composition slot, the corresponding Makefile
 * target is dispatched, and SKIPs are reported honestly when
 * tooling is missing (Frama-C, sby, pytest, …).  No subsystem is
 * silently downgraded.
 *
 * This module is policy-only: zero allocation, no I/O, no network.
 * It composes with `invariants.h` to produce the Verified Agent
 * report consumed by the v57 driver and by `scripts/v57/verify_agent.sh`.
 */
#ifndef CREATION_OS_V57_VERIFIED_AGENT_H
#define CREATION_OS_V57_VERIFIED_AGENT_H

#include "invariants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *slot;            /* "execution_sandbox", "harness", … */
    const char *owner_versions;  /* "v48", "v53 + v54", … */
    const char *make_target;     /* "make check-v48"; non-NULL */
    v57_tier_t  best_tier;       /* honest tier for this slot today */
    const char *summary;         /* one-line description */
} v57_slot_t;

/* Number of composition slots in the Verified Agent. */
int                v57_slot_count(void);
const v57_slot_t  *v57_slot_get(int index);
const v57_slot_t  *v57_slot_find(const char *slot);

/* Aggregate report: how many slots reach tier M / F / I / P,
 * and how many invariants reach each tier.  counts arrays must
 * hold V57_TIER_N + 1 entries each (last is "none"). */
typedef struct {
    int slot_counts[V57_TIER_N + 1];
    int invariant_counts[V57_TIER_N + 1];
    int n_slots;
    int n_invariants;
} v57_report_t;

void v57_report_build(v57_report_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V57_VERIFIED_AGENT_H */
