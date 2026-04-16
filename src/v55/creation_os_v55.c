/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v55 — σ₃-Speculative scaffold.
 *
 * Wires three 2026 insights into one deterministic, self-testable C11
 * module set:
 *
 *   1. σ₃ three-component decomposition
 *      (Taparia et al., arXiv:2603.24967, March 2026).
 *      Deterministic proxies on a softmax output:
 *          σ_input      = top-K Gini dispersion
 *          σ_knowledge  = 1 − max(P)
 *          σ_decoding   = H / log2(N)
 *
 *   2. EARS adaptive rejection sampling for speculative decoding
 *      (Sun, Mao, Xu, Chen — arXiv:2512.13194, December 2025).
 *      Accept if r < min(1, P_t/P_d + α · σ_knowledge).
 *
 *   3. EASD entropy-aware quality gate on spec-decoding
 *      (Su, Zhang, He — arXiv:2512.23765, December 2025).
 *      Force target resample when both draft & target are uncertain
 *      AND their top-N candidates overlap.
 *
 * Hardware-first path (per repo convention):
 *   - NEON SIMD hot entropy loop (vld1q/vmlaq, 4 accumulators, prefetch).
 *   - Branchless fast log₂ approximation (IEEE-754 exponent + poly).
 *   - Scalar fallback bit-identical on non-ARM.
 *   - 64-byte aligned scratch; compile flag -march=native on aarch64
 *     gets NEON for free; x86 falls to scalar.
 *
 * Honest scope (tier C / scaffold):
 *   - NO network, NO inference engine, NO tokenizer, NO API keys.
 *   - "Logits" / "probs" are caller-supplied float arrays.
 *   - Self-test asserts the math on deterministic synthetic cases;
 *     it does not and cannot certify spec-decode throughput on a real
 *     draft/target pair.  That is a harness-tier exercise left to
 *     bitnet.cpp / a real draft model.
 *
 * Invariant #3 from creation.md applies: this binary must not open
 * sockets or make syscalls beyond stdio and aligned_alloc.
 */
#include "sigma3.h"
#include "ears.h"
#include "easd.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Tiny test harness                                                  */
/* ------------------------------------------------------------------ */
static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(cond, label) do { \
    if (cond) { g_pass++; printf("[v55 self-test] OK  %s\n", (label)); } \
    else      { g_fail++; printf("[v55 self-test] FAIL %s\n", (label)); } \
} while (0)

static int approx(float a, float b, float tol) {
    float d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

/* ------------------------------------------------------------------ */
/* σ₃ tests                                                           */
/* ------------------------------------------------------------------ */
static void test_softmax_sums_to_one(void)
{
    float logits[8] = {1.0f, 2.0f, 3.0f, 0.5f, -1.0f, 4.2f, 2.7f, 0.0f};
    v55_softmax_inplace(logits, 8);
    float s = 0.0f;
    for (int i = 0; i < 8; i++) s += logits[i];
    TEST_OK(approx(s, 1.0f, 1e-5f), "softmax sums to 1");
}

static void test_sigma3_uniform(void)
{
    int n = 16;
    float probs[16];
    for (int i = 0; i < n; i++) probs[i] = 1.0f / (float)n;
    v55_sigma3_t s;
    v55_sigma3_from_probs(probs, n, 8, &s);
    /* Uniform: σ_decoding ≈ 1, σ_knowledge = 1 - 1/n (high), σ_input ≈ 1. */
    TEST_OK(approx(s.sigma_decoding, 1.0f, 0.02f), "σ3 uniform: sigma_decoding ≈ 1");
    TEST_OK(s.sigma_knowledge > 0.9f,              "σ3 uniform: sigma_knowledge > 0.9");
    TEST_OK(approx(s.sigma_input, 1.0f, 0.05f),    "σ3 uniform: sigma_input ≈ 1");
}

static void test_sigma3_one_hot(void)
{
    int n = 16;
    float probs[16] = {0};
    probs[7] = 1.0f;
    v55_sigma3_t s;
    v55_sigma3_from_probs(probs, n, 8, &s);
    TEST_OK(s.sigma_decoding < 0.02f,   "σ3 one-hot: sigma_decoding ≈ 0");
    TEST_OK(s.sigma_knowledge < 0.02f,  "σ3 one-hot: sigma_knowledge ≈ 0");
    TEST_OK(s.sigma_input < 0.02f,      "σ3 one-hot: sigma_input ≈ 0");
    TEST_OK(s.sigma_total < 0.05f,      "σ3 one-hot: sigma_total ≈ 0");
}

static void test_sigma3_two_peak(void)
{
    int n = 16;
    float probs[16] = {0};
    probs[2] = 0.45f;
    probs[9] = 0.45f;
    float leftover = 0.10f / 14.0f;
    for (int i = 0; i < n; i++) {
        if (i != 2 && i != 9) probs[i] = leftover;
    }
    v55_sigma3_t s;
    v55_sigma3_from_probs(probs, n, 2, &s);
    /* two equal peaks ⇒ top-2 dispersion near maximum */
    TEST_OK(s.sigma_input > 0.95f,         "σ3 two-peak (K=2): sigma_input near max");
    /* knowledge = 1 - 0.45 = 0.55 */
    TEST_OK(approx(s.sigma_knowledge, 0.55f, 0.01f), "σ3 two-peak: sigma_knowledge ≈ 0.55");
    TEST_OK(s.sigma_decoding > 0.3f && s.sigma_decoding < 0.9f,
            "σ3 two-peak: sigma_decoding in (0.3, 0.9)");
}

static void test_sigma3_total_bounded(void)
{
    int n = 32;
    float probs[32];
    for (int i = 0; i < n; i++) probs[i] = 1.0f / (float)n;
    v55_sigma3_t s;
    v55_sigma3_from_probs(probs, n, 8, &s);
    TEST_OK(s.sigma_total >= 0.0f && s.sigma_total <= 1.0f,
            "σ3: total in [0,1]");
}

static void test_sigma3_from_logits(void)
{
    /* Peaked logits should yield low σ across the board. */
    int n = 8;
    float logits[8] = {-3.f, -2.f, 7.0f, -1.f, -2.f, -1.5f, -3.f, -2.f};
    float scratch[8];
    v55_sigma3_t s;
    v55_sigma3_from_logits(logits, n, 4, scratch, &s);
    TEST_OK(s.sigma_knowledge < 0.1f, "σ3 from_logits peaked: knowledge low");
    TEST_OK(s.sigma_decoding  < 0.2f, "σ3 from_logits peaked: decoding low");
}

/* ------------------------------------------------------------------ */
/* EARS tests                                                         */
/* ------------------------------------------------------------------ */
static void test_ears_alpha_zero_matches_vanilla(void)
{
    v55_ears_params_t p = { .alpha = 0.0f, .max_threshold = 1.0f };
    /* vanilla spec-decode: accept iff r < min(1, p_t/p_d) */
    float t1 = v55_ears_threshold(&p, 0.60f, 0.80f, 0.50f);  /* σ ignored */
    TEST_OK(approx(t1, 0.75f, 1e-5f), "EARS α=0: threshold = P_t/P_d");

    int accept = v55_ears_accept(&p, 0.60f, 0.80f, 0.70f, 0.50f);
    TEST_OK(accept == 1, "EARS α=0: accept when r(0.70) < P_t/P_d(0.75)");

    int reject = v55_ears_accept(&p, 0.60f, 0.80f, 0.80f, 0.50f);
    TEST_OK(reject == 0, "EARS α=0: reject when r(0.80) > P_t/P_d(0.75)");
}

static void test_ears_relaxes_under_knowledge_gap(void)
{
    v55_ears_params_t p = { .alpha = 0.50f, .max_threshold = 1.0f };
    /* Base ratio = 0.6; σ_knowledge = 0.4 ⇒ threshold = 0.6 + 0.2 = 0.8 */
    float t = v55_ears_threshold(&p, 0.30f, 0.50f, 0.40f);
    TEST_OK(approx(t, 0.80f, 1e-5f), "EARS α=0.5, σ=0.4: threshold = 0.80");

    /* r = 0.72 → vanilla would reject (> 0.6), EARS accepts (< 0.8). */
    int vanilla_t = v55_ears_accept(&(v55_ears_params_t){.alpha=0,.max_threshold=1},
                                    0.30f, 0.50f, 0.72f, 0.40f);
    int ears_t = v55_ears_accept(&p, 0.30f, 0.50f, 0.72f, 0.40f);
    TEST_OK(vanilla_t == 0 && ears_t == 1,
            "EARS rescues acceptance when target uncertain (random-rejection fix)");
}

static void test_ears_threshold_clamp(void)
{
    v55_ears_params_t p = { .alpha = 1.0f, .max_threshold = 1.0f };
    /* base = 0.9, σ = 0.9 ⇒ raw = 1.8, clamp to 1.0 */
    float t = v55_ears_threshold(&p, 0.90f, 1.00f, 0.90f);
    TEST_OK(approx(t, 1.0f, 1e-5f), "EARS: threshold clamped at max=1.0");
}

static void test_ears_batch_matches_scalar(void)
{
    v55_ears_params_t p = { .alpha = 0.3f, .max_threshold = 1.0f };
    float pt[4] = {0.3f, 0.6f, 0.2f, 0.9f};
    float pd[4] = {0.5f, 0.7f, 0.8f, 0.4f};
    float rnd[4] = {0.4f, 0.5f, 0.9f, 0.2f};
    float sk[4] = {0.2f, 0.1f, 0.7f, 0.05f};
    int mask[4] = {0};
    v55_ears_accept_batch(&p, pt, pd, rnd, sk, mask, 4);
    int all_match = 1;
    for (int i = 0; i < 4; i++) {
        int scalar = v55_ears_accept(&p, pt[i], pd[i], rnd[i], sk[i]);
        if (scalar != mask[i]) all_match = 0;
    }
    TEST_OK(all_match, "EARS batch matches scalar for n=4");
}

/* ------------------------------------------------------------------ */
/* EASD tests                                                         */
/* ------------------------------------------------------------------ */
static void set_peaked(float *p, int n, int idx, float peak)
{
    float leftover = (1.0f - peak) / (float)(n - 1);
    for (int i = 0; i < n; i++) p[i] = (i == idx) ? peak : leftover;
}

static void test_easd_confident_no_reject(void)
{
    int n = 16;
    float t[16], d[16];
    set_peaked(t, n, 3, 0.90f);
    set_peaked(d, n, 3, 0.88f);
    v55_easd_params_t p = { .top_n = 8, .h_thresh = 0.75f, .overlap_thresh = 0.5f };
    v55_easd_decision_t dec;
    v55_easd_decide(&p, t, d, n, &dec);
    TEST_OK(dec.reject == 0, "EASD confident agreement: no reject");
    TEST_OK(dec.overlap_frac > 0.5f, "EASD confident agreement: high overlap observed");
}

static void test_easd_both_uncertain_overlap_reject(void)
{
    int n = 16;
    float t[16], d[16];
    /* near-uniform with slight top-8 bias → both high-entropy,
     * top-8 indices coincide → reject */
    for (int i = 0; i < n; i++) {
        t[i] = (i < 8) ? 0.10f : 0.025f;
        d[i] = (i < 8) ? 0.11f : 0.01625f;
    }
    v55_easd_params_t p = { .top_n = 8, .h_thresh = 0.75f, .overlap_thresh = 0.5f };
    v55_easd_decision_t dec;
    v55_easd_decide(&p, t, d, n, &dec);
    TEST_OK(dec.h_target_norm > 0.90f && dec.h_draft_norm > 0.90f,
            "EASD both-uncertain: both H_norm > 0.9");
    TEST_OK(dec.overlap_frac == 1.0f, "EASD both-uncertain: top-8 overlap = 1.0");
    TEST_OK(dec.reject == 1, "EASD both-uncertain + overlap: REJECT");
}

static void test_easd_disagree_high_entropy_no_reject(void)
{
    int n = 16;
    float t[16], d[16];
    /* both high-entropy but TOP-K DISAGREE → models are informatively
     * uncertain; easd should NOT reject (overlap < threshold) */
    for (int i = 0; i < n; i++) {
        t[i] = (i < 8)  ? 0.10f : 0.025f;   /* top-8 = {0..7} */
        d[i] = (i >= 8) ? 0.10f : 0.025f;   /* top-8 = {8..15} */
    }
    v55_easd_params_t p = { .top_n = 8, .h_thresh = 0.75f, .overlap_thresh = 0.5f };
    v55_easd_decision_t dec;
    v55_easd_decide(&p, t, d, n, &dec);
    TEST_OK(dec.overlap_frac < 0.5f, "EASD disagree: low top-K overlap");
    TEST_OK(dec.reject == 0,          "EASD disagree: NO reject (uncertainty is informative)");
}

static void test_easd_asymmetric_no_reject(void)
{
    int n = 16;
    float t[16], d[16];
    set_peaked(t, n, 3, 0.90f);          /* target confident */
    for (int i = 0; i < n; i++) d[i] = 1.0f / (float)n;  /* draft uniform */
    v55_easd_params_t p = { .top_n = 8, .h_thresh = 0.75f, .overlap_thresh = 0.5f };
    v55_easd_decision_t dec;
    v55_easd_decide(&p, t, d, n, &dec);
    TEST_OK(dec.reject == 0,
            "EASD asymmetric (target sure, draft unsure): no reject (EARS regime)");
}

/* ------------------------------------------------------------------ */
/* Architecture banner                                                */
/* ------------------------------------------------------------------ */
static void print_architecture(void)
{
    printf(
"╔══════════════════════════════════════════════════════════════════╗\n"
"║ Creation OS v55 — σ₃-Speculative (scaffold)                     ║\n"
"╠══════════════════════════════════════════════════════════════════╣\n"
"║  Three 2026 insights, wired into C11 + NEON:                    ║\n"
"║                                                                  ║\n"
"║   σ₃ decomposition   Taparia et al. 2603.24967 (Mar 2026)       ║\n"
"║     σ_input      ← top-K Gini dispersion                        ║\n"
"║     σ_knowledge  ← 1 − max(P)                                   ║\n"
"║     σ_decoding   ← H / log2(N)                                  ║\n"
"║                                                                  ║\n"
"║   EARS accept     Sun  et al. 2512.13194 (Dec 2025)             ║\n"
"║     r < min(1, P_t/P_d + α · σ_knowledge)                       ║\n"
"║     Reported +18.12 %% throughput, −0.84 %% accuracy             ║\n"
"║                                                                  ║\n"
"║   EASD gate       Su   et al. 2512.23765 (Dec 2025)             ║\n"
"║     reject  ≡  H_t > τ AND H_d > τ AND overlap_K > ρ            ║\n"
"║     Reported +3.54 %% accuracy (reasoning)                       ║\n"
"║                                                                  ║\n"
"║  Hardware path:  NEON vld1q/vmlaq, 4 accumulators, __prefetch,  ║\n"
"║                  branchless fast log₂ (IEEE-754 + minimax poly),║\n"
"║                  scalar fallback bit-identical on non-ARM.      ║\n"
"║                                                                  ║\n"
"║  Scope (tier C / scaffold): no network, no engine; deterministic║\n"
"║  proxies on caller-supplied softmax output.                     ║\n"
"║                                                                  ║\n"
"║  One invariant.                            K_eff = (1 − σ) · K  ║\n"
"╚══════════════════════════════════════════════════════════════════╝\n");
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--architecture") == 0) {
        print_architecture();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--self-test") != 0) {
        fprintf(stderr, "usage: %s [--self-test | --architecture]\n", argv[0]);
        return 2;
    }

    test_softmax_sums_to_one();
    test_sigma3_uniform();
    test_sigma3_one_hot();
    test_sigma3_two_peak();
    test_sigma3_total_bounded();
    test_sigma3_from_logits();

    test_ears_alpha_zero_matches_vanilla();
    test_ears_relaxes_under_knowledge_gap();
    test_ears_threshold_clamp();
    test_ears_batch_matches_scalar();

    test_easd_confident_no_reject();
    test_easd_both_uncertain_overlap_reject();
    test_easd_disagree_high_entropy_no_reject();
    test_easd_asymmetric_no_reject();

    int total = g_pass + g_fail;
    printf("[v55 self-test] %d/%d %s (σ₃-speculative scaffold; no network, no engine)\n",
           g_pass, total, g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
