/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v53 — σ-governed harness architecture (scaffold).
 *
 * Ships a self-test for four small surfaces:
 *   1. σ-TAOR loop: step transitions, σ-abstain, drift abstain,
 *      no-progress abstain, completion, budget exhaustion.
 *   2. σ-triggered sub-agent dispatch: main σ below trigger → no spawn;
 *      main σ above trigger → spawn w/ fresh-context σ; specialist cap.
 *   3. σ-prioritized compression: learning moments + invariant refs +
 *      tool-use score above routine filler; batch drop targets.
 *   4. creation.md loader: invariants count ≥ 5, σ-profile rows ≥ 3.
 *
 * Honest scope: no transformer, no real tool runtime, no real
 * sub-process dispatch. Every function is deterministic and runs in
 * pure C11. See docs/v53/ARCHITECTURE.md + docs/WHAT_IS_REAL.md.
 */
#include "harness/loop.h"
#include "harness/dispatch.h"
#include "harness/compress.h"
#include "harness/project_context.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int v53_fail(const char *m)
{
    fprintf(stderr, "[v53 self-test] FAIL %s\n", m);
    return 1;
}

static void v53_make_logits_peaked(float *logits, int n)
{
    for (int i = 0; i < n; i++) logits[i] = -4.0f;
    logits[0] = 8.0f;
}

static void v53_make_logits_uniform(float *logits, int n)
{
    for (int i = 0; i < n; i++) logits[i] = 0.001f * (float)(i % 3);
}

/* ─── σ-TAOR loop ─────────────────────────────────────────── */

static int test_loop_completes_on_goal(void)
{
    v53_harness_state_t h;
    v53_harness_init(&h, "easy goal", 10, 0.75f);
    float logits[32];
    v53_make_logits_peaked(logits, 32);
    /* Caller reports progress = 1.0 and no tool calls proposed → complete */
    v53_loop_outcome_t o = v53_taor_step(&h, logits, 32, 1.0f, 0);
    if (o != V53_LOOP_COMPLETE) return v53_fail("expected COMPLETE");
    fprintf(stderr, "[v53 self-test] OK loop COMPLETE on verified goal\n");
    return 0;
}

static int test_loop_abstain_sigma(void)
{
    v53_harness_state_t h;
    v53_harness_init(&h, "hard goal", 10, 0.50f); /* aggressive threshold */
    float logits[32];
    v53_make_logits_uniform(logits, 32); /* σ ≈ 1.0 */
    v53_loop_outcome_t o = v53_taor_step(&h, logits, 32, 0.1f, 1);
    if (o != V53_LOOP_ABSTAIN_SIGMA) {
        return v53_fail("expected ABSTAIN_SIGMA on uniform logits");
    }
    fprintf(stderr, "[v53 self-test] OK loop ABSTAIN_SIGMA (%s)\n",
            v53_outcome_str(o));
    return 0;
}

static int test_loop_abstain_drift(void)
{
    /* Drift test needs σ that is BELOW sigma_threshold but ABOVE drift_cap,
     * so the per-iter σ-gate passes but the EWMA slowly crosses the cap. */
    v53_harness_state_t h;
    v53_harness_init(&h, "drifting", 200, 0.99f);
    h.drift_cap = 0.30f;
    h.no_progress_cap = 10000; /* disable no-progress abstain for this test */
    float logits[32];
    for (int i = 0; i < 32; i++) logits[i] = 0.0f;
    logits[0] = 4.0f; /* yields normalized entropy ≈ 0.56 (below 0.99, above 0.30) */
    v53_loop_outcome_t last = V53_LOOP_CONTINUE;
    float prog = 0.0f;
    for (int i = 0; i < 80 && last == V53_LOOP_CONTINUE; i++) {
        prog += 0.001f; /* trivial progress so no-progress never fires */
        last = v53_taor_step(&h, logits, 32, prog, 1);
    }
    if (last != V53_LOOP_ABSTAIN_DRIFT) {
        return v53_fail("expected ABSTAIN_DRIFT after sustained σ");
    }
    fprintf(stderr,
            "[v53 self-test] OK loop ABSTAIN_DRIFT after cumulative σ > cap\n");
    return 0;
}

static int test_loop_abstain_no_progress(void)
{
    v53_harness_state_t h;
    v53_harness_init(&h, "stalled", 50, 0.99f);
    h.drift_cap = 0.99f;
    h.no_progress_cap = 3;
    float logits[32];
    v53_make_logits_peaked(logits, 32);
    v53_loop_outcome_t last = V53_LOOP_CONTINUE;
    for (int i = 0; i < 20 && last == V53_LOOP_CONTINUE; i++) {
        last = v53_taor_step(&h, logits, 32, 0.2f, 1); /* constant progress */
    }
    if (last != V53_LOOP_ABSTAIN_NO_PROG) {
        return v53_fail("expected ABSTAIN_NO_PROG on constant progress");
    }
    fprintf(stderr, "[v53 self-test] OK loop ABSTAIN_NO_PROG\n");
    return 0;
}

static int test_loop_budget_exhausted(void)
{
    v53_harness_state_t h;
    v53_harness_init(&h, "spin", 3, 0.99f);
    h.drift_cap = 0.99f;
    h.no_progress_cap = 1000; /* disable no-progress */
    float logits[32];
    v53_make_logits_peaked(logits, 32);
    v53_loop_outcome_t last = V53_LOOP_CONTINUE;
    float prog = 0.0f;
    while (last == V53_LOOP_CONTINUE) {
        prog += 0.01f;
        last = v53_taor_step(&h, logits, 32, prog, 1);
    }
    if (last != V53_LOOP_BUDGET_EXHAUSTED) {
        return v53_fail("expected BUDGET_EXHAUSTED");
    }
    fprintf(stderr, "[v53 self-test] OK loop BUDGET_EXHAUSTED (3 iters)\n");
    return 0;
}

/* ─── σ-triggered dispatch ────────────────────────────────── */

static int test_dispatch_below_trigger(void)
{
    v53_specialist_cfg_t cfgs[4];
    int n = v53_default_specialists(cfgs, 4);
    if (n <= 0) return v53_fail("no default specialists");
    sigma_decomposed_t low = {0.10f, 0, 0};
    v53_dispatch_result_t r = v53_dispatch_if_needed(cfgs, n, "security", &low);
    if (r.spawned) return v53_fail("should not spawn below trigger");
    fprintf(stderr, "[v53 self-test] OK dispatch no-spawn (%s)\n", r.summary);
    return 0;
}

static int test_dispatch_above_trigger(void)
{
    v53_specialist_cfg_t cfgs[4];
    int n = v53_default_specialists(cfgs, 4);
    sigma_decomposed_t high = {0.80f, 0, 0};
    v53_dispatch_result_t r = v53_dispatch_if_needed(cfgs, n, "security", &high);
    if (!r.spawned) return v53_fail("should spawn above trigger");
    if (!(r.specialist_sigma_observed > 0.0f &&
          r.specialist_sigma_observed < high.total)) {
        return v53_fail("specialist σ should be lower than main σ");
    }
    fprintf(stderr,
            "[v53 self-test] OK dispatch spawn (main σ=0.80 → specialist σ=%.2f)\n",
            (double)r.specialist_sigma_observed);
    return 0;
}

static int test_dispatch_unknown_domain(void)
{
    v53_specialist_cfg_t cfgs[4];
    int n = v53_default_specialists(cfgs, 4);
    sigma_decomposed_t high = {0.80f, 0, 0};
    v53_dispatch_result_t r = v53_dispatch_if_needed(cfgs, n, "ufo", &high);
    if (r.spawned) return v53_fail("unknown domain must not spawn");
    fprintf(stderr, "[v53 self-test] OK dispatch unknown-domain refuses\n");
    return 0;
}

/* ─── σ-prioritized compression ───────────────────────────── */

static int test_compress_learning_outscores_filler(void)
{
    v53_message_meta_t learn  = {0, 0.80f, 1, 1, 0};
    v53_message_meta_t filler = {1, 0.05f, 0, 0, 0};
    float sl = v53_compress_score(&learn);
    float sf = v53_compress_score(&filler);
    if (!(sl > sf)) return v53_fail("learning should outscore filler");
    fprintf(stderr,
            "[v53 self-test] OK compress score learning=%.2f > filler=%.2f\n",
            (double)sl, (double)sf);
    return 0;
}

static int test_compress_invariant_reference_boosts_score(void)
{
    v53_message_meta_t plain     = {0, 0.10f, 0, 0, 0};
    v53_message_meta_t invariant = {1, 0.10f, 0, 0, 1};
    if (!(v53_compress_score(&invariant) > v53_compress_score(&plain))) {
        return v53_fail("invariant ref should boost score");
    }
    fprintf(stderr,
            "[v53 self-test] OK compress invariant reference boosts score\n");
    return 0;
}

static int test_compress_batch_drop_fraction(void)
{
    v53_message_meta_t ms[5] = {
        {0, 0.05f, 0, 0, 0}, /* filler */
        {1, 0.80f, 1, 1, 0}, /* learning */
        {2, 0.10f, 0, 0, 1}, /* invariant */
        {3, 0.02f, 0, 0, 0}, /* filler */
        {4, 0.60f, 2, 0, 0}, /* tool-heavy */
    };
    v53_compress_decision_t out[5];
    v53_compress_batch(ms, 5, 0.40f, out); /* drop ~2 */
    int dropped = 0;
    for (int i = 0; i < 5; i++) {
        if (!out[i].keep) dropped++;
    }
    if (dropped < 1 || dropped > 3) {
        return v53_fail("drop count outside [1,3]");
    }
    if (out[1].keep == 0 || out[2].keep == 0) {
        return v53_fail("learning + invariant must be kept");
    }
    fprintf(stderr,
            "[v53 self-test] OK compress batch dropped=%d/5 (learning + invariant kept)\n",
            dropped);
    return 0;
}

/* ─── creation.md loader ──────────────────────────────────── */

static int test_project_context_load(void)
{
    v53_project_context_t c;
    v53_load_project_context("creation.md", &c);
    if (!c.ok) return v53_fail("creation.md not loaded");
    if (c.invariants_count < 5) {
        return v53_fail("expected ≥ 5 invariants in creation.md");
    }
    if (c.conventions_count < 1) {
        return v53_fail("expected ≥ 1 convention in creation.md");
    }
    if (c.sigma_profile_rows < 3) {
        return v53_fail("expected ≥ 3 σ-profile rows");
    }
    fprintf(stderr,
            "[v53 self-test] OK creation.md parsed (invariants=%d, conventions=%d, σ-profile rows=%d)\n",
            c.invariants_count, c.conventions_count, c.sigma_profile_rows);
    return 0;
}

static int test_project_context_missing_file(void)
{
    v53_project_context_t c;
    v53_load_project_context("/nonexistent/path/creation.md", &c);
    if (c.ok) return v53_fail("missing file must yield ok=0");
    fprintf(stderr, "[v53 self-test] OK loader reports ok=0 on missing file\n");
    return 0;
}

/* ─── runner ──────────────────────────────────────────────── */

static int v53_run_self_test(void)
{
    int (*tests[])(void) = {
        test_loop_completes_on_goal,
        test_loop_abstain_sigma,
        test_loop_abstain_drift,
        test_loop_abstain_no_progress,
        test_loop_budget_exhausted,
        test_dispatch_below_trigger,
        test_dispatch_above_trigger,
        test_dispatch_unknown_domain,
        test_compress_learning_outscores_filler,
        test_compress_invariant_reference_boosts_score,
        test_compress_batch_drop_fraction,
        test_project_context_load,
        test_project_context_missing_file,
    };
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));
    int failed = 0;
    for (int i = 0; i < N; i++) {
        if (tests[i]()) failed++;
    }
    fprintf(stderr,
            "[v53 self-test] %d/%d PASS (σ-governed harness scaffold; not a live agent)\n",
            N - failed, N);
    return failed ? 1 : 0;
}

static void v53_print_banner(void)
{
    puts("╔══════════════════════════════════════════════════╗");
    puts("║  Creation OS v53 — σ-governed harness            ║");
    puts("║                                                  ║");
    puts("║  while (tool_call && σ < threshold)              ║");
    puts("║         && making_progress) { … }                ║");
    puts("║                                                  ║");
    puts("║  creation.md = invariants, not instructions.     ║");
    puts("║  subagent spawn = σ-triggered, not manual.       ║");
    puts("║  context compression = σ-quality, not temporal.  ║");
    puts("║                                                  ║");
    puts("║  Structural critique — not a Claude Code clone.  ║");
    puts("║  See docs/v53/ARCHITECTURE.md + POSITIONING.md.  ║");
    puts("╚══════════════════════════════════════════════════╝");
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        return v53_run_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--architecture") == 0) {
        v53_print_banner();
        return 0;
    }
    v53_print_banner();
    puts("");
    puts("Usage:");
    puts("  creation_os_v53 --self-test      run σ-harness scaffold checks");
    puts("  creation_os_v53 --architecture   print the banner only");
    (void)v53_outcome_str; /* referenced for linkage */
    return 0;
}
