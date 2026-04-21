/* cos chat — interactive σ-gated REPL.
 *
 * Reads one prompt per line from stdin, runs it through the
 * sigma_pipeline orchestrator (with the Atlantean Codex loaded
 * as system prompt by default), and prints the response plus a
 * compact receipt:
 *
 *     > What is 2+2?
 *     4
 *     [σ=0.030 | FRESH | LOCAL | rethink=0 | €0.0000]
 *
 *     Second identical prompt → engram HIT → CACHE (same answer).
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
 *   --once             — one turn: requires --prompt (CI / headless)
 *   --prompt TEXT      — user line for --once
 *   --transcript PATH  — append one JSONL record (Python cos_chat shape)
 *   --no-transcript    — skip JSONL for --once
 *   --max-tokens N     — accepted for CLI parity; C path ignores today
 *
 * Optional local inference (BitNet-class subprocess):
 *
 *   CREATION_OS_BITNET_EXE=/path/to/bitnet-cli
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_persist.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"
#include "escalation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static void fprint_json_string(FILE *fp, const char *s) {
    fputc('"', fp);
    if (s != NULL) {
        for (; *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') {
                fputc('\\', fp);
                fputc((int)c, fp);
            } else if (c < 0x20u) {
                fprintf(fp, "\\u%04x", (unsigned)c);
            } else {
                fputc((int)c, fp);
            }
        }
    }
    fputc('"', fp);
}

static int run_chat_once(FILE *tout, cos_pipeline_config_t *cfg_rw,
                         const char *prompt) {
    clock_t t0 = clock();
    cos_pipeline_result_t r;
    if (cos_sigma_pipeline_run(cfg_rw, prompt, &r) != 0) {
        fprintf(stderr, "cos chat: pipeline_run failed\n");
        return 3;
    }
    clock_t t1 = clock();
    double elapsed_ms =
        (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
    if (elapsed_ms < 0.0) elapsed_ms = 0.0;

    const char *route = r.escalated ? "ESCALATE" : "LOCAL";
    fprintf(stdout,
            "round 0  %s  [σ_peak=%.2f action=%s route=%s]\n",
            r.response != NULL ? r.response : "",
            (double)r.sigma,
            cos_sigma_action_label(r.final_action),
            route);
    fprintf(stdout,
            "[σ=%.3f | %s | %s | rethink=%d | €%.4f]\n",
            (double)r.sigma,
            r.engram_hit ? "CACHE" : "FRESH",
            r.escalated ? "CLOUD" : "LOCAL",
            r.rethink_count,
            r.cost_eur);

    if (tout != NULL) {
        fprintf(tout, "{\"ts_ms\":%.3f,\"prompt\":",
                (double)time(NULL) * 1000.0);
        fprint_json_string(tout, prompt);
        fprintf(tout, ",\"rounds\":[{\"text\":");
        fprint_json_string(tout, r.response != NULL ? r.response : "");
        fprintf(tout,
                ",\"tokens\":1,\"elapsed_ms\":%.3f,"
                "\"sigma_peak\":%.6f,\"sigma_mean_avg\":%.6f,"
                "\"terminal_action\":\"%s\","
                "\"terminal_route\":\"%s\","
                "\"segments\":0,\"eos\":true}],\"final_text\":",
                elapsed_ms, (double)r.sigma, (double)r.sigma,
                cos_sigma_action_label(r.final_action),
                route);
        fprint_json_string(tout, r.response != NULL ? r.response : "");
        fprintf(tout, "}\n");
        fflush(tout);
    }
    return 0;
}

int main(int argc, char **argv) {
    int     use_codex     = 1;
    int     use_seed      = 0;
    const char *codex_path = NULL;
    int     local_only    = 0;
    int     swarm_mode    = 0;
    int     verbose       = 0;
    int     banner_only   = 0;
    int     once_mode     = 0;
    const char *once_prompt = NULL;
    const char *transcript_path = NULL;
    int     no_transcript = 0;
    int     have_tau_accept = 0;
    int     have_tau_rethink = 0;
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
        else if (strcmp(argv[i], "--once")       == 0) once_mode = 1;
        else if (strcmp(argv[i], "--prompt")     == 0 && i + 1 < argc)
            once_prompt = argv[++i];
        else if (strcmp(argv[i], "--transcript") == 0 && i + 1 < argc)
            transcript_path = argv[++i];
        else if (strcmp(argv[i], "--no-transcript") == 0) no_transcript = 1;
        else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) ++i; /* parity */
        else if (strcmp(argv[i], "--tau-accept") == 0 && i + 1 < argc) {
            tau_accept = (float)atof(argv[++i]);
            have_tau_accept = 1;
        } else if (strcmp(argv[i], "--tau-rethink")== 0 && i + 1 < argc) {
            tau_rethink = (float)atof(argv[++i]);
            have_tau_rethink = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stdout,
                "cos chat — σ-gated interactive inference\n"
                "  --no-codex          start without Codex as system prompt\n"
                "  --codex-seed        load the compact seed Codex form\n"
                "  --codex-path PATH   load a user-supplied system prompt\n"
                "  --local-only        never escalate to cloud\n"
                "  --swarm             cloud tier uses swarm consensus (stub)\n"
                "  --verbose           print a per-turn diagnostic\n"
                "  --tau-accept  F     override τ_accept (default 0.40 interactive,\n"
                "                      0.30 for --once to match Python harness)\n"
                "  --tau-rethink F     override τ_rethink (default 0.60 / 0.70)\n"
                "  --once              one headless turn (needs --prompt)\n"
                "  --prompt TEXT       prompt for --once\n"
                "  --transcript PATH   JSONL transcript for --once\n"
                "  --no-transcript     skip JSONL for --once\n"
                "  --max-tokens N      accepted for argv parity (ignored)\n"
                "  --banner-only       print banner and exit (CI hook)\n"
                "  --help              this text\n"
                "\n"
                "  CREATION_OS_BITNET_EXE  optional path to local inference binary\n"
                "                          (see src/import/bitnet_spawn.h).\n");
            return 0;
        }
    }

    if (once_mode) {
        if (!have_tau_accept)  tau_accept  = 0.30f;
        if (!have_tau_rethink) tau_rethink = 0.70f;
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

    /* DEV-3: rehydrate engram from ~/.cos/engram.db (default; override
     * with COS_ENGRAM_DB=/abs/path or COS_ENGRAM_DISABLE=1).  The
     * write-through hook is installed into cfg below.  We open the
     * handle unconditionally so that even transient --once runs
     * benefit from cross-session cache hits ("What is 2+2?" answered
     * once, cached forever). */
    cos_engram_persist_t *persist = NULL;
    const char *disable_persist = getenv("COS_ENGRAM_DISABLE");
    if (disable_persist == NULL || disable_persist[0] != '1') {
        if (cos_engram_persist_open(NULL, &persist) == 0) {
            /* N_SLOTS=64, so cap the rehydrate to match — extra rows
             * would just evict each other during load. */
            int n = cos_engram_persist_load(persist, &engram, N_SLOTS);
            if (verbose && n > 0) {
                fprintf(stderr, "engram: loaded %d row%s from %s\n",
                        n, n == 1 ? "" : "s",
                        cos_engram_persist_path(persist));
            }
        }
        /* On open failure we silently continue in-memory-only; a
         * broken ~/.cos/ should never brick the CLI. */
    }

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
    cfg.generate     = cos_cli_chat_generate;
    /* DEV-6: thread the loaded Codex through to the BitNet backend
     * via generate_ctx.  cos_cli_chat_generate reads
     * (const cos_sigma_codex_t *)ctx->bytes and installs it as the
     * /v1/chat/completions `system` message (for the blocking path)
     * or the manually-prepended prefix (for the streaming /completion
     * path).  When --no-codex is passed, ctx is NULL and the backend
     * runs with no system prompt. */
    cfg.generate_ctx = have_codex ? (void *)&codex : NULL;
    /* DEV-5: dispatch via the API escalation module.  When
     * CREATION_OS_ESCALATION_PROVIDER + matching API key are set in
     * the environment, this reaches out to Claude/OpenAI/DeepSeek
     * over HTTPS and appends a (student → teacher) distill pair to
     * ~/.cos/distill_pairs.jsonl.  With no provider configured,
     * cos_cli_escalate_api falls through to cos_cli_stub_escalate
     * so CI and offline sessions keep the deterministic shape. */
    cfg.escalate     = cos_cli_escalate_api;
    if (persist != NULL) {
        cfg.on_engram_store     = cos_engram_persist_store;
        cfg.on_engram_store_ctx = persist;
    }

    if (banner_only) {
        print_banner(&cfg, have_codex ? &codex : NULL);
        cos_sigma_pipeline_free_engram_values(&engram);
        cos_sigma_codex_free(&codex);
        cos_engram_persist_close(persist);
        return 0;
    }

    if (once_mode) {
        if (once_prompt == NULL || once_prompt[0] == '\0') {
            fprintf(stderr, "cos chat: --once requires --prompt\n");
            cos_sigma_pipeline_free_engram_values(&engram);
            cos_sigma_codex_free(&codex);
            cos_engram_persist_close(persist);
            return 2;
        }
        const char *exe = getenv("CREATION_OS_BITNET_EXE");
        const char *backend =
            (exe != NULL && exe[0] != '\0') ? "bridge" : "stub";
        fprintf(stdout,
                "cos chat  ·  backend=%s  ·  τ_accept=%g  τ_rethink=%g\n",
                backend, (double)tau_accept, (double)tau_rethink);
        fflush(stdout);

        FILE *tout = NULL;
        if (!no_transcript) {
            const char *p = transcript_path;
            if (p == NULL || p[0] == '\0')
                p = getenv("COS_CHAT_TRANSCRIPT");
            if (p == NULL || p[0] == '\0')
                p = "./cos_chat_transcript.jsonl";
            tout = fopen(p, "a");
            if (tout == NULL) {
                fprintf(stderr, "cos chat: cannot open transcript: %s\n", p);
                cos_sigma_pipeline_free_engram_values(&engram);
                cos_sigma_codex_free(&codex);
                cos_engram_persist_close(persist);
                return 4;
            }
        }
        int rc = run_chat_once(tout, &cfg, once_prompt);
        if (tout != NULL)
            fclose(tout);
        cos_sigma_pipeline_free_engram_values(&engram);
        cos_sigma_codex_free(&codex);
        cos_engram_persist_close(persist);
        return rc;
    }

    print_banner(&cfg, have_codex ? &codex : NULL);

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
    cos_engram_persist_close(persist);
    return 0;
}
