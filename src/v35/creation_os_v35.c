/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v35 — σ-guided speculative decoding (POSIX lab)
 *
 * Not merge-gate. Full BitNet+Qwen throughput rows remain **N-tier** until
 * weights + harness are archived (see docs/WHAT_IS_REAL.md).
 */
#if defined(_WIN32)
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "creation_os_v35: POSIX-only.\n");
    return 2;
}
#else

#include "../sigma/decompose.h"
#include "spec_decode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int self_test_draft_length(void)
{
    sigma_decomposed_t low_u;
    memset(&low_u, 0, sizeof low_u);
    low_u.epistemic = 0.05f;
    int k0 = cos_spec_compute_draft_length(low_u, 8);
    if (k0 != 8) {
        fprintf(stderr, "[v35 self-test] FAIL expected K=8 for low epistemic got %d\n", k0);
        return 1;
    }
    sigma_decomposed_t mid;
    memset(&mid, 0, sizeof mid);
    mid.epistemic = 0.35f;
    int k1 = cos_spec_compute_draft_length(mid, 8);
    if (k1 != 4) {
        fprintf(stderr, "[v35 self-test] FAIL expected K=4 for mid epistemic got %d\n", k1);
        return 1;
    }
    sigma_decomposed_t hi;
    memset(&hi, 0, sizeof hi);
    hi.epistemic = 3.0f;
    int k2 = cos_spec_compute_draft_length(hi, 8);
    if (k2 != 0) {
        fprintf(stderr, "[v35 self-test] FAIL expected K=0 for high epistemic got %d\n", k2);
        return 1;
    }
    fprintf(stderr, "[v35 self-test] OK draft_length (K=%d,%d,%d)\n", k0, k1, k2);
    return 0;
}

static int self_test_dual_verify(void)
{
    int agree[] = { 1, 1, 1 };
    float verif_epi[] = { 0.1f, 0.9f, 0.1f };
    cos_spec_verify_stats_t st;
    cos_spec_verify_with_dual_sigma(agree, verif_epi, 3, 0.7f, &st);
    if (st.accepted != 1 || st.abstained != 1 || st.rejected != 0) {
        fprintf(stderr, "[v35 self-test] FAIL dual verify stats a=%d r=%d z=%d\n", st.accepted, st.rejected,
            st.abstained);
        return 1;
    }
    int agree2[] = { 1, 0, 1 };
    float verif_epi2[] = { 0.1f, 0.1f, 0.1f };
    cos_spec_verify_with_dual_sigma(agree2, verif_epi2, 3, 0.7f, &st);
    if (st.accepted != 1 || st.rejected != 1) {
        fprintf(stderr, "[v35 self-test] FAIL reject path\n");
        return 1;
    }
    fprintf(stderr, "[v35 self-test] OK dual_sigma verify\n");
    return 0;
}

static int self_test_routing_load(void)
{
    cos_spec_routing_t r;
    if (cos_spec_routing_load("config/spec_routing.json", &r) != 0) {
        fprintf(stderr, "[v35 self-test] SKIP spec_routing.json missing\n");
        return 0;
    }
    if (fabsf(r.sigma_local_threshold - 0.4f) > 1e-3f) {
        fprintf(stderr, "[v35 self-test] FAIL sigma_local_threshold parse\n");
        return 1;
    }
    fprintf(stderr, "[v35 self-test] OK spec_routing load\n");
    return 0;
}

/** Deterministic toy “throughput” for harness smoke (not M-tier). */
static void bench_spec_synthetic(void)
{
    const int tokens = 512;
    struct {
        const char *name;
        float scale;
        float accept;
    } modes[] = { { "bitnet_only", 1.0f, 1.0f }, { "spec_fixed_k4", 2.2f, 0.62f },
        { "spec_sigma_adaptive", 2.9f, 0.71f }, { "spec_sigma_dual_abstain", 2.7f, 0.68f } };
    printf("SPEC_BENCH_SYNTHETIC tokens=%d (toy; not M-tier)\n", tokens);
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        double tps = 120.0 * (double)modes[i].scale;
        printf("MODE %s tokens_per_sec=%.1f acceptance=%.3f abstain_rate=%.3f truthfulqa_acc_na=n/a cps_na=n/a\n",
            modes[i].name, tps, (double)modes[i].accept, 0.04 + 0.01 * (double)i);
    }
}

static void print_help(const char *argv0)
{
    printf("usage: %s [--self-test] [--bench-spec-synthetic] [--help]\n", argv0);
    printf("\n");
    printf("v35 lab: σ-guided adaptive draft length + dual-model verify policy hooks.\n");
    printf("\n");
    printf("Not part of merge-gate.\n");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            int a = self_test_draft_length();
            int b = self_test_dual_verify();
            int c = self_test_routing_load();
            return (a == 0 && b == 0 && c == 0) ? 0 : 2;
        }
        if (!strcmp(argv[i], "--bench-spec-synthetic")) {
            bench_spec_synthetic();
            return 0;
        }
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_help(argv[0]);
            return 0;
        }
    }
    print_help(argv[0]);
    fprintf(stderr, "Hint: %s --self-test\n", argv[0]);
    return 2;
}
#endif
