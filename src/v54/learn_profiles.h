/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-profile learner — scaffold.
 *
 * Updates a subagent's per-domain σ-profile and observed-accuracy
 * channel with exponential moving averages. Pure function over the
 * subagent struct; no clock, no I/O, no persistence. Persistence is
 * the caller's job (a future `profiles.bin` mmap layer).
 */
#ifndef CREATION_OS_V54_LEARN_PROFILES_H
#define CREATION_OS_V54_LEARN_PROFILES_H

#include "proconductor.h"
#include "dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V54_GROUND_TRUTH_UNKNOWN (-1)

/* Update one (agent, domain) slot with a measured σ + optional
 * ground-truth flag (0 = wrong, 1 = correct, -1 = unknown). */
void v54_learn_profile_update(v54_subagent_t *agent,
                              v54_domain_t domain,
                              float observed_sigma,
                              int was_correct);

/* Convenience: update the agent based on an aggregation outcome and
 * a single (domain, ground_truth) signal. If `winner_only` is true,
 * only the winning agent is updated; otherwise all agents that
 * participated have their σ-profile in `domain` nudged by their
 * reported σ. */
void v54_learn_from_aggregation(v54_proconductor_t *p,
                                v54_domain_t domain,
                                const v54_response_t *responses, int n,
                                const v54_aggregation_t *agg,
                                int ground_truth /* -1 / 0 / 1 */,
                                int winner_only);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V54_LEARN_PROFILES_H */
