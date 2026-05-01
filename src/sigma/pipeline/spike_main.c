/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Spike self-test + canonical LIF demo.
 *
 * Single neuron: V_th=1.0, decay=0.90, V_reset=0.
 * Input stream (10 steps): 0.30, 0.25, 0.20, 0.30, 0.40,
 *                          0.50, 0.10, 0.80, 0.20, 0.60.
 * Population: 16 neurons, two bursts of input — σ drops from
 * ≈1.0 (silent) to ≈0.4 (firing) as the population engages.
 */

#include "spike.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_spike_self_test();

    cos_spike_neuron_t n;
    cos_sigma_spike_init(&n, 1.0f, 0.90f, 0.0f);
    float inputs[10] = { 0.30f, 0.25f, 0.20f, 0.30f, 0.40f,
                         0.50f, 0.10f, 0.80f, 0.20f, 0.60f };
    int  first_spike = -1;
    for (int i = 0; i < 10; ++i) {
        if (cos_sigma_spike_step(&n, inputs[i]) == 1 && first_spike < 0)
            first_spike = i;
    }
    int   total_spikes = n.n_spikes;
    int   total_steps  = n.n_steps;
    float final_sigma  = cos_sigma_spike_sigma(&n);

    cos_spike_neuron_t pop[16];
    for (int i = 0; i < 16; ++i)
        cos_sigma_spike_init(&pop[i], 1.0f, 0.90f, 0.0f);
    for (int t = 0; t < 20; ++t)
        for (int i = 0; i < 16; ++i)
            cos_sigma_spike_step(&pop[i], 0.05f);
    float s_silent = cos_sigma_spike_population_sigma(pop, 16, 20, 1);
    /* Same population, heavy drive. */
    for (int i = 0; i < 16; ++i)
        cos_sigma_spike_init(&pop[i], 1.0f, 0.90f, 0.0f);
    for (int t = 0; t < 20; ++t)
        for (int i = 0; i < 16; ++i)
            cos_sigma_spike_step(&pop[i], 1.5f);
    float s_burst = cos_sigma_spike_population_sigma(pop, 16, 20, 1);

    printf("{\"kernel\":\"sigma_spike\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"neuron\":{\"threshold\":%.2f,\"decay\":%.2f,"
                         "\"inputs\":10,"
                         "\"total_spikes\":%d,"
                         "\"total_steps\":%d,"
                         "\"first_spike_step\":%d,"
                         "\"final_sigma\":%.4f},"
             "\"population\":{\"size\":16,\"window\":20,"
                              "\"sigma_silent\":%.4f,"
                              "\"sigma_burst\":%.4f}"
             "},"
           "\"pass\":%s}\n",
           rc,
           (double)n.threshold, (double)n.decay,
           total_spikes, total_steps, first_spike,
           (double)final_sigma,
           (double)s_silent, (double)s_burst,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Spike demo: first_spike=%d silent_σ=%.2f burst_σ=%.2f\n",
                first_spike, (double)s_silent, (double)s_burst);
    }
    return rc;
}
