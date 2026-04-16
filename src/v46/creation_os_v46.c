/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v46 — σ-optimized BitNet pipeline lab (fast σ from logits, SIMD reductions, adaptive quant policy)
 *
 * Not merge-gate. No in-tree bitnet.cpp forward + wall-time σ A/B until harness exists.
 */
#include "adaptive_quant.h"
#include "fast_sigma.h"
#include "sigma_simd.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m)
{
    fprintf(stderr, "[v46 self-test] FAIL %s\n", m);
    return 1;
}

static int test_fast_sigma(void)
{
    enum { n = 32 };
    float logits[n];
    for (int i = 0; i < n; i++) {
        logits[i] = (i == 3) ? 2.0f : -1.0f;
    }
    sigma_decomposed_t s;
    memset(&s, 0, sizeof s);
    v46_fast_sigma_from_logits(logits, n, &s);
    if (!(s.total > 0.0f) || !isfinite(s.total) || !isfinite(s.epistemic)) {
        return fail("fast_sigma finite");
    }
    float H = v46_softmax_entropy_from_logits(logits, n);
    if (!(H >= 0.0f) || !isfinite(H)) {
        return fail("entropy");
    }
    float m = v46_top1_minus_top2_margin(logits, n);
    if (!(m > 0.0f)) {
        return fail("margin");
    }
    fprintf(stderr, "[v46 self-test] OK fast_sigma (H=%g m=%g epi=%g)\n", (double)H, (double)m, (double)s.epistemic);
    return 0;
}

static int test_adaptive_quant(void)
{
    sigma_decomposed_t low = {0};
    low.epistemic = 0.1f;
    v46_quant_config_t q1 = v46_sigma_adaptive_quant(&low);
    if (q1.activation_bits != 4 || fabsf(q1.sparsity - 0.55f) > 1e-3f) {
        return fail("quant low epistemic");
    }
    sigma_decomposed_t hi = {0};
    hi.epistemic = 0.9f;
    v46_quant_config_t q2 = v46_sigma_adaptive_quant(&hi);
    if (q2.sparsity != 0.0f) {
        return fail("quant high epistemic sparsity");
    }
    fprintf(stderr, "[v46 self-test] OK adaptive_quant\n");
    return 0;
}

static int test_simd_sum(void)
{
    float x[12];
    for (int i = 0; i < 12; i++) {
        x[i] = (float)(i + 1);
    }
    float s = v46_sum_f32_simd(x, 12);
    float expect = 78.0f;
    if (fabsf(s - expect) > 1e-3f) {
        return fail("simd sum");
    }
    float mx = v46_max_f32_simd(x, 12);
    if (fabsf(mx - 12.0f) > 1e-3f) {
        return fail("simd max");
    }
    (void)v46_simd_capabilities();
    fprintf(stderr, "[v46 self-test] OK sigma_simd (caps=%u)\n", (unsigned)v46_simd_capabilities());
    return 0;
}

static int test_latency_profile(void)
{
    v46_latency_profile_t p;
    v46_latency_profile_reset(&p);
    v46_latency_profile_add(&p, 29.0f, 0.3f);
    if (p.sigma_overhead_pct < 1.0f) {
        return fail("latency pct max");
    }
    fprintf(stderr, "[v46 self-test] OK latency_profile\n");
    return 0;
}

static int self_test(void)
{
    if (test_fast_sigma() != 0) {
        return 1;
    }
    if (test_adaptive_quant() != 0) {
        return 1;
    }
    if (test_simd_sum() != 0) {
        return 1;
    }
    if (test_latency_profile() != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    if (argv[1] && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    fprintf(stderr, "creation_os_v46: pass --self-test\n");
    return 2;
}
