/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Grounding self-test binary + canonical demo. */

#include "grounding.h"

#include <stdio.h>
#include <string.h>

static const char *verdict_name(cos_ground_verdict_t v) {
    switch (v) {
        case COS_GROUND_GROUNDED:    return "GROUNDED";
        case COS_GROUND_UNSUPPORTED: return "UNSUPPORTED";
        case COS_GROUND_CONFLICTED:  return "CONFLICTED";
        case COS_GROUND_SKIPPED:     return "SKIPPED";
        default:                     return "?";
    }
}

int main(int argc, char **argv) {
    int rc = cos_sigma_grounding_self_test();

    cos_ground_store_t st;
    cos_ground_source_t slots[4];
    cos_sigma_grounding_init(&st, slots, 4, 0.40f, 0.40f);
    cos_sigma_grounding_register(&st,
        "Paris is the capital of France.", 0.05f);
    cos_sigma_grounding_register(&st,
        "Nitrogen gas is not flammable under atmospheric conditions.", 0.08f);

    const char *output =
        "Paris is the capital of France. "
        "Nitrogen gas is flammable when lit. "
        "Octopuses have three hearts.";

    cos_ground_claim_t claims[8];
    float rate = 0.0f;
    int n = cos_sigma_grounding_ground(&st, output, 0.10f, claims, 8, &rate);

    /* Count per-verdict buckets. */
    int n_g = 0, n_u = 0, n_c = 0, n_s = 0;
    for (int i = 0; i < n; ++i) {
        switch (claims[i].verdict) {
            case COS_GROUND_GROUNDED:    ++n_g; break;
            case COS_GROUND_UNSUPPORTED: ++n_u; break;
            case COS_GROUND_CONFLICTED:  ++n_c; break;
            case COS_GROUND_SKIPPED:     ++n_s; break;
        }
    }

    printf("{\"kernel\":\"sigma_grounding\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"tau_claim\":%.4f,\"tau_source\":%.4f,"
             "\"n_sources\":%d,\"n_claims\":%d,"
             "\"buckets\":{"
                 "\"grounded\":%d,\"unsupported\":%d,"
                 "\"conflicted\":%d,\"skipped\":%d},"
             "\"ground_rate\":%.4f,"
             "\"claim0\":{\"verdict\":\"%s\",\"source_hash\":\"%016llx\"},"
             "\"claim1\":{\"verdict\":\"%s\",\"source_hash\":\"%016llx\"},"
             "\"claim2\":{\"verdict\":\"%s\",\"source_hash\":\"%016llx\"},"
             "\"empty_hash\":\"%016llx\""
           "},"
           "\"pass\":%s}\n",
           rc,
           (double)st.tau_claim, (double)st.tau_source,
           st.count, n,
           n_g, n_u, n_c, n_s,
           (double)rate,
           verdict_name(claims[0].verdict),
           (unsigned long long)claims[0].source_hash,
           verdict_name(claims[1].verdict),
           (unsigned long long)claims[1].source_hash,
           verdict_name(claims[2].verdict),
           (unsigned long long)claims[2].source_hash,
           (unsigned long long)cos_sigma_grounding_fnv1a64("", 0),
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Grounding demo:\n");
        for (int i = 0; i < n; ++i) {
            fprintf(stderr, "  [%s] σ_c=%.2f σ_s=%.2f src=%016llx  %s\n",
                verdict_name(claims[i].verdict),
                (double)claims[i].sigma_claim,
                (double)claims[i].sigma_source,
                (unsigned long long)claims[i].source_hash,
                claims[i].text ? claims[i].text : "");
        }
        fprintf(stderr, "  ground_rate: %.2f\n", (double)rate);
    }
    return rc;
}
