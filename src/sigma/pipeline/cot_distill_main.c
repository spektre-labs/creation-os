/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-CoT-distill demo + pinned JSON receipt. */

#include "cot_distill.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = cos_sigma_cot_self_test();

    cos_sigma_cot_state_t st;
    cos_sigma_cot_init(&st, /*tau_rethink=*/0.50f, /*tau_conf=*/0.20f);

    /* Two canonical CoT pairs. */

    /* Pair 1: math question where the teacher rethinks twice. */
    const char *cot1 =
        "Step 1: Parse the number 137.\n"
        "Step 2: Check divisibility by small primes (2, 3, 5, 7, 11).\n"
        "Wait, I should also test 11 carefully.\n"
        "Step 3: None divide 137, so it is prime.\n"
        "Step 4: Final answer: 137 is prime.";
    float sig1[5] = { 0.04f, 0.15f, 0.65f, 0.08f, 0.02f };

    /* Pair 2: factual question, single smooth rethink. */
    const char *cot2 =
        "Thought: The speed of light in vacuum.\n"
        "Step 1: Recall c ≈ 3 × 10^8 m/s.\n"
        "Wait, more precisely 299 792 458 m/s.\n"
        "Step 2: Final answer.";
    float sig2[4] = { 0.06f, 0.10f, 0.52f, 0.03f };

    cos_sigma_cot_pair_t pairs[2] = {
        {.input="Is 137 prime?",
         .teacher_cot=cot1, .teacher_answer="yes",
         .sigma_seq=sig1, .n_sigma=5, .timestamp_ns=1},
        {.input="What is the speed of light?",
         .teacher_cot=cot2, .teacher_answer="299792458 m/s",
         .sigma_seq=sig2, .n_sigma=4, .timestamp_ns=2},
    };
    for (int i = 0; i < 2; i++) cos_sigma_cot_append(&st, &pairs[i]);

    cos_sigma_cot_stats_t snap;
    cos_sigma_cot_stats(&st, &snap);

    printf("{\"kernel\":\"sigma_cot_distill\","
           "\"self_test_rc\":%d,"
           "\"tau_rethink\":%.4f,"
           "\"tau_confident\":%.4f,"
           "\"total_seen\":%llu,"
           "\"rethink_points_total\":%llu,"
           "\"mean_cot_value\":%.4f,"
           "\"mean_max_step\":%.4f,"
           "\"pairs\":[",
           rc,
           (double)st.tau_rethink,
           (double)st.tau_confident,
           (unsigned long long)snap.total_seen,
           (unsigned long long)snap.rethink_points_total,
           (double)snap.mean_cot_value,
           (double)snap.mean_max_step);

    for (int i = 0; i < 2; i++) {
        const cos_sigma_cot_record_t *r = cos_sigma_cot_at(&st, i);
        if (!r) continue;
        printf("%s{\"pair\":%d,"
               "\"n_steps\":%d,"
               "\"n_rethink\":%d,"
               "\"sigma_cot_value\":%.4f,"
               "\"sigma_max_step\":%.4f,"
               "\"rethink_at\":[",
               i ? "," : "", i + 1,
               r->n_steps, r->n_rethink,
               (double)r->sigma_cot_value,
               (double)r->sigma_max_step);
        int first = 1;
        for (int j = 0; j < r->n_steps; j++) {
            if (r->steps[j].is_rethink) {
                printf("%s%d", first ? "" : ",", j + 1);
                first = 0;
            }
        }
        printf("],\"sigma_per_step\":[");
        for (int j = 0; j < r->n_steps; j++)
            printf("%s%.4f", j ? "," : "", (double)r->steps[j].sigma);
        printf("]}");
    }

    printf("],\"pass\":%s}\n", rc == 0 ? "true" : "false");

    cos_sigma_cot_free(&st);
    return rc == 0 ? 0 : 1;
}
