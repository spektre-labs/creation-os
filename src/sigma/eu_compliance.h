/*
 * EU AI Act–oriented compliance snapshot (documentation / CLI layer).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EU_COMPLIANCE_H
#define COS_SIGMA_EU_COMPLIANCE_H

#ifdef __cplusplus
extern "C" {
#endif

struct cos_eu_compliance {
    int  risk_tier; /* 1 minimal … 4 unacceptable */
    int  human_oversight;
    int  transparency;
    int  audit_trail;
    int  incident_reporting;
    int  data_governance;
    int  compliant;
    char report[2048];
};

struct cos_eu_compliance cos_eu_check(void);

/** Returns malloc'd UTF-8 report (caller frees), or NULL. */
char *cos_eu_report(const struct cos_eu_compliance *c);

/** NIST AI RMF–style checklist text; static buffer, do not free. */
const char *cos_nist_rmf_report(void);

/** EU AI Act Article 15 evidence (σ + SHA-256 receipts); static buffer. */
const char *cos_eu_ai_act_article15_evidence_md(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_EU_COMPLIANCE_H */
