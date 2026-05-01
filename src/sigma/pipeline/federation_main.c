/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Federation self-test + canonical 5-node aggregate demo.
 *
 * 5 nodes, 4-dim Δweight:
 *   trusted-1   σ=0.10, n=200, small update
 *   trusted-2   σ=0.15, n=150, small update
 *   drift       σ=0.30, n=100, slightly larger
 *   malicious   σ=0.90, n=500, huge Δ   (σ-gate rejects)
 *   poison      σ=0.20, n=100, norm ~100× cohort (poison gate)
 */

#include "federation.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_fed_self_test();

    cos_fed_t f;
    cos_fed_update_t slots[5];
    cos_sigma_fed_init(&f, slots, 5, /*sigma_reject=*/0.70f,
                                      /*poison_max_scale=*/3.0f);

    float d_t1 [4] = {  0.10f,  0.05f,  0.02f,  0.04f };
    float d_t2 [4] = {  0.12f,  0.06f,  0.03f,  0.05f };
    float d_dr [4] = {  0.20f,  0.10f,  0.05f,  0.08f };
    float d_mal[4] = {  5.00f,  5.00f,  5.00f,  5.00f };
    float d_poi[4] = { 50.00f, 50.00f, 50.00f, 50.00f };

    cos_sigma_fed_add(&f, "trusted-1", d_t1,  4, 0.10f, 200);
    cos_sigma_fed_add(&f, "trusted-2", d_t2,  4, 0.15f, 150);
    cos_sigma_fed_add(&f, "drift",     d_dr,  4, 0.30f, 100);
    cos_sigma_fed_add(&f, "malicious", d_mal, 4, 0.90f, 500);
    cos_sigma_fed_add(&f, "poison",    d_poi, 4, 0.20f, 100);

    float global[4];
    cos_fed_report_t rep;
    cos_sigma_fed_aggregate(&f, global, &rep);

    printf("{\"kernel\":\"sigma_federation\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"updates\":%d,"
             "\"n_params\":%d,"
             "\"sigma_reject\":%.2f,"
             "\"poison_scale\":%.2f,"
             "\"accepted\":%d,"
             "\"rejected_sigma\":%d,"
             "\"rejected_poison\":%d,"
             "\"abstained\":%s,"
             "\"median_norm\":%.4f,"
             "\"total_weight\":%.2f,"
             "\"delta\":[%.4f,%.4f,%.4f,%.4f],"
             "\"delta_l2\":%.4f,"
             "\"accepted_ids\":[%s%s%s]"
             "},"
           "\"pass\":%s}\n",
           rc,
           f.count, f.n_params,
           (double)f.sigma_reject, (double)f.poison_max_scale,
           rep.n_accepted, rep.n_rejected_sigma, rep.n_rejected_poison,
           rep.abstained ? "true" : "false",
           (double)rep.median_norm,
           (double)rep.total_weight,
           (double)global[0], (double)global[1],
           (double)global[2], (double)global[3],
           (double)rep.delta_l2_norm,
           slots[0].accepted ? "\"trusted-1\"" : "",
           slots[1].accepted ? (slots[0].accepted ? ",\"trusted-2\"" : "\"trusted-2\"") : "",
           slots[2].accepted ? (slots[0].accepted || slots[1].accepted ? ",\"drift\"" : "\"drift\"") : "",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Federation demo: accepted=%d rejected=σ%d+poison%d\n",
                rep.n_accepted, rep.n_rejected_sigma, rep.n_rejected_poison);
    }
    return rc;
}
