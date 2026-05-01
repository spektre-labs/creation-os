/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos-bench-suite-sci — SCI-6 CLI.
 *
 * Builds the multi-dataset σ-gate table from a manifest of detail
 * JSONLs and writes benchmarks/suite/full_results.json.  Missing
 * datasets are recorded as "measured": false and get zero-filled
 * metrics; this is intentional so the table remains complete.
 *
 * Default manifest (when --manifest is omitted):
 *   - truthfulqa       benchmarks/pipeline/truthfulqa_817_detail.jsonl
 *                      (mode filter: "pipeline")
 *   - arc_challenge    benchmarks/suite/arc_challenge_detail.jsonl
 *   - arc_easy         benchmarks/suite/arc_easy_detail.jsonl
 *   - gsm8k            benchmarks/suite/gsm8k_detail.jsonl
 *   - hellaswag        benchmarks/suite/hellaswag_detail.jsonl
 *
 * A maintainer plugs in the missing detail JSONLs by running
 * cos-chat over the corresponding dataset.  The aggregator does not
 * fabricate numbers.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "suite_sci.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *path;
    const char *mode;
} default_entry_t;

static const default_entry_t DEFAULT_MANIFEST[] = {
    { "truthfulqa",    "benchmarks/pipeline/truthfulqa_817_detail.jsonl",
                       "pipeline" },
    { "arc_challenge", "benchmarks/suite/arc_challenge_detail.jsonl",
                       "pipeline" },
    { "arc_easy",      "benchmarks/suite/arc_easy_detail.jsonl",
                       "pipeline" },
    { "gsm8k",         "benchmarks/suite/gsm8k_detail.jsonl",
                       "pipeline" },
    { "hellaswag",     "benchmarks/suite/hellaswag_detail.jsonl",
                       "pipeline" },
    { NULL, NULL, NULL }
};

static void usage(void) {
    fprintf(stderr,
        "cos-bench-suite-sci — multi-dataset σ-gate evaluation (SCI-6)\n"
        "\n"
        "  cos-bench-suite-sci [--alpha A]      (default: 0.30)\n"
        "                      [--delta D]      (default: 0.10)\n"
        "                      [--out PATH]     (default: benchmarks/suite/full_results.json)\n"
        "                      [--self-test]\n"
        "\n"
        "Aggregates the σ-gate evaluation across TruthfulQA + ARC-\n"
        "{Challenge,Easy} + GSM8K + HellaSwag from their respective\n"
        "per-row detail JSONLs.  Datasets whose detail file is missing\n"
        "appear in the output with \"measured\": false; no numbers are\n"
        "fabricated.  See docs/selective_prediction.md §4 for how to\n"
        "produce a detail JSONL from cos-chat.\n");
}

int main(int argc, char **argv) {
    float alpha = 0.30f;
    float delta = 0.10f;
    const char *outpath = "benchmarks/suite/full_results.json";
    int self_test = 0;

    for (int i = 1; i < argc; ++i) {
        if      (!strcmp(argv[i], "--alpha") && i + 1 < argc) alpha = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--delta") && i + 1 < argc) delta = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--out")   && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--self-test")) self_test = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "cos-bench-suite-sci: unknown arg: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (self_test) {
        int rc = cos_suite_sci_self_test();
        printf("cos-bench-suite-sci: self-test %s\n",
               rc == 0 ? "PASS" : "FAIL");
        return rc == 0 ? 0 : 1;
    }

    cos_suite_sci_report_t rep;
    memset(&rep, 0, sizeof(rep));

    for (int i = 0; DEFAULT_MANIFEST[i].name &&
                    rep.n_rows < COS_SUITE_SCI_MAX_ROWS; ++i) {
        cos_suite_sci_row_t row;
        memset(&row, 0, sizeof(row));
        if (cos_suite_sci_eval(DEFAULT_MANIFEST[i].name,
                               DEFAULT_MANIFEST[i].path,
                               DEFAULT_MANIFEST[i].mode,
                               alpha, delta, &row) != 0) {
            fprintf(stderr, "cos-bench-suite-sci: eval failed: %s\n",
                    DEFAULT_MANIFEST[i].name);
            continue;
        }
        rep.rows[rep.n_rows++] = row;
    }

    if (cos_suite_sci_write_json(outpath, &rep, alpha, delta) != 0) {
        fprintf(stderr, "cos-bench-suite-sci: write failed: %s\n", outpath);
        return 1;
    }

    printf("Creation OS — σ-gate benchmark suite\n");
    printf("  α=%.4f  δ=%.4f  output=%s\n\n", alpha, delta, outpath);
    printf("  %-16s %-8s %-6s %-8s %-8s %-8s %-8s %-8s %-6s\n",
           "dataset", "measured", "N",
           "acc_all", "acc_acc", "cov_acc", "σ_mean", "τ", "valid");
    int measured_rows = 0, guarantee_rows = 0;
    for (int i = 0; i < rep.n_rows; ++i) {
        const cos_suite_sci_row_t *r = &rep.rows[i];
        printf("  %-16s %-8s %-6d %-8.4f %-8.4f %-8.4f %-8.4f %-8.4f %-6s\n",
               r->name,
               r->measured ? "yes" : "NO",
               r->n_rows,
               r->accuracy_all,
               r->accuracy_accepted,
               r->coverage_accepted,
               r->sigma_mean,
               r->tau,
               r->tau_valid ? "yes" : "NO");
        if (r->measured)  measured_rows += 1;
        if (r->tau_valid) guarantee_rows += 1;
    }
    printf("\n  measured datasets  : %d / %d\n", measured_rows, rep.n_rows);
    printf("  conformal guarantee: %d / %d\n", guarantee_rows, rep.n_rows);
    printf("\n  missing detail JSONLs are not fabricated — re-run\n");
    printf("  cos-chat over each dataset to fill them in.\n");
    return 0;
}
