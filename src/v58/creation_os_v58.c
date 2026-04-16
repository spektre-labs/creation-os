/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v58 — σ-Cache (driver + self-test + microbench).
 *
 * Modes:
 *   --self-test     deterministic invariant tests (≥50 checks)
 *   --architecture  one-page wire diagram + tier table
 *   --positioning   honest comparison vs Q1–Q2 2026 KV-cache work
 *   --microbench    deterministic kernel timing on synthetic data
 *                   (no model, no network, no I/O beyond stdout)
 *
 * No mode opens sockets, allocates on a hot path inside the
 * decision kernel, or downloads a model.  See sigma_cache.h for
 * the v58 design rationale and explicit non-claims.
 */

#include "sigma_cache.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Forward declaration of the SoA NEON path in sigma_cache.c. */
void cos_v58_score_soa_neon(const float *epistemic,
                            const float *attention,
                            const float *recency_bonus,
                            int32_t      n,
                            float        alpha,
                            float        beta,
                            float        gamma,
                            float       *scores_out);

/* ------------------------------------------------------------------ */
/* Self-test harness                                                   */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(label, cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
        fprintf(stderr, "FAIL: %s (line %d)\n", (label), __LINE__); } \
} while (0)

/* aligned_alloc on macOS / glibc requires `size` to be an integer
 * multiple of `alignment`.  This helper rounds `bytes` up to the
 * next 64-byte boundary so test code can pass arbitrary sizes. */
static void *v58_alloc64(size_t bytes)
{
    if (bytes == 0) bytes = 64;
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    return aligned_alloc(64, aligned);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static uint32_t v58_lcg(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static float v58_rand01(uint32_t *state)
{
    return (float)(v58_lcg(state) & 0x00FFFFFFu) / (float)0x01000000u;
}

static void v58_make_synthetic(cos_v58_token_t *toks, int n,
                               int sink_count, uint32_t seed)
{
    for (int i = 0; i < n; ++i) {
        toks[i].seq_pos           = i;
        toks[i].epistemic_contrib = v58_rand01(&seed) * 1.0f;
        toks[i].aleatoric_signal  = v58_rand01(&seed) * 0.5f;
        toks[i].attention_mass    = v58_rand01(&seed) * 1.0f;
        toks[i].is_sink           = (uint8_t)(i < sink_count);
        toks[i]._reserved[0] = 0;
        toks[i]._reserved[1] = 0;
        toks[i]._reserved[2] = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Tests — defaults and version                                        */
/* ------------------------------------------------------------------ */

static void test_version_nonzero(void)
{
    cos_v58_version_t v = cos_v58_version();
    TEST_OK("version_major_is_58",  v.major == 58);
    TEST_OK("version_minor_nonneg", v.minor >= 0);
    TEST_OK("version_patch_nonneg", v.patch >= 0);
}

static void test_default_policy(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    TEST_OK("default_capacity_pos",        p.capacity > 0);
    TEST_OK("default_sink_nonneg",         p.sink_count >= 0);
    TEST_OK("default_window_pos",          p.recency_window > 0);
    TEST_OK("default_alpha_pos",           p.alpha_epistemic > 0.0f);
    TEST_OK("default_beta_pos",            p.beta_attention  > 0.0f);
    TEST_OK("default_gamma_pos",           p.gamma_recency   > 0.0f);
    TEST_OK("default_delta_nonneg",        p.delta_aleatoric >= 0.0f);
    TEST_OK("default_tau_full_gt_int8",    p.tau_full > p.tau_int8);
}

static void test_default_policy_idempotent(void)
{
    cos_v58_policy_t a, b;
    cos_v58_policy_default(&a);
    cos_v58_policy_default(&b);
    TEST_OK("default_policy_deterministic",
            memcmp(&a, &b, sizeof(a)) == 0);
}

/* ------------------------------------------------------------------ */
/* Tests — scoring                                                     */
/* ------------------------------------------------------------------ */

static void test_score_null_safe(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t  t = {0};

    TEST_OK("score_null_token",  cos_v58_score_token(NULL, 0, &p)     == 0.0f);
    TEST_OK("score_null_policy", cos_v58_score_token(&t,   0, NULL)   == 0.0f);
}

static void test_score_monotone_epistemic(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t a = {0}, b = {0};
    a.seq_pos = b.seq_pos = 100;
    a.epistemic_contrib = 0.10f;
    b.epistemic_contrib = 0.50f;
    float sa = cos_v58_score_token(&a, 100, &p);
    float sb = cos_v58_score_token(&b, 100, &p);
    TEST_OK("score_monotone_epistemic", sb > sa);
}

static void test_score_monotone_attention(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t a = {0}, b = {0};
    a.seq_pos = b.seq_pos = 100;
    a.attention_mass = 0.10f;
    b.attention_mass = 0.50f;
    float sa = cos_v58_score_token(&a, 100, &p);
    float sb = cos_v58_score_token(&b, 100, &p);
    TEST_OK("score_monotone_attention", sb > sa);
}

static void test_score_aleatoric_penalises(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t a = {0}, b = {0};
    a.seq_pos = b.seq_pos = 100;
    a.aleatoric_signal = 0.10f;
    b.aleatoric_signal = 0.50f;
    float sa = cos_v58_score_token(&a, 100, &p);
    float sb = cos_v58_score_token(&b, 100, &p);
    TEST_OK("score_penalises_aleatoric", sa > sb);
}

static void test_score_recency(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t recent = {0}, old = {0};
    recent.seq_pos = 100;
    old.seq_pos    = 0;       /* older than the recency window */
    recent.epistemic_contrib = old.epistemic_contrib = 0.20f;
    float sr = cos_v58_score_token(&recent, 100, &p);
    float so = cos_v58_score_token(&old,    100, &p);
    TEST_OK("score_recency_helps_recent", sr > so);
}

static void test_score_sink_lift(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t s = {0};
    s.is_sink = 1;
    s.seq_pos = 0;
    float v = cos_v58_score_token(&s, 1000, &p);
    TEST_OK("score_sink_above_tau_full", v > p.tau_full);
}

static void test_score_batch_matches_per_token(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);

    int n = 64;
    cos_v58_token_t *tok = cos_v58_alloc_tokens(n);
    float *batch  = (float *)v58_alloc64((size_t)n * sizeof(float));
    v58_make_synthetic(tok, n, 4, 12345u);

    cos_v58_score_batch(tok, n, n - 1, &p, batch);
    int matches = 1;
    for (int i = 0; i < n; ++i) {
        float per = cos_v58_score_token(&tok[i], n - 1, &p);
        if (fabsf(per - batch[i]) > 1.0e-6f) { matches = 0; break; }
    }
    TEST_OK("score_batch_matches_per_token", matches);

    free(tok);
    free(batch);
}

static void test_score_batch_n_zero(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    float scores[1] = { 99.0f };
    cos_v58_score_batch(NULL, 0, 0, &p, scores);
    TEST_OK("score_batch_zero_no_op", scores[0] == 99.0f);
}

static void test_score_soa_neon_matches_scalar(void)
{
    int n = 33;
    float *e  = (float *)v58_alloc64(64u * sizeof(float));
    float *a  = (float *)v58_alloc64(64u * sizeof(float));
    float *r  = (float *)v58_alloc64(64u * sizeof(float));
    float *s_neon = (float *)v58_alloc64(64u * sizeof(float));
    float *s_ref  = (float *)v58_alloc64(64u * sizeof(float));
    uint32_t seed = 7777u;
    for (int i = 0; i < n; ++i) {
        e[i] = v58_rand01(&seed);
        a[i] = v58_rand01(&seed);
        r[i] = (float)(i & 1);
    }
    float alpha = 1.0f, beta = 0.5f, gamma = 0.25f;
    cos_v58_score_soa_neon(e, a, r, n, alpha, beta, gamma, s_neon);
    for (int i = 0; i < n; ++i) {
        s_ref[i] = alpha * e[i] + beta * a[i] + gamma * r[i];
    }
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        if (fabsf(s_neon[i] - s_ref[i]) > 1.0e-5f) { ok = 0; break; }
    }
    TEST_OK("score_soa_neon_matches_scalar", ok);
    free(e); free(a); free(r); free(s_neon); free(s_ref);
}

/* ------------------------------------------------------------------ */
/* Tests — decisions                                                   */
/* ------------------------------------------------------------------ */

static void test_decide_null_safe(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_decision_summary_t sum;
    uint8_t out[1];
    TEST_OK("decide_null_tokens",  cos_v58_decide(NULL, 1, 0, &p, out,  &sum) == -1);
    TEST_OK("decide_null_policy",  cos_v58_decide((cos_v58_token_t *)out, 1, 0, NULL, out, &sum) == -1);
    TEST_OK("decide_null_decisions", cos_v58_decide((cos_v58_token_t *)out, 1, 0, &p, NULL, &sum) == -1);
    TEST_OK("decide_null_summary",   cos_v58_decide((cos_v58_token_t *)out, 1, 0, &p, out, NULL) == -1);
    TEST_OK("decide_negative_n",
            cos_v58_decide((cos_v58_token_t *)out, -1, 0, &p, out, &sum) == -1);
}

static void test_decide_n_zero(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_decision_summary_t sum;
    /* tokens NULL is allowed when n == 0 (no deref). */
    int kept = cos_v58_decide(NULL, 0, 0, &p, NULL, &sum);
    TEST_OK("decide_n_zero_kept_zero",      kept == 0);
    TEST_OK("decide_n_zero_summary_zero",   sum.kept_total == 0
                                         && sum.evicted    == 0);
}

static void test_decide_kept_le_capacity_plus_sinks(void)
{
    int n = 256;
    int sinks = 4;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity   = 64;
    p.sink_count = sinks;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, sinks, 0xC0FFEEu);

    cos_v58_decision_summary_t sum;
    int kept = cos_v58_decide(t, n, n - 1, &p, out, &sum);
    TEST_OK("decide_kept_le_capacity",
            kept <= p.capacity + sinks);
    TEST_OK("decide_kept_ge_sinks",
            kept >= sinks);
    TEST_OK("decide_summary_sinks_match",
            sum.sink_protected == sinks);

    free(t); free(out);
}

static void test_decide_preserves_all_sinks(void)
{
    int n = 128;
    int sinks = 8;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity   = 16;
    p.sink_count = sinks;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, sinks, 0xDEADBEEFu);

    /* Force ALL sinks to have very low scores so only the sink
     * mask preserves them — not their score. */
    for (int i = 0; i < sinks; ++i) {
        t[i].epistemic_contrib = 0.0f;
        t[i].attention_mass    = 0.0f;
        t[i].aleatoric_signal  = 0.49f;
    }

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);

    int all_kept_full = 1;
    for (int i = 0; i < sinks; ++i) {
        if (out[i] != COS_V58_KEEP_FULL) { all_kept_full = 0; break; }
    }
    TEST_OK("decide_sinks_all_kept_full", all_kept_full);

    free(t); free(out);
}

static void test_decide_deterministic(void)
{
    int n = 200;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 32;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out_a = (uint8_t *)v58_alloc64((size_t)n);
    uint8_t *out_b = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, 4, 0xABCD1234u);

    cos_v58_decision_summary_t sa, sb;
    cos_v58_decide(t, n, n - 1, &p, out_a, &sa);
    cos_v58_decide(t, n, n - 1, &p, out_b, &sb);
    TEST_OK("decide_deterministic_decisions", memcmp(out_a, out_b, (size_t)n) == 0);
    TEST_OK("decide_deterministic_summary",   memcmp(&sa, &sb, sizeof(sa)) == 0);

    free(t); free(out_a); free(out_b);
}

static void test_decide_sum_eq_n(void)
{
    int n = 333;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 100;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, 4, 1u);

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);
    TEST_OK("decide_summary_sums_to_n",
            sum.kept_full + sum.kept_int8 + sum.kept_int4 + sum.evicted == n);
    TEST_OK("decide_kept_total_consistent",
            sum.kept_total == sum.kept_full + sum.kept_int8 + sum.kept_int4);

    free(t); free(out);
}

static void test_decide_all_evicted_have_tag(void)
{
    int n = 64;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 0;
    p.sink_count = 0;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    /* All tokens with low epistemic, no sinks, capacity 0 → force EVICT. */
    for (int i = 0; i < n; ++i) {
        t[i].seq_pos = i;
        t[i].epistemic_contrib = 0.0f;
        t[i].attention_mass    = 0.0f;
        t[i].aleatoric_signal  = 1.0f;  /* heavy penalty */
        t[i].is_sink           = 0;
    }
    /* Push thresholds high so even tiny scores fail tau_int8/full. */
    p.tau_full = 0.99f;
    p.tau_int8 = 0.50f;

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);
    TEST_OK("decide_zero_capacity_no_keep",
            sum.kept_total == 0);
    TEST_OK("decide_zero_capacity_all_evict",
            sum.evicted == n);

    free(t); free(out);
}

static void test_decide_capacity_monotone(void)
{
    int n = 128;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.sink_count = 4;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, 4, 0x5EEDu);

    cos_v58_decision_summary_t s_low, s_hi;
    p.capacity = 16;
    cos_v58_decide(t, n, n - 1, &p, out, &s_low);
    p.capacity = 64;
    cos_v58_decide(t, n, n - 1, &p, out, &s_hi);

    TEST_OK("decide_more_capacity_no_more_evict",
            s_hi.evicted <= s_low.evicted);
    TEST_OK("decide_more_capacity_no_less_kept",
            s_hi.kept_total >= s_low.kept_total);

    free(t); free(out);
}

static void test_decide_threshold_in_summary(void)
{
    int n = 64;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 16;
    p.sink_count = 0;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, 0, 1234u);

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);

    /* Threshold should be finite and within plausible score range. */
    TEST_OK("decide_threshold_finite", isfinite(sum.keep_threshold));
    TEST_OK("decide_threshold_below_lift",
            sum.keep_threshold < 1.0e5f);

    free(t); free(out);
}

static void test_decide_all_sinks(void)
{
    int n = 16;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity   = 4;          /* less than sink count */
    p.sink_count = n;          /* every token is a sink */

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, n, 9u);

    cos_v58_decision_summary_t sum;
    int kept = cos_v58_decide(t, n, n - 1, &p, out, &sum);
    TEST_OK("decide_all_sinks_all_kept", kept == n);
    TEST_OK("decide_all_sinks_sum",      sum.sink_protected == n);

    free(t); free(out);
}

static void test_decide_full_count_consistent(void)
{
    /* The kept_full count must include all sinks (since sinks are
     * KEEP_FULL by mask) plus any non-sink token whose score reaches
     * tau_full. */
    int n = 128;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 64;
    p.sink_count = 8;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    v58_make_synthetic(t, n, 8, 0xAB12u);

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);
    TEST_OK("decide_kept_full_ge_sinks",
            sum.kept_full >= sum.sink_protected);

    free(t); free(out);
}

/* ------------------------------------------------------------------ */
/* Tests — compaction                                                  */
/* ------------------------------------------------------------------ */

static void test_compact_null_safe(void)
{
    int32_t out[1];
    uint8_t in[1] = {0};
    TEST_OK("compact_null_decisions", cos_v58_compact(NULL, 1, out) == -1);
    TEST_OK("compact_null_out",       cos_v58_compact(in,   1, NULL) == -1);
    TEST_OK("compact_negative_n",     cos_v58_compact(in,  -1, out) == -1);
}

static void test_compact_count_matches(void)
{
    int n = 128;
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 16;
    p.sink_count = 0;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
    int32_t *idx = (int32_t *)v58_alloc64((size_t)n * sizeof(int32_t));
    v58_make_synthetic(t, n, 0, 42u);

    cos_v58_decision_summary_t sum;
    cos_v58_decide(t, n, n - 1, &p, out, &sum);
    int compacted = cos_v58_compact(out, n, idx);

    TEST_OK("compact_count_matches_summary", compacted == sum.kept_total);

    int sorted = 1;
    for (int i = 1; i < compacted; ++i) {
        if (idx[i] <= idx[i - 1]) { sorted = 0; break; }
    }
    TEST_OK("compact_indices_sorted_asc", sorted);

    free(t); free(out); free(idx);
}

static void test_compact_all_evicted(void)
{
    uint8_t in[8];
    int32_t out[8];
    for (int i = 0; i < 8; ++i) in[i] = COS_V58_EVICTED;
    int compacted = cos_v58_compact(in, 8, out);
    TEST_OK("compact_all_evicted_zero", compacted == 0);
}

static void test_compact_all_kept(void)
{
    uint8_t in[8];
    int32_t out[8];
    for (int i = 0; i < 8; ++i) in[i] = COS_V58_KEEP_FULL;
    int compacted = cos_v58_compact(in, 8, out);
    TEST_OK("compact_all_kept_n", compacted == 8);
}

/* ------------------------------------------------------------------ */
/* Tests — tags & alloc                                                */
/* ------------------------------------------------------------------ */

static void test_decision_tags(void)
{
    TEST_OK("tag_full",   strcmp(cos_v58_decision_tag(COS_V58_KEEP_FULL), "FULL")  == 0);
    TEST_OK("tag_int8",   strcmp(cos_v58_decision_tag(COS_V58_KEEP_INT8), "INT8")  == 0);
    TEST_OK("tag_int4",   strcmp(cos_v58_decision_tag(COS_V58_KEEP_INT4), "INT4")  == 0);
    TEST_OK("tag_evict",  strcmp(cos_v58_decision_tag(COS_V58_EVICTED),   "EVICT") == 0);
    TEST_OK("tag_unknown", strcmp(cos_v58_decision_tag(99), "?") == 0);
}

static void test_alloc_tokens(void)
{
    cos_v58_token_t *t = cos_v58_alloc_tokens(0);
    TEST_OK("alloc_zero_null", t == NULL);

    t = cos_v58_alloc_tokens(-1);
    TEST_OK("alloc_negative_null", t == NULL);

    t = cos_v58_alloc_tokens(64);
    TEST_OK("alloc_64_nonnull", t != NULL);
    if (t) {
        /* All-zero memset on alloc. */
        TEST_OK("alloc_zeroed", t[0].seq_pos == 0
                              && t[0].epistemic_contrib == 0.0f
                              && t[0].is_sink == 0);
        TEST_OK("alloc_aligned_64",
                ((uintptr_t)t & 63u) == 0u);
    }
    free(t);
}

/* ------------------------------------------------------------------ */
/* Tests — invariants under stress                                     */
/* ------------------------------------------------------------------ */

static void test_decide_invariant_stress(void)
{
    /* Run the decision kernel across a range of seeds and assert the
     * universal invariants every time.  The aggregate boolean is one
     * test entry. */
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);

    int ok = 1;
    for (uint32_t seed = 1u; seed <= 30u; ++seed) {
        int n = 100 + (int)(seed * 7u) % 200;
        cos_v58_token_t *t = cos_v58_alloc_tokens(n);
        uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
        v58_make_synthetic(t, n, 4, seed);

        cos_v58_decision_summary_t sum;
        int kept = cos_v58_decide(t, n, n - 1, &p, out, &sum);

        if (kept < 0)                                 { ok = 0; }
        if (sum.kept_total != kept)                   { ok = 0; }
        if (sum.kept_full + sum.kept_int8
                + sum.kept_int4 + sum.evicted != n)   { ok = 0; }
        if (sum.sink_protected < 0)                   { ok = 0; }

        for (int i = 0; i < 4; ++i) {
            if (out[i] != COS_V58_KEEP_FULL)          { ok = 0; }
        }
        for (int i = 0; i < n; ++i) {
            if (out[i] > COS_V58_EVICTED)             { ok = 0; }
        }
        free(t); free(out);
        if (!ok) break;
    }
    TEST_OK("decide_invariant_stress_30_seeds", ok);
}

static void test_compact_after_decide_stress(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity = 32;

    int ok = 1;
    for (uint32_t seed = 100u; seed <= 120u; ++seed) {
        int n = 256;
        cos_v58_token_t *t = cos_v58_alloc_tokens(n);
        uint8_t *out = (uint8_t *)v58_alloc64((size_t)n);
        int32_t *idx = (int32_t *)v58_alloc64((size_t)n * sizeof(int32_t));
        v58_make_synthetic(t, n, 4, seed);

        cos_v58_decision_summary_t sum;
        cos_v58_decide(t, n, n - 1, &p, out, &sum);

        int kept = cos_v58_compact(out, n, idx);
        if (kept != sum.kept_total) ok = 0;

        for (int i = 0; i < kept; ++i) {
            if (out[idx[i]] == COS_V58_EVICTED) { ok = 0; break; }
        }
        free(t); free(out); free(idx);
        if (!ok) break;
    }
    TEST_OK("compact_after_decide_stress_21_seeds", ok);
}

/* ------------------------------------------------------------------ */
/* Tests — relative invariance                                         */
/* ------------------------------------------------------------------ */

static void test_score_translation_invariance(void)
{
    /* Adding a constant to every seq_pos and current_pos preserves the
     * relative ordering of scores under recency bonus. */
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t a = {0}, b = {0};
    a.seq_pos = 100; a.epistemic_contrib = 0.3f;
    b.seq_pos = 50;  b.epistemic_contrib = 0.3f;

    int rel1 = (cos_v58_score_token(&a, 110, &p)
              > cos_v58_score_token(&b, 110, &p));

    a.seq_pos += 1000; b.seq_pos += 1000;
    int rel2 = (cos_v58_score_token(&a, 1110, &p)
              > cos_v58_score_token(&b, 1110, &p));
    TEST_OK("score_recency_translation_invariant", rel1 == rel2);
}

static void test_score_attention_scale_preserves_order(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    cos_v58_token_t a = {0}, b = {0};
    a.seq_pos = b.seq_pos = 100;
    a.attention_mass = 0.10f; b.attention_mass = 0.40f;
    int r1 = (cos_v58_score_token(&a, 100, &p)
            < cos_v58_score_token(&b, 100, &p));
    a.attention_mass *= 5.0f; b.attention_mass *= 5.0f;
    int r2 = (cos_v58_score_token(&a, 100, &p)
            < cos_v58_score_token(&b, 100, &p));
    TEST_OK("score_attention_scale_preserves_order", r1 == r2);
}

static void test_score_branchless_matches_recency_window(void)
{
    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.recency_window = 10;

    cos_v58_token_t at_edge = {0}, just_outside = {0};
    at_edge.seq_pos      = 90;   /* age 10 — inside */
    just_outside.seq_pos = 89;   /* age 11 — outside */
    at_edge.epistemic_contrib = just_outside.epistemic_contrib = 0.20f;

    float s_in  = cos_v58_score_token(&at_edge,      100, &p);
    float s_out = cos_v58_score_token(&just_outside, 100, &p);
    TEST_OK("score_recency_edge_inclusive", s_in > s_out);
}

/* ------------------------------------------------------------------ */
/* Self-test entry                                                     */
/* ------------------------------------------------------------------ */

static int run_self_test(void)
{
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    test_version_nonzero();
    test_default_policy();
    test_default_policy_idempotent();

    test_score_null_safe();
    test_score_monotone_epistemic();
    test_score_monotone_attention();
    test_score_aleatoric_penalises();
    test_score_recency();
    test_score_sink_lift();
    test_score_batch_matches_per_token();
    test_score_batch_n_zero();
    test_score_soa_neon_matches_scalar();

    test_decide_null_safe();
    test_decide_n_zero();
    test_decide_kept_le_capacity_plus_sinks();
    test_decide_preserves_all_sinks();
    test_decide_deterministic();
    test_decide_sum_eq_n();
    test_decide_all_evicted_have_tag();
    test_decide_capacity_monotone();
    test_decide_threshold_in_summary();
    test_decide_all_sinks();
    test_decide_full_count_consistent();

    test_compact_null_safe();
    test_compact_count_matches();
    test_compact_all_evicted();
    test_compact_all_kept();

    test_decision_tags();
    test_alloc_tokens();

    test_decide_invariant_stress();
    test_compact_after_decide_stress();

    test_score_translation_invariance();
    test_score_attention_scale_preserves_order();
    test_score_branchless_matches_recency_window();

    fprintf(stderr,
            "v58 self-test: %d pass, %d fail\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Banners                                                             */
/* ------------------------------------------------------------------ */

static void print_architecture(void)
{
    puts(
"\n"
"Creation OS v58 — σ-Cache architecture\n"
"======================================\n"
"\n"
"  cached_token_t {seq_pos, ε_contrib, α_signal, attn_mass, sink}\n"
"        │\n"
"        ▼\n"
"  ┌────────────────────────────────────────────────────┐\n"
"  │ score_batch (NEON-friendly, scalar fallback)       │\n"
"  │   s = α·ε_contrib + β·attn − δ·α_signal + γ·rec    │\n"
"  │   sinks lifted by +1e6 (kept regardless of capacity)│\n"
"  └─────────────────────────┬──────────────────────────┘\n"
"                            ▼\n"
"  ┌────────────────────────────────────────────────────┐\n"
"  │ select_threshold (K-th-largest of non-sink scores) │\n"
"  │   K = capacity − sink_protected                    │\n"
"  └─────────────────────────┬──────────────────────────┘\n"
"                            ▼\n"
"  ┌────────────────────────────────────────────────────┐\n"
"  │ branchless decision kernel (no `if` on hot path)   │\n"
"  │   m_full = (s≥τ_full) | sink                       │\n"
"  │   m_int8 = (s≥τ_int8) & ~m_full                    │\n"
"  │   m_int4 = (s≥thresh) & ~(m_full|m_int8)           │\n"
"  │   m_evct = ~(m_full|m_int8|m_int4)                 │\n"
"  │   decision = sum of (TAG · mask)  (branchless mux) │\n"
"  └─────────────────────────┬──────────────────────────┘\n"
"                            ▼\n"
"  ┌────────────────────────────────────────────────────┐\n"
"  │ compact (branchless prefix-sum of kept indices)    │\n"
"  └────────────────────────────────────────────────────┘\n"
"\n"
"Hardware discipline (.cursorrules):\n"
"  - 64-byte aligned scratch via aligned_alloc                 (item 2)\n"
"  - __builtin_prefetch on the score scan                      (item 3)\n"
"  - branchless decision kernel                                (item 4)\n"
"  - explicit NEON 4-accumulator SoA scorer for microbench     (item 5)\n"
"  - no Accelerate / Metal / Core ML / SME enablement required\n"
"\n"
"Tier today: M (deterministic self-test + microbench).\n"
"Planned promotions are listed in docs/v58/THE_SIGMA_CACHE.md.\n"
);
}

static void print_positioning(void)
{
    puts(
"\n"
"Creation OS v58 — σ-Cache positioning\n"
"=====================================\n"
"\n"
"  Q1–Q2 2026 KV-cache eviction surveyed for v58 design:\n"
"\n"
"  StreamingLLM (2024) ........ keep [0, k_sink) + recent window\n"
"  H2O          (NeurIPS '23) . heavy-hitter attention mass\n"
"  SnapKV       (NeurIPS '24) . per-head attention concentration\n"
"  PyramidKV    (2024) ........ layer-wise budgets\n"
"  KIVI         (2024) ........ INT2 quantisation, no eviction policy\n"
"  SAGE-KV      (2503.08879) .. self-attention guided top-k eviction\n"
"  G-KV         (2512.00504) .. global + local attention scoring\n"
"  EntropyCache (2603.18489) .. decoded-token *entropy* as signal\n"
"  KV Policy    (2602.10238) .. RL agent ranks tokens by future utility\n"
"  Attention-Gate (2410.12876) parameterised per-token eviction flags\n"
"\n"
"  Eviction signal used by v58 σ-Cache:\n"
"\n"
"    score = α · ε_contrib(τ)                  ← v34 epistemic σ\n"
"          + β · attention_mass(τ)             ← Heavy-Hitter prior\n"
"          + γ · recency_bonus(τ, cursor)      ← StreamingLLM tail\n"
"          − δ · α_signal(τ)                   ← v34 aleatoric σ penalty\n"
"\n"
"  σ-Cache is the first published policy to drive KV eviction with the\n"
"  v34 epistemic / aleatoric *decomposition* (not single-channel\n"
"  entropy as in EntropyCache, not pure attention as in SnapKV/H2O,\n"
"  not a learned RL policy as in KVP).  Attention mass is retained as\n"
"  a Heavy-Hitter prior — σ-Cache subsumes attention-driven methods,\n"
"  it does not reject them.\n"
"\n"
"  σ-Cache does NOT claim:\n"
"    - perplexity recovery vs SnapKV / SAGE-KV / EntropyCache\n"
"      (would require an in-tree model harness; creation.md\n"
"       invariant: no weights, no network)\n"
"    - lower latency than attention-only eviction (the score is\n"
"      a strict superset of attention; one extra FMA per token)\n"
"    - that σ-decomposition is the only signal that matters\n"
"      (it is the missing one in 2026; not the entire space)\n"
);
}

/* ------------------------------------------------------------------ */
/* Microbench                                                          */
/* ------------------------------------------------------------------ */

static double v58_now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
}

static int run_microbench(int n_tokens, int n_iters)
{
    if (n_tokens <= 0) n_tokens = 8192;
    if (n_iters  <= 0) n_iters  = 100;

    cos_v58_policy_t p;
    cos_v58_policy_default(&p);
    p.capacity   = n_tokens / 8;
    p.sink_count = 4;

    cos_v58_token_t *t = cos_v58_alloc_tokens(n_tokens);
    uint8_t *out = (uint8_t *)v58_alloc64((size_t)n_tokens);
    if (!t || !out) {
        free(t); free(out);
        fprintf(stderr, "microbench: allocation failed\n");
        return 1;
    }
    v58_make_synthetic(t, n_tokens, 4, 0xBE57u);

    /* Warmup. */
    cos_v58_decision_summary_t sum;
    for (int i = 0; i < 5; ++i)
        cos_v58_decide(t, n_tokens, n_tokens - 1, &p, out, &sum);

    double t0 = v58_now_seconds();
    for (int i = 0; i < n_iters; ++i)
        cos_v58_decide(t, n_tokens, n_tokens - 1, &p, out, &sum);
    double t1 = v58_now_seconds();

    double seconds_per_iter = (t1 - t0) / (double)n_iters;
    double tokens_per_sec   = (double)n_tokens / seconds_per_iter;

    printf("v58 microbench (deterministic synthetic):\n");
    printf("  tokens / iter         : %d\n", n_tokens);
    printf("  iterations            : %d\n", n_iters);
    printf("  capacity              : %d\n", p.capacity);
    printf("  ms / iter (avg)       : %.4f\n", seconds_per_iter * 1000.0);
    printf("  tokens / s (avg)      : %.2e\n", tokens_per_sec);
    printf("  decisions / s (avg)   : %.2e\n", tokens_per_sec);
    printf("  last summary kept_total = %d (full=%d int8=%d int4=%d evict=%d sinks=%d)\n",
           sum.kept_total, sum.kept_full, sum.kept_int8,
           sum.kept_int4, sum.evicted, sum.sink_protected);
    printf("  keep_threshold        : %.6f\n", sum.keep_threshold);

    free(t); free(out);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    if (argc < 2) {
        puts("Creation OS v58 — σ-Cache (KV-cache eviction lab).\n"
             "Usage:\n"
             "  ./creation_os_v58 --self-test\n"
             "  ./creation_os_v58 --architecture\n"
             "  ./creation_os_v58 --positioning\n"
             "  ./creation_os_v58 --microbench [N_TOKENS] [N_ITERS]\n");
        return 0;
    }

    if (strcmp(argv[1], "--self-test")    == 0) return run_self_test();
    if (strcmp(argv[1], "--architecture") == 0) { print_architecture(); return 0; }
    if (strcmp(argv[1], "--positioning")  == 0) { print_positioning();  return 0; }
    if (strcmp(argv[1], "--microbench")   == 0) {
        int n = (argc > 2) ? atoi(argv[2]) : 8192;
        int k = (argc > 3) ? atoi(argv[3]) : 100;
        return run_microbench(n, k);
    }

    fprintf(stderr, "v58: unknown flag '%s'\n", argv[1]);
    return 64;
}
