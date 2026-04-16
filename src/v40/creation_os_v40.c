/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v40 — σ-threshold lab: channel independence + syndrome decoder (POSIX)
 *
 * Not merge-gate.
 */
#include "../sigma/independence_test.h"
#include "../sigma/syndrome_decoder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int test_independence_uncorrelated(void)
{
    const int nch = 8;
    const int ns = 256;
    float buf[nch * ns];
    memset(buf, 0, sizeof buf);
    for (int c = 0; c < nch; c++) {
        unsigned seed = 2166136261u ^ (unsigned)(c + 1) * 0x9E3779B1u;
        for (int s = 0; s < ns; s++) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            float u = (float)(seed & 0xffffu) / 65535.0f;
            buf[c * ns + s] = u;
        }
    }
    sigma_independence_t r = measure_channel_independence(buf, nch, ns);
    if (r.n_independent < 4) {
        fprintf(stderr, "[v40 self-test] FAIL expected many independent channels (got %d)\n", r.n_independent);
        return 1;
    }
    if (r.threshold_regime != 1) {
        fprintf(stderr, "[v40 self-test] FAIL expected threshold_regime=1\n");
        return 1;
    }
    fprintf(stderr, "[v40 self-test] OK independence (mean_abs_pair=%g n_ind=%d regime=%d)\n",
        (double)r.mean_correlation, r.n_independent, r.threshold_regime);
    return 0;
}

static int test_independence_identical_pairs(void)
{
    const int nch = 8;
    const int ns = 64;
    float buf[nch * ns];
    memset(buf, 0, sizeof buf);
    for (int s = 0; s < ns; s++) {
        float x = sinf((float)(s + 1) * 0.31f);
        for (int c = 0; c < nch; c++) {
            buf[c * ns + s] = x;
        }
    }
    sigma_independence_t r = measure_channel_independence(buf, nch, ns);
    if (r.n_independent > 3) {
        fprintf(stderr, "[v40 self-test] FAIL expected low independence on near-duplicate channels (got %d)\n",
            r.n_independent);
        return 1;
    }
    fprintf(stderr, "[v40 self-test] OK dependence trap (mean_abs_pair=%g n_ind=%d regime=%d)\n",
        (double)r.mean_correlation, r.n_independent, r.threshold_regime);
    return 0;
}

static int test_syndrome(void)
{
    float sigma[CH_COUNT];
    float thr[CH_COUNT];
    memset(sigma, 0, sizeof sigma);
    memset(thr, 0, sizeof thr);
    for (int i = 0; i < CH_COUNT; i++) {
        thr[i] = 0.5f;
    }

    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_EMIT) {
        fprintf(stderr, "[v40 self-test] FAIL emit\n");
        return 1;
    }

    sigma[CH_REPETITION] = 1.0f;
    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_RESAMPLE) {
        fprintf(stderr, "[v40 self-test] FAIL resample\n");
        return 1;
    }

    memset(sigma, 0, sizeof sigma);
    sigma[CH_TOP_MARGIN] = 1.0f;
    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_CITE) {
        fprintf(stderr, "[v40 self-test] FAIL cite\n");
        return 1;
    }

    memset(sigma, 0, sizeof sigma);
    sigma[0] = 1.0f;
    sigma[1] = 1.0f;
    sigma[2] = 1.0f;
    sigma[3] = 1.0f;
    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_ABSTAIN) {
        fprintf(stderr, "[v40 self-test] FAIL abstain\n");
        return 1;
    }

    memset(sigma, 0, sizeof sigma);
    sigma[CH_EPISTEMIC] = 1.0f;
    sigma[CH_ALEATORIC] = 1.0f;
    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_FALLBACK) {
        fprintf(stderr, "[v40 self-test] FAIL fallback\n");
        return 1;
    }

    memset(sigma, 0, sizeof sigma);
    sigma[CH_LOGIT_ENTROPY] = 1.0f;
    sigma[CH_HARDWARE] = 1.0f;
    if (decode_sigma_syndrome(sigma, CH_COUNT, thr) != ACTION_DECOMPOSE) {
        fprintf(stderr, "[v40 self-test] FAIL decompose\n");
        return 1;
    }

    fprintf(stderr, "[v40 self-test] OK syndrome decoder\n");
    return 0;
}

static int self_test(void)
{
    if (test_independence_uncorrelated() != 0) {
        return 1;
    }
    if (test_independence_identical_pairs() != 0) {
        return 1;
    }
    if (test_syndrome() != 0) {
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
    fprintf(stderr, "creation_os_v40: pass --self-test\n");
    return 2;
}
