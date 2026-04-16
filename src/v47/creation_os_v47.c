/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v47 — verified-architecture lab: ACSL σ-kernel + ZK-σ API stub +
 * SymbiYosys extended harness driver (see Makefile `verify-*`).
 */
#include "sigma_kernel_verified.h"
#include "zk_sigma.h"

#include "../sigma/decompose.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m)
{
    fprintf(stderr, "[v47 self-test] FAIL %s\n", m);
    return 1;
}

static int nearly_same(float a, float b)
{
    return fabsf(a - b) <= 5e-4f * fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
}

static int test_dirichlet_matches_shipped(void)
{
    enum { n = 24 };
    float logits[n];
    for (int i = 0; i < n; i++) {
        logits[i] = (i == 5) ? 1.5f : -0.25f * (float)i;
    }
    sigma_decomposed_t a = {0};
    sigma_decomposed_t b = {0};
    sigma_decompose_dirichlet_evidence(logits, n, &a);
    v47_verified_dirichlet_decompose(logits, n, &b);
    if (!nearly_same(a.total, b.total) || !nearly_same(a.aleatoric, b.aleatoric) || !nearly_same(a.epistemic, b.epistemic)) {
        return fail("dirichlet mismatch vs shipped double path");
    }
    fprintf(stderr, "[v47 self-test] OK dirichlet parity (total=%g)\n", (double)b.total);
    return 0;
}

static int test_zk_sigma_stub(void)
{
    enum { n = 16 };
    float logits[n];
    for (int i = 0; i < n; i++) {
        logits[i] = -0.1f * (float)i;
    }
    zk_sigma_proof_t z = prove_sigma(logits, n);
    if (!z.verified) {
        return fail("prove_sigma");
    }
    if (!verify_sigma_proof(&z)) {
        return fail("verify_sigma_proof");
    }
    fprintf(stderr, "[v47 self-test] OK zk_sigma stub (packed witness; not ZK)\n");
    return 0;
}

static int test_entropy_finite(void)
{
    enum { n = 11 };
    float logits[n];
    for (int i = 0; i < n; i++) {
        logits[i] = -2.0f + 0.17f * (float)i;
    }
    float H = v47_verified_softmax_entropy(logits, n);
    if (!isfinite(H) || H < 0.0f) {
        return fail("entropy");
    }
    float m = v47_verified_top_margin01(logits, n);
    if (!isfinite(m) || m < 0.0f || m > 1.0f) {
        return fail("margin01 range");
    }
    fprintf(stderr, "[v47 self-test] OK entropy/margin (H=%g m=%g)\n", (double)H, (double)m);
    return 0;
}

static int self_test(void)
{
    if (test_dirichlet_matches_shipped() != 0) {
        return 1;
    }
    if (test_entropy_finite() != 0) {
        return 1;
    }
    if (test_zk_sigma_stub() != 0) {
        return 1;
    }
    return 0;
}

/** Read whitespace-separated floats from `path` into `out` (cap `maxn`). Returns count or -1. */
static int read_float_file(const char *path, float *out, int maxn)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    int k = 0;
    while (k < maxn && fscanf(f, "%f", &out[k]) == 1) {
        k++;
    }
    fclose(f);
    return k;
}

static int emit_sigma_json(const float *logits, int n)
{
    sigma_decomposed_t s = {0};
    v47_verified_dirichlet_decompose(logits, n, &s);
    float H = v47_verified_softmax_entropy(logits, n);
    float m = v47_verified_top_margin01(logits, n);
    const char *action = (s.epistemic > 0.2f) ? "abstain" : "emit";
    printf(
        "{\"entropy\":%.9g,\"aleatoric\":%.9g,\"epistemic\":%.9g,\"total\":%.9g,\"margin\":%.9g,\"action\":\"%s\"}\n",
        (double)H,
        (double)s.aleatoric,
        (double)s.epistemic,
        (double)s.total,
        (double)m,
        action);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--sigma-file") == 0) {
        const char *path = (argc >= 3) ? argv[2] : NULL;
        if (!path) {
            fprintf(stderr, "usage: creation_os_v47 --sigma-file path/to/floats.txt\n");
            return 2;
        }
        enum { cap = V47_VERIFIED_MAX_N };
        float logits[cap];
        int n = read_float_file(path, logits, cap);
        if (n < 2) {
            fprintf(stderr, "creation_os_v47: need >=2 floats in %s\n", path);
            return 2;
        }
        return emit_sigma_json(logits, n);
    }
    fprintf(stderr, "creation_os_v47: pass --self-test or --sigma-file <path>\n");
    return 2;
}
