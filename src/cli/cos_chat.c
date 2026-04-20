/* cos chat — interactive σ-gated REPL.
 *
 * Reads one prompt per line from stdin, runs it through the
 * sigma_pipeline orchestrator (with the Atlantean Codex loaded
 * as system prompt by default), and prints the response plus a
 * compact receipt:
 *
 *     > low:what is 2+2?
 *     a confident local answer (stub; real BitNet would fill this in).
 *     [σ=0.050 | FRESH | LOCAL | rethink=0 | €0.0001]
 *
 * Special commands:
 *   "exit" / "quit"   — end the session and print a summary
 *   "cost"            — print the current sovereign ledger
 *   "status"          — print config (τ, codex, mode, engram count)
 *
 * Flags:
 *   --no-codex         — start without loading the Codex
 *   --codex-seed       — load the compact seed form instead
 *   --codex-path PATH  — load a user-supplied system prompt
 *   --local-only       — never escalate to cloud
 *   --swarm            — escalate path uses swarm consensus (stub)
 *   --verbose          — append a one-liner diagnostic per turn
 *   --tau-accept F     — override τ_accept (default 0.40)
 *   --tau-rethink F    — override τ_rethink (default 0.60)
 *   --banner-only      — print the banner and exit (smoke-test hook)
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mode_label(cos_pipeline_mode_t m) {
    switch (m) {
        case COS_PIPELINE_MODE_LOCAL_ONLY: return "LOCAL_ONLY";
        case COS_PIPELINE_MODE_HYBRID:     return "HYBRID";
        case COS_PIPELINE_MODE_SWARM:      return "SWARM";
        default: return "?";
    }
}

static void print_banner(const cos_pipeline_config_t *cfg,
                         const cos_sigma_codex_t *codex) {
    fprintf(stdout,
        "Creation OS · σ-gated chat\n"
        "  1 = 1 — declared must equal realized\n"
        "  mode:   %s\n"
        "  codex:  %s%s\n"
        "  τ_acc:  %.2f   τ_rethink: %.2f   max_rethink: %d\n"
        "  type 'exit' to quit · 'cost' for ledger · 'status' for config\n"
        "─────────────────────────────────────────────────────────\n",
        mode_label(cfg->mode),
        codex != NULL ? "loaded" : "off",
        codex != NULL
            ? (codex->is_seed ? " (seed)" : " (full)")
            : "",
        (double)cfg->tau_accept, (double)cfg->tau_rethink,
        cfg->max_rethink);
    fflush(stdout);
}

static void print_cost(const cos_sigma_sovereign_t *s) {
    if (s == NULL) { fprintf(stdout, "  (no ledger)\n"); return; }
    float frac  = cos_sigma_sovereign_fraction(s);
    double eur  = s->eur_local_total + s->eur_cloud_total;
    double per  = cos_sigma_sovereign_eur_per_call(s);
    double mo30 = cos_sigma_sovereign_monthly_eur_estimate(s, 100);
    cos_sovereign_verdict_t v = cos_sigma_sovereign_verdict(s);
    const char *vs = (v == COS_SOVEREIGN_FULL) ? "FULL"
                   : (v == COS_SOVEREIGN_HYBRID) ? "HYBRID"
                   : "DEPENDENT";
    fprintf(stdout,
        "  ledger: %u local · %u cloud · %u abstain  (%.1f%% local, %s)\n"
        "          €%.4f total   €%.6f / call   €%.2f / mo @ 100 calls/day\n",
        s->n_local, s->n_cloud, s->n_abstain,
        (double)(frac * 100.0f), vs, eur, per, mo30);
    fflush(stdout);
}

static void print_status(const cos_pipeline_config_t *cfg,
                         const cos_sigma_engram_t   *engram,
                         const cos_sigma_codex_t    *codex) {
    fprintf(stdout,
        "  status:\n"
        "    mode       : %s\n"
        "    τ_accept   : %.4f\n"
        "    τ_rethink  : %.4f\n"
        "    max_rethink: %d\n"
        "    engram     : %u / %u slots (put=%u hit=%u miss=%u evict=%u)\n"
        "    codex      : %s%s  (chapters=%d bytes=%zu hash=%016llx)\n",
        mode_label(cfg->mode),
        (double)cfg->tau_accept, (double)cfg->tau_rethink,
        cfg->max_rethink,
        engram != NULL ? engram->count : 0,
        engram != NULL ? engram->cap   : 0,
        engram != NULL ? engram->n_put : 0,
        engram != NULL ? engram->n_get_hit : 0,
        engram != NULL ? engram->n_get_miss : 0,
        engram != NULL ? engram->n_evict : 0,
        codex != NULL ? "loaded" : "off",
        codex != NULL ? (codex->is_seed ? " (seed)" : " (full)") : "",
        codex != NULL ? codex->chapters_found : 0,
        codex != NULL ? codex->size : 0,
        codex != NULL ? (unsigned long long)codex->hash_fnv1a64 : 0ULL);
    fflush(stdout);
}

int main(int argc, char **argv) {
    int     use_codex     = 1;
    int     use_seed      = 0;
    const char *codex_path = NULL;
    int     local_only    = 0;
    int     swarm_mode    = 0;
    int     verbose       = 0;
    int     banner_only   = 0;
    float   tau_accept    = 0.40f;
    float   tau_rethink   = 0.60f;

    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--no-codex")   == 0) use_codex = 0;
        else if (strcmp(argv[i], "--codex-seed") == 0) { use_codex = 1; use_seed = 1; }
        else if (strcmp(argv[i], "--codex-path") == 0 && i + 1 < argc) codex_path = argv[++i];
        else if (strcmp(argv[i], "--local-only") == 0) local_only = 1;
        else if (strcmp(argv[i], "--swarm")      == 0) swarm_mode = 1;
        else if (strcmp(argv[i], "--verbose")    == 0) verbose = 1;
        else if (strcmp(argv[i], "--banner-only")== 0) banner_only = 1;
        else if (strcmp(argv[i], "--tau-accept") == 0 && i + 1 < argc)
            tau_accept = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--tau-rethink")== 0 && i + 1 < argc)
            tau_rethink = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stdout,
                "cos chat — σ-gated interactive inference\n"
                "  --no-codex          start without Codex as system prompt\n"
                "  --codex-seed        load the compact seed Codex form\n"
                "  --codex-path PATH   load a user-supplied system prompt\n"
                "  --local-only        never escalate to cloud\n"
                "  --swarm             cloud tier uses swarm consensus (stub)\n"
                "  --verbose           print a per-turn diagnostic\n"
                "  --tau-accept  F     override τ_accept (default 0.40)\n"
                "  --tau-rethink F     override τ_rethink (default 0.60)\n"
                "  --banner-only       print banner and exit (CI hook)\n"
                "  --help              this text\n");
            return 0;
        }
    }

    /* Codex load (optional). */
    cos_sigma_codex_t codex;
    memset(&codex, 0, sizeof(codex));
    int have_codex = 0;
    if (use_codex) {
        int rc;
        if (codex_path != NULL)     rc = cos_sigma_codex_load(codex_path, &codex);
        else if (use_seed)          rc = cos_sigma_codex_load_seed(&codex);
        else                        rc = cos_sigma_codex_load(NULL, &codex);
        if (rc == 0) have_codex = 1;
        else fprintf(stderr, "warning: codex load failed (rc=%d) — "
                            "continuing without soul\n", rc);
    }

    /* Shared state. */
    enum { N_SLOTS = 64 };
    cos_sigma_engram_entry_t slots[N_SLOTS];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    cos_sigma_engram_init(&engram, slots, N_SLOTS, 0.25f, 200, 20);

    cos_sigma_sovereign_t sv;
    cos_sigma_sovereign_init(&sv, 0.85f);
    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);

    cos_pipeline_config_t cfg;
    cos_sigma_pipeline_config_defaults(&cfg);
    cfg.tau_accept   = tau_accept;
    cfg.tau_rethink  = tau_rethink;
    cfg.max_rethink  = 3;
    cfg.mode         = local_only ? COS_PIPELINE_MODE_LOCAL_ONLY
                      : swarm_mode ? COS_PIPELINE_MODE_SWARM
                      : COS_PIPELINE_MODE_HYBRID;
    cfg.codex        = have_codex ? &codex : NULL;
    cfg.engram       = &engram;
    cfg.sovereign    = &sv;
    cfg.agent        = &ag;
    cfg.generate     = cos_cli_stub_generate;
    cfg.escalate     = cos_cli_stub_escalate;

    print_banner(&cfg, have_codex ? &codex : NULL);
    if (banner_only) {
        cos_sigma_pipeline_free_engram_values(&engram);
        cos_sigma_codex_free(&codex);
        return 0;
    }

    char line[2048];
    int turn = 0;
    for (;;) {
        fprintf(stdout, "\n> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) break;  /* EOF */

        /* strip trailing newline */
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) {
            line[--n] = '\0';
        }
        if (n == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strcmp(line, "cost")   == 0) { print_cost(&sv); continue; }
        if (strcmp(line, "status") == 0) { print_status(&cfg, &engram,
                                                        have_codex ? &codex : NULL);
                                            continue; }

        turn++;
        cos_pipeline_result_t r;
        cos_sigma_pipeline_run(&cfg, line, &r);

        fprintf(stdout, "%s\n", r.response);
        fprintf(stdout,
            "[σ=%.3f | %s | %s | rethink=%d | €%.4f]\n",
            (double)r.sigma,
            r.engram_hit ? "CACHE" : "FRESH",
            r.escalated  ? "CLOUD" : "LOCAL",
            r.rethink_count,
            r.cost_eur);
        if (verbose && r.diagnostic != NULL) {
            fprintf(stdout, "  diag: %s\n", r.diagnostic);
        }
    }

    fprintf(stdout, "\n─── Session summary ───\n");
    fprintf(stdout, "  turns: %d\n", turn);
    print_cost(&sv);
    fprintf(stdout, "  assert(declared == realized);\n  1 = 1.\n");

    cos_sigma_pipeline_free_engram_values(&engram);
    cos_sigma_codex_free(&codex);
    return 0;
}
