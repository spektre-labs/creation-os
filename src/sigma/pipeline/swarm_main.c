/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Swarm self-test binary. */

#include "swarm.h"

#include <stdio.h>
#include <string.h>

static const char *verdict_label(cos_swarm_verdict_t v) {
    switch (v) {
        case COS_SWARM_ABSTAIN:   return "ABSTAIN";
        case COS_SWARM_CONSENSUS: return "CONSENSUS";
        case COS_SWARM_ACCEPT:    return "ACCEPT";
    }
    return "?";
}

int main(int argc, char **argv) {
    int rc = cos_sigma_swarm_self_test();

    /* Canonical demo: four models over the same query. */
    cos_swarm_response_t r[4] = {
        {.sigma = 0.60f, .cost_eur = 0.000f, .model_id = "bitnet-2b"},
        {.sigma = 0.20f, .cost_eur = 0.005f, .model_id = "claude-haiku"},
        {.sigma = 0.40f, .cost_eur = 0.010f, .model_id = "gpt-4.1"},
        {.sigma = 0.30f, .cost_eur = 0.002f, .model_id = "deepseek"},
    };
    cos_swarm_decision_t d =
        cos_sigma_swarm_consensus(r, 4, 0.30f, 0.60f);

    /* Escalation ladder trace: start with bitnet alone, keep adding. */
    int ladder_adds = 0;
    int n_used = 1;
    while (cos_sigma_swarm_should_escalate(r, n_used, 4, 0.30f)) {
        ++n_used;
        ++ladder_adds;
        if (n_used > 4) break;
    }

    printf("{\"kernel\":\"sigma_swarm\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"n\":4,"
             "\"verdict\":\"%s\","
             "\"winner_idx\":%d,"
             "\"winner_model\":\"%s\","
             "\"winner_sigma\":%.4f,"
             "\"sigma_swarm\":%.4f,"
             "\"mean_weight\":%.4f,"
             "\"total_cost_eur\":%.4f},"
           "\"escalation_ladder\":{\"started_with\":1,"
             "\"added\":%d,\"final_used\":%d,\"tau_stop\":0.30},"
           "\"pass\":%s}\n",
           rc,
           verdict_label(d.verdict),
           d.winner_idx,
           (d.winner_idx >= 0) ? r[d.winner_idx].model_id : "-",
           (double)d.winner_sigma,
           (double)d.sigma_swarm,
           (double)d.mean_weight,
           (double)d.total_cost_eur,
           ladder_adds, n_used,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Swarm demo (4 models):\n");
        for (int i = 0; i < 4; ++i)
            fprintf(stderr, "  %-14s σ=%.2f  €%.3f\n",
                r[i].model_id, (double)r[i].sigma,
                (double)r[i].cost_eur);
        fprintf(stderr, "  → verdict: %s  (winner idx %d, σ_swarm %.3f)\n",
            verdict_label(d.verdict), d.winner_idx,
            (double)d.sigma_swarm);
        fprintf(stderr, "  escalation ladder added %d model(s) "
            "from bitnet-only; stopped at n=%d.\n",
            ladder_adds, n_used);
    }
    return rc;
}
