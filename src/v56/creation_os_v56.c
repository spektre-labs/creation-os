/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v56 — σ-Constitutional scaffold.
 *
 * Wires four Q1-2026 insights into one deterministic, self-testable
 * C11 module set:
 *
 *   1. Rule-based process verifier (VPRM)
 *      "Beyond Outcome Verification: Verifiable Process Reward
 *      Models for Structured Reasoning", arXiv:2601.17223 (2026).
 *      Deterministic per-step rules → precision-first F1;
 *      sigma_verifier = 1 − precision.
 *
 *   2. σ-gated In-Place Test-Time Training budget controller
 *      "In-Place Test-Time Training", arXiv:2604.06169 (Apr 2026).
 *      Fast-weight updates allowed only when σ drift exceeds
 *      threshold within bounded buffer; σ-regression ⇒ rollback.
 *
 *   3. Commutator-defect σ-channel (grokking phase transition)
 *      "Grokking as a Phase Transition between Competing Basins",
 *      arXiv:2603.01192 (Mar 2026).
 *      Defect spikes 10–1000× above baseline 600–1600 steps
 *      before generalization.
 *
 *   4. Apple Neural Engine conv-as-matmul layout helper
 *      2026 reverse-engineering: MIL matmul silently does not
 *      execute on ANE; all matmuls must be rewritten as 1×1
 *      convolutions with spatial dims ≥16 ∧ multiple-of-16.
 *      Pure integer layout math; no Core ML dispatch.
 *
 * Hardware-first path (per repo convention):
 *   - NEON SIMD 4-accumulator reduction for the grokking defect
 *     kernel (||g_new − g_prev||² and ||g_new||² + ||g_prev||²).
 *   - Branchless policy arithmetic in ipttt / verifier / ane_layout.
 *   - Scalar fallback bit-identical on non-ARM.
 *   - No allocations; caller brings buffers.
 *
 * Honest scope (tier C / scaffold):
 *   - NO network, NO inference engine, NO tokenizer, NO Core ML.
 *   - "Steps", "gradients", and "probs" are caller-supplied arrays.
 *   - Self-test asserts the math on deterministic synthetic cases;
 *     it does not and cannot certify end-to-end TTT gains on a real
 *     transformer, nor ANE throughput on device.
 *
 * Invariant #3 from creation.md applies: this binary must not open
 * sockets or make syscalls beyond stdio.
 */
#include "verifier.h"
#include "ipttt.h"
#include "grokking.h"
#include "ane_layout.h"

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
    if (cond) { g_pass++; printf("[v56 self-test] OK  %s\n", (label)); } \
    else      { g_fail++; printf("[v56 self-test] FAIL %s\n", (label)); } \
} while (0)

static int approx(float a, float b, float tol) {
    float d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

/* ------------------------------------------------------------------ */
/* Verifier tests                                                     */
/* ------------------------------------------------------------------ */
static void test_verifier_nonempty(void)
{
    v56_step_t steps[3] = {
        { .text = "step one",        .score = 0.1f },
        { .text = "",                .score = 0.2f },
        { .text = "step three",      .score = 0.3f },
    };
    v56_rule_t rules[1] = { { .kind = V56_RULE_NONEMPTY } };
    v56_verify_result_t r;
    v56_verify(steps, 3, rules, 1, &r);
    TEST_OK(r.n_pass_pairs == 2,             "verifier nonempty: 2 pass");
    TEST_OK(r.n_fail_pairs == 1,             "verifier nonempty: 1 fail");
    TEST_OK(approx(r.precision, 2.0f/3.0f, 1e-4f),
                                             "verifier nonempty: precision = 2/3");
    TEST_OK(approx(r.sigma_verifier, 1.0f/3.0f, 1e-4f),
                                             "verifier nonempty: sigma = 1/3");
}

static void test_verifier_monotone(void)
{
    v56_step_t good[4] = {
        { .text = "a", .score = 0.1f },
        { .text = "b", .score = 0.3f },
        { .text = "c", .score = 0.3f },
        { .text = "d", .score = 0.7f },
    };
    v56_rule_t rules[1] = { { .kind = V56_RULE_SCORE_MONOTONE } };
    v56_verify_result_t r;
    v56_verify(good, 4, rules, 1, &r);
    TEST_OK(r.pass_overall == 1, "verifier monotone: all-increasing passes");

    v56_step_t bad[3] = {
        { .text = "a", .score = 0.5f },
        { .text = "b", .score = 0.3f },  /* regression */
        { .text = "c", .score = 0.4f },
    };
    v56_verify(bad, 3, rules, 1, &r);
    TEST_OK(r.pass_overall == 0, "verifier monotone: regression detected");
    TEST_OK(r.n_fail_pairs == 1, "verifier monotone: exactly one failure");
}

static void test_verifier_repeat_and_bounds(void)
{
    v56_step_t s[3] = {
        { .text = "alpha", .score = 0.4f },
        { .text = "beta",  .score = 0.5f },
        { .text = "alpha", .score = 0.6f },   /* repeat of s[0] */
    };
    v56_rule_t rules[2] = {
        { .kind = V56_RULE_NO_EXACT_REPEAT },
        { .kind = V56_RULE_SCORE_BOUNDED, .lo = 0.0f, .hi = 1.0f },
    };
    v56_verify_result_t r;
    v56_verify(s, 3, rules, 2, &r);
    TEST_OK(r.pass_overall == 0,                "verifier: repeat flagged");
    TEST_OK(r.n_fail_pairs == 1,                "verifier: exactly 1 pair failed");
    /* precision = 5/6 */
    TEST_OK(approx(r.precision, 5.0f/6.0f, 1e-4f),
                                                "verifier: precision = 5/6");
}

static void test_verifier_entropy_filter(void)
{
    /* Gibberish-ish (high char entropy) vs ordinary English */
    v56_step_t s[2] = {
        { .text = "the quick brown fox jumps over the lazy dog", .score = 0.1f },
        { .text = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"
                  "\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f "
                  "!@#$%^&*()_+=-[]{}|;:<>,.?/",
          .score = 0.2f },
    };
    v56_rule_t rule = { .kind = V56_RULE_CHAR_ENTROPY, .hi = 0.7f };
    v56_verify_result_t r;
    v56_verify(s, 2, &rule, 1, &r);
    TEST_OK(r.n_pass_pairs == 1 && r.n_fail_pairs == 1,
            "verifier entropy: English passes, random-bytes fails");
}

static void test_verifier_all_pass_sigma_zero(void)
{
    v56_step_t s[2] = {
        { .text = "hello world", .score = 0.2f },
        { .text = "goodbye world", .score = 0.4f },
    };
    v56_rule_t rules[3] = {
        { .kind = V56_RULE_NONEMPTY },
        { .kind = V56_RULE_MIN_LENGTH, .int_param = 3 },
        { .kind = V56_RULE_SCORE_MONOTONE },
    };
    v56_verify_result_t r;
    v56_verify(s, 2, rules, 3, &r);
    TEST_OK(r.pass_overall == 1,                 "verifier all-pass: OK");
    TEST_OK(approx(r.sigma_verifier, 0.0f, 1e-5f),
                                                 "verifier all-pass: sigma = 0");
    TEST_OK(approx(r.f1, 1.0f, 1e-5f),            "verifier all-pass: F1 = 1");
}

/* ------------------------------------------------------------------ */
/* IP-TTT tests                                                       */
/* ------------------------------------------------------------------ */
static void test_ipttt_cooldown_blocks_initial(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 4 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.01f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 10,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    v56_ipttt_observe_sigma(&s, 0.5f);   /* only 1 step; cooldown not met */
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 1024, &d);
    TEST_OK(d.allow == 0,                           "IP-TTT cooldown denies early update");
    TEST_OK(d.reason == V56_IPTTT_DENY_COOLDOWN,    "IP-TTT cooldown reason exact");
}

static void test_ipttt_no_drift_no_update(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 4 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.01f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 2,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    /* steady, low σ — no drift */
    for (int i = 0; i < 30; i++) v56_ipttt_observe_sigma(&s, 0.10f);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 1024, &d);
    TEST_OK(d.allow == 0,                           "IP-TTT steady: deny update");
    TEST_OK(d.reason == V56_IPTTT_DENY_NO_DRIFT,    "IP-TTT steady: no_drift reason");
}

static void test_ipttt_allows_on_drift(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 4 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.01f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 2,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    /* long calm baseline, then a spike in recent σ */
    for (int i = 0; i < 500; i++) v56_ipttt_observe_sigma(&s, 0.10f);
    for (int i = 0; i < 8;   i++) v56_ipttt_observe_sigma(&s, 0.45f);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 1024, &d);
    TEST_OK(d.allow == 1,                "IP-TTT: drift spike triggers allow");
    TEST_OK(d.observed_drift > 0.05f,    "IP-TTT: drift > threshold");
    TEST_OK(d.bytes_reserved == 1024,    "IP-TTT: bytes reserved match request");
}

static void test_ipttt_budget_cap(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 4096,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.01f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 1,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    for (int i = 0; i < 200; i++) v56_ipttt_observe_sigma(&s, 0.10f);
    for (int i = 0; i < 5;   i++) v56_ipttt_observe_sigma(&s, 0.50f);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 8192, &d);   /* exceeds budget */
    TEST_OK(d.allow == 0,                            "IP-TTT budget: deny over-budget");
    TEST_OK(d.reason == V56_IPTTT_DENY_BUDGET,       "IP-TTT budget: reason exact");
}

static void test_ipttt_rollback_on_regression(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 1 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.02f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 1,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    for (int i = 0; i < 200; i++) v56_ipttt_observe_sigma(&s, 0.10f);
    for (int i = 0; i < 5;   i++) v56_ipttt_observe_sigma(&s, 0.45f);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 2048, &d);
    float alpha_before = s.alpha_current;
    /* Caller reports post-update σ that is WORSE than baseline → rollback */
    v56_ipttt_commit_or_rollback(&p, &s, 2048, 0.90f);
    TEST_OK(s.updates_rolled_back == 1,         "IP-TTT rollback: counter incremented");
    TEST_OK(s.alpha_current < alpha_before,     "IP-TTT rollback: alpha shrunk");
    TEST_OK(s.bytes_used == 0,                  "IP-TTT rollback: bytes not committed");
    (void)d;
}

static void test_ipttt_commit_on_improvement(void)
{
    v56_ipttt_params_t p = {
        .update_budget_bytes = 1 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.02f,
        .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4,
        .min_steps_between = 1,
    };
    v56_ipttt_state_t s;
    v56_ipttt_init(&p, &s);
    for (int i = 0; i < 200; i++) v56_ipttt_observe_sigma(&s, 0.10f);
    for (int i = 0; i < 5;   i++) v56_ipttt_observe_sigma(&s, 0.45f);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &s, 2048, &d);
    /* Post-update σ drops back toward baseline → commit */
    v56_ipttt_commit_or_rollback(&p, &s, 2048, 0.10f);
    TEST_OK(s.updates_applied == 1,             "IP-TTT commit: applied counter");
    TEST_OK(s.bytes_used == 2048,               "IP-TTT commit: bytes tracked");
    TEST_OK(s.updates_rolled_back == 0,         "IP-TTT commit: no rollback");
}

/* ------------------------------------------------------------------ */
/* Grokking tests                                                     */
/* ------------------------------------------------------------------ */
static void test_grok_defect_identical(void)
{
    float a[16], b[16];
    for (int i = 0; i < 16; i++) { a[i] = (float)(i + 1) * 0.1f; b[i] = a[i]; }
    float d = v56_grok_defect(a, b, 16);
    TEST_OK(approx(d, 0.0f, 1e-5f), "grok defect: identical vectors ⇒ 0");
}

static void test_grok_defect_opposite(void)
{
    float a[16], b[16];
    for (int i = 0; i < 16; i++) { a[i] = 1.0f; b[i] = -1.0f; }
    float d = v56_grok_defect(a, b, 16);
    /* ||a-b||² = 4·n, ||a||²+||b||² = 2·n → ratio = 2, clamped to 1.0 */
    TEST_OK(approx(d, 1.0f, 1e-5f), "grok defect: opposite vectors ⇒ 1 (clamped)");
}

static void test_grok_defect_orthogonal(void)
{
    /* a = e1, b = e2 → ||a-b||² = 2, ||a||²+||b||² = 2, ratio = 1.0 */
    float a[16] = {0}, b[16] = {0};
    a[0] = 1.0f; b[1] = 1.0f;
    float d = v56_grok_defect(a, b, 16);
    TEST_OK(approx(d, 1.0f, 1e-5f), "grok defect: orthogonal unit vectors ⇒ 1");
}

static void test_grok_neon_matches_scalar(void)
{
    /* Build a vector with dim = 19 (not a multiple of 4) to hit
     * both the vectorized loop and the tail scalar fixup. */
    float a[19], b[19];
    for (int i = 0; i < 19; i++) {
        a[i] = 0.05f * (float)(i + 1);
        b[i] = 0.05f * (float)(i + 2) + (i % 3) * 0.01f;
    }
    float d = v56_grok_defect(a, b, 19);
    /* Compute scalar reference by hand */
    float dd = 0.0f, ss = 0.0f;
    for (int i = 0; i < 19; i++) {
        float df = a[i] - b[i];
        dd += df * df;
        ss += a[i] * a[i] + b[i] * b[i];
    }
    float ref = dd / (ss + 1e-12f);
    TEST_OK(approx(d, ref, 1e-5f), "grok defect: NEON tail-fixup matches scalar (dim=19)");
}

static void test_grok_phase_transition_detection(void)
{
    v56_grok_params_t p = { .spike_multiplier = 10.0f,
                            .baseline_warmup  = 20,
                            .baseline_ewma    = 0.98f };
    v56_grok_state_t  s;
    v56_grok_init(&p, &s);

    float a[16], b[16];
    for (int i = 0; i < 16; i++) { a[i] = 1.0f; b[i] = 1.0f; }
    /* Warm baseline near 0 by feeding tiny perturbations. */
    for (int step = 0; step < 50; step++) {
        for (int i = 0; i < 16; i++) b[i] = 1.0f + 1e-4f * (float)(step + i);
        v56_grok_observe(&p, &s, a, b, 16);
    }
    float base_before = s.baseline_defect;
    /* Now inject a big direction change: anti-parallel vectors. */
    for (int i = 0; i < 16; i++) b[i] = -1.0f;
    v56_grok_observe(&p, &s, a, b, 16);
    TEST_OK(s.phase_transition_detected == 1,
            "grok: anti-parallel spike triggers phase_transition");
    TEST_OK(s.latest_defect > base_before * 10.0f,
            "grok: spike magnitude > 10× baseline");
}

/* ------------------------------------------------------------------ */
/* ANE layout tests                                                   */
/* ------------------------------------------------------------------ */
static void test_ane_round_up(void)
{
    TEST_OK(v56_ane_round_up_spatial(1)  == 16,  "ANE round: 1 → 16");
    TEST_OK(v56_ane_round_up_spatial(15) == 16,  "ANE round: 15 → 16");
    TEST_OK(v56_ane_round_up_spatial(16) == 16,  "ANE round: 16 → 16 (idempotent)");
    TEST_OK(v56_ane_round_up_spatial(17) == 32,  "ANE round: 17 → 32");
    TEST_OK(v56_ane_round_up_spatial(48) == 48,  "ANE round: 48 → 48");
}

static void test_ane_layout_small_matmul_fails(void)
{
    v56_gemm_t g = { .M = 8, .K = 8, .N = 8 };
    v56_ane_layout_t L;
    v56_ane_layout_from_gemm(&g, &L);
    TEST_OK(L.ane_compatible == 0, "ANE small matmul (8x8x8): NOT compatible");
    TEST_OK(L.spatial_h == 1 && L.spatial_w == 8, "ANE layout: spatial = (1, N)");
    TEST_OK(L.pad_h == 15 && L.pad_w == 8,         "ANE pad plan: (H+=15, W+=8)");
}

static void test_ane_layout_aligned_matmul_passes_width(void)
{
    /* Width aligned, but H=1 still fails (spatial_h < 16). */
    v56_gemm_t g = { .M = 64, .K = 128, .N = 64 };
    v56_ane_layout_t L;
    v56_ane_layout_from_gemm(&g, &L);
    TEST_OK(L.spatial_w == 64,                     "ANE: W = N = 64 aligned");
    TEST_OK(L.ane_compatible == 0,                 "ANE: still fails on H=1");
    TEST_OK(L.pad_h == 15,                         "ANE: needs 15 pad on H");
}

static void test_ane_layout_shapes_nchw(void)
{
    v56_gemm_t g = { .M = 64, .K = 128, .N = 64 };
    v56_ane_layout_t L;
    v56_ane_layout_from_gemm(&g, &L);
    TEST_OK(L.input_shape[0]  == 1   && L.input_shape[1]  == 128 &&
            L.input_shape[2]  == 1   && L.input_shape[3]  == 64,
            "ANE: input_shape = [1, K, 1, N]");
    TEST_OK(L.weight_shape[0] == 64  && L.weight_shape[1] == 128 &&
            L.weight_shape[2] == 1   && L.weight_shape[3] == 1,
            "ANE: weight_shape = [M, K, 1, 1]");
}

static void test_ane_layout_reason_string(void)
{
    v56_gemm_t g = { .M = 64, .K = 128, .N = 8 };
    v56_ane_layout_t L;
    v56_ane_layout_from_gemm(&g, &L);
    TEST_OK(L.reason != NULL,                       "ANE: reason string present");
    TEST_OK(strstr(L.reason, "spatial") != NULL,    "ANE: reason mentions spatial");
}

/* ------------------------------------------------------------------ */
/* Integration test — the σ-Constitutional loop                        */
/* ------------------------------------------------------------------ */
static void test_integration_full_loop(void)
{
    /* Scenario: a proposer emits a 4-step chain with rising scores;
     * verifier runs; if verifier passes AND σ is low, no TTT update
     * is needed; if σ spikes (verifier.sigma_verifier high), IP-TTT
     * decides; grokking observer feeds defect into the side channel. */

    v56_step_t steps[4] = {
        { .text = "reformulate the goal", .score = 0.10f },
        { .text = "list candidate methods", .score = 0.25f },
        { .text = "select best method by bound", .score = 0.40f },
        { .text = "execute selected method", .score = 0.55f },
    };
    v56_rule_t rules[3] = {
        { .kind = V56_RULE_NONEMPTY },
        { .kind = V56_RULE_NO_EXACT_REPEAT },
        { .kind = V56_RULE_SCORE_MONOTONE },
    };
    v56_verify_result_t vr;
    v56_verify(steps, 4, rules, 3, &vr);
    TEST_OK(vr.pass_overall == 1,       "integration: verifier passes clean chain");
    TEST_OK(vr.sigma_verifier < 0.05f,  "integration: sigma_verifier low on pass");

    /* IP-TTT: feed σ_verifier into the budget controller.  Low σ → no update. */
    v56_ipttt_params_t p = {
        .update_budget_bytes = 1 << 20,
        .sigma_drift_threshold = 0.05f,
        .alpha_base = 0.01f, .rollback_shrink = 0.5f,
        .max_fast_weight_layers = 4, .min_steps_between = 3,
    };
    v56_ipttt_state_t st;
    v56_ipttt_init(&p, &st);
    for (int i = 0; i < 50; i++) v56_ipttt_observe_sigma(&st, vr.sigma_verifier);
    v56_ipttt_decision_t d;
    v56_ipttt_decide(&p, &st, 4096, &d);
    TEST_OK(d.allow == 0, "integration: clean chain ⇒ IP-TTT denies update (no drift)");

    /* Grokking observer: feed two aligned gradient-proxy vectors (learning stable). */
    v56_grok_params_t gp = { .spike_multiplier = 10.0f,
                             .baseline_warmup  = 5,
                             .baseline_ewma    = 0.98f };
    v56_grok_state_t  gs;
    v56_grok_init(&gp, &gs);
    float ga[16], gb[16];
    for (int i = 0; i < 16; i++) { ga[i] = 1.0f; gb[i] = 1.0f + 1e-5f * (float)i; }
    for (int i = 0; i < 10; i++) v56_grok_observe(&gp, &gs, ga, gb, 16);
    TEST_OK(gs.phase_transition_detected == 0, "integration: stable trajectory ⇒ no phase transition");

    /* ANE layout: confirm we can compute a dispatch plan for the chain's
     * projection GEMM even though it's tiny. */
    v56_gemm_t geo = { .M = 32, .K = 64, .N = 16 };
    v56_ane_layout_t L;
    v56_ane_layout_from_gemm(&geo, &L);
    TEST_OK(L.weight_shape[0] == 32,   "integration: ANE weight shape[0] = M");
    TEST_OK(L.input_shape[3]  == 16,   "integration: ANE input  shape[3] = N");
}

/* ------------------------------------------------------------------ */
/* Architecture banner                                                */
/* ------------------------------------------------------------------ */
static void print_architecture(void)
{
    printf(
"+==================================================================+\n"
"| Creation OS v56 -- sigma-Constitutional (scaffold)               |\n"
"+==================================================================+\n"
"|  Four Q1-2026 insights, wired into C11 + NEON:                   |\n"
"|                                                                  |\n"
"|   Verifier   VPRM / ThinkPRM    arXiv:2601.17223    (2026)       |\n"
"|     Deterministic rule-based PRM; precision-first F1.            |\n"
"|     sigma_verifier = 1 - precision.                              |\n"
"|                                                                  |\n"
"|   IP-TTT     in-place TTT       arXiv:2604.06169    (Apr 2026)   |\n"
"|     sigma-gated fast-weight updates; bounded buffer;             |\n"
"|     sigma-regression => rollback.                                |\n"
"|                                                                  |\n"
"|   Grokking   SLT commutator     arXiv:2603.01192    (Mar 2026)   |\n"
"|     Commutator-defect sigma-channel; 10-1000x spike              |\n"
"|     precedes generalization by 600-1600 steps.                   |\n"
"|                                                                  |\n"
"|   ANE layout matmul->1x1 conv   2026 reverse-engineering          |\n"
"|     spatial dims >= 16 AND multiple-of-16 required;              |\n"
"|     pure integer layout math, no Core ML dispatch.               |\n"
"|                                                                  |\n"
"|  Hardware path:  NEON 4-accumulator defect reduction,            |\n"
"|                  branchless policy arithmetic,                   |\n"
"|                  scalar fallback bit-identical on non-ARM.       |\n"
"|                                                                  |\n"
"|  Scope (tier C / scaffold): no network, no engine, no Core ML;   |\n"
"|  deterministic math on caller-supplied arrays.                   |\n"
"|                                                                  |\n"
"|  One invariant.                  K_eff = (1 - sigma) * K          |\n"
"+==================================================================+\n");
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

    /* Verifier */
    test_verifier_nonempty();
    test_verifier_monotone();
    test_verifier_repeat_and_bounds();
    test_verifier_entropy_filter();
    test_verifier_all_pass_sigma_zero();

    /* IP-TTT */
    test_ipttt_cooldown_blocks_initial();
    test_ipttt_no_drift_no_update();
    test_ipttt_allows_on_drift();
    test_ipttt_budget_cap();
    test_ipttt_rollback_on_regression();
    test_ipttt_commit_on_improvement();

    /* Grokking */
    test_grok_defect_identical();
    test_grok_defect_opposite();
    test_grok_defect_orthogonal();
    test_grok_neon_matches_scalar();
    test_grok_phase_transition_detection();

    /* ANE layout */
    test_ane_round_up();
    test_ane_layout_small_matmul_fails();
    test_ane_layout_aligned_matmul_passes_width();
    test_ane_layout_shapes_nchw();
    test_ane_layout_reason_string();

    /* Integration */
    test_integration_full_loop();

    int total = g_pass + g_fail;
    printf("[v56 self-test] %d/%d %s (sigma-Constitutional scaffold; no network, no engine)\n",
           g_pass, total, g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
