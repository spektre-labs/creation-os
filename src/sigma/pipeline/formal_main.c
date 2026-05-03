/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Formal: runtime T3–T6 harness plus optional Lean / Frama-C CLI hooks.
 *
 * Default (no flags): JSON ledger from cos_sigma_formal_check_all.
 *
 *   ./creation_os_sigma_formal --lean --check-all
 *   ./creation_os_sigma_formal --frama-c --wp-all
 *   ./creation_os_sigma_formal --summary
 *   ./creation_os_sigma_formal --no-sorry
 */

#include "formal.h"
#include "formal_complete.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lean_check_all(void) {
    int s = system("bash scripts/v133/formal_lean_check_all.sh");
    return (s == 0) ? 0 : 1;
}

static int framac_wp_all(void) {
    int s = system("bash scripts/v133/run_frama_c_wp_all.sh");
    return (s == 0) ? 0 : 1;
}

static int formal_summary(void) {
    cos_formal_report_t rep;
    static const char *lean_paths[] = {
        "hw/formal/v259/Measurement.lean",
        "formal/lean/CreationOS/V133.lean",
    };
    static const char *acsl_paths[] = {
        "hw/formal/v259/sigma_measurement.h.acsl",
        "hw/formal/v133/sigma_stack_contracts.acsl",
    };
    if (cos_formal_complete_scan(lean_paths, 2, acsl_paths, 2,
                                 "include/cos_version.h", &rep) != 0) {
        fprintf(stderr, "cos formal --summary: scan failed\n");
        return 1;
    }
    printf("cos formal --summary\n"
           "  Lean 4:  %d theorems parsed (%d concrete, %d abstract, %d pending sorry)\n"
           "  Ledger:  header %d/%d — effective window %d\n"
           "  ACSL:    %d requires + %d ensures lines (tracked bytes %d)\n"
           "  Tier-1 Frama-C Wp (gate+clamp): scripts/v259/run_frama_c_wp.sh — 15 goals\n",
           rep.n_theorems,
           rep.discharged_concrete,
           rep.discharged_abstract,
           rep.pending,
           rep.header_proofs, rep.header_proofs_total,
           rep.effective_discharged,
           rep.acsl_requires, rep.acsl_ensures, rep.acsl_file_bytes);
    return 0;
}

static int no_sorry(void) {
    int s = system("bash scripts/v133/check_lean_no_sorry.sh");
    return (s == 0) ? 0 : 1;
}

int main(int argc, char **argv) {
    cos_formal_result_t r[4];
    int rc;
    int discharged_count = 0;
    int i;

    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        fprintf(stderr,
                "creation_os_sigma_formal — runtime σ-formal harness / CLI\n"
                "  (default)              JSON T3–T6 ledger\n"
                "  --lean --check-all     lake build Measurement + CreationOS.V133\n"
                "  --frama-c --wp-all     tier-1 v259 Wp when frama-c installed\n"
                "  --summary              formal-complete scan summary\n"
                "  --no-sorry             deny `sorry` in Lean artefacts\n"
                "  --demo                 stderr one-line T3–T6 count (default mode only)\n");
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "--lean") == 0 &&
        strcmp(argv[2], "--check-all") == 0)
        return lean_check_all() ? 1 : 0;
    if (argc >= 3 && strcmp(argv[1], "--frama-c") == 0 &&
        strcmp(argv[2], "--wp-all") == 0)
        return framac_wp_all() ? 1 : 0;
    if (argc >= 2 && strcmp(argv[1], "--summary") == 0)
        return formal_summary() ? 1 : 0;
    if (argc >= 2 && strcmp(argv[1], "--no-sorry") == 0)
        return no_sorry() ? 1 : 0;

    rc = cos_sigma_formal_check_all(r);
    for (i = 0; i < 4; ++i)
        if (r[i].discharged) discharged_count++;

    printf("{\"kernel\":\"sigma_formal\","
           "\"self_test_rc\":%d,"
           "\"ledger\":{"
             "\"theorems\":[", rc);
    for (i = 0; i < 4; ++i) {
        if (i > 0) printf(",");
        printf("{\"id\":\"%s\",\"desc\":\"%s\","
               "\"witnesses\":%u,\"violations\":%u,"
               "\"discharged\":%s",
               r[i].theorem_id, r[i].description,
               r[i].witnesses, r[i].violations,
               r[i].discharged ? "true" : "false");
        if (strcmp(r[i].theorem_id, "T6") == 0) {
            printf(",\"median_ns\":%llu,\"p99_ns\":%llu,\"bound_ns\":%llu",
                   (unsigned long long)r[i].t6_median_ns,
                   (unsigned long long)r[i].t6_p99_ns,
                   (unsigned long long)r[i].t6_bound_ns);
        }
        printf("}");
    }
    printf("],\"discharged\":\"%d/4\"},"
           "\"pass\":%s}\n",
           discharged_count,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0)
        fprintf(stderr, "σ-Formal ledger: %d/4 theorems discharged\n",
                discharged_count);

    return rc ? 1 : 0;
}
