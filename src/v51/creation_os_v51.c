/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v51 — AGI-complete integration scaffold (lab).
 *
 * Wires the v33–v50 σ labs into a single Perceive → Plan → Execute →
 * Verify → Reflect → Learn loop plus a σ-gated agent. Honest scope:
 *
 *   - This binary is an **integration scaffold (C)**, NOT a running
 *     transformer. No BitNet forward pass is embedded; no network
 *     calls; no dynamic allocation in the hot path.
 *   - σ math goes through `sigma_decompose_dirichlet_evidence` from
 *     `src/sigma/decompose.h` (v34 shipped shape).
 *   - Tier tags for every claim live in `docs/WHAT_IS_REAL.md` and the
 *     architecture map is `docs/v51/ARCHITECTURE.md`.
 *
 * Self-test prints a short [v51 self-test] summary and exits 0 / 1.
 */
#include "cognitive_loop.h"
#include "agent.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int v51_fail(const char *msg)
{
    fprintf(stderr, "[v51 self-test] FAIL %s\n", msg);
    return 1;
}

static int v51_nearly(float a, float b, float eps)
{
    float diff = fabsf(a - b);
    if (diff <= eps) {
        return 1;
    }
    return 0;
}

static void v51_make_logits_low_sigma(float *logits, int n)
{
    /* one strongly-peaked token → low σ */
    for (int i = 0; i < n; i++) {
        logits[i] = -4.0f;
    }
    logits[0] = 8.0f;
}

static void v51_make_logits_high_sigma(float *logits, int n)
{
    /* near-uniform → high σ */
    for (int i = 0; i < n; i++) {
        logits[i] = 0.001f * (float)(i % 3);
    }
}

/* ─── tests ────────────────────────────────────────────────────── */

static int test_thresholds_default(void)
{
    v51_thresholds_t t;
    v51_default_thresholds(&t);
    if (!(t.answer_below < t.think_below &&
          t.think_below < t.deep_below &&
          t.deep_below < t.fallback_below &&
          t.fallback_below <= 1.0f)) {
        return v51_fail("thresholds not monotonically ordered in [0,1]");
    }
    fprintf(stderr, "[v51 self-test] OK thresholds monotonic\n");
    return 0;
}

static int test_classify_difficulty(void)
{
    sigma_decomposed_t easy = {0.10f, 0.05f, 0.05f};
    sigma_decomposed_t mid  = {0.40f, 0.25f, 0.15f};
    sigma_decomposed_t hard = {0.80f, 0.50f, 0.30f};
    if (v51_classify_difficulty(&easy) != V51_DIFF_EASY)   return v51_fail("easy");
    if (v51_classify_difficulty(&mid)  != V51_DIFF_MEDIUM) return v51_fail("mid");
    if (v51_classify_difficulty(&hard) != V51_DIFF_HARD)   return v51_fail("hard");
    fprintf(stderr, "[v51 self-test] OK classify_difficulty easy/mid/hard\n");
    return 0;
}

static int test_decode_action_coverage(void)
{
    v51_thresholds_t t;
    v51_default_thresholds(&t);
    sigma_decomposed_t s0 = {0.05f, 0, 0}; /* ANSWER */
    sigma_decomposed_t s1 = {0.35f, 0, 0}; /* THINK */
    sigma_decomposed_t s2 = {0.65f, 0, 0}; /* DEEP */
    sigma_decomposed_t s3 = {0.85f, 0, 0}; /* FALLBACK */
    sigma_decomposed_t s4 = {0.95f, 0, 0}; /* ABSTAIN */
    if (v51_decode_action(&s0, &t) != V51_ACTION_ANSWER)   return v51_fail("s0");
    if (v51_decode_action(&s1, &t) != V51_ACTION_THINK)    return v51_fail("s1");
    if (v51_decode_action(&s2, &t) != V51_ACTION_DEEP)     return v51_fail("s2");
    if (v51_decode_action(&s3, &t) != V51_ACTION_FALLBACK) return v51_fail("s3");
    if (v51_decode_action(&s4, &t) != V51_ACTION_ABSTAIN)  return v51_fail("s4");
    fprintf(stderr, "[v51 self-test] OK decode_action covers all 5 branches\n");
    return 0;
}

static int test_budget_monotone(void)
{
    sigma_decomposed_t lo = {0.10f, 0, 0};
    sigma_decomposed_t hi = {0.90f, 0, 0};
    int b_lo = v51_compute_think_budget(&lo);
    int b_hi = v51_compute_think_budget(&hi);
    if (!(b_hi > b_lo))          return v51_fail("budget not monotone");
    if (b_lo < 16 || b_hi > 512) return v51_fail("budget bounds");
    int n_lo = v51_compute_n_samples(&lo);
    int n_hi = v51_compute_n_samples(&hi);
    if (!(n_hi > n_lo))          return v51_fail("n_samples not monotone");
    fprintf(stderr, "[v51 self-test] OK budget/n_samples monotone (lo=%d,%d hi=%d,%d)\n",
            b_lo, n_lo, b_hi, n_hi);
    return 0;
}

static int test_cognitive_step_low_sigma(void)
{
    float logits[32];
    v51_make_logits_low_sigma(logits, 32);
    v51_cognitive_state_t s;
    v51_cognitive_step("2+2=?", logits, 32, NULL, &s);
    if (s.initial_sigma.total < 0.0f || s.initial_sigma.total > 1.0f) {
        return v51_fail("σ out of [0,1]");
    }
    if (s.planned_action != V51_ACTION_ANSWER) {
        return v51_fail("low-σ should plan ANSWER");
    }
    if (s.response == NULL)   return v51_fail("response NULL");
    if (!s.experience_logged) return v51_fail("experience not logged");
    fprintf(stderr, "[v51 self-test] OK cognitive_step low-σ (σ=%.3f action=ANSWER)\n",
            (double)s.initial_sigma.total);
    return 0;
}

static int test_cognitive_step_high_sigma(void)
{
    float logits[32];
    v51_make_logits_high_sigma(logits, 32);
    v51_cognitive_state_t s;
    v51_cognitive_step("what is the prime factorization of 2837491?",
                       logits, 32, NULL, &s);
    if (s.planned_action == V51_ACTION_ANSWER) {
        return v51_fail("high-σ should NOT plan plain ANSWER");
    }
    if (s.response == NULL) {
        return v51_fail("response NULL on high-σ");
    }
    fprintf(stderr, "[v51 self-test] OK cognitive_step high-σ (σ=%.3f action=%d)\n",
            (double)s.initial_sigma.total, (int)s.planned_action);
    return 0;
}

static int test_cognitive_step_null_logits(void)
{
    v51_cognitive_state_t s;
    v51_cognitive_step("no logits", NULL, 0, NULL, &s);
    if (!v51_nearly(s.initial_sigma.total, 0.5f, 1e-6f)) {
        return v51_fail("null-logits synthetic σ should be 0.5");
    }
    if (s.response == NULL) {
        return v51_fail("null-logits response NULL");
    }
    fprintf(stderr, "[v51 self-test] OK cognitive_step null logits (synthetic σ=0.5)\n");
    return 0;
}

static int test_sandbox_sigma_deny(void)
{
    v51_sandbox_t sb;
    v51_agent_default_sandbox(&sb);
    sigma_decomposed_t high = {0.80f, 0, 0};
    v51_sandbox_decision_t d = v51_sandbox_check(&sb, "fs.write", &high);
    if (d.allowed) return v51_fail("sandbox should deny tool when σ above threshold");
    fprintf(stderr, "[v51 self-test] OK sandbox σ-deny (reason=\"%s\")\n", d.reason);
    return 0;
}

static int test_sandbox_policy_fail_closed(void)
{
    v51_tool_policy_t pols[] = {
        {"fs.read", 1},
        {"fs.write", 0},
    };
    v51_sandbox_t sb;
    v51_agent_default_sandbox(&sb);
    sb.policies = pols;
    sb.n_policies = 2;
    sigma_decomposed_t low = {0.10f, 0, 0};

    v51_sandbox_decision_t allow = v51_sandbox_check(&sb, "fs.read", &low);
    if (!allow.allowed) return v51_fail("fs.read should be allowed");

    v51_sandbox_decision_t deny = v51_sandbox_check(&sb, "fs.write", &low);
    if (deny.allowed) return v51_fail("fs.write should be denied by policy");

    v51_sandbox_decision_t unknown = v51_sandbox_check(&sb, "net.exec", &low);
    if (unknown.allowed) return v51_fail("unknown tool must fail-closed under policy list");

    fprintf(stderr, "[v51 self-test] OK sandbox fail-closed on unknown tool\n");
    return 0;
}

static int test_agent_run_low_sigma_reaches_goal(void)
{
    v51_agent_t a;
    v51_agent_init(&a, "v51-agent", 8);
    float logits[32];
    v51_make_logits_low_sigma(logits, 32);
    int steps = v51_agent_run(&a, "answer an easy question",
                              logits, 32, "fs.read");
    if (steps <= 0)         return v51_fail("agent took zero steps");
    if (!a.goal_reached)    return v51_fail("low-σ agent should reach goal");
    if (a.abstained)        return v51_fail("low-σ agent should not abstain");
    fprintf(stderr, "[v51 self-test] OK agent low-σ reaches goal in %d step(s)\n", steps);
    return 0;
}

static int test_agent_run_high_sigma_abstains(void)
{
    v51_agent_t a;
    v51_agent_init(&a, "v51-agent", 8);
    float logits[32];
    v51_make_logits_high_sigma(logits, 32);
    int steps = v51_agent_run(&a, "answer a hard question",
                              logits, 32, "fs.write");
    if (steps != 1)    return v51_fail("high-σ agent should stop after 1 step (abstain)");
    if (!a.abstained)  return v51_fail("high-σ agent should abstain");
    if (a.goal_reached) return v51_fail("high-σ agent must not claim goal reached");
    if (a.tools_executed != 0) return v51_fail("no tool should execute after abstain");
    fprintf(stderr, "[v51 self-test] OK agent high-σ abstains cleanly (no tool exec)\n");
    return 0;
}

static int test_agent_policy_denied_tool_logs_to_memory(void)
{
    v51_tool_policy_t pols[] = {
        {"fs.read", 1},
        {"fs.write", 0},
    };
    v51_agent_t a;
    v51_agent_init(&a, "v51-agent", 2);
    a.sandbox.policies = pols;
    a.sandbox.n_policies = 2;

    float logits[32];
    v51_make_logits_low_sigma(logits, 32);
    int steps = v51_agent_run(&a, "try a forbidden tool",
                              logits, 32, "fs.write");
    if (steps <= 0)                 return v51_fail("no steps");
    if (a.tools_denied <= 0)        return v51_fail("deny counter not incremented");
    if (a.memory.count <= 0)        return v51_fail("memory empty");
    fprintf(stderr, "[v51 self-test] OK agent logs denied tool to memory (count=%d)\n",
            a.memory.count);
    return 0;
}

static int test_memory_ring_caps(void)
{
    v51_memory_t m; memset(&m, 0, sizeof(m));
    v51_experience_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.tool, "probe", sizeof(e.tool) - 1);
    for (int i = 0; i < V51_AGENT_MEMORY_MAX + 5; i++) {
        v51_memory_append(&m, &e);
    }
    if (m.count != V51_AGENT_MEMORY_MAX) {
        return v51_fail("ring buffer cap");
    }
    fprintf(stderr, "[v51 self-test] OK memory ring caps at %d\n", m.count);
    return 0;
}

/* ─── runner ──────────────────────────────────────────────────── */

static int v51_run_self_test(void)
{
    int (*tests[])(void) = {
        test_thresholds_default,
        test_classify_difficulty,
        test_decode_action_coverage,
        test_budget_monotone,
        test_cognitive_step_low_sigma,
        test_cognitive_step_high_sigma,
        test_cognitive_step_null_logits,
        test_sandbox_sigma_deny,
        test_sandbox_policy_fail_closed,
        test_agent_run_low_sigma_reaches_goal,
        test_agent_run_high_sigma_abstains,
        test_agent_policy_denied_tool_logs_to_memory,
        test_memory_ring_caps,
    };
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));
    int failed = 0;
    for (int i = 0; i < N; i++) {
        if (tests[i]()) {
            failed++;
        }
    }
    fprintf(stderr, "[v51 self-test] %d/%d PASS (integration scaffold; not a live engine)\n",
            N - failed, N);
    return failed ? 1 : 0;
}

static void v51_print_architecture_banner(void)
{
    puts("╔══════════════════════════════════════════════════╗");
    puts("║  Creation OS v51 — AGI-complete scaffold         ║");
    puts("║  Perceive → Plan → Execute → Verify → Reflect    ║");
    puts("║             → Learn                              ║");
    puts("║                                                  ║");
    puts("║  NOT a running engine. NOT a product claim.      ║");
    puts("║  Wiring shape for v33–v50 labs. See              ║");
    puts("║  docs/v51/ARCHITECTURE.md + WHAT_IS_REAL.md.     ║");
    puts("╚══════════════════════════════════════════════════╝");
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        return v51_run_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--architecture") == 0) {
        v51_print_architecture_banner();
        return 0;
    }
    v51_print_architecture_banner();
    puts("");
    puts("Usage:");
    puts("  creation_os_v51 --self-test      run integration scaffold checks");
    puts("  creation_os_v51 --architecture   print the scaffold banner");
    return 0;
}
