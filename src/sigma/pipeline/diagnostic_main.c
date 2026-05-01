/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Diagnostic self-test binary + canonical decomposition demo. */

#include "diagnostic.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_diagnostic_self_test();

    /* Canonical demo: three-way tie among tokens 0, 1, 2; token 3
     * suppressed.  This is the textbook "model is unsure" case. */
    float lp[4] = {0.0f, 0.0f, 0.0f, -10.0f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.90f);

    printf("{\"kernel\":\"sigma_diagnostic\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"vocab_size\":4,"
             "\"sigma\":%.4f,"
             "\"factor_entropy\":%.4f,"
             "\"factor_gap\":%.4f,"
             "\"factor_maxprob\":%.4f,"
             "\"entropy_nats\":%.4f,"
             "\"effective_k\":%d,"
             "\"max_prob\":%.4f,"
             "\"gap_top12\":%.4f,"
             "\"top\":[{\"idx\":%d,\"prob\":%.4f},"
                      "{\"idx\":%d,\"prob\":%.4f},"
                      "{\"idx\":%d,\"prob\":%.4f}],"
             "\"counterfactual\":{\"target\":%.4f,\"sigma\":%.4f}},"
           "\"pass\":%s}\n",
           rc,
           (double)d.sigma,
           (double)d.factor_entropy,
           (double)d.factor_gap,
           (double)d.factor_maxprob,
           (double)d.entropy,
           d.effective_k,
           (double)d.max_prob,
           (double)d.gap_top12,
           d.top_idx[0], (double)d.top_prob[0],
           d.top_idx[1], (double)d.top_prob[1],
           d.top_idx[2], (double)d.top_prob[2],
           (double)d.cf_target, (double)d.cf_sigma,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr,
            "σ-Diagnostic: σ = %.4f\n"
            "  factor_entropy = %.4f  (H = %.4f nats over ln4)\n"
            "  factor_gap     = %.4f  (top1 − top2 = %.4f)\n"
            "  factor_maxprob = %.4f  (p_top1 = %.4f)\n"
            "  effective_k    = %d\n"
            "  counterfactual: lift top-1 to %.2f → σ = %.4f\n",
            (double)d.sigma,
            (double)d.factor_entropy, (double)d.entropy,
            (double)d.factor_gap,     (double)d.gap_top12,
            (double)d.factor_maxprob, (double)d.max_prob,
            d.effective_k,
            (double)d.cf_target, (double)d.cf_sigma);
    }
    return rc;
}
