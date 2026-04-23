/*
 * Compliance checks derived from runtime proof receipts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "compliance.h"
#include "reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static int classify_risk(float sigma_combined)
{
    if (sigma_combined < 0.25f)
        return 1;
    if (sigma_combined < 0.55f)
        return 2;
    if (sigma_combined < 0.85f)
        return 3;
    return 4;
}

int cos_compliance_check_output(const struct cos_proof_receipt *receipt,
                                struct cos_compliance_check *check)
{
    int nonzero;

    if (receipt == NULL || check == NULL)
        return -1;

    memset(check, 0, sizeof(*check));
    check->violations[0] = '\0';

    nonzero = 0;
    for (int i = 0; i < 32; ++i)
        nonzero |= receipt->receipt_hash[i];

    check->risk_classification = classify_risk(receipt->sigma_combined);

    check->human_oversight_active =
        receipt->sovereign_brake ? 1 : 0;

    check->audit_trail_complete =
        nonzero ? 1 : 0;

    check->safety_refusal_proven =
        (receipt->gate_decision == COS_SIGMA_ACTION_ABSTAIN && nonzero)
            ? 1
            : 0;

    check->eu_ai_act_compliant =
        nonzero && receipt->within_compute_budget;

    if (receipt->injection_detected && nonzero) {
        snprintf(check->violations, sizeof check->violations,
                 "Injection signal recorded in receipt.");
        check->eu_ai_act_compliant = 0;
    }

    if (!receipt->within_compute_budget) {
        snprintf(check->violations, sizeof check->violations,
                 "Compute budget exceeded (per receipt).");
        check->eu_ai_act_compliant = 0;
    }

    return 0;
}

char *cos_compliance_report(const struct cos_compliance_check *check)
{
    char *s;
    int   n;

    if (check == NULL)
        return NULL;
    s = malloc(768);
    if (s == NULL)
        return NULL;

    n = snprintf(s, 768,
                 "{\"eu_ai_act_compliant\":%d,"
                 "\"safety_refusal_proven\":%d,"
                 "\"risk_classification\":%d,"
                 "\"human_oversight_active\":%d,"
                 "\"audit_trail_complete\":%d,"
                 "\"violations\":\"%s\"}",
                 check->eu_ai_act_compliant,
                 check->safety_refusal_proven,
                 check->risk_classification,
                 check->human_oversight_active,
                 check->audit_trail_complete,
                 check->violations);
    if (n < 0 || n >= 768) {
        free(s);
        return NULL;
    }
    return s;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int cmp_fail(const char *m)
{
    fprintf(stderr, "compliance self-test: %s\n", m);
    return 1;
}
#endif

int cos_compliance_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_proof_receipt r;
    struct cos_compliance_check c;

    memset(&r, 0, sizeof r);
    r.sigma_combined = 0.9f;
    r.gate_decision  = COS_SIGMA_ACTION_ACCEPT;
    r.within_compute_budget = 1;
    r.receipt_hash[0]      = 0xab;

    if (cos_compliance_check_output(&r, &c) != 0)
        return cmp_fail("check_output");

    if (c.risk_classification != 4)
        return cmp_fail("risk tier");

    r.sigma_combined = 0.1f;
    r.gate_decision  = COS_SIGMA_ACTION_ABSTAIN;
    if (cos_compliance_check_output(&r, &c) != 0)
        return cmp_fail("refusal");
    if (c.safety_refusal_proven != 1)
        return cmp_fail("refusal proven");

    fprintf(stderr, "compliance self-test: OK\n");
#endif
    return 0;
}
