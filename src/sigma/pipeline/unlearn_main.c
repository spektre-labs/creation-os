/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Unlearn self-test binary + GDPR-style demo. */

#include "unlearn.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_unlearn_self_test();

    /* Canonical demo: subject "user_42" requests removal.
     * Weights have a dominant component aligned with their target
     * activation; σ-Unlearn iterates until orthogonality. */
    float w[4] = {5.0f, 0.1f, 0.1f, 0.1f};
    float t[4] = {3.0f, 0.0f, 0.0f, 0.0f};
    cos_unlearn_request_t req = {
        .subject_hash = cos_sigma_unlearn_hash("user_42"),
        .strength     = 0.5f,
        .max_iters    = 20,
        .sigma_target = 0.95f,
    };
    cos_unlearn_result_t res;
    cos_sigma_unlearn_iterate(w, t, 4, &req, &res);

    printf("{\"kernel\":\"sigma_unlearn\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"subject_hash\":\"%llu\","
             "\"strength\":%.4f,"
             "\"max_iters\":%d,"
             "\"sigma_target\":%.4f,"
             "\"sigma_before\":%.4f,"
             "\"sigma_after\":%.4f,"
             "\"n_iters\":%d,"
             "\"l1_shrunk\":%.4f,"
             "\"succeeded\":%s,"
             "\"final_weights\":[%.4f,%.4f,%.4f,%.4f]},"
           "\"pass\":%s}\n",
           rc,
           (unsigned long long)res.subject_hash,
           (double)req.strength, req.max_iters,
           (double)req.sigma_target,
           (double)res.sigma_before, (double)res.sigma_after,
           res.n_iters, (double)res.l1_shrunk,
           res.succeeded ? "true" : "false",
           (double)w[0], (double)w[1], (double)w[2], (double)w[3],
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Unlearn demo (subject hash %llu):\n",
            (unsigned long long)res.subject_hash);
        fprintf(stderr, "  before  σ = %.4f  |cos| = %.4f\n",
            (double)res.sigma_before,
            1.0 - (double)res.sigma_before);
        fprintf(stderr, "  after   σ = %.4f  over %d iterations\n",
            (double)res.sigma_after, res.n_iters);
        fprintf(stderr, "  result: %s (target %.2f)\n",
            res.succeeded ? "FORGOTTEN" : "incomplete",
            (double)req.sigma_target);
    }
    return rc;
}
