/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v59 — σ-Budget driver: deterministic ≥60-test self-test,
 * architecture / positioning banners, and a microbench harness for the
 * online decision kernel + NEON SoA scoring path.
 */

#include "sigma_budget.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(label, cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
        fprintf(stderr, "FAIL: %s (line %d)\n", (label), __LINE__); } \
} while (0)

/* aligned_alloc helper: rounds `bytes` up to a multiple of 64 so macOS
 * accepts it (strict alignment requirement).  The v58 driver learned
 * this the hard way; v59 inherits the lesson. */
static void *v59_alloc64(size_t bytes)
{
    if (bytes == 0) bytes = 64;
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    return aligned_alloc(64, aligned);
}

/* Deterministic LCG (Numerical Recipes constants). */
static uint32_t v59_lcg_next(uint32_t *state)
{
    *state = (*state) * 1664525u + 1013904223u;
    return *state;
}

static float v59_lcg_uniform(uint32_t *state)
{
    return (float)(v59_lcg_next(state) >> 8) * (1.0f / (float)(1u << 24));
}

static void v59_fill_synthetic(cos_v59_step_t *out, int32_t n,
                               uint32_t seed)
{
    uint32_t s = seed ? seed : 1u;
    for (int32_t i = 0; i < n; ++i) {
        out[i].step_idx          = i;
        out[i].epistemic         = 0.30f * v59_lcg_uniform(&s) + 0.10f;
        out[i].aleatoric         = 0.20f * v59_lcg_uniform(&s);
        out[i].answer_delta      = (v59_lcg_uniform(&s) < 0.20f) ? 1.0f : 0.0f;
        out[i].reflection_signal = (v59_lcg_uniform(&s) < 0.10f) ? 1.0f : 0.0f;
    }
}

/* ------------------------------------------------------------------ */
/* Section 1 — Version / defaults                                      */
/* ------------------------------------------------------------------ */

static void test_version(void)
{
    cos_v59_version_t v = cos_v59_version();
    TEST_OK("version_major_59", v.major == 59);
    TEST_OK("version_minor_0",  v.minor == 0);
    TEST_OK("version_patch_ge1", v.patch >= 1);
}

static void test_defaults(void)
{
    cos_v59_policy_t p;
    cos_v59_policy_default(&p);
    TEST_OK("def_cap_32",        p.budget_max_steps == 32);
    TEST_OK("def_min_2",         p.budget_min_steps == 2);
    TEST_OK("def_alpha_pos",     p.alpha_epistemic > 0.0f);
    TEST_OK("def_beta_pos",      p.beta_stability  > 0.0f);
    TEST_OK("def_delta_pos",     p.delta_aleatoric > 0.0f);
    TEST_OK("def_tau_exit_positive", p.tau_exit > 0.0f);
    TEST_OK("def_alpha_dom_in_0_1",
            p.alpha_dominance > 0.0f && p.alpha_dominance < 1.0f);
    TEST_OK("def_sigma_high_pos", p.sigma_high > 0.0f);
}

static void test_defaults_null_safe(void)
{
    cos_v59_policy_default(NULL);
    TEST_OK("def_null_safe", 1);
}

static void test_version_consistent_across_calls(void)
{
    cos_v59_version_t a = cos_v59_version();
    cos_v59_version_t b = cos_v59_version();
    TEST_OK("version_consistent", a.major == b.major
                               && a.minor == b.minor
                               && a.patch == b.patch);
}

/* ------------------------------------------------------------------ */
/* Section 2 — Scoring                                                 */
/* ------------------------------------------------------------------ */

static void test_score_null_safe(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    TEST_OK("score_null_step",   cos_v59_score_step(NULL, &p)  == 0.0f);
    TEST_OK("score_null_policy",
            cos_v59_score_step(&(cos_v59_step_t){0}, NULL) == 0.0f);
    cos_v59_score_batch(NULL, 4, &p, NULL);
    TEST_OK("score_batch_null_safe", 1);
}

static void test_score_monotone_epistemic(void)
{
    /* Readiness: higher ε → LOWER readiness (further from exit). */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t lo = {0, 0.10f, 0.05f, 0.0f, 0.0f, {0}};
    cos_v59_step_t hi = {0, 0.60f, 0.05f, 0.0f, 0.0f, {0}};
    TEST_OK("score_monotone_epistemic_antiready",
            cos_v59_score_step(&hi, &p) < cos_v59_score_step(&lo, &p));
}

static void test_score_monotone_aleatoric(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t lo = {0, 0.30f, 0.05f, 0.0f, 0.0f, {0}};
    cos_v59_step_t hi = {0, 0.30f, 0.60f, 0.0f, 0.0f, {0}};
    TEST_OK("score_monotone_aleatoric",
            cos_v59_score_step(&hi, &p) < cos_v59_score_step(&lo, &p));
}

static void test_score_stability_reward(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t stable   = {0, 0.2f, 0.1f, 0.0f, 0.0f, {0}};
    cos_v59_step_t unstable = {0, 0.2f, 0.1f, 1.0f, 0.0f, {0}};
    TEST_OK("score_stability_reward",
            cos_v59_score_step(&stable, &p)
            > cos_v59_score_step(&unstable, &p));
}

static void test_score_reflection_lifts(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t no  = {0, 0.2f, 0.1f, 0.0f, 0.0f, {0}};
    cos_v59_step_t yes = {0, 0.2f, 0.1f, 0.0f, 1.0f, {0}};
    TEST_OK("score_reflection_lifts",
            cos_v59_score_step(&yes, &p) > cos_v59_score_step(&no, &p));
}

static void test_score_batch_matches_scalar(void)
{
    int n = 64;
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t *st = cos_v59_alloc_steps(n);
    float *batch       = (float *)v59_alloc64((size_t)n * sizeof(float));
    v59_fill_synthetic(st, n, 0xCAFEBABEu);
    cos_v59_score_batch(st, n, &p, batch);

    int ok = 1;
    for (int i = 0; i < n; ++i) {
        float s = cos_v59_score_step(&st[i], &p);
        if (fabsf(s - batch[i]) > 1e-6f) { ok = 0; break; }
    }
    TEST_OK("score_batch_matches_scalar", ok);
    free(st); free(batch);
}

static void test_score_batch_n_zero(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    float out[4] = {7.7f, 7.7f, 7.7f, 7.7f};
    cos_v59_score_batch((cos_v59_step_t *)"", 0, &p, out);
    TEST_OK("score_batch_n_zero_no_write",
            out[0] == 7.7f && out[3] == 7.7f);
}

static void test_score_soa_neon_matches_scalar(void)
{
    int n = 64;
    float *e  = (float *)v59_alloc64((size_t)n * sizeof(float));
    float *a  = (float *)v59_alloc64((size_t)n * sizeof(float));
    float *st = (float *)v59_alloc64((size_t)n * sizeof(float));
    float *rf = (float *)v59_alloc64((size_t)n * sizeof(float));
    float *sn = (float *)v59_alloc64((size_t)n * sizeof(float));
    float *sr = (float *)v59_alloc64((size_t)n * sizeof(float));
    uint32_t s = 0x11235813u;
    for (int i = 0; i < n; ++i) {
        e[i]  = v59_lcg_uniform(&s);
        a[i]  = v59_lcg_uniform(&s);
        st[i] = v59_lcg_uniform(&s);
        rf[i] = (v59_lcg_uniform(&s) < 0.3f) ? 1.0f : 0.0f;
    }
    float A = 1.0f, B = 0.75f, C = 0.25f, D = 1.5f;
    cos_v59_score_soa_neon(e, a, st, rf, n, A, B, C, D, sn);
    for (int i = 0; i < n; ++i) {
        sr[i] = B * st[i] + C * rf[i] - A * e[i] - D * a[i];
    }
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        if (fabsf(sn[i] - sr[i]) > 1e-4f) { ok = 0; break; }
    }
    TEST_OK("score_soa_neon_matches_scalar", ok);
    free(e); free(a); free(st); free(rf); free(sn); free(sr);
}

static void test_score_translation_invariant_in_step_idx(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t a = {0,  0.30f, 0.10f, 0.0f, 0.0f, {0}};
    cos_v59_step_t b = {17, 0.30f, 0.10f, 0.0f, 0.0f, {0}};
    TEST_OK("score_translation_invariant_in_step_idx",
            cos_v59_score_step(&a, &p) == cos_v59_score_step(&b, &p));
}

/* ------------------------------------------------------------------ */
/* Section 3 — Online decision                                         */
/* ------------------------------------------------------------------ */

static void test_decide_null_safe(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    uint8_t d = 0;
    TEST_OK("decide_null_steps",    cos_v59_decide_online(NULL, 1, &p, &d) == -1);
    cos_v59_step_t s = {0};
    TEST_OK("decide_null_policy",   cos_v59_decide_online(&s, 1, NULL, &d) == -1);
    TEST_OK("decide_null_out",      cos_v59_decide_online(&s, 1, &p, NULL) == -1);
    TEST_OK("decide_n_zero_rejects", cos_v59_decide_online(&s, 0, &p, &d) == -1);
}

static void test_decide_continue_low_sigma_early_steps(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    /* One step, below min; cannot exit, cannot abstain, low σ, mid score
     * → CONTINUE. */
    cos_v59_step_t s = {0, 0.25f, 0.05f, 0.5f, 0.0f, {0}};
    uint8_t d = 0;
    TEST_OK("decide_rc",   cos_v59_decide_online(&s, 1, &p, &d) == 0);
    TEST_OK("decide_continue_early", d == COS_V59_CONTINUE);
}

static void test_decide_early_exit_on_cap(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    p.budget_max_steps = 3;
    cos_v59_step_t steps[3];
    v59_fill_synthetic(steps, 3, 42u);
    uint8_t d = 0;
    cos_v59_decide_online(steps, 3, &p, &d);
    TEST_OK("decide_exit_on_cap", d == COS_V59_EARLY_EXIT);
}

static void test_decide_expand_on_high_epistemic(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t steps[4] = {
        {0, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        {1, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        {2, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        /* ε+α = 1.5 ≥ 1.0 (high σ), α/(ε+α) = 0.1/1.5 ≈ 0.07 ≪ 0.65 → EXPAND */
        {3, 1.40f, 0.10f, 0.0f, 0.0f, {0}},
    };
    uint8_t d = 0;
    cos_v59_decide_online(steps, 4, &p, &d);
    TEST_OK("decide_expand_on_high_eps", d == COS_V59_EXPAND);
}

static void test_decide_abstain_on_high_aleatoric(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t steps[4] = {
        {0, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        {1, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        {2, 0.2f, 0.05f, 1.0f, 0.0f, {0}},
        /* σ = 1.5 (high), α/(ε+α) = 1.3/1.5 ≈ 0.87 ≥ 0.65 → ABSTAIN */
        {3, 0.20f, 1.30f, 0.0f, 0.0f, {0}},
    };
    uint8_t d = 0;
    cos_v59_decide_online(steps, 4, &p, &d);
    TEST_OK("decide_abstain_on_high_alpha", d == COS_V59_ABSTAIN);
}

static void test_decide_early_exit_on_ready(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    /* Readiness score: β·1 + γ·0 − α_ε·0.02 − δ·0.02 = 0.75 − 0.02 − 0.03
     * = 0.70 ≥ 0.50 (tau_exit), σ = 0.04 (low) → EARLY_EXIT */
    cos_v59_step_t steps[4] = {
        {0, 0.3f, 0.05f, 1.0f, 0.0f, {0}},
        {1, 0.3f, 0.05f, 1.0f, 0.0f, {0}},
        {2, 0.3f, 0.05f, 1.0f, 0.0f, {0}},
        {3, 0.02f, 0.02f, 0.0f, 0.0f, {0}},
    };
    uint8_t d = 0;
    cos_v59_decide_online(steps, 4, &p, &d);
    TEST_OK("decide_exit_on_ready", d == COS_V59_EARLY_EXIT);
}

static void test_decide_no_exit_below_min_steps(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    p.budget_min_steps = 5;
    /* Very ready (stable + low σ) but below min → CONTINUE. */
    cos_v59_step_t s = {0, 0.01f, 0.01f, 0.0f, 0.0f, {0}};
    uint8_t d = 0;
    cos_v59_decide_online(&s, 1, &p, &d);
    TEST_OK("decide_no_exit_below_min", d == COS_V59_CONTINUE);
}

static void test_decide_cap_beats_abstain(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    p.budget_max_steps = 3;
    cos_v59_step_t steps[3] = {
        {0, 0.1f, 0.05f, 1.0f, 0.0f, {0}},
        {1, 0.1f, 0.05f, 1.0f, 0.0f, {0}},
        /* Would otherwise ABSTAIN but cap fires first. */
        {2, 0.10f, 2.00f, 0.0f, 0.0f, {0}},
    };
    uint8_t d = 0;
    cos_v59_decide_online(steps, 3, &p, &d);
    TEST_OK("decide_cap_beats_abstain", d == COS_V59_EARLY_EXIT);
}

static void test_decide_produces_one_of_four_tags(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t *s = cos_v59_alloc_steps(40);
    v59_fill_synthetic(s, 40, 0xBEEFu);
    int all_in_set = 1;
    for (int i = 1; i <= 40; ++i) {
        uint8_t d = 0;
        cos_v59_decide_online(s, i, &p, &d);
        if (d != COS_V59_CONTINUE && d != COS_V59_EARLY_EXIT
         && d != COS_V59_EXPAND   && d != COS_V59_ABSTAIN) {
            all_in_set = 0; break;
        }
    }
    TEST_OK("decide_tag_in_valid_set", all_in_set);
    free(s);
}

static void test_decide_deterministic(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 25;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    v59_fill_synthetic(s, n, 0xDEC1DE5u);
    uint8_t a = 0, b = 0;
    cos_v59_decide_online(s, n, &p, &a);
    cos_v59_decide_online(s, n, &p, &b);
    TEST_OK("decide_deterministic", a == b);
    free(s);
}

static void test_decide_idempotent_on_appended_stable_history(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 10;
    cos_v59_step_t *s = cos_v59_alloc_steps(n + 5);
    v59_fill_synthetic(s, n, 0xA55A5A5Au);
    /* Replicate last step: since decision only looks at steps[n-1],
     * appending identical copies must not change the decision. */
    uint8_t d1 = 0, d2 = 0;
    cos_v59_decide_online(s, n, &p, &d1);
    for (int i = 0; i < 5; ++i) s[n + i] = s[n - 1];
    cos_v59_decide_online(s, n + 5, &p, &d2);
    TEST_OK("decide_idempotent_on_stable_append", d1 == d2);
    free(s);
}

static void test_decide_abstain_needs_above_min(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    p.budget_min_steps = 4;
    /* Single step with very high α/(ε+α); would ABSTAIN if allowed, but
     * we are below min → CONTINUE. */
    cos_v59_step_t s = {0, 0.05f, 2.00f, 0.0f, 0.0f, {0}};
    uint8_t d = 0;
    cos_v59_decide_online(&s, 1, &p, &d);
    TEST_OK("decide_abstain_needs_above_min", d == COS_V59_CONTINUE);
}

static void test_decide_expand_requires_high_sigma(void)
{
    /* ε dominates but σ below sigma_high → not EXPAND. */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t steps[3] = {
        {0, 0.2f, 0.0f, 1.0f, 0.0f, {0}},
        {1, 0.2f, 0.0f, 1.0f, 0.0f, {0}},
        {2, 0.2f, 0.0f, 1.0f, 0.0f, {0}},
    };
    p.sigma_high = 5.0f;  /* force "low σ" regime                     */
    uint8_t d = 0;
    cos_v59_decide_online(steps, 3, &p, &d);
    TEST_OK("decide_expand_not_fired_below_sigma_high",
            d != COS_V59_EXPAND);
}

static void test_decide_monotone_budget_min(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t s = {0, 0.05f, 0.05f, 1.0f, 0.0f, {0}};
    /* Same step; raise budget_min_steps; decision must stay CONTINUE. */
    int all_cont = 1;
    for (int m = 1; m <= 8; ++m) {
        p.budget_min_steps = m;
        uint8_t d = 0;
        cos_v59_decide_online(&s, 1, &p, &d);
        if (d != COS_V59_CONTINUE) { all_cont = 0; break; }
    }
    TEST_OK("decide_monotone_budget_min", all_cont);
}

static void test_decide_abstain_beats_expand_and_exit(void)
{
    /* ε+α = 1.8 ≥ 1.0 (high σ), α/(ε+α) = 1.2/1.8 ≈ 0.67 ≥ 0.65
     * → ABSTAIN wins (priority > EXPAND > EARLY_EXIT-on-ready). */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t steps[4] = {
        {0, 0.1f, 0.1f, 1.0f, 0.0f, {0}},
        {1, 0.1f, 0.1f, 1.0f, 0.0f, {0}},
        {2, 0.1f, 0.1f, 1.0f, 0.0f, {0}},
        {3, 0.60f, 1.20f, 0.0f, 0.0f, {0}},
    };
    uint8_t d = 0;
    cos_v59_decide_online(steps, 4, &p, &d);
    TEST_OK("decide_abstain_beats_other_signals", d == COS_V59_ABSTAIN);
}

/* ------------------------------------------------------------------ */
/* Section 4 — Trace summary                                           */
/* ------------------------------------------------------------------ */

static void test_summary_null_safe(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_trace_summary_t sum;
    TEST_OK("sum_null_policy",
            cos_v59_summarize_trace(NULL, 0, NULL, &sum) == -1);
    TEST_OK("sum_null_out",
            cos_v59_summarize_trace(NULL, 0, &p, NULL)   == -1);
    TEST_OK("sum_neg_n",
            cos_v59_summarize_trace(NULL,-1, &p, &sum)   == -1);
    TEST_OK("sum_null_steps_positive_n",
            cos_v59_summarize_trace(NULL, 3, &p, &sum)   == -1);
}

static void test_summary_n_zero(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_trace_summary_t sum;
    TEST_OK("sum_n_zero_ok",
            cos_v59_summarize_trace(NULL, 0, &p, &sum) == 0);
    TEST_OK("sum_n_zero_counts_zero",
            sum.n_steps == 0 && sum.continues == 0
         && sum.early_exits == 0 && sum.expansions == 0
         && sum.abstentions == 0);
}

static void test_summary_counts_add_up_to_n(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 40;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    v59_fill_synthetic(s, n, 0xABABABABu);
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(s, n, &p, &sum);
    TEST_OK("sum_counts_add_to_n",
            sum.continues + sum.early_exits
          + sum.expansions + sum.abstentions == n);
    free(s);
}

static void test_summary_totals_match(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 20;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    v59_fill_synthetic(s, n, 0x7F7F7F7Fu);
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(s, n, &p, &sum);
    float e_sum = 0, a_sum = 0;
    for (int i = 0; i < n; ++i) {
        e_sum += s[i].epistemic;
        a_sum += s[i].aleatoric;
    }
    TEST_OK("sum_totals_epistemic",
            fabsf(sum.total_epistemic - e_sum) < 1e-4f);
    TEST_OK("sum_totals_aleatoric",
            fabsf(sum.total_aleatoric - a_sum) < 1e-4f);
    free(s);
}

static void test_summary_final_decision_matches_online(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 15;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    v59_fill_synthetic(s, n, 0x01020304u);
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(s, n, &p, &sum);
    uint8_t d = 0;
    cos_v59_decide_online(s, n, &p, &d);
    TEST_OK("sum_final_decision_matches_online", sum.final_decision == d);
    free(s);
}

static void test_summary_deterministic(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 30;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    v59_fill_synthetic(s, n, 0xDEADBEEFu);
    cos_v59_trace_summary_t a, b;
    cos_v59_summarize_trace(s, n, &p, &a);
    cos_v59_summarize_trace(s, n, &p, &b);
    TEST_OK("sum_deterministic",
            memcmp(&a, &b, sizeof(a)) == 0);
    free(s);
}

static void test_summary_stress_random(void)
{
    int all_invariants_hold = 1;
    uint32_t s = 0x51515151u;
    for (int trial = 0; trial < 12; ++trial) {
        int n = 8 + (int)(v59_lcg_next(&s) % 100u);
        cos_v59_policy_t p; cos_v59_policy_default(&p);
        p.budget_max_steps = 5 + (int)(v59_lcg_next(&s) % 50u);
        p.budget_min_steps = 1 + (int)(v59_lcg_next(&s) % 4u);

        cos_v59_step_t *st = cos_v59_alloc_steps(n);
        v59_fill_synthetic(st, n, v59_lcg_next(&s));

        cos_v59_trace_summary_t sum;
        cos_v59_summarize_trace(st, n, &p, &sum);

        int sum_ok =
            sum.n_steps == n
         && sum.continues   + sum.early_exits
          + sum.expansions  + sum.abstentions == n
         && sum.total_epistemic  >= 0.0f
         && sum.total_aleatoric  >= 0.0f
         && (sum.final_decision == COS_V59_CONTINUE
          || sum.final_decision == COS_V59_EARLY_EXIT
          || sum.final_decision == COS_V59_EXPAND
          || sum.final_decision == COS_V59_ABSTAIN);
        if (!sum_ok) { all_invariants_hold = 0; free(st); break; }
        free(st);
    }
    TEST_OK("sum_stress_invariants_hold", all_invariants_hold);
}

/* ------------------------------------------------------------------ */
/* Section 5 — Decision tags / allocator                               */
/* ------------------------------------------------------------------ */

static void test_decision_tags(void)
{
    TEST_OK("tag_continue",
            strcmp(cos_v59_decision_tag(COS_V59_CONTINUE), "CONTINUE") == 0);
    TEST_OK("tag_early_exit",
            strcmp(cos_v59_decision_tag(COS_V59_EARLY_EXIT), "EARLY_EXIT") == 0);
    TEST_OK("tag_expand",
            strcmp(cos_v59_decision_tag(COS_V59_EXPAND), "EXPAND") == 0);
    TEST_OK("tag_abstain",
            strcmp(cos_v59_decision_tag(COS_V59_ABSTAIN), "ABSTAIN") == 0);
    TEST_OK("tag_unknown_is_q",
            strcmp(cos_v59_decision_tag(0xEE), "?") == 0);
}

static void test_alloc_steps(void)
{
    TEST_OK("alloc_zero_null", cos_v59_alloc_steps(0) == NULL);
    TEST_OK("alloc_neg_null",  cos_v59_alloc_steps(-5) == NULL);
    cos_v59_step_t *s = cos_v59_alloc_steps(17);
    TEST_OK("alloc_positive_nonnull", s != NULL);
    int zeroed = 1;
    for (int i = 0; i < 17; ++i) {
        if (s[i].epistemic != 0.0f || s[i].aleatoric != 0.0f
         || s[i].answer_delta != 0.0f || s[i].reflection_signal != 0.0f) {
            zeroed = 0; break;
        }
    }
    TEST_OK("alloc_zeroed", zeroed);
    int aligned = (((uintptr_t)s) & 63u) == 0;
    TEST_OK("alloc_64_aligned", aligned);
    free(s);
}

/* ------------------------------------------------------------------ */
/* Section 6 — End-to-end scenarios                                    */
/* ------------------------------------------------------------------ */

static void test_e2e_easy_problem_exits_early(void)
{
    /* Easy problem: ε shrinks fast, α tiny, answer stabilises at step 3. */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t st[8] = {
        {0, 0.45f, 0.02f, 1.0f, 0.0f, {0}},
        {1, 0.35f, 0.02f, 1.0f, 0.0f, {0}},
        {2, 0.20f, 0.02f, 0.0f, 0.0f, {0}},
        {3, 0.10f, 0.02f, 0.0f, 1.0f, {0}},
        {4, 0.05f, 0.02f, 0.0f, 0.0f, {0}},
        {5, 0.03f, 0.02f, 0.0f, 0.0f, {0}},
        {6, 0.02f, 0.01f, 0.0f, 0.0f, {0}},
        {7, 0.01f, 0.01f, 0.0f, 0.0f, {0}},
    };
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(st, 8, &p, &sum);
    TEST_OK("e2e_easy_has_early_exits", sum.early_exits >= 1);
}

static void test_e2e_ambiguous_problem_triggers_abstain(void)
{
    /* Ambiguous problem: α large, ε small. */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t st[6] = {
        {0, 0.10f, 0.60f, 1.0f, 0.0f, {0}},
        {1, 0.10f, 0.90f, 1.0f, 0.0f, {0}},
        {2, 0.10f, 1.10f, 0.0f, 0.0f, {0}},
        {3, 0.10f, 1.30f, 0.0f, 0.0f, {0}},
        {4, 0.10f, 1.50f, 0.0f, 0.0f, {0}},
        {5, 0.10f, 1.80f, 0.0f, 0.0f, {0}},
    };
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(st, 6, &p, &sum);
    TEST_OK("e2e_ambiguous_has_abstentions", sum.abstentions >= 1);
}

static void test_e2e_hard_but_tractable_triggers_expand(void)
{
    /* Hard: ε persistently large, α small.  Pulsar-style — EXPAND. */
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t st[8] = {
        {0, 1.20f, 0.05f, 0.0f, 0.0f, {0}},
        {1, 1.30f, 0.05f, 0.0f, 0.0f, {0}},
        {2, 1.40f, 0.05f, 0.0f, 0.0f, {0}},
        {3, 1.50f, 0.05f, 0.0f, 0.0f, {0}},
        {4, 1.60f, 0.05f, 0.0f, 0.0f, {0}},
        {5, 1.70f, 0.05f, 0.0f, 0.0f, {0}},
        {6, 1.80f, 0.05f, 0.0f, 0.0f, {0}},
        {7, 1.90f, 0.05f, 0.0f, 0.0f, {0}},
    };
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(st, 8, &p, &sum);
    TEST_OK("e2e_hard_tractable_expands", sum.expansions >= 1);
}

static void test_e2e_never_all_same_tag_under_mixed_input(void)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    int n = 80;
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    uint32_t rng = 0xFACEB00Cu;
    for (int i = 0; i < n; ++i) {
        s[i].step_idx          = i;
        s[i].epistemic         = 2.0f * v59_lcg_uniform(&rng);
        s[i].aleatoric         = 2.0f * v59_lcg_uniform(&rng);
        s[i].answer_delta      = (v59_lcg_uniform(&rng) < 0.5f) ? 1.0f : 0.0f;
        s[i].reflection_signal = (v59_lcg_uniform(&rng) < 0.2f) ? 1.0f : 0.0f;
    }
    cos_v59_trace_summary_t sum;
    cos_v59_summarize_trace(s, n, &p, &sum);
    int categories = (sum.continues  > 0)
                   + (sum.early_exits> 0)
                   + (sum.expansions > 0)
                   + (sum.abstentions> 0);
    TEST_OK("e2e_mixed_input_uses_multiple_tags", categories >= 2);
    free(s);
}

/* ------------------------------------------------------------------ */
/* Top-level                                                           */
/* ------------------------------------------------------------------ */

static int run_self_test(void)
{
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    test_version();
    test_defaults();
    test_defaults_null_safe();
    test_version_consistent_across_calls();

    test_score_null_safe();
    test_score_monotone_epistemic();
    test_score_monotone_aleatoric();
    test_score_stability_reward();
    test_score_reflection_lifts();
    test_score_batch_matches_scalar();
    test_score_batch_n_zero();
    test_score_soa_neon_matches_scalar();
    test_score_translation_invariant_in_step_idx();

    test_decide_null_safe();
    test_decide_continue_low_sigma_early_steps();
    test_decide_early_exit_on_cap();
    test_decide_expand_on_high_epistemic();
    test_decide_abstain_on_high_aleatoric();
    test_decide_early_exit_on_ready();
    test_decide_no_exit_below_min_steps();
    test_decide_cap_beats_abstain();
    test_decide_produces_one_of_four_tags();
    test_decide_deterministic();
    test_decide_idempotent_on_appended_stable_history();
    test_decide_abstain_needs_above_min();
    test_decide_expand_requires_high_sigma();
    test_decide_monotone_budget_min();
    test_decide_abstain_beats_expand_and_exit();

    test_summary_null_safe();
    test_summary_n_zero();
    test_summary_counts_add_up_to_n();
    test_summary_totals_match();
    test_summary_final_decision_matches_online();
    test_summary_deterministic();
    test_summary_stress_random();

    test_decision_tags();
    test_alloc_steps();

    test_e2e_easy_problem_exits_early();
    test_e2e_ambiguous_problem_triggers_abstain();
    test_e2e_hard_but_tractable_triggers_expand();
    test_e2e_never_all_same_tag_under_mixed_input();

    fprintf(stderr, "v59 self-test: %d pass, %d fail\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Architecture / positioning banners                                  */
/* ------------------------------------------------------------------ */

static void print_architecture(void)
{
    printf(
"\nCreation OS v59 — σ-Budget architecture\n"
"======================================\n\n"
"  reasoning_step_t {step_idx, ε, α, answer_delta, reflection}\n"
"        │\n"
"        ▼\n"
"  cos_v59_score_step        (scalar, branchless, 3-FMA)\n"
"  cos_v59_score_soa_neon    (NEON 4-accumulator SoA reduction)\n"
"        │\n"
"        ▼\n"
"  cos_v59_decide_online     (branchless 4-way mux)\n"
"      sink       → EARLY_EXIT   (at budget cap OR score-stable)\n"
"      σ high + α-dom → ABSTAIN (irreducible; don't burn compute)\n"
"      σ high + ε-dom → EXPAND  (reducible; branch / self-verify)\n"
"      else           → CONTINUE\n"
"        │\n"
"        ▼\n"
"  cos_v59_summarize_trace   (offline audit: histogram + totals)\n\n"
"  .cursorrules discipline:  aligned_alloc(64, …)  |  prefetch(16)\n"
"                           |  branchless kernel  |  NEON 4-accum\n"
"  dispatch:  P-cores (policy hot path)  |  no GPU  |  no Accelerate\n\n");
}

static void print_positioning(void)
{
    printf(
"\nCreation OS v59 — σ-Budget positioning (Q2 2026)\n"
"================================================\n\n"
"              signal used      per-step decision surface\n"
"  TAB          GRPO-learned    {stop, continue}           35-40%% savings\n"
"  CoDE-Stop    confidence-dyn  {stop, continue}           25-50%% savings\n"
"  LYNX         probe scalar    {exit, stay}               40-70%% savings\n"
"  DTSR         reflection cnt  {stop, continue}           28-35%% savings\n"
"  DiffAdapt    entropy scalar  {easy, normal, hard}       22%% savings\n"
"  Coda         rollout diff.   {compute gate}             60%% on easy\n"
"  AdaCtrl      RL policy       {length control}           62-91%% on easy\n"
"  RiskControl  dual-threshold  {stop-confident|unsolvable} 52%% savings\n"
"  ─────────────────────────────────────────────────────────────────────\n"
"  Creation OS   σ = (ε, α)     {CONTINUE, EARLY_EXIT,\n"
"  v59 σ-Budget   decomposed    EXPAND, ABSTAIN}           M-tier, 60/60\n\n"
"  v59 is the only policy that separates reducible (ε) from\n"
"  irreducible (α) uncertainty at decision time.  ABSTAIN is a\n"
"  decision no scalar-signal method can produce faithfully —\n"
"  extending compute on α-dominated problems wastes energy.\n\n");
}

/* ------------------------------------------------------------------ */
/* Microbench                                                          */
/* ------------------------------------------------------------------ */

static double v59_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void run_microbench_one(int n, int iters)
{
    cos_v59_policy_t p; cos_v59_policy_default(&p);
    cos_v59_step_t *s = cos_v59_alloc_steps(n);
    if (!s) { fprintf(stderr, "alloc failed\n"); return; }
    v59_fill_synthetic(s, n, 0x1234ABCDu);

    /* Warm. */
    uint8_t d = 0;
    for (int k = 0; k < 2; ++k) cos_v59_decide_online(s, n, &p, &d);

    double t0 = v59_now_ms();
    cos_v59_trace_summary_t last;
    for (int it = 0; it < iters; ++it) {
        cos_v59_summarize_trace(s, n, &p, &last);
    }
    double t1 = v59_now_ms();
    double ms = (t1 - t0) / (double)iters;
    double dps = ((double)n / (t1 - t0)) * (double)iters * 1000.0;

    printf("v59 microbench (deterministic synthetic):\n"
           "  steps / iter          : %d\n"
           "  iterations            : %d\n"
           "  ms / iter (avg)       : %.4f\n"
           "  decisions / s (avg)   : %.2e\n"
           "  last summary: cont=%d exit=%d expand=%d abstain=%d (final=%s)\n"
           "  ε_total = %.3f   α_total = %.3f   final_score = %.3f\n\n",
           n, iters, ms, dps,
           last.continues, last.early_exits,
           last.expansions, last.abstentions,
           cos_v59_decision_tag(last.final_decision),
           last.total_epistemic, last.total_aleatoric,
           last.final_step_score);

    free(s);
}

static int run_microbench(void)
{
    printf("----- N=64 iters=5000 -----\n");
    run_microbench_one(64, 5000);
    printf("----- N=512 iters=1000 -----\n");
    run_microbench_one(512, 1000);
    printf("----- N=4096 iters=200 -----\n");
    run_microbench_one(4096, 200);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--self-test")   == 0) return run_self_test();
        if (strcmp(argv[1], "--architecture")== 0) { print_architecture(); return 0; }
        if (strcmp(argv[1], "--positioning") == 0) { print_positioning(); return 0; }
        if (strcmp(argv[1], "--microbench")  == 0) return run_microbench();
    }
    printf("Creation OS v59 — σ-Budget (adaptive test-time compute lab).\n"
           "Usage:\n"
           "  ./creation_os_v59 --self-test\n"
           "  ./creation_os_v59 --architecture\n"
           "  ./creation_os_v59 --positioning\n"
           "  ./creation_os_v59 --microbench\n");
    return 0;
}
