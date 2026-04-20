/*
 * σ-formal-complete demo — parses the v259 Lean/ACSL artefacts and
 * emits a deterministic JSON status envelope the CI check pins.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "formal_complete.h"

#include <stdio.h>
#include <string.h>

static const char *kind_str(int k) {
    switch (k) {
        case COS_FORMAL_DISCHARGED_CONCRETE: return "discharged_concrete";
        case COS_FORMAL_DISCHARGED_ABSTRACT: return "discharged_abstract";
        case COS_FORMAL_PENDING:             return "pending";
        default:                             return "unknown";
    }
}

int main(void) {
    cos_formal_report_t r;
    int rc = cos_formal_complete_scan(
        "hw/formal/v259/Measurement.lean",
        "hw/formal/v259/sigma_measurement.h.acsl",
        "include/cos_version.h",
        &r);
    int st = cos_formal_complete_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_formal_complete\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"scan_rc\": %d,\n", rc);
    printf("  \"lean\": {\n");
    printf("    \"n_theorems\": %d,\n", r.n_theorems);
    printf("    \"discharged_concrete\": %d,\n", r.discharged_concrete);
    printf("    \"discharged_abstract\": %d,\n", r.discharged_abstract);
    printf("    \"pending\": %d,\n", r.pending);
    printf("    \"entries\": [\n");
    for (int i = 0; i < r.n_theorems; i++) {
        const cos_formal_theorem_t *t = &r.theorems[i];
        printf("      { \"name\": \"%s\", \"kind\": \"%s\","
               " \"line\": %d, \"has_sorry\": %s, \"abstract\": %s }%s\n",
               t->name, kind_str(t->kind), t->line,
               t->has_sorry ? "true" : "false",
               t->abstract_variant ? "true" : "false",
               i + 1 == r.n_theorems ? "" : ",");
    }
    printf("    ]\n");
    printf("  },\n");
    printf("  \"acsl\": {\n");
    printf("    \"requires\": %d,\n", r.acsl_requires);
    printf("    \"ensures\":  %d,\n", r.acsl_ensures);
    printf("    \"file_bytes\": %d\n", r.acsl_file_bytes);
    printf("  },\n");
    printf("  \"ledger\": {\n");
    printf("    \"header_proofs\": %d,\n", r.header_proofs);
    printf("    \"header_proofs_total\": %d,\n", r.header_proofs_total);
    printf("    \"effective_discharged\": %d,\n", r.effective_discharged);
    printf("    \"ledger_matches_truth\": %s\n",
           r.ledger_matches_truth ? "true" : "false");
    printf("  }\n");
    printf("}\n");
    return st == 0 ? 0 : 1;
}
