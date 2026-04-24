/*
 * cos compliance — EU / NIST-oriented compliance snapshot (CLI sibling).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "compliance.h"
#include "eu_compliance.h"
#include "proof_receipt.h"
#include "reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int eu = 0, nist = 0, selftest = 0;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--eu") == 0)
            eu = 1;
        else if (strcmp(argv[i], "--nist") == 0)
            nist = 1;
        else if (strcmp(argv[i], "--self-test") == 0)
            selftest = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stdout,
                    "usage: cos-compliance [--eu] [--nist] [--self-test]\n"
                    "  default     EU AI Act–oriented snapshot + JSON\n"
                    "  --eu        include Article mapping text (default on)\n"
                    "  --nist      NIST AI RMF mapping\n"
                    "  --self-test legacy compliance.c self-test only\n");
            return 0;
        }
    }

    if (selftest)
        return cos_compliance_self_test() != 0 ? 1 : 0;

    {
        struct cos_eu_compliance c = cos_eu_check();
        char                    *js;

        if (nist)
            fputs(cos_nist_rmf_report(), stdout);
        if (eu || !nist)
            fputs(c.report, stdout);
        fputc('\n', stdout);

        js = cos_eu_report(&c);
        if (js == NULL)
            return 2;
        fputs(js, stdout);
        free(js);
        return c.compliant ? 0 : 1;
    }
}
