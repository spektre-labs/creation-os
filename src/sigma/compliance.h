/*
 * EU AI Act oriented compliance checks over proof receipts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_COMPLIANCE_H
#define COS_SIGMA_COMPLIANCE_H

#include "proof_receipt.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_compliance_check {
    int eu_ai_act_compliant;
    int safety_refusal_proven;
    int risk_classification;
    int human_oversight_active;
    int audit_trail_complete;
    char violations[512];
};

int cos_compliance_check_output(const struct cos_proof_receipt *receipt,
                                struct cos_compliance_check *check);

char *cos_compliance_report(const struct cos_compliance_check *check);

int cos_compliance_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_COMPLIANCE_H */
