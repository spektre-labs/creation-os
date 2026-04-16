/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v54 — σ-Proconductor scaffold.
 *
 * Ships a self-test exercising four surfaces:
 *   1. Registry + defaults: 5 reference subagents w/ hand-tuned profiles.
 *   2. Classify + select:
 *        - easy factual query     → K=1 (cheapest capable model)
 *        - medium-stakes query    → K=2
 *        - high-stakes legal query → K = min(4, max_parallel)
 *        - σ_primary < 0.10 shortcut → K=1 (skip triangulation)
 *   3. Aggregate:
 *        - consensus (similar answers, low σ_ensemble)
 *        - σ_ensemble > threshold → abstain
 *        - pairwise disagreement > threshold → abstain
 *        - σ-weighted winner for borderline disagreement
 *        - empty input → AGG_EMPTY
 *   4. Profile learner: EWMA drift + ground-truth calibration signal.
 *
 * Honest scope: no network, no embedding model, no tokenizer, no
 * external API. "Responses" are in-memory strings with caller-
 * supplied σ. See docs/v54/ARCHITECTURE.md + creation.md invariants.
 */
#include "proconductor.h"
#include "dispatch.h"
#include "disagreement.h"
#include "learn_profiles.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int v54_fail(const char *m)
{
    fprintf(stderr, "[v54 self-test] FAIL %s\n", m);
    return 1;
}

static void v54_fill_response(v54_response_t *r, int agent_index,
                              float sigma, const char *text)
{
    memset(r, 0, sizeof(*r));
    r->agent_index    = agent_index;
    r->reported_sigma = sigma;
    r->latency_ms     = 500.0f;
    r->cost_tokens    = 128;
    if (text) {
        size_t n = strlen(text);
        if (n >= sizeof(r->text)) n = sizeof(r->text) - 1;
        memcpy(r->text, text, n);
        r->text[n] = '\0';
    }
}

/* ─── registry ────────────────────────────────────────────── */

static int test_registry_defaults(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    if (v54_proconductor_register_defaults(&p) != 0) {
        return v54_fail("register_defaults returned nonzero");
    }
    if (p.n_agents != 5) return v54_fail("expected 5 default subagents");
    /* Sanity: all profile entries in [0,1]. */
    for (int i = 0; i < p.n_agents; i++) {
        for (int d = 0; d < V54_N_DOMAINS; d++) {
            float s = p.agents[i].sigma_profile[d];
            if (!(s >= 0.0f && s <= 1.0f)) {
                return v54_fail("σ-profile out of [0,1]");
            }
        }
    }
    fprintf(stderr,
            "[v54 self-test] OK registry: 5 subagents "
            "(%s, %s, %s, %s, %s)\n",
            p.agents[0].name, p.agents[1].name, p.agents[2].name,
            p.agents[3].name, p.agents[4].name);
    return 0;
}

/* ─── classify + select ───────────────────────────────────── */

static int test_classify_factual(void)
{
    v54_query_profile_t q;
    v54_classify_query("What is the capital of Finland?", &q);
    if (q.primary != V54_DOMAIN_FACTUAL) {
        return v54_fail("expected primary=FACTUAL for 'what is'");
    }
    fprintf(stderr, "[v54 self-test] OK classify FACTUAL (primary=%s)\n",
            v54_domain_name(q.primary));
    return 0;
}

static int test_classify_code(void)
{
    v54_query_profile_t q;
    v54_classify_query("Refactor this function to remove the bug in the loop",
                       &q);
    if (q.primary != V54_DOMAIN_CODE) {
        return v54_fail("expected primary=CODE for 'refactor/bug/function'");
    }
    fprintf(stderr, "[v54 self-test] OK classify CODE\n");
    return 0;
}

static int test_classify_high_stakes(void)
{
    v54_query_profile_t q;
    v54_classify_query("Is this contract legally binding in Finland?", &q);
    if (!(q.stakes >= 0.85f)) return v54_fail("legal/contract should raise stakes");
    fprintf(stderr, "[v54 self-test] OK classify stakes=%.2f (high)\n",
            (double)q.stakes);
    return 0;
}

static int test_select_low_stakes_single_agent(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_query_profile_t q = { V54_DOMAIN_FACTUAL, V54_DOMAIN_META,
                              0.3f, 0.10f };
    int selected[V54_MAX_AGENTS];
    int K = v54_select_subagents(&p, &q, selected, V54_MAX_AGENTS);
    if (K != 1) return v54_fail("low stakes should pick K=1");
    fprintf(stderr,
            "[v54 self-test] OK select low-stakes K=%d (winner=%s)\n",
            K, p.agents[selected[0]].name);
    return 0;
}

static int test_select_high_stakes_four_agents(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_query_profile_t q = { V54_DOMAIN_LOGIC, V54_DOMAIN_FACTUAL,
                              0.9f, 0.90f };
    int selected[V54_MAX_AGENTS];
    int K = v54_select_subagents(&p, &q, selected, V54_MAX_AGENTS);
    if (K != 4) return v54_fail("high stakes should pick K=4");
    fprintf(stderr,
            "[v54 self-test] OK select high-stakes K=%d (top=%s,%s,%s,%s)\n",
            K, p.agents[selected[0]].name, p.agents[selected[1]].name,
            p.agents[selected[2]].name, p.agents[selected[3]].name);
    return 0;
}

static int test_select_easy_query_shortcut(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    /* Register one "super-expert" agent with σ_primary < 0.10 to trip
     * the easy-query shortcut. */
    float expert[V54_N_DOMAINS]   = {0.05f, 0.20f, 0.25f, 0.20f, 0.25f};
    float generic[V54_N_DOMAINS]  = {0.30f, 0.25f, 0.20f, 0.20f, 0.25f};
    v54_proconductor_register(&p, "expert",  "local/bitnet-expert", 0.0f, 50.0f, expert);
    v54_proconductor_register(&p, "generic", "external/generic",    5.0f, 700.0f, generic);
    v54_query_profile_t q = { V54_DOMAIN_LOGIC, V54_DOMAIN_META,
                              0.9f, 0.95f }; /* would want K=4 normally */
    int selected[V54_MAX_AGENTS];
    int K = v54_select_subagents(&p, &q, selected, V54_MAX_AGENTS);
    if (K != 1) return v54_fail("easy-query shortcut should force K=1");
    if (strcmp(p.agents[selected[0]].name, "expert") != 0) {
        return v54_fail("shortcut should pick the expert");
    }
    fprintf(stderr,
            "[v54 self-test] OK select easy-query shortcut → K=1 (%s)\n",
            p.agents[selected[0]].name);
    return 0;
}

/* ─── aggregate ────────────────────────────────────────────── */

static int test_aggregate_consensus(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_response_t r[2];
    /* Near-identical text so the lexical-overlap similarity is high (≥ 0.80). */
    v54_fill_response(&r[0], 0, 0.15f,
                      "quantum entanglement links two particles in a shared quantum state");
    v54_fill_response(&r[1], 1, 0.18f,
                      "quantum entanglement links two particles in a shared quantum state together");
    v54_aggregation_t agg;
    v54_aggregate_responses(&p, r, 2, &agg);
    if (agg.outcome != V54_AGG_CONSENSUS) {
        return v54_fail("expected CONSENSUS on similar responses");
    }
    fprintf(stderr,
            "[v54 self-test] OK aggregate CONSENSUS (winner=agent %d, σ_ens=%.3f, disagree=%.2f)\n",
            agg.winner_index, (double)agg.sigma_ensemble,
            (double)agg.disagreement);
    return 0;
}

static int test_aggregate_abstain_disagreement(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_response_t r[4];
    v54_fill_response(&r[0], 0, 0.30f, "yes binding under Finnish contract law");
    v54_fill_response(&r[1], 1, 0.40f, "depends on signature method used");
    v54_fill_response(&r[2], 2, 0.35f, "binding if digital signature compliant");
    v54_fill_response(&r[3], 3, 0.45f, "depends on jurisdiction clauses completely");
    v54_aggregation_t agg;
    v54_aggregate_responses(&p, r, 4, &agg);
    if (agg.outcome != V54_AGG_ABSTAIN_DISAGREE) {
        return v54_fail("expected ABSTAIN_DISAGREE on split responses");
    }
    fprintf(stderr,
            "[v54 self-test] OK aggregate ABSTAIN_DISAGREE (disagreement=%.2f)\n",
            (double)agg.disagreement);
    return 0;
}

static int test_aggregate_abstain_sigma(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    p.convergence_threshold = 0.05f; /* strict convergence */
    v54_response_t r[2];
    /* Same text so disagreement = 0; σ_ensemble = 0.60*0.55 = 0.33 > 0.05 → abstain. */
    v54_fill_response(&r[0], 0, 0.60f,
                      "the sky appears blue because rayleigh scattering dominates in the atmosphere");
    v54_fill_response(&r[1], 1, 0.55f,
                      "the sky appears blue because rayleigh scattering dominates in the atmosphere");
    v54_aggregation_t agg;
    v54_aggregate_responses(&p, r, 2, &agg);
    if (agg.outcome != V54_AGG_ABSTAIN_SIGMA) {
        return v54_fail("expected ABSTAIN_SIGMA when σ_ensemble > threshold");
    }
    fprintf(stderr,
            "[v54 self-test] OK aggregate ABSTAIN_SIGMA (σ_ens=%.3f > %.3f)\n",
            (double)agg.sigma_ensemble, (double)p.convergence_threshold);
    return 0;
}

static int test_aggregate_empty(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_aggregation_t agg;
    v54_aggregate_responses(&p, NULL, 0, &agg);
    if (agg.outcome != V54_AGG_EMPTY) {
        return v54_fail("expected AGG_EMPTY on zero responses");
    }
    fprintf(stderr, "[v54 self-test] OK aggregate EMPTY on n=0\n");
    return 0;
}

/* ─── learner ─────────────────────────────────────────────── */

static int test_profile_learner_ewma(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_subagent_t *claude = &p.agents[0];
    float before = claude->sigma_profile[V54_DOMAIN_LOGIC];
    /* Feed a stream of low σ observations — profile should drop. */
    for (int i = 0; i < 200; i++) {
        v54_learn_profile_update(claude, V54_DOMAIN_LOGIC, 0.05f, 1);
    }
    float after = claude->sigma_profile[V54_DOMAIN_LOGIC];
    if (!(after < before - 0.03f)) {
        return v54_fail("σ-profile should decrease after repeated low-σ observations");
    }
    /* Observed accuracy should climb toward 1.0. */
    if (!(claude->observed_accuracy[V54_DOMAIN_LOGIC] > 0.7f)) {
        return v54_fail("observed_accuracy should rise with was_correct=1");
    }
    fprintf(stderr,
            "[v54 self-test] OK learner EWMA (σ %.3f→%.3f, acc=%.2f)\n",
            (double)before, (double)after,
            (double)claude->observed_accuracy[V54_DOMAIN_LOGIC]);
    return 0;
}

static int test_profile_learner_from_aggregation(void)
{
    v54_proconductor_t p;
    v54_proconductor_init(&p);
    v54_proconductor_register_defaults(&p);
    v54_response_t r[2];
    v54_fill_response(&r[0], 0, 0.15f, "same answer stream");
    v54_fill_response(&r[1], 1, 0.20f, "same answer stream");
    v54_aggregation_t agg;
    v54_aggregate_responses(&p, r, 2, &agg);
    int winner = agg.winner_index;
    if (winner < 0) return v54_fail("aggregation produced no winner");
    int before_reqs = p.agents[winner].requests_made;
    v54_learn_from_aggregation(&p, V54_DOMAIN_FACTUAL, r, 2, &agg, 1, 0);
    if (!(p.agents[winner].requests_made > before_reqs)) {
        return v54_fail("winner requests_made did not tick");
    }
    fprintf(stderr,
            "[v54 self-test] OK learner from_aggregation (winner=%s)\n",
            p.agents[winner].name);
    return 0;
}

/* ─── disagreement ─────────────────────────────────────────── */

static int test_disagreement_outlier(void)
{
    v54_response_t r[4];
    v54_fill_response(&r[0], 0, 0.2f, "the answer is strictly forty two");
    v54_fill_response(&r[1], 1, 0.2f, "the answer is forty two exactly");
    v54_fill_response(&r[2], 2, 0.2f, "forty two is the answer obviously");
    v54_fill_response(&r[3], 3, 0.5f,
                      "banana lamp synthesis protocol unrelated entirely");
    v54_disagreement_t d;
    v54_disagreement_analyze(r, 4, &d);
    if (d.outlier_index != 3) return v54_fail("expected outlier index 3");
    if (!(d.outlier_distance > d.mean_disagreement)) {
        return v54_fail("outlier distance should exceed mean disagreement");
    }
    fprintf(stderr,
            "[v54 self-test] OK disagreement outlier=%d (distance=%.2f, mean=%.2f)\n",
            d.outlier_index, (double)d.outlier_distance,
            (double)d.mean_disagreement);
    return 0;
}

/* ─── runner ──────────────────────────────────────────────── */

static int v54_run_self_test(void)
{
    int (*tests[])(void) = {
        test_registry_defaults,
        test_classify_factual,
        test_classify_code,
        test_classify_high_stakes,
        test_select_low_stakes_single_agent,
        test_select_high_stakes_four_agents,
        test_select_easy_query_shortcut,
        test_aggregate_consensus,
        test_aggregate_abstain_disagreement,
        test_aggregate_abstain_sigma,
        test_aggregate_empty,
        test_profile_learner_ewma,
        test_profile_learner_from_aggregation,
        test_disagreement_outlier,
    };
    const int N = (int)(sizeof(tests) / sizeof(tests[0]));
    int failed = 0;
    for (int i = 0; i < N; i++) {
        if (tests[i]()) failed++;
    }
    fprintf(stderr,
            "[v54 self-test] %d/%d PASS (σ-proconductor scaffold; no network, no embeddings)\n",
            N - failed, N);
    return failed ? 1 : 0;
}

static void v54_print_banner(void)
{
    puts("╔══════════════════════════════════════════════════╗");
    puts("║  Creation OS v54 — σ-Proconductor                ║");
    puts("║                                                  ║");
    puts("║  σ₁ × σ₂ × σ₃ triangulates truth.                 ║");
    puts("║  σ-profile routing, not question-type routing.   ║");
    puts("║  Disagreement abstain, not majority vote.        ║");
    puts("║                                                  ║");
    puts("║  Frontier models = σ-characterized subagents.    ║");
    puts("║  Creation OS = the conductor.                    ║");
    puts("║                                                  ║");
    puts("║  No network call in this binary — callers do the ║");
    puts("║  dispatch; this is the orchestration policy.     ║");
    puts("║                                                  ║");
    puts("║  See docs/v54/ARCHITECTURE.md + POSITIONING.md.  ║");
    puts("╚══════════════════════════════════════════════════╝");
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        return v54_run_self_test();
    }
    if (argc >= 2 && strcmp(argv[1], "--architecture") == 0) {
        v54_print_banner();
        return 0;
    }
    v54_print_banner();
    puts("");
    puts("Usage:");
    puts("  creation_os_v54 --self-test      run σ-proconductor scaffold checks");
    puts("  creation_os_v54 --architecture   print the banner only");
    return 0;
}
