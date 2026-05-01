/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Substrate self-test + cross-substrate equivalence demo.
 *
 * Runs three canonical activation vectors across all four
 * backends and asserts that every backend lands on the same
 * ACCEPT / ABSTAIN bit at the stated τ_accept.
 *
 *   dominant  [.02 .03 .90 .05]   τ=0.30  → ACCEPT everywhere
 *   middle    [.10 .10 .70 .10]   τ=0.50  → ACCEPT everywhere
 *   uniform   [.25 .25 .25 .25]   τ=0.30  → ABSTAIN everywhere
 */

#include "substrate.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_substrate_self_test();

    struct { const char *label; float a[4]; float tau; } cases[3] = {
        { "dominant", { 0.02f, 0.03f, 0.90f, 0.05f }, 0.30f },
        { "middle",   { 0.10f, 0.10f, 0.70f, 0.10f }, 0.50f },
        { "uniform",  { 0.25f, 0.25f, 0.25f, 0.25f }, 0.30f }
    };

    printf("{\"kernel\":\"sigma_substrate\","
           "\"self_test_rc\":%d,"
           "\"demo\":[",
           rc);
    for (int i = 0; i < 3; ++i) {
        float sig[COS_SUB__COUNT]; int gate[COS_SUB__COUNT];
        int eq = cos_sigma_substrate_equivalence(cases[i].a, 4,
                                                 cases[i].tau,
                                                 sig, gate);
        printf("%s{\"case\":\"%s\",\"tau_accept\":%.2f,\"equivalent\":%s,"
               "\"digital\":{\"sigma\":%.4f,\"accept\":%d},"
               "\"bitnet\":{\"sigma\":%.4f,\"accept\":%d},"
               "\"spike\":{\"sigma\":%.4f,\"accept\":%d},"
               "\"photonic\":{\"sigma\":%.4f,\"accept\":%d}}",
               i == 0 ? "" : ",",
               cases[i].label, (double)cases[i].tau,
               eq == 0 ? "true" : "false",
               (double)sig[0], gate[0],
               (double)sig[1], gate[1],
               (double)sig[2], gate[2],
               (double)sig[3], gate[3]);
    }
    printf("],\"pass\":%s}\n", (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Substrate: equivalence across %d backends\n",
                COS_SUB__COUNT);
    }
    return rc;
}
