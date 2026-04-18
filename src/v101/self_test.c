/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v101 σ-BitNet-Bridge — self-test of the σ-channel math.
 *
 * These tests exercise only `cos_v101_sigma_from_logits()`, which is pure
 * C and does not depend on bitnet.cpp.  They therefore run in both stub
 * mode and real mode and are always part of `check-v101`.
 */
#include "bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass;
static int g_fail;

static void check(const char *name, int cond)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL  %s\n", name);
    }
}

static void fill_uniform(float *logits, int n_vocab)
{
    for (int i = 0; i < n_vocab; i++) logits[i] = 0.0f;
}

static void fill_peaked(float *logits, int n_vocab, int peak_id, float peak_logit)
{
    for (int i = 0; i < n_vocab; i++) logits[i] = 0.0f;
    logits[peak_id] = peak_logit;
}

static void fill_two_peaks(float *logits, int n_vocab, int a, int b, float l)
{
    for (int i = 0; i < n_vocab; i++) logits[i] = 0.0f;
    logits[a] = l;
    logits[b] = l;
}

int cos_v101_self_test(void)
{
    g_pass = 0;
    g_fail = 0;

    const int n_vocab = 128256;
    float *logits = (float *)calloc((size_t)n_vocab, sizeof(float));
    if (!logits) {
        printf("v101 σ-BitNet-Bridge: calloc fail\n");
        return -1;
    }
    cos_v101_sigma_t s;

    /* 1. Uniform logits → entropy_norm ≈ 1, margin ≈ 1, σ near upper range.
     * Both aggregators (arithmetic mean and geometric mean / product) are
     * close to 1 when every channel is close to 1, so we check both fields
     * directly and then check the default `sigma` alias. */
    fill_uniform(logits, n_vocab);
    check("uniform: sigma ok", cos_v101_sigma_from_logits(logits, n_vocab, &s) == 0);
    check("uniform: entropy_norm ≈ 1", s.entropy_norm > 0.99f);
    check("uniform: margin ≈ 1",       s.margin       > 0.99f);
    check("uniform: p_max ≈ 1",        s.p_max        > 0.99f);
    check("uniform: sigma_arith_mean ≥ 0.7", s.sigma_arith_mean >= 0.7f);
    check("uniform: sigma_product    ≥ 0.7", s.sigma_product    >= 0.7f);
    check("uniform: default sigma   ≥ 0.7",  s.sigma            >= 0.7f);
    check("uniform: all channels in [0,1]",
          s.entropy_norm >= 0.f && s.entropy_norm <= 1.f &&
          s.margin       >= 0.f && s.margin       <= 1.f &&
          s.top_k_mass   >= 0.f && s.top_k_mass   <= 1.f &&
          s.tail_mass    >= 0.f && s.tail_mass    <= 1.f &&
          s.logit_spread >= 0.f && s.logit_spread <= 1.f &&
          s.p_max        >= 0.f && s.p_max        <= 1.f &&
          s.n_effective  >= 0.f && s.n_effective  <= 1.f &&
          s.logit_std    >= 0.f && s.logit_std    <= 1.f);

    /* 2. Single sharp peak → all channels near 0.  Product is < mean here
     * (the geometric mean is dragged down by the zero-floor channels). */
    fill_peaked(logits, n_vocab, 42, 50.f);
    check("peaked: sigma ok",          cos_v101_sigma_from_logits(logits, n_vocab, &s) == 0);
    check("peaked: entropy_norm ≈ 0",  s.entropy_norm < 0.01f);
    check("peaked: margin ≈ 0",        s.margin       < 0.01f);
    check("peaked: p_max ≈ 0",         s.p_max        < 0.01f);
    check("peaked: sigma_arith_mean < 0.4", s.sigma_arith_mean < 0.4f);
    check("peaked: sigma_product    < 0.4", s.sigma_product    < 0.4f);
    check("peaked: default sigma   < 0.4",  s.sigma            < 0.4f);
    check("peaked: sigma_product ≤ sigma_arith_mean",
          s.sigma_product <= s.sigma_arith_mean + 1e-6f);

    /* 3. Two equal peaks → margin ≈ 1 (top1 == top2), entropy finite. */
    fill_two_peaks(logits, n_vocab, 1, 2, 50.f);
    check("two peaks: sigma ok",       cos_v101_sigma_from_logits(logits, n_vocab, &s) == 0);
    check("two peaks: margin > 0.99",  s.margin > 0.99f);
    check("two peaks: entropy small",  s.entropy_norm < 0.1f);

    /* 4. Error handling. */
    check("null logits → -1", cos_v101_sigma_from_logits(NULL, n_vocab, &s) == -1);
    check("null out → -1",    cos_v101_sigma_from_logits(logits, n_vocab, NULL) == -1);
    check("n_vocab < 2 → -1", cos_v101_sigma_from_logits(logits, 1, &s) == -1);

    /* 5. Determinism: same input, same output, bit for bit. */
    fill_uniform(logits, n_vocab);
    logits[7] = 3.14f;
    logits[11] = 2.72f;
    cos_v101_sigma_t s1, s2;
    cos_v101_sigma_from_logits(logits, n_vocab, &s1);
    cos_v101_sigma_from_logits(logits, n_vocab, &s2);
    check("deterministic σ", memcmp(&s1, &s2, sizeof(s1)) == 0);

    /* 6. Monotonic: raising the top logit without changing the rest
     *    should (weakly) decrease the σ (model becomes more confident).
     *    Both aggregators must satisfy this. */
    fill_uniform(logits, n_vocab);
    logits[0] = 1.f;
    cos_v101_sigma_from_logits(logits, n_vocab, &s1);
    logits[0] = 10.f;
    cos_v101_sigma_from_logits(logits, n_vocab, &s2);
    check("σ (default) monotone in peak sharpness", s2.sigma <= s1.sigma);
    check("σ_arith_mean monotone in peak sharpness",
          s2.sigma_arith_mean <= s1.sigma_arith_mean);
    check("σ_product monotone in peak sharpness",
          s2.sigma_product    <= s1.sigma_product);

    /* 7a. v105 regression: sigma_product must equal the geometric mean of
     *     the eight channels with the same 1e-6 epsilon floor used by
     *     `benchmarks/v104/operators.py::op_sigma_product`.  This is the
     *     direct regression tying the C path to the Python path the v104
     *     operator search was validated on. */
    fill_peaked(logits, n_vocab, 42, 50.f);
    cos_v101_sigma_from_logits(logits, n_vocab, &s1);
    {
        const double EPS = 1e-6;
        const float ch[COS_V101_SIGMA_CHANNELS] = {
            s1.entropy_norm, s1.margin, s1.top_k_mass, s1.tail_mass,
            s1.logit_spread, s1.p_max, s1.n_effective, s1.logit_std,
        };
        double acc = 0.0;
        for (int i = 0; i < COS_V101_SIGMA_CHANNELS; i++) {
            double v = (double)ch[i];
            if (v < EPS) v = EPS;
            acc += log(v);
        }
        double expected = exp(acc / (double)COS_V101_SIGMA_CHANNELS);
        double got      = (double)s1.sigma_product;
        /* 1e-4 tolerance absorbs the double→float round-trip per channel. */
        check("v105 regression: sigma_product == geom_mean(channels)",
              fabs(expected - got) < 1e-4);
    }

    /* 7b. v105: aggregator selector round-trips.  Default is PRODUCT;
     *    switching to MEAN and back restores the default and changes the
     *    value of the `sigma` field appropriately. */
    cos_v101_sigma_agg_t prev = cos_v101_get_default_aggregator();
    check("v105: default aggregator is PRODUCT", prev == COS_V101_AGG_PRODUCT);
    cos_v101_sigma_from_logits(logits, n_vocab, &s1);  /* logits[0] = 10 */
    cos_v101_set_default_aggregator(COS_V101_AGG_MEAN);
    cos_v101_sigma_from_logits(logits, n_vocab, &s2);
    check("v105: MEAN mode → sigma == sigma_arith_mean",
          s2.sigma == s2.sigma_arith_mean);
    cos_v101_set_default_aggregator(COS_V101_AGG_PRODUCT);
    cos_v101_sigma_from_logits(logits, n_vocab, &s2);
    check("v105: PRODUCT mode → sigma == sigma_product",
          s2.sigma == s2.sigma_product);
    check("v105: aggregator names are 'mean' and 'product'",
          strcmp(cos_v101_aggregator_name(COS_V101_AGG_MEAN),    "mean")    == 0 &&
          strcmp(cos_v101_aggregator_name(COS_V101_AGG_PRODUCT), "product") == 0);
    cos_v101_set_default_aggregator(prev);

    free(logits);
    printf("v101 σ-BitNet-Bridge: %d PASS / %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : -1;
}
