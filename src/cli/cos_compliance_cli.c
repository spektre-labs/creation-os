/*
 * cos compliance — compliance checks over proof receipts (CLI).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "compliance.h"
#include "proof_receipt.h"
#include "reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int eu = 0;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--eu") == 0)
            eu = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stdout,
                    "usage: cos compliance [--eu]\n"
                    "  default     run compliance module self-test\n"
                    "  --eu        print EU AI Act oriented compliance JSON\n");
            return 0;
        }
    }

    if (eu) {
        struct cos_proof_receipt    r;
        struct cos_compliance_check c;
        char                         *rep;

        memset(&r, 0, sizeof r);
        r.sigma_combined        = 0.99f;
        r.within_compute_budget = 1;
        r.receipt_hash[0]       = 0x01;
        r.gate_decision         = COS_SIGMA_ACTION_ACCEPT;
        r.sovereign_brake       = 1;

        cos_compliance_check_output(&r, &c);
        rep = cos_compliance_report(&c);
        if (rep == NULL)
            return 2;
        fputs(rep, stdout);
        fputc('\n', stdout);
        free(rep);
        return 0;
    }

    return cos_compliance_self_test() != 0 ? 1 : 0;
}
