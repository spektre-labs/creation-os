/* cos-calibrate — SCI-1 / SCI-2 CLI.
 *
 * Reads a detail JSONL with per-row σ and correctness, runs split
 * conformal calibration at the requested (α, δ), and writes
 * ~/.cos/calibration.json.  Optional --per-domain partitions the
 * dataset on the row-level "category" tag and produces one τ per
 * partition, with a global "all" row kept first.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "conformal.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define COS_CONFORMAL_MAX_SAMPLES 8192

static const char *DEFAULT_DATASET =
    "benchmarks/pipeline/truthfulqa_817_detail.jsonl";

static int resolve_home(char *out, size_t cap) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home || !*home) return -1;
    if (snprintf(out, cap, "%s", home) >= (int)cap) return -1;
    return 0;
}

static int ensure_parent_dir(const char *path) {
    char buf[1024];
    if (snprintf(buf, sizeof(buf), "%s", path) >= (int)sizeof(buf)) return -1;
    char *slash = strrchr(buf, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (*buf == '\0') return 0;
    if (mkdir(buf, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static void usage(void) {
    fprintf(stderr,
        "cos-calibrate — conformal σ-calibration (SCI-1 / SCI-2)\n"
        "\n"
        "  cos-calibrate [--dataset PATH]   (default: %s)\n"
        "                [--mode MODE]       (default: pipeline)\n"
        "                [--alpha A]         (default: 0.10)\n"
        "                [--delta D]         (default: 0.05)\n"
        "                [--out PATH]        (default: ~/.cos/calibration.json)\n"
        "                [--per-domain]      (SCI-2: one τ per category)\n"
        "                [--classify-prompts] (SCI-2: factual/code/reasoning)\n"
        "                [--self-test]\n"
        "\n"
        "The calibration writes a JSON file whose shape is:\n"
        "  { \"schema\": \"cos.conformal.v1\",\n"
        "    \"alpha\":..., \"delta\":...,\n"
        "    \"per_domain\": [ { \"domain\":\"all\", \"tau\":..., ... }, ... ] }\n"
        "\n"
        "Guarantee (Angelopoulos-Bates \"Learn-then-Test\", specialised\n"
        "to binary selective prediction): with probability ≥ 1 − δ over\n"
        "the calibration draw, accepting σ ≤ τ yields\n"
        "\n"
        "    P(wrong | σ ≤ τ)  ≤  α.\n"
        "\n",
        DEFAULT_DATASET);
}

int main(int argc, char **argv) {
    const char *dataset = DEFAULT_DATASET;
    const char *mode    = "pipeline";
    const char *outpath = NULL;
    float alpha = 0.10f;
    float delta = 0.05f;
    int per_domain = 0;
    int classify   = 0;
    int self_test  = 0;

    for (int i = 1; i < argc; ++i) {
        if      (!strcmp(argv[i], "--dataset") && i + 1 < argc) dataset = argv[++i];
        else if (!strcmp(argv[i], "--mode")    && i + 1 < argc) mode    = argv[++i];
        else if (!strcmp(argv[i], "--out")     && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--alpha")   && i + 1 < argc) alpha   = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--delta")   && i + 1 < argc) delta   = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--per-domain")) per_domain = 1;
        else if (!strcmp(argv[i], "--classify-prompts")) { classify = 1; per_domain = 1; }
        else if (!strcmp(argv[i], "--self-test"))  self_test  = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "cos-calibrate: unknown arg: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (self_test) {
        int rc = cos_conformal_self_test();
        printf("cos-calibrate: self-test %s\n", rc == 0 ? "PASS" : "FAIL");
        return rc == 0 ? 0 : 1;
    }

    char default_out[1024] = {0};
    if (!outpath) {
        char home[512] = {0};
        if (resolve_home(home, sizeof(home)) != 0) {
            fprintf(stderr, "cos-calibrate: cannot resolve HOME\n");
            return 1;
        }
        snprintf(default_out, sizeof(default_out),
                 "%s/.cos/calibration.json", home);
        outpath = default_out;
    }

    cos_conformal_sample_t *samples = (cos_conformal_sample_t *)
        calloc((size_t)COS_CONFORMAL_MAX_SAMPLES, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "cos-calibrate: OOM\n");
        return 1;
    }

    int n = cos_conformal_load_jsonl_ex(dataset, mode, classify, samples,
                                        COS_CONFORMAL_MAX_SAMPLES);
    if (n < 0) {
        fprintf(stderr, "cos-calibrate: cannot read dataset: %s\n", dataset);
        free(samples);
        return 1;
    }

    cos_conformal_bundle_t bundle;
    memset(&bundle, 0, sizeof(bundle));

    if (per_domain) {
        if (cos_conformal_per_domain(samples, n, alpha, delta, &bundle) != 0) {
            fprintf(stderr, "cos-calibrate: per-domain calibration failed\n");
            free(samples);
            return 1;
        }
    } else {
        cos_conformal_report_t r;
        if (cos_conformal_calibrate(samples, n, alpha, delta, &r) != 0) {
            fprintf(stderr, "cos-calibrate: calibration failed\n");
            free(samples);
            return 1;
        }
        snprintf(r.domain, sizeof(r.domain), "%s", "all");
        bundle.reports[0] = r;
        bundle.n_reports  = 1;
    }

    if (ensure_parent_dir(outpath) != 0) {
        fprintf(stderr, "cos-calibrate: cannot create parent of %s\n", outpath);
        free(samples);
        return 1;
    }
    if (cos_conformal_write_bundle_json(outpath, &bundle) != 0) {
        fprintf(stderr, "cos-calibrate: write failed: %s\n", outpath);
        free(samples);
        return 1;
    }

    printf("Creation OS — conformal σ-calibration\n");
    printf("  dataset     : %s\n", dataset);
    printf("  mode filter : %s\n", mode);
    printf("  α (risk)    : %.4f\n", alpha);
    printf("  δ (conf.)   : %.4f\n", delta);
    printf("  samples     : %d loaded\n", n);
    printf("  output      : %s\n", outpath);
    printf("\n");
    printf("  %-16s %-8s %-10s %-10s %-10s %-6s\n",
           "domain", "τ", "risk_hat", "risk_ucb", "coverage", "valid");
    for (int i = 0; i < bundle.n_reports; ++i) {
        const cos_conformal_report_t *r = &bundle.reports[i];
        printf("  %-16s %-8.4f %-10.4f %-10.4f %-10.4f %-6s\n",
               r->domain, r->tau, r->empirical_risk, r->risk_ucb,
               r->coverage, r->valid ? "yes" : "NO");
    }
    printf("\n  Guarantee: with prob. ≥ 1 − δ over the calibration,\n");
    printf("             accepting σ ≤ τ yields P(wrong | accept) ≤ α.\n");

    free(samples);
    return 0;
}
