/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos-multi-sigma — SCI-5 CLI (self-test + demo).
 *
 * The ensemble is a library surface; this CLI exists so CI can
 * exercise the math end-to-end and so operators can inspect the
 * component breakdown from the command line.
 *
 * Modes:
 *   --self-test            Runs cos_multi_sigma_self_test(); exits 0/1.
 *   --demo                 Prints a 3-response consistency check on a
 *                          hand-rolled fixture + combined σ with the
 *                          default weights.  Deterministic.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "multi_sigma.h"

#include <stdio.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
        "cos-multi-sigma — multi-σ ensemble (SCI-5)\n"
        "\n"
        "  cos-multi-sigma --self-test\n"
        "  cos-multi-sigma --demo\n"
        "\n"
        "Components:\n"
        "  σ_logprob      max per-token (1 − p_selected)\n"
        "  σ_entropy      mean normalised Shannon entropy per-token\n"
        "  σ_perplexity   1 − exp(mean logprob)\n"
        "  σ_consistency  1 − mean pairwise Jaccard of K regenerations\n"
        "\n"
        "Default weights: (0.50, 0.20, 0.10, 0.20); renormalised to 1.\n");
}

static int run_demo(void) {
    /* Hand-rolled fixture — no model calls. */
    float logprobs[4]    = { -0.05f, -0.50f, -0.10f, -0.02f };
    float row_peak[4]    = { 0.0f, -5.0f, -5.0f, -5.0f };
    float row_flat[4]    = { 0.0f, 0.0f, 0.0f, 0.0f };
    float row_mid[4]     = { 0.0f, -1.0f, -2.0f, -3.0f };
    float row_sharp[4]   = { 0.0f, -3.0f, -4.0f, -5.0f };
    const float *rows[4] = { row_peak, row_mid, row_sharp, row_flat };
    int ts[4]            = { 4, 4, 4, 4 };

    const char *texts[3] = {
        "Paris is the capital of France.",
        "The capital of France is Paris.",
        "The answer to the question is ambiguous.",
    };

    float s_lp = cos_multi_sigma_from_logprob(logprobs, 4);
    float s_en = cos_multi_sigma_entropy(rows, ts, 4);
    float s_pp = cos_multi_sigma_perplexity(logprobs, 4);
    float s_cs = cos_multi_sigma_consistency(texts, 3);

    cos_multi_sigma_t ens;
    cos_multi_sigma_weights_t w = cos_multi_sigma_default_weights();
    (void)cos_multi_sigma_combine(s_lp, s_en, s_pp, s_cs, &w, &ens);

    printf("cos-multi-sigma — demo (deterministic fixture)\n\n");
    printf("  component       value    weight\n");
    printf("  σ_logprob       %.4f   %.2f\n", s_lp, w.w_logprob);
    printf("  σ_entropy       %.4f   %.2f\n", s_en, w.w_entropy);
    printf("  σ_perplexity    %.4f   %.2f\n", s_pp, w.w_perplexity);
    printf("  σ_consistency   %.4f   %.2f\n", s_cs, w.w_consistency);
    printf("  ---------------------------\n");
    printf("  σ_combined      %.4f\n\n", ens.sigma_combined);

    printf("  K=3 regenerations used for consistency:\n");
    for (int i = 0; i < 3; ++i) printf("    [%d] %s\n", i + 1, texts[i]);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) { usage(); return 2; }
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--self-test")) {
            int rc = cos_multi_sigma_self_test();
            printf("cos-multi-sigma: self-test %s\n", rc == 0 ? "PASS" : "FAIL");
            return rc == 0 ? 0 : 1;
        }
        if (!strcmp(argv[i], "--demo"))      return run_demo();
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        }
    }
    usage();
    return 2;
}
