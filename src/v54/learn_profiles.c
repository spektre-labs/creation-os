/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-profile learner — scaffold.
 */
#include "learn_profiles.h"

static float v54_clip01(float x)
{
    if (!(x >= 0.0f)) return 0.0f;
    if (x > 1.0f)    return 1.0f;
    return x;
}

void v54_learn_profile_update(v54_subagent_t *agent,
                              v54_domain_t domain,
                              float observed_sigma,
                              int was_correct)
{
    if (!agent) return;
    if (domain < 0 || domain >= V54_N_DOMAINS) return;
    float s = v54_clip01(observed_sigma);

    /* EWMA: new = 0.95·old + 0.05·obs  (slow drift, keeps history). */
    float cur = agent->sigma_profile[domain];
    agent->sigma_profile[domain] = 0.95f * cur + 0.05f * s;

    if (was_correct == 0 || was_correct == 1) {
        float cal = (was_correct == 1) ? 1.0f : 0.0f;
        float cura = agent->observed_accuracy[domain];
        agent->observed_accuracy[domain] = 0.95f * cura + 0.05f * cal;
    }
    agent->requests_made++;
}

void v54_learn_from_aggregation(v54_proconductor_t *p,
                                v54_domain_t domain,
                                const v54_response_t *responses, int n,
                                const v54_aggregation_t *agg,
                                int ground_truth,
                                int winner_only)
{
    if (!p || !responses || n <= 0 || !agg) return;

    if (winner_only) {
        if (agg->winner_index < 0 || agg->winner_index >= p->n_agents) return;
        v54_subagent_t *a = &p->agents[agg->winner_index];
        /* Find the matching response. */
        float obs = 0.5f;
        for (int i = 0; i < n; i++) {
            if (responses[i].agent_index == agg->winner_index) {
                obs = responses[i].reported_sigma;
                break;
            }
        }
        v54_learn_profile_update(a, domain, obs, ground_truth);
        return;
    }

    for (int i = 0; i < n; i++) {
        int idx = responses[i].agent_index;
        if (idx < 0 || idx >= p->n_agents) continue;
        /* Ground-truth only counted for the winning response; others
         * get an unknown flag so we don't punish/reward them on a
         * signal they didn't produce. */
        int gt = (idx == agg->winner_index) ? ground_truth
                                            : V54_GROUND_TRUTH_UNKNOWN;
        v54_learn_profile_update(&p->agents[idx], domain,
                                 responses[i].reported_sigma, gt);
    }
}
