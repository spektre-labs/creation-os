/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos benchmark — pipeline benchmark harness.
 *
 * Runs a fixed fixture set of prompts through multiple pipeline
 * configurations and prints a comparison table so the Codex effect
 * and the σ-gate effect are measured, not asserted.
 *
 * Configurations:
 *   1. bitnet_only        — NO σ-gate, no codex (every answer accepted
 *                           as the first local draft)
 *   2. pipeline_no_codex  — σ-gate + RETHINK + escalate, no Codex
 *   3. pipeline_codex     — σ-gate + RETHINK + escalate + Codex
 *   4. api_only           — every call escalated (cloud tier)
 *
 * Fixtures use the stub generator's prompt-prefix convention:
 *   "low:"    → σ=0.05  (ACCEPT)
 *   "improve:"→ σ drops (RETHINK → ACCEPT)
 *   "mid:"    → plateau σ (RETHINK ×3 → ABSTAIN → escalate)
 *   "high:"   → σ=0.92  (immediate ABSTAIN → escalate)
 *
 * The fixture is 20 prompts chosen so the mix is realistic:
 * 60% low, 20% improve, 15% mid, 5% high.
 *
 * Output is a pinnable pipe-delimited table.  No external data
 * dependencies — fixtures are inline constants — so the benchmark
 * is reproducible bit-for-bit on every host.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"

#include "../sigma/metrics/energy_metric.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Inline fixture.  Keep it tiny — the point is the TABLE, not the
 * eval.  For real evals, feed TruthfulQA via a JSONL reader. */
static const char *FIXTURES[] = {
    /* 12 low  */
    "low:what is 2+2?",            "low:capital of France?",
    "low:hello, describe math",    "low:what color is the sky?",
    "low:who wrote Hamlet?",       "low:define gravity",
    "low:define entropy",          "low:what is a function?",
    "low:what is a vector?",       "low:what is a prime?",
    "low:what is recursion?",      "low:what does HTTP stand for?",
    /* 4 improve */
    "improve:explain entanglement", "improve:derive kinetic energy",
    "improve:explain GANs",         "improve:define category theory",
    /* 3 mid */
    "mid:prove P=NP",              "mid:ultimate moral framework",
    "mid:solve Goldbach",
    /* 1 high */
    "high:unfalsifiable question",
};
enum { N_FIXTURES = (int)(sizeof(FIXTURES) / sizeof(FIXTURES[0])) };

/* Per-config accumulator. */
typedef struct {
    const char *name;
    int    n_total;
    int    n_accept;
    int    n_abstain;
    int    n_escalated;
    int    n_engram_hit;
    int    n_rethink;
    double sum_sigma;
    double sum_cost_eur;
    cos_sigma_sovereign_t sv;
} bench_t;

static void run_config(bench_t *b, const cos_pipeline_config_t *base,
                       const cos_sigma_codex_t *codex, int enable_gate) {
    b->n_total = b->n_accept = b->n_abstain = 0;
    b->n_escalated = b->n_engram_hit = b->n_rethink = 0;
    b->sum_sigma = b->sum_cost_eur = 0.0;
    cos_sigma_sovereign_init(&b->sv, 0.85f);

    cos_pipeline_config_t cfg = *base;
    cfg.codex     = codex;
    cfg.sovereign = &b->sv;
    /* bitnet_only: disable σ-gate by widening accept band to 1.0
     * (every σ ACCEPTs). */
    if (!enable_gate) {
        cfg.tau_accept  = 1.01f;
        cfg.tau_rethink = 1.02f;
        cfg.max_rethink = 1;
    }
    /* fresh engram per config so HIT counts are comparable. */
    enum { CAP = 64 };
    cos_sigma_engram_entry_t slots[CAP];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    cos_sigma_engram_init(&engram, slots, CAP, 0.25f, 200, 20);
    cfg.engram = &engram;

    for (int i = 0; i < N_FIXTURES; ++i) {
        cos_pipeline_result_t r;
        cos_sigma_pipeline_run(&cfg, FIXTURES[i], &r);
        b->n_total++;
        b->sum_sigma     += (double)r.sigma;
        b->sum_cost_eur  += r.cost_eur;
        b->n_rethink     += r.rethink_count;
        if (r.engram_hit)  b->n_engram_hit++;
        if (r.escalated)   b->n_escalated++;
        if (r.final_action == COS_SIGMA_ACTION_ACCEPT) b->n_accept++;
        else if (r.final_action == COS_SIGMA_ACTION_ABSTAIN) b->n_abstain++;
    }
    cos_sigma_pipeline_free_engram_values(&engram);
}

/* api_only: force every call to escalate by driving the generator
 * always through the "high:" path wired into the escalator. */
static int api_only_generate(const char *prompt, int round, void *ctx,
                             const char **out_text, float *out_sigma,
                             double *out_cost_eur) {
    (void)prompt; (void)round; (void)ctx;
    *out_text     = "local draft (api_only config)";
    *out_sigma    = 0.92f;   /* always ABSTAIN → escalate */
    *out_cost_eur = 0.0001;
    return 0;
}

static void run_api_only(bench_t *b) {
    b->n_total = b->n_accept = b->n_abstain = 0;
    b->n_escalated = b->n_engram_hit = b->n_rethink = 0;
    b->sum_sigma = b->sum_cost_eur = 0.0;
    cos_sigma_sovereign_init(&b->sv, 0.85f);

    enum { CAP = 64 };
    cos_sigma_engram_entry_t slots[CAP];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    cos_sigma_engram_init(&engram, slots, CAP, 0.25f, 200, 20);

    cos_pipeline_config_t cfg;
    cos_sigma_pipeline_config_defaults(&cfg);
    cfg.tau_accept   = 0.40f;
    cfg.tau_rethink  = 0.60f;
    cfg.max_rethink  = 1;   /* skip rethink; escalate immediately */
    cfg.mode         = COS_PIPELINE_MODE_HYBRID;
    cfg.sovereign    = &b->sv;
    cfg.engram       = &engram;
    cfg.generate     = api_only_generate;
    cfg.escalate     = cos_cli_stub_escalate;

    for (int i = 0; i < N_FIXTURES; ++i) {
        cos_pipeline_result_t r;
        cos_sigma_pipeline_run(&cfg, FIXTURES[i], &r);
        b->n_total++;
        b->sum_sigma    += (double)r.sigma;
        b->sum_cost_eur += r.cost_eur;
        if (r.escalated)  b->n_escalated++;
        if (r.final_action == COS_SIGMA_ACTION_ACCEPT) b->n_accept++;
        else if (r.final_action == COS_SIGMA_ACTION_ABSTAIN) b->n_abstain++;
    }
    cos_sigma_pipeline_free_engram_values(&engram);
}

static void print_row(const bench_t *b) {
    double acc  = (b->n_total > 0)
                 ? (double)b->n_accept / b->n_total : 0.0;
    double cov  = (b->n_total > 0)
                 ? 1.0 - (double)b->n_abstain / b->n_total : 0.0;
    double sig  = (b->n_total > 0)
                 ? b->sum_sigma / b->n_total : 0.0;
    double per  = (b->n_total > 0)
                 ? b->sum_cost_eur / b->n_total : 0.0;
    printf(" %-22s | %5.2f | %4.2f | €%7.4f | €%8.5f | %.3f | %3d | %3d | %3d | %3d\n",
           b->name, acc, cov, b->sum_cost_eur, per, sig,
           b->n_rethink, b->n_escalated, b->n_engram_hit, b->n_abstain);
}

/* CLOSE-7 — pinned Codex vs no-Codex comparison on the stub harness. */
static void bench_rates(const bench_t *b, int n_tot, double *acc, double *cov,
                        double *mean_sig, double *esc_rate) {
    if (b == NULL || n_tot <= 0) {
        *acc = *cov = *mean_sig = *esc_rate = 0.0;
        return;
    }
    *acc       = (double)b->n_accept / (double)n_tot;
    *cov       = 1.0 - (double)b->n_abstain / (double)n_tot;
    *mean_sig  = b->sum_sigma / (double)n_tot;
    *esc_rate  = (double)b->n_escalated / (double)n_tot;
}

static int write_codex_comparison(const char *path, int n_fix,
                                  const bench_t *b_nc, const bench_t *b_cx,
                                  int codex_loaded, unsigned long long codex_hash) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "cos benchmark: cannot open %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    double acc_nc, cov_nc, sig_nc, esc_nc;
    double acc_cx, cov_cx, sig_cx, esc_cx;
    bench_rates(b_nc, n_fix, &acc_nc, &cov_nc, &sig_nc, &esc_nc);
    bench_rates(b_cx, n_fix, &acc_cx, &cov_cx, &sig_cx, &esc_cx);
    fprintf(f,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"evidence_class\": \"repository stub fixture (cos_cli_stub_generate); not LM harness accuracy\",\n"
            "  \"generator\": \"cos_cli_stub_generate\",\n"
            "  \"fixtures\": %d,\n"
            "  \"codex_loaded\": %s,\n"
            "  \"codex_hash\": \"%016llx\",\n"
            "  \"pipeline_no_codex\": {\n"
            "    \"accept_rate\": %.6f,\n"
            "    \"coverage\": %.6f,\n"
            "    \"mean_sigma\": %.6f,\n"
            "    \"escalation_rate\": %.6f,\n"
            "    \"n_accept\": %d,\n"
            "    \"n_escalated\": %d,\n"
            "    \"eur_total\": %.8f\n"
            "  },\n"
            "  \"pipeline_codex\": {\n"
            "    \"accept_rate\": %.6f,\n"
            "    \"coverage\": %.6f,\n"
            "    \"mean_sigma\": %.6f,\n"
            "    \"escalation_rate\": %.6f,\n"
            "    \"n_accept\": %d,\n"
            "    \"n_escalated\": %d,\n"
            "    \"eur_total\": %.8f\n"
            "  },\n"
            "  \"delta\": {\n"
            "    \"accept_rate\": %.6f,\n"
            "    \"mean_sigma\": %.6f,\n"
            "    \"escalation_rate\": %.6f,\n"
            "    \"eur_total\": %.8f\n"
            "  },\n"
            "  \"honest_note\": \"When codex_loaded is false, pipeline_codex matches pipeline_no_codex by construction. "
            "For frontier accuracy, archive lm-eval JSON per docs/CLAIM_DISCIPLINE.md.\"\n"
            "}\n",
            n_fix,
            codex_loaded ? "true" : "false",
            codex_loaded ? codex_hash : 0ULL,
            acc_nc, cov_nc, sig_nc, esc_nc,
            b_nc->n_accept, b_nc->n_escalated, b_nc->sum_cost_eur,
            acc_cx, cov_cx, sig_cx, esc_cx,
            b_cx->n_accept, b_cx->n_escalated, b_cx->sum_cost_eur,
            acc_cx - acc_nc, sig_cx - sig_nc, esc_cx - esc_nc,
            b_cx->sum_cost_eur - b_nc->sum_cost_eur);
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    int fmt_json = 0;
    const char *emit_comparison = NULL;
    int codex_mode = 0; /* 0 = try load, 1 = require codex, 2 = force off */
    int energy_only = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) fmt_json = 1;
        else if (strcmp(argv[i], "--emit-comparison") == 0 && i + 1 < argc)
            emit_comparison = argv[++i];
        else if (strcmp(argv[i], "--codex") == 0) codex_mode = 1;
        else if (strcmp(argv[i], "--no-codex") == 0) codex_mode = 2;
        else if (strcmp(argv[i], "--energy") == 0) energy_only = 1;
        else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stdout,
                "cos benchmark — pipeline comparison (N=%d fixtures)\n"
                "  --json                 emit per-config JSON instead of a table\n"
                "  --emit-comparison PATH write Codex vs no-Codex stub summary JSON\n"
                "  --codex                require a loadable Codex (exit 1 if missing)\n"
                "  --no-codex             never load Codex (pipeline_codex row uses NULL soul)\n"
                "  --energy               ULTRA-7: print reasoning/J demo table (no fixtures)\n",
                N_FIXTURES);
            return 0;
        }
    }

    if (energy_only) {
        if (cos_ultra_energy_metric_self_test() != 0) return 3;
        cos_ultra_energy_print_demo_table(stdout);
        return 0;
    }

    /* Baseline shared config. */
    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);
    cos_pipeline_config_t base;
    cos_sigma_pipeline_config_defaults(&base);
    base.tau_accept   = 0.40f;
    base.tau_rethink  = 0.60f;
    base.max_rethink  = 3;
    base.mode         = COS_PIPELINE_MODE_HYBRID;
    base.agent        = &ag;
    base.generate     = cos_cli_stub_generate;
    base.escalate     = cos_cli_stub_escalate;

    /* Optional Codex. */
    cos_sigma_codex_t codex;
    memset(&codex, 0, sizeof(codex));
    int have_codex = 0;
    if (codex_mode == 2) {
        have_codex = 0;
    } else {
        have_codex = (cos_sigma_codex_load(NULL, &codex) == 0);
        if (codex_mode == 1 && !have_codex) {
            fprintf(stderr,
                    "cos benchmark: --codex requires a loadable Codex "
                    "(see cos_sigma_codex_load)\n");
            return 1;
        }
    }

    bench_t b_bn = {.name = "bitnet_only"};
    bench_t b_nc = {.name = "pipeline_no_codex"};
    bench_t b_cx = {.name = "pipeline_codex"};
    bench_t b_ap = {.name = "api_only"};

    run_config(&b_bn, &base, NULL,      /* gate */ 0);
    run_config(&b_nc, &base, NULL,      /* gate */ 1);
    run_config(&b_cx, &base,
               have_codex ? &codex : NULL, /* gate */ 1);
    run_api_only(&b_ap);

    if (emit_comparison != NULL) {
        if (write_codex_comparison(emit_comparison, N_FIXTURES,
                                   &b_nc, &b_cx, have_codex,
                                   have_codex
                                       ? (unsigned long long)codex.hash_fnv1a64
                                       : 0ULL) != 0) {
            cos_sigma_codex_free(&codex);
            return 2;
        }
    }

    if (fmt_json) {
        printf("{\"fixtures\":%d,\"codex_loaded\":%s,"
               "\"codex_hash\":\"%016llx\","
               "\"configs\":[",
               N_FIXTURES,
               have_codex ? "true" : "false",
               have_codex ? (unsigned long long)codex.hash_fnv1a64 : 0ULL);
        const bench_t *all[] = { &b_bn, &b_nc, &b_cx, &b_ap };
        for (int i = 0; i < 4; ++i) {
            const bench_t *b = all[i];
            double acc = (b->n_total > 0)
                        ? (double)b->n_accept / b->n_total : 0.0;
            double cov = (b->n_total > 0)
                        ? 1.0 - (double)b->n_abstain / b->n_total : 0.0;
            printf("%s{\"name\":\"%s\",\"n\":%d,\"accept\":%d,"
                   "\"abstain\":%d,\"escalated\":%d,\"engram_hit\":%d,"
                   "\"rethink_total\":%d,"
                   "\"accept_rate\":%.4f,\"coverage\":%.4f,"
                   "\"mean_sigma\":%.4f,\"eur_total\":%.6f,"
                   "\"eur_per_call\":%.6f}",
                   i == 0 ? "" : ",",
                   b->name, b->n_total, b->n_accept, b->n_abstain,
                   b->n_escalated, b->n_engram_hit, b->n_rethink,
                   acc, cov,
                   b->n_total > 0 ? b->sum_sigma / b->n_total : 0.0,
                   b->sum_cost_eur,
                   b->n_total > 0 ? b->sum_cost_eur / b->n_total : 0.0);
        }
        printf("],"
               "\"codex_delta\":{\"d_accept\":%.4f,\"d_sigma\":%.4f,"
                                "\"d_cost_eur\":%.4f}}\n",
               (double)(b_cx.n_accept - b_nc.n_accept) / (double)N_FIXTURES,
               (b_cx.sum_sigma    - b_nc.sum_sigma)    / (double)N_FIXTURES,
               (b_cx.sum_cost_eur - b_nc.sum_cost_eur));
    } else {
        printf("cos benchmark — %d fixtures, codex=%s\n",
               N_FIXTURES, have_codex ? "loaded" : "off");
        printf(" %-22s | %5s | %4s | %8s | %9s | %5s | %3s | %3s | %3s | %3s\n",
               "config", "acc", "cov", "€total", "€/call", "σmean",
               "re", "esc", "hit", "abs");
        printf(" -----------------------+-------+------+----------+-----------+-------+-----+-----+-----+----\n");
        print_row(&b_bn);
        print_row(&b_nc);
        print_row(&b_cx);
        print_row(&b_ap);

        double d_acc = (double)(b_cx.n_accept - b_nc.n_accept) / (double)N_FIXTURES;
        double d_sig = (b_cx.sum_sigma - b_nc.sum_sigma) / (double)N_FIXTURES;
        printf("\n codex effect: Δaccept = %+.3f, Δmean_σ = %+.4f\n", d_acc, d_sig);
        if (b_ap.sum_cost_eur > 0.0) {
            double saved = 100.0 * (1.0 - b_cx.sum_cost_eur / b_ap.sum_cost_eur);
            printf(" vs api_only:  saved %.1f%% on the same fixture set\n", saved);
        }
        printf(" assert(declared == realized);  1 = 1\n");
    }

    cos_sigma_codex_free(&codex);
    return 0;
}
