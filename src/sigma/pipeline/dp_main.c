/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-DP self-test + canonical demo.
 *
 * Demonstrates (a) deterministic replay, (b) ε-budget tracking
 * across 3 rounds under ε_round = 1.0, ε_total = 5.0, (c) σ_dp
 * monotonicity as ε tightens.  The emitted JSON is byte-stable on
 * any tier-1 host and is pinned by benchmarks/sigma_pipeline/
 * check_sigma_dp.sh.
 */

#include "dp.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = cos_sigma_dp_self_test();

    cos_sigma_dp_state_t st;
    cos_sigma_dp_init(&st, /*ε=*/1.0f, /*δ=*/1e-6f,
                           /*clip=*/1.0f, /*ε_total=*/5.0f,
                           /*seed=*/0xC05D5EEDULL);

    float grads[3][4] = {
        { 0.50f, -0.30f,  0.20f,  0.10f },
        { 0.10f,  0.05f,  0.02f,  0.01f },
        { 2.00f,  0.00f,  0.00f,  0.00f },   /* will clip hard     */
    };
    cos_sigma_dp_report_t rep[3];
    for (int r = 0; r < 3; r++)
        cos_sigma_dp_add_noise(&st, grads[r], 4, &rep[r]);

    printf("{\"kernel\":\"sigma_dp\","
           "\"self_test_rc\":%d,"
           "\"rounds\":[",
           rc);
    for (int r = 0; r < 3; r++) {
        printf("%s{"
               "\"round\":%d,"
               "\"pre_clip_norm\":%.4f,"
               "\"post_clip_norm\":%.4f,"
               "\"post_noise_norm\":%.4f,"
               "\"noise_scale\":%.4f,"
               "\"noise_l2\":%.4f,"
               "\"sigma_dp\":%.4f,"
               "\"eps_spent\":%.4f,"
               "\"eps_remaining\":%.4f,"
               "\"rounds_left\":%d"
               "}",
               r ? "," : "",
               r + 1,
               (double)rep[r].pre_clip_norm,
               (double)rep[r].post_clip_norm,
               (double)rep[r].post_noise_norm,
               (double)rep[r].noise_scale,
               (double)rep[r].noise_l2,
               (double)rep[r].sigma_dp,
               (double)st.epsilon_spent - (2 - r) * (double)st.epsilon,
               (double)rep[r].epsilon_remaining,
               (int)(rep[r].epsilon_remaining / st.epsilon));
    }

    printf("],"
           "\"final\":{"
             "\"eps_spent\":%.4f,"
             "\"eps_total\":%.4f,"
             "\"eps_remaining\":%.4f,"
             "\"rounds_left\":%d,"
             "\"rounds_applied\":%d"
           "},"
           "\"pass\":%s}\n",
           (double)st.epsilon_spent,
           (double)st.epsilon_total,
           (double)cos_sigma_dp_remaining(&st),
           cos_sigma_dp_rounds_left(&st),
           st.rounds,
           rc == 0 ? "true" : "false");

    return rc == 0 ? 0 : 1;
}
