/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Meta self-test + canonical 4-domain competence demo. */

#include "meta.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_meta_self_test();

    /* Canonical demo: four domains, 3 observations each. */
    cos_meta_store_t store;
    cos_meta_domain_t slots[8];
    cos_sigma_meta_init(&store, slots, 8);

    cos_sigma_meta_observe(&store, "factual_qa",  0.15f);
    cos_sigma_meta_observe(&store, "factual_qa",  0.20f);
    cos_sigma_meta_observe(&store, "factual_qa",  0.18f);

    cos_sigma_meta_observe(&store, "math",        0.65f);
    cos_sigma_meta_observe(&store, "math",        0.60f);
    cos_sigma_meta_observe(&store, "math",        0.70f);

    cos_sigma_meta_observe(&store, "code",        0.35f);
    cos_sigma_meta_observe(&store, "code",        0.40f);
    cos_sigma_meta_observe(&store, "code",        0.30f);

    cos_sigma_meta_observe(&store, "commonsense", 0.70f);
    cos_sigma_meta_observe(&store, "commonsense", 0.75f);
    cos_sigma_meta_observe(&store, "commonsense", 0.71f);

    cos_sigma_meta_assess(&store);

    const cos_meta_domain_t *focus = cos_sigma_meta_recommend_focus(&store);

    printf("{\"kernel\":\"sigma_meta\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"count\":%d,\"total_samples\":%d,"
             "\"overall_mean_sigma\":%.4f,"
             "\"domains\":["
               "{\"name\":\"factual_qa\",\"sigma_mean\":%.4f,\"competence\":\"%s\"},"
               "{\"name\":\"math\",\"sigma_mean\":%.4f,\"competence\":\"%s\"},"
               "{\"name\":\"code\",\"sigma_mean\":%.4f,\"competence\":\"%s\"},"
               "{\"name\":\"commonsense\",\"sigma_mean\":%.4f,\"competence\":\"%s\"}"
             "],"
             "\"strongest\":\"%s\",\"weakest\":\"%s\","
             "\"focus\":\"%s\""
             "},"
           "\"pass\":%s}\n",
           rc,
           store.count, store.total_samples,
           (double)store.overall_mean_sigma,
           (double)store.slots[0].sigma_mean,
               cos_sigma_meta_competence_name(store.slots[0].competence),
           (double)store.slots[1].sigma_mean,
               cos_sigma_meta_competence_name(store.slots[1].competence),
           (double)store.slots[2].sigma_mean,
               cos_sigma_meta_competence_name(store.slots[2].competence),
           (double)store.slots[3].sigma_mean,
               cos_sigma_meta_competence_name(store.slots[3].competence),
           store.slots[store.strongest_idx].domain,
           store.slots[store.weakest_idx].domain,
           focus ? focus->domain : "?",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Meta demo: focus=%s overall=%.3f\n",
                focus ? focus->domain : "(none)",
                (double)store.overall_mean_sigma);
    }
    return rc;
}
