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
 *   --stream           — stream tokens from llama-server (default: ON in REPL)
 *   --no-stream        — wait for full response (default for --once)
 *   --tui              — ANSI live layout (default on interactive TTY)
 *   --no-tui           — plain transcript (also for piped stdin)
 *   --spec | --speculative-decode — dual-model draft (2B) then verify (9B) by σ
 *   --prompt TEXT      — user line for --once
 *   --transcript PATH  — append one JSONL record (Python cos_chat shape)
 *   --no-transcript    — skip JSONL for --once
 *   --max-tokens N     — accepted for CLI parity; C path ignores today
 *   --tools            — σ-gated shell tool REPL (HORIZON-1; no LLM)
 *   --tools-self-test  — 20-case classifier regression (exit 0/1)
 *   --tools-dry-run    — gate only, never exec /bin/sh
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
#include "inplace_ttt.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"
#include "escalation.h"

/* FINAL-2: σ-native cognitive stack wired into cos chat.
 *   Phase A: σ_combined (SCI-5 multi-σ ensemble shadow)
 *   Phase B: conformal τ (SCI-1) auto-loaded from ~/.cos/calibration.json
 *   Phase C: meta-cognitive σ per turn (ULTRA-5) under --verbose
 *   Phase D: per-session coherence summary (ULTRA-9) on exit
 */
#include "conformal.h"
#include "multi_sigma.h"
#include "text_similarity.h"
#include "introspection.h"
#include "coherence.h"
#include "sigma_tools.h"
#include "state_ledger.h"
#include "adaptive_tau.h"
#include "error_attribution.h"
#include "engram_episodic.h"
#include "ttt_runtime.h"

#include "inference_cache.h"
#include "knowledge_graph.h"
#include "speculative_sigma.h"
#include "spike_engine.h"
#include "adaptive_compute.h"
#include "proof_receipt.h"
#include "constitution.h"
#include "license_attest.h"
#include "speed_metrics.h"
#include "semantic_entropy.h"
#include "semantic_sigma.h"
#include "response_cache.h"
#include "cross_model_sigma.h"
#include "model_cascade.h"
#include "semantic_cache.h"
#include "bitnet_server.h"
#include "ollama_detect.h"
#include "text_similarity.h"
#include "cos_tui.h"
#include "speculative_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>

/** Wall time for throughput (preferred over CPU `clock()` on multicore). */
static double chat_wall_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

/** COS_BITNET_STREAM=1 → tokens printed live in stub_gen; skip duplicate body. */
static int cos_chat_stream_env_on(void)
{
    const char *s = getenv("COS_BITNET_STREAM");
    return (s != NULL && s[0] == '1');
}
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdint.h>

#ifndef COS_CHAT_VERSION_STRING
#define COS_CHAT_VERSION_STRING "v3.0.0"
#endif

static volatile sig_atomic_t g_cos_chat_sigint = 0;
static void (*g_cos_chat_prev_sigint)(int) = SIG_DFL;

/** Wall ms spent in semantic_entropy / semantic_sigma HTTP this turn. */
static double g_cos_chat_semantic_phase_ms;

static long  g_cos_chat_sess_tokens;
static int   g_cos_chat_sess_queries;
static int   g_cos_chat_tok_ring[24];
static int   g_cos_chat_tok_ring_n;

static const char *chat_spark_utf8(double u01)
{
    static const char *blk[8] = {
        "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
        "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
    };
    int idx;
    if (!(u01 == u01) || u01 < 0.)
        u01 = 0.;
    if (u01 > 1.)
        u01 = 1.;
    idx = (int)(u01 * 7.999);
    if (idx < 0)
        idx = 0;
    if (idx > 7)
        idx = 7;
    return blk[idx];
}

static void cos_chat_token_session_note(int tokens_per_turn)
{
    if (tokens_per_turn < 1)
        tokens_per_turn = 1;
    g_cos_chat_sess_tokens += (long)tokens_per_turn;
    g_cos_chat_sess_queries++;
    if (g_cos_chat_tok_ring_n < 24)
        g_cos_chat_tok_ring[g_cos_chat_tok_ring_n++] = tokens_per_turn;
    else {
        memmove(g_cos_chat_tok_ring, g_cos_chat_tok_ring + 1,
                sizeof(int) * 23u);
        g_cos_chat_tok_ring[23] = tokens_per_turn;
    }

    {
        const char *en = getenv("COS_CHAT_TOKEN_JSONL");
        const char *home;
        char        dir[512], path[640];
        FILE       *fp;
        struct timespec ts;
        long long   ms_wall = 0;

        if (en == NULL || en[0] != '1' || en[1] != '\0')
            return;
        home = getenv("HOME");
        if (home == NULL || home[0] == '\0')
            return;
        if (snprintf(dir, sizeof dir, "%s/.cos/logs", home) >= (int)sizeof dir)
            return;
#if defined(__unix__) || defined(__APPLE__)
        mkdir(dir, 0700);
#endif
        if (snprintf(path, sizeof path, "%s/chat_tokens.jsonl", dir)
            >= (int)sizeof path)
            return;
        fp = fopen(path, "a");
        if (fp == NULL)
            return;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
            ms_wall = (long long)ts.tv_sec * 1000LL
                    + (long long)ts.tv_nsec / 1000000LL;
        fprintf(fp,
                "{\"t\":%lld,\"tokens_turn\":%d,\"queries\":%d,"
                "\"total_tokens\":%ld,\"avg_tokens\":%.4f}\n",
                ms_wall, tokens_per_turn, g_cos_chat_sess_queries,
                g_cos_chat_sess_tokens,
                (double)g_cos_chat_sess_tokens
                    / (double)g_cos_chat_sess_queries);
        fclose(fp);
    }
}

static void cos_chat_token_trend_footer(FILE *out)
{
    int i, n, first = 0, last = 0, nf, nl;

    if (g_cos_chat_sess_queries <= 0 || out == NULL)
        return;
    fprintf(out, "  tokens/query (session): %.1f\n",
            (double)g_cos_chat_sess_tokens / (double)g_cos_chat_sess_queries);
    n = g_cos_chat_tok_ring_n;
    if (n < 4)
        return;
    nf = n / 2;
    nl = n - nf;
    for (i = 0; i < nf; i++)
        first += g_cos_chat_tok_ring[i];
    for (; i < n; i++)
        last += g_cos_chat_tok_ring[i];
    first = (nf > 0) ? first / nf : 0;
    last  = (nl > 0) ? last / nl : 0;
    fputs("  Tokens/query trend: ", out);
    for (i = 0; i < n; i++) {
        double u = (double)g_cos_chat_tok_ring[i] / 512.0;
        if (u < 0.)
            u = 0.;
        if (u > 1.)
            u = 1.;
        fputs(chat_spark_utf8(u), out);
    }
    if (last < first - 2)
        fputs(" [DECLINING ✓]\n", out);
    else if (last > first + 2)
        fputs(" [RISING]\n", out);
    else
        fputs(" [STABLE]\n", out);
}

static void cos_chat_sigint_handler(int signo)
{
    (void)signo;
    g_cos_chat_sigint = 1;
    cos_bitnet_server_io_cancel_request();
}

static void cos_chat_jsonl_turn(const char *prompt_line,
                                const cos_pipeline_result_t *r,
                                double elapsed_ms,
                                int sigma_drift,
                                int tokens_turn)
{
    const char *en = getenv("COS_CHAT_JSONL_LOG");
    if (en == NULL || en[0] != '1' || prompt_line == NULL || r == NULL)
        return;
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return;
    char dir[512], path[640];
    if (snprintf(dir, sizeof dir, "%s/.cos/logs", home) >= (int)sizeof dir)
        return;
#if defined(__unix__) || defined(__APPLE__)
    mkdir(dir, 0700);
#endif
    if (snprintf(path, sizeof path, "%s/cos_chat.jsonl", dir) >= (int)sizeof path)
        return;
    FILE *fp = fopen(path, "a");
    if (fp == NULL)
        return;
    struct timespec ts;
    long long ms_wall = 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        ms_wall = (long long)ts.tv_sec * 1000LL
                + (long long)ts.tv_nsec / 1000000LL;
    uint64_t ph = cos_sigma_engram_hash(prompt_line);
    const char *route = getenv("COS_SPEC_DECODE_ROUTE");
    if (route == NULL || route[0] == '\0')
        route = r->escalated ? "CLOUD" : "LOCAL";
    const char *act = "ABSTAIN";
    if (r->final_action == COS_SIGMA_ACTION_ACCEPT)
        act = "ACCEPT";
    else if (r->final_action == COS_SIGMA_ACTION_RETHINK)
        act = "RETHINK";
    const char *model = getenv("COS_OLLAMA_MODEL");
    if (model == NULL || model[0] == '\0')
        model = getenv("COS_BITNET_CHAT_MODEL");
    if (model == NULL || model[0] == '\0')
        model = "default";
    fprintf(fp,
            "{\"t\":%lld,\"prompt_hash\":%llu,\"sigma\":%.4f,"
            "\"action\":\"%s\",\"route\":\"%s\",\"model\":\"%s\","
            "\"latency_ms\":%.0f,\"tokens\":%d,\"cost_eur\":%.6f,"
            "\"sigma_drift\":%d,"
            "\"degradation_level\":0,\"retries\":0,\"error\":null}\n",
            ms_wall, (unsigned long long)ph, (double)r->sigma, act, route,
            model, elapsed_ms, tokens_turn, r->cost_eur, sigma_drift);
    fclose(fp);
}

/* Mirrors pipeline.c ABSTAIN_TEXT for speculative early-abstain path only. */
static const char COS_CHAT_PIPELINE_ABSTAIN[] =
    "I am uncertain and cannot answer this locally "
    "(σ above threshold; escalation not permitted).";

/* When the σ pipeline cannot run (missing weights, server down, etc.),
 * print next steps — never leave the user at a bare errno-style line. */
static void chat_emit_pipeline_failure_help(void)
{
    int tty = isatty(fileno(stderr));
    if (!tty) {
        fputs(
            "\ncos chat: model or inference server unavailable.\n"
            "  To use live inference, run:\n"
            "    ./scripts/install.sh\n"
            "  To see a demo without a model:\n"
            "    ./cos demo\n"
            "  External llama-server:\n"
            "    export COS_BITNET_SERVER_EXTERNAL=1\n"
            "    export COS_BITNET_SERVER_PORT=8088\n\n",
            stderr);
        return;
    }
    fputs(
        "\n"
        "\u256d\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u256e\n"
        "\u2502 Model not found or inference server unavailable.        \u2502\n"
        "\u2502                                                            \u2502\n"
        "\u2502 To use live inference, run:                                \u2502\n"
        "\u2502   ./scripts/install.sh                                     \u2502\n"
        "\u2502                                                            \u2502\n"
        "\u2502 To see a demo without a model:                             \u2502\n"
        "\u2502   ./cos demo                                               \u2502\n"
        "\u2502                                                            \u2502\n"
        "\u2502 External llama-server (example env):                       \u2502\n"
        "\u2502   export COS_BITNET_SERVER_EXTERNAL=1                    \u2502\n"
        "\u2502   export COS_BITNET_SERVER_PORT=8088                     \u2502\n"
        "\u2570\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u256f\n",
        stderr);
}

/* NEXT-1 polish: backend detection + one-line action decorator. */
static const char *cos_chat_backend_label(void) {
    const char *exe = getenv("CREATION_OS_BITNET_EXE");
    if (exe != NULL && exe[0] != '\0')
        return "bridge · BitNet b1.58 2B · Local · CPU";
    if (cos_cli_use_bitnet_http() != 0)
        return "external · OpenAI-compat HTTP (Ollama / llama-server)";
    return "stub · deterministic (no weights)";
}

/* AGI-1: SQLite engram → few-shot prefix (ICL as TTT proxy). */
static int chat_icl_compose(void *icl_ctx, uint64_t exclude_prompt_hash,
                            float exemplar_max_sigma, int k_shots,
                            const char *user_prompt,
                            char *out, size_t out_cap) {
    cos_engram_persist_t *p = (cos_engram_persist_t *)icl_ctx;
    return cos_inplace_ttt_compose_from_engram(
        p, exclude_prompt_hash, exemplar_max_sigma, k_shots,
        user_prompt, out, out_cap);
}

/* ----------------------------------------------------------------------
 * FINAL-2 Phase B: load conformal τ from ~/.cos/calibration.json.
 *
 * Returns 0 on success and fills *out_tau / *out_alpha / *out_domain.
 * Returns -1 when no valid report is available (caller keeps static τ).
 * ---------------------------------------------------------------------- */
static int calibration_default_path(char *buf, size_t cap) {
    const char *env = getenv("COS_CALIBRATION");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') return -1;
    snprintf(buf, cap, "%s/.cos/calibration.json", home);
    return 0;
}

static int load_conformal_tau(const char *path,
                              float *out_tau,
                              float *out_alpha,
                              char  *out_domain, size_t domain_cap) {
    cos_conformal_bundle_t b;
    if (cos_conformal_read_bundle_json(path, &b) != 0) return -1;
    /* Report index 0 is "all"; prefer it when valid. */
    for (int i = 0; i < b.n_reports; ++i) {
        const cos_conformal_report_t *r = &b.reports[i];
        if (!r->valid) continue;
        if (out_tau)    *out_tau   = r->tau;
        if (out_alpha)  *out_alpha = r->alpha;
        if (out_domain) snprintf(out_domain, domain_cap, "%s", r->domain);
        return 0;
    }
    return -1;
}

/* ----------------------------------------------------------------------
 * FINAL-2 Phase A: σ_combined shadow ensemble.
 *
 * The scalar σ from the pipeline is treated as σ_logprob.  We call the
 * same local generator K_REGEN extra times (fresh round sampling) to
 * compute σ_consistency over the K+1 regenerations.  σ_perplexity is a
 * sequence-level smoothing of σ_logprob (kept as r.sigma here — when a
 * real backend supplies per-token logprobs this becomes the mean-logprob
 * derivation in multi_sigma.c).  σ_entropy falls back to the scalar in
 * the absence of top-k logprobs; it is still a bounded-in-[0,1] signal
 * so the ensemble stays in [0,1].
 *
 * This is a diagnostic shadow: gating still uses the pipeline scalar σ.
 * When logprob streams become available (llama-server already emits
 * them — see DEV-1 + bitnet_server.c) the per-token primitives will
 * feed straight into the same cos_multi_sigma_combine() call.
 * ---------------------------------------------------------------------- */
#define COS_CHAT_MULTI_K_REGEN 2  /* total regenerations = K_REGEN + 1 */
#define COS_CHAT_MULTI_TEXT_CAP 2048
#define COS_CHAT_SEM_SHADOW_MAX 5

typedef struct {
    cos_multi_sigma_t ens;
    int   have;
    int   k_used;
} chat_multi_t;

static void chat_metacog_levels(const char *prompt,
                                const cos_pipeline_result_t *r,
                                int max_rethink,
                                cos_ultra_metacog_levels_t *lv);

static int chat_multi_shadow(cos_pipeline_config_t *cfg,
                             int http_sem_triple,
                             const char *input,
                             const cos_pipeline_result_t *r_meta,
                             const char *primary_text,
                             float primary_sigma,
                             chat_multi_t *out) {
    if (out == NULL || cfg == NULL) return -1;
    memset(out, 0, sizeof(*out));

    int   n_lp_snap = 0;
    float lp_sig_snap[8];
    (void)cos_bitnet_server_copy_last_token_sigmas(lp_sig_snap, 8,
                                                  &n_lp_snap);
    int has_lp = (n_lp_snap > 0);

    float sigma_meta = 0.10f;
    if (r_meta != NULL && input != NULL) {
        cos_ultra_metacog_levels_t lv;
        chat_metacog_levels(input, r_meta, cfg->max_rethink, &lv);
        sigma_meta =
            0.25f
            * (lv.sigma_perception + lv.sigma_self + lv.sigma_social
               + lv.sigma_situational);
        if (sigma_meta < 0.0f) sigma_meta = 0.0f;
        if (sigma_meta > 1.0f) sigma_meta = 1.0f;
    }

    const int    K = COS_CHAT_MULTI_K_REGEN;
    const char *texts[COS_CHAT_SEM_SHADOW_MAX];
    char         copies[COS_CHAT_SEM_SHADOW_MAX][COS_CHAT_MULTI_TEXT_CAP];
    int          n = 0;

    int want_http_triple =
        (http_sem_triple != 0) && (cos_cli_use_bitnet_http() != 0)
        && (input != NULL);
    float sigma_lp_triple = -1.0f;
    int   triple_fail_null = 0; /* ti>0: transport/timeout or empty body */

    if (want_http_triple) {
        const char *sysp =
            (cfg->codex != NULL && cfg->codex->bytes != NULL
             && cfg->codex->size > 0)
                ? cfg->codex->bytes
                : NULL;
        static const char k_shadow_follow_sysp[] =
            "Give only the direct answer. Maximum one sentence. No explanation.";
        const float temps[3] = { 0.3f, 0.1f, 1.5f };
        enum { k_shadow_np = 60 };
        n = 0;
        for (int ti = 0; ti < 3 && n < COS_CHAT_SEM_SHADOW_MAX; ++ti) {
            if (ti == 0) {
                cos_bitnet_server_params_t pp;
                memset(&pp, 0, sizeof(pp));
                pp.n_predict     = k_shadow_np;
                pp.n_probs       = 3;
                pp.seed          = 700 + ti * 7919;
                pp.temperature   = temps[ti];
                pp.system_prompt = sysp;
                cos_bitnet_server_result_t rr;
                memset(&rr, 0, sizeof(rr));
                if (cos_bitnet_server_complete(input, &pp, &rr) == 0
                    && rr.text != NULL && rr.text[0] != '\0') {
                    snprintf(copies[n], sizeof(copies[n]), "%s", rr.text);
                    texts[n] = copies[n];
                    sigma_lp_triple = rr.sigma;
                    n++;
                }
            } else {
                char *tx = cos_bitnet_query_temp_with_options(
                    0, input, k_shadow_follow_sysp, temps[ti], k_shadow_np);
                if (tx != NULL) {
                    if (tx[0] != '\0') {
                        snprintf(copies[n], sizeof(copies[n]), "%s", tx);
                        texts[n] = copies[n];
                        n++;
                    } else
                        triple_fail_null = 1;
                    free(tx);
                } else
                    triple_fail_null = 1;
            }
        }
        (void)cos_bitnet_server_copy_last_token_sigmas(lp_sig_snap, 8,
                                                      &n_lp_snap);
        has_lp = (n_lp_snap > 0);
    } else {
        if (primary_text != NULL) {
            snprintf(copies[n], sizeof(copies[n]), "%s", primary_text);
            texts[n] = copies[n];
            n++;
        }
        for (int k = 0; k < K; ++k) {
            if (cfg->generate == NULL) break;
            const char *t = NULL;
            float       s = 1.0f;
            double      c = 0.0;
            if (cfg->generate(input, k + 1, cfg->generate_ctx,
                              &t, &s, &c) != 0
                || t == NULL)
                break;
            snprintf(copies[n], sizeof(copies[n]), "%s", t);
            texts[n] = copies[n];
            n++;
        }
    }

    float sigma_consistency = 0.0f;
    if (want_http_triple && !triple_fail_null && n == 3) {
        char fs0[512], fs1[512], fs2[512];
        cos_text_first_sentence(texts[0], fs0, sizeof fs0);
        cos_text_first_sentence(texts[1], fs1, sizeof fs1);
        cos_text_first_sentence(texts[2], fs2, sizeof fs2);
        float s01 = cos_text_jaccard(fs0, fs1);
        float s02 = cos_text_jaccard(fs0, fs2);
        float s12 = cos_text_jaccard(fs1, fs2);
        float mean_sim = (s01 + s02 + s12) / 3.0f;
        float sigma_sem = 1.0f - mean_sim;
        if (mean_sim > 0.95f)
            sigma_sem = 0.0f;
        else if (mean_sim < 0.05f)
            sigma_sem = 0.95f;
        sigma_consistency = sigma_sem;
    } else if (want_http_triple && !triple_fail_null && n == 2) {
        char fa[512], fb[512];
        cos_text_first_sentence(texts[0], fa, sizeof fa);
        cos_text_first_sentence(texts[1], fb, sizeof fb);
        float s01      = cos_text_jaccard(fa, fb);
        float mean_sim = s01;
        float sigma_sem = 1.0f - mean_sim;
        if (mean_sim > 0.95f)
            sigma_sem = 0.0f;
        else if (mean_sim < 0.05f)
            sigma_sem = 0.95f;
        sigma_consistency = sigma_sem;
    } else if (n >= 2) {
        sigma_consistency = cos_multi_sigma_consistency(texts, n);
    }
    if (sigma_consistency < 0.0f) sigma_consistency = 0.0f;

    float s_lp = primary_sigma;
    if (want_http_triple && n >= 1 && sigma_lp_triple >= 0.0f)
        s_lp = sigma_lp_triple;
    if (s_lp < 0.0f) s_lp = 0.0f;
    if (s_lp > 1.0f) s_lp = 1.0f;

    if (want_http_triple && triple_fail_null) {
        /* Timeout / transport / empty follow-up: shadow = logprob only. */
        out->ens.sigma_logprob     = s_lp;
        out->ens.sigma_entropy     = s_lp;
        out->ens.sigma_perplexity  = s_lp;
        out->ens.sigma_consistency = s_lp;
        out->ens.sigma_combined    = s_lp;
    } else if (want_http_triple && n >= 2) {
        float s_sem = sigma_consistency;
        if (s_sem < 0.0f) s_sem = 0.0f;
        float blend = 0.7f * s_sem + 0.3f * s_lp;
        if (blend < 0.0f) blend = 0.0f;
        if (blend > 1.0f) blend = 1.0f;
        out->ens.sigma_logprob     = s_lp;
        out->ens.sigma_entropy     = s_lp;
        out->ens.sigma_perplexity  = s_lp;
        out->ens.sigma_consistency = s_sem;
        out->ens.sigma_combined    = blend;
    } else {
        if (cos_multi_sigma_combine(s_lp, s_lp, s_lp,
                                    sigma_consistency, NULL, &out->ens)
            != 0)
            return -1;
    }

    out->have   = 1;
    out->k_used = n;
    (void)sigma_meta;
    (void)has_lp; /* retained for future weighting vs token logprobs */
    return 0;
}

/* ----------------------------------------------------------------------
 * FINAL-2 Phase C: per-turn meta-cognitive σ (ULTRA-5).
 *
 * Channel mapping (heuristic until dedicated probes land):
 *   σ_perception  — short prompts (≤ 3 words) get a bump: we do not have
 *                   enough context to read the user's intent cleanly.
 *   σ_self        — the pipeline's scalar σ for this turn.
 *   σ_social      — high when the prompt looks like it needs clarification
 *                   (trailing '?' + extreme brevity, or pronouns without
 *                   antecedents).  Conservative default 0.
 *   σ_situational — rethink_count / max(max_rethink, 1); grows as the
 *                   loop approaches its budget.
 * ---------------------------------------------------------------------- */
static int count_words(const char *s) {
    int n = 0, in = 0;
    for (; s && *s; ++s) {
        int w = (*s > ' ');
        if (w && !in) { n++; in = 1; }
        else if (!w)  { in = 0; }
    }
    return n;
}

static void chat_metacog_levels(const char *prompt,
                                const cos_pipeline_result_t *r,
                                int max_rethink,
                                cos_ultra_metacog_levels_t *lv) {
    memset(lv, 0, sizeof(*lv));
    int words = count_words(prompt);
    lv->sigma_perception = (words <= 3) ? 0.35f : 0.05f;
    lv->sigma_self       = (r != NULL) ? r->sigma : 1.0f;
    lv->sigma_social     = 0.0f;
    if (prompt != NULL) {
        size_t L = strlen(prompt);
        if (L > 0 && prompt[L-1] == '?' && words <= 4) {
            lv->sigma_social = 0.45f;  /* "who? what? how?"-class */
        }
    }
    int budget = max_rethink > 0 ? max_rethink : 1;
    lv->sigma_situational =
        (r != NULL) ? ((float)r->rethink_count / (float)budget) : 0.0f;
    if (lv->sigma_situational > 1.0f) lv->sigma_situational = 1.0f;
}

/* ----------------------------------------------------------------------
 * FINAL-2 Phase D: per-session coherence window (ULTRA-9).
 *
 * We keep a ring of the last COS_CHAT_COH_CAP σ samples; on session end
 * we compute the mean over the window and dσ/dt as the simple
 * (last - first) / max(1, n-1) slope per turn.  The report uses a
 * nominal K=2 capacity ceiling (BitNet-class local) and K_crit=1.0 so
 * degradation trips DRIFTING before AT_RISK for demo purposes.
 * ---------------------------------------------------------------------- */
#define COS_CHAT_COH_CAP 32
typedef struct {
    float samples[COS_CHAT_COH_CAP];
    int   n;
    int   head;
} chat_coh_t;

static void coh_reset(chat_coh_t *c) {
    memset(c, 0, sizeof(*c));
}

static void coh_push(chat_coh_t *c, float sigma) {
    if (c == NULL) return;
    c->samples[c->head] = sigma;
    c->head = (c->head + 1) % COS_CHAT_COH_CAP;
    if (c->n < COS_CHAT_COH_CAP) c->n += 1;
}

static void coh_summary(const chat_coh_t *c,
                        float *out_mean, float *out_slope) {
    if (out_mean)  *out_mean  = 0.0f;
    if (out_slope) *out_slope = 0.0f;
    if (c == NULL || c->n == 0) return;
    double acc = 0.0;
    /* Reconstruct ordered samples: samples in insertion order from
     * (head - n) mod cap up through (head - 1). */
    int start = (c->head - c->n + COS_CHAT_COH_CAP) % COS_CHAT_COH_CAP;
    float first = c->samples[start];
    float last  = c->samples[(c->head - 1 + COS_CHAT_COH_CAP) %
                             COS_CHAT_COH_CAP];
    for (int i = 0; i < c->n; ++i) {
        acc += (double)c->samples[(start + i) % COS_CHAT_COH_CAP];
    }
    if (out_mean)  *out_mean = (float)(acc / (double)c->n);
    if (out_slope) *out_slope =
        (c->n > 1) ? (last - first) / (float)(c->n - 1) : 0.0f;
}

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
    /* Strict substrings required by benchmarks/sigma_pipeline/check_cos_cli.sh:
     *   "Creation OS · σ-gated chat", "1 = 1",
     *   "codex:  loaded" (2 spaces) OR "codex:  off" (2 spaces).
     * All preserved inside the boxed NEXT-1 polish layout. */
    const char *backend = cos_chat_backend_label();
    fprintf(stdout,
        "┌─ Creation OS · σ-gated chat · %s · ULTRA ─┐\n"
        "   1 = 1 — declared must equal realized\n"
        "   mode   : %s\n"
        "   backend: %s\n"
        "   codex:  %s%s\n"
        "   σ-gate : τ_acc=%.2f · τ_rethink=%.2f · max_rethink=%d\n",
        COS_CHAT_VERSION_STRING,
        mode_label(cfg->mode),
        backend,
        codex != NULL ? "loaded" : "off",
        codex != NULL ? (codex->is_seed ? " (seed)" : " (full)") : "",
        (double)cfg->tau_accept, (double)cfg->tau_rethink,
        cfg->max_rethink);
    if (cfg->engram != NULL) {
        fprintf(stdout,
            "   engram : %u entries (persist: ~/.cos/engram.db)\n",
            cfg->engram->count);
    }
    fprintf(stdout,
        "   cost   : €0.0000 this session · local-first\n"
        "   type 'exit' · 'cost' · 'status' · 'help'\n"
        "└────────────────────────────────────────────────────┘\n");
    fflush(stdout);
}

/* NEXT-1: polished per-turn receipt.
 *
 * Encodes every terminal state into a single visually-scannable line
 * (plus an advisory when ABSTAIN).  Preserves the ACCEPT/RETHINK/ABSTAIN
 * substring required by benchmarks/sigma_pipeline/check_cos_chat.sh. */
static void print_receipt_polished(const cos_pipeline_result_t *r,
                                   double elapsed_ms,
                                   int conformal_active,
                                   float conformal_alpha,
                                   int semantic_cache_hit,
                                   const struct cos_speed_metrics *spd) {
    const char *act = cos_sigma_action_label(r->final_action);
    const char *src = semantic_cache_hit ? "CACHE_SEMANTIC"
                     : r->engram_hit     ? "CACHE"
                                         : "FRESH";
    const char *route = r->escalated ? "CLOUD" : "LOCAL";
    if (r->escalated) {
        const char *ert = cos_cli_escalation_route_receipt();
        if (ert != NULL) route = ert;
    } else {
        const char *sdr = getenv("COS_SPEC_DECODE_ROUTE");
        if (sdr != NULL && strcmp(sdr, "DRAFT") == 0)
            route = "DRAFT(2B)";
        else if (sdr != NULL && strcmp(sdr, "VERIFY") == 0)
            route = "VERIFY(9B)";
    }
    double       ms =
        (r->engram_hit || semantic_cache_hit) ? 0.0 : elapsed_ms;
    float        tps = 0.0f;
    if (spd != NULL) tps = spd->tokens_per_second;

    char rethink_tag[48];
    rethink_tag[0] = '\0';
    if (r->rethink_count > 0) {
        snprintf(rethink_tag, sizeof(rethink_tag),
                 " | RETHINK ×%d", r->rethink_count);
    }

    char conformal_tag[64];
    conformal_tag[0] = '\0';
    if (conformal_active) {
        snprintf(conformal_tag, sizeof(conformal_tag),
                 " | conformal@α=%.2f", (double)conformal_alpha);
    }

    float ttft_ms = (spd != NULL) ? spd->ttft_ms : 0.0f;

    if (r->final_action == COS_SIGMA_ACTION_ABSTAIN) {
        fprintf(stdout,
                "[σ=%.3f | ABSTAIN%s%s | %.1f tok/s | TTFT %.0fms | %.0f ms | "
                "€%.4f]\n"
                "  (σ above τ_rethink — cannot guarantee accuracy at "
                "this confidence; reformulate or accept uncertainty.)\n",
                (double)r->sigma, conformal_tag, rethink_tag, (double)tps,
                (double)ttft_ms, ms, r->cost_eur);
    } else {
        fprintf(stdout,
                "[σ=%.3f | %s | %s | %s%s%s | %.1f tok/s | TTFT %.0fms | €%.4f]\n",
                (double)r->sigma, act, src, route,
                conformal_tag, rethink_tag, (double)tps,
                (double)ttft_ms, r->cost_eur);
    }
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

/* FINAL-2 per-turn toggles carried through to the --once / REPL paths. */
typedef struct {
    int   multi_sigma;
    int   conformal_active;
    float conformal_alpha;
    int   verbose;
    int   coherence_active;
    chat_coh_t *coh;
    int   semantic_cache;
    int   speculative_sigma;
    int   knowledge_graph;
    int   spike_mode;
    int   adaptive_compute;
    int   energy_report;
    int   proof_receipt_echo;
    int   semantic_entropy;
    int   semantic_sigma;
    /** 1 = skip HTTP triple-sample σ (single logprob shadow only). */
    int   fast_sigma;
    int   cascade;
    int   cross_model_sigma;
    int   semantic_lru;
} chat_feat_t;

/** Optional semantic-entropy σ after the main pipeline (extra HTTP samples). */
static void chat_maybe_semantic_entropy(cos_pipeline_config_t *cfg_rw,
                                        const chat_feat_t     *feat,
                                        const char            *user_prompt,
                                        cos_pipeline_result_t *r,
                                        int early_spec, int semantic_hit, int prc,
                                        int repl_use_tui) {
    if (cfg_rw == NULL || r == NULL || feat == NULL || !feat->semantic_entropy)
        return;
    if (early_spec || semantic_hit || prc != 0)
        return;
    if (r->response == NULL || r->response[0] == '\0')
        return;
    if (strcmp(r->response, COS_CHAT_PIPELINE_ABSTAIN) == 0)
        return;

    const char *sysp =
        (cfg_rw->codex != NULL && cfg_rw->codex->bytes != NULL
         && cfg_rw->codex->size > 0)
            ? cfg_rw->codex->bytes
            : NULL;

    int         port = 0;
    const char *pe   = getenv("COS_BITNET_SERVER_PORT");
    if (pe != NULL && pe[0] != '\0')
        port = atoi(pe);
    const char *model = getenv("COS_BITNET_CHAT_MODEL");

    int   ncl = 1;
    struct timespec __t0, __t1;
    (void)clock_gettime(CLOCK_MONOTONIC, &__t0);
    float se  = cos_semantic_entropy_ex(user_prompt, sysp, port,
                                         (model != NULL && model[0] != '\0')
                                             ? model
                                             : NULL,
                                         3, &ncl);
    (void)clock_gettime(CLOCK_MONOTONIC, &__t1);
    g_cos_chat_semantic_phase_ms +=
        (double)(__t1.tv_sec - __t0.tv_sec) * 1000.0
        + (double)(__t1.tv_nsec - __t0.tv_nsec) / 1.0e6;

    float        prior_sigma = r->sigma;
    const char *mode         = getenv("COS_SEMANTIC_ENTROPY_MODE");
    float        new_s;
    if (mode != NULL && strcmp(mode, "blend") == 0)
        new_s = 0.5f * r->sigma + 0.5f * se;
    else if (mode != NULL && strcmp(mode, "max") == 0)
        new_s = (r->sigma > se) ? r->sigma : se;
    else
        new_s = se;

    r->sigma = new_s;
    r->final_action =
        cos_sigma_reinforce_round(new_s, cfg_rw->tau_accept, cfg_rw->tau_rethink,
                                  r->rethink_count);

    FILE *out = (repl_use_tui != 0) ? stderr : stdout;
    fprintf(out,
            "[semantic-entropy σ=%.3f n_clusters=%d prior_σ=%.3f "
            "mode=%s]\n",
            (double)se, ncl, (double)prior_sigma,
            (mode != NULL && mode[0] != '\0') ? mode : "replace");
}

/** Jaccard multi-answer σ; two extra HTTP at 0.7 / 1.2 when primary set;
 *  blend 0.7·σ_semantic + 0.3·prior (prior = logprob σ before completes). */
static void chat_maybe_semantic_sigma(cos_pipeline_config_t *cfg_rw,
                                      const chat_feat_t     *feat,
                                      const char            *user_prompt,
                                      cos_pipeline_result_t *r,
                                      int early_spec, int semantic_hit, int prc,
                                      int repl_use_tui) {
    if (cfg_rw == NULL || r == NULL || feat == NULL || !feat->semantic_sigma)
        return;
    if (early_spec || semantic_hit || prc != 0)
        return;
    if (r->response == NULL || r->response[0] == '\0')
        return;
    if (strcmp(r->response, COS_CHAT_PIPELINE_ABSTAIN) == 0)
        return;
    if (feat->cross_model_sigma && cos_cli_use_bitnet_http() == 0)
        return;

    const char *sysp =
        (cfg_rw->codex != NULL && cfg_rw->codex->bytes != NULL
         && cfg_rw->codex->size > 0)
            ? cfg_rw->codex->bytes
            : NULL;

    int         port = 0;
    const char *pe   = getenv("COS_BITNET_SERVER_PORT");
    if (pe != NULL && pe[0] != '\0')
        port = atoi(pe);
    const char *model = getenv("COS_BITNET_CHAT_MODEL");

    float prior = r->sigma;

    cos_semantic_sigma_result det;
    memset(&det, 0, sizeof(det));
    /* Snapshot per-token σ before auxiliary completes overwrite server state. */
    struct timespec __ts0, __ts1;
    (void)clock_gettime(CLOCK_MONOTONIC, &__ts0);
    float se;
    if (feat->cross_model_sigma) {
        se = cos_cross_model_sigma(port, user_prompt, sysp);
        det.n_samples   = 3;
        det.n_clusters  = 1;
        det.similarities[0] = det.similarities[1] = det.similarities[2] =
            1.0f - se;
    } else {
        se = cos_semantic_sigma_ex(user_prompt, sysp, port,
                                   (model != NULL && model[0] != '\0')
                                       ? model
                                       : NULL,
                                   3, r->response, &det);
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &__ts1);
    g_cos_chat_semantic_phase_ms +=
        (double)(__ts1.tv_sec - __ts0.tv_sec) * 1000.0
        + (double)(__ts1.tv_nsec - __ts0.tv_nsec) / 1.0e6;

    float combined = 0.7f * se + 0.3f * prior;
    combined         = (combined < 0.0f) ? 0.0f : combined;
    combined         = (combined > 1.0f) ? 1.0f : combined;
    r->sigma         = combined;
    r->final_action  = cos_sigma_reinforce_round(
        combined, cfg_rw->tau_accept, cfg_rw->tau_rethink, r->rethink_count);

    FILE *out = (repl_use_tui != 0) ? stderr : stdout;
    {
        const char *tag = feat->cross_model_sigma ? "cross-model-sigma"
                                                  : "semantic-sigma";
        fprintf(out,
                "[%s σ=%.3f j01=%.3f j02=%.3f j12=%.3f n_cl=%d "
                "blend=0.7·σ_se+0.3·σ_prior]\n",
                tag, (double)se, (double)det.similarities[0],
                (double)det.similarities[1], (double)det.similarities[2],
                det.n_clusters);
    }
}

typedef struct {
    int saved_max_rethink;
    int saved_ttt;
} chat_budget_snap_t;

typedef struct {
    struct cos_speculative_sigma ps;
    struct cos_compute_budget    bu;
    int                          need_predict;
    int                          skip_multi_verify;
    int                          spike_any;
    int                          spike_nf;
    chat_budget_snap_t           bsnap;
    int                          did_budget;
} chat_sigma_layer_t;

static struct cos_spike_engine g_cos_spike;
static int                   g_cos_spike_ready;

static uint64_t g_spike_last_bsc[COS_INF_W];
static int      g_spike_have_last;
static char     g_spike_last_resp[4096];
static char     g_spike_ham_buf[4096];
static char     g_cos_sem_lru_buf[4096];
static char     g_cos_cascade_buf[4096];
static char     g_cos_eng_cascade_buf[4096];

static struct cos_state_ledger g_ag_ledger;
static int                     g_ag_ledger_ready;
static int                     g_ag_turns;

static const char *chat_err_src(enum cos_error_source s) {
    switch (s) {
        case COS_ERR_NONE: return "NONE";
        case COS_ERR_EPISTEMIC: return "EPISTEMIC";
        case COS_ERR_ALEATORIC: return "ALEATORIC";
        case COS_ERR_REASONING: return "REASONING";
        case COS_ERR_MEMORY: return "MEMORY";
        case COS_ERR_NOVEL_DOMAIN: return "NOVEL_DOMAIN";
        case COS_ERR_INJECTION: return "INJECTION";
        default: return "?";
    }
}

static void chat_agi_after_turn(cos_pipeline_config_t *cfg_rw,
                                const char *prompt,
                                cos_pipeline_result_t *r,
                                const chat_feat_t *feat,
                                chat_multi_t *ms,
                                int have_ms) {
    cos_ultra_metacog_levels_t lv;
    chat_metacog_levels(prompt, r, cfg_rw->max_rethink, &lv);
    cos_multi_sigma_t mc;
    if (have_ms && ms != NULL) {
        mc = ms->ens;
    } else {
        memset(&mc, 0, sizeof(mc));
        float s = r->sigma;
        if (s < 0.f) s = 0.f;
        if (s > 1.f) s = 1.f;
        mc.sigma_logprob = s;
        mc.sigma_entropy = s;
        mc.sigma_perplexity = s;
        mc.sigma_consistency = 0.f;
        mc.sigma_combined = s;
    }
    float meta[4] = {
        lv.sigma_perception, lv.sigma_self,
        lv.sigma_social, lv.sigma_situational,
    };
    struct cos_error_attribution attr =
        cos_error_attribute(mc.sigma_logprob, mc.sigma_entropy,
                           mc.sigma_consistency, lv.sigma_perception);

    if (!g_ag_ledger_ready) {
        cos_state_ledger_init(&g_ag_ledger);
        g_ag_ledger_ready = 1;
    }
    cos_state_ledger_fill_multi(&g_ag_ledger, &mc);
    cos_state_ledger_update(&g_ag_ledger, mc.sigma_combined, meta,
                           (int)r->final_action);
    cos_state_ledger_add_cost(&g_ag_ledger, (float)r->cost_eur);
    if (r->engram_hit) cos_state_ledger_note_cache_hit(&g_ag_ledger);

    {
        const char *sdr = getenv("COS_SPEC_DECODE_ROUTE");
        if (sdr != NULL) {
            int draft = (strcmp(sdr, "DRAFT") == 0);
            cos_state_ledger_note_spec_decode(&g_ag_ledger, draft);
        }
    }

    struct cos_engram_episode ep;
    ep.timestamp_ms = (int64_t)time(NULL) * 1000LL;
    ep.prompt_hash = cos_engram_prompt_hash(prompt);
    ep.sigma_combined = mc.sigma_combined;
    ep.action = (int)r->final_action;
    ep.was_correct = -1;
    ep.attribution = attr.source;
    ep.ttt_applied   = r->ttt_applied ? 1 : 0;
    cos_engram_episode_store(&ep);

    g_ag_turns++;
    int ev = 50;
    const char *es = getenv("COS_EPISODE_CONSOLIDATE_EVERY");
    if (es != NULL && es[0] != '\0') ev = atoi(es);
    if (ev > 0 && (g_ag_turns % ev) == 0)
        cos_engram_consolidate(ev * 100);

    if (feat != NULL && feat->verbose) {
        fprintf(stdout,
                "[attribution: source=%s confidence=%.2f]\n"
                "  reason=\"%s\"\n"
                "  fix=\"%s\"\n",
                chat_err_src(attr.source), (double)attr.confidence,
                attr.reason, attr.fix);
        cos_state_ledger_print_summary(stdout, &g_ag_ledger);
        fprintf(stdout,
                "[episode stored: σ=%.3f action=%s]\n",
                (double)mc.sigma_combined,
                cos_sigma_action_label(r->final_action));
    }
}

static void chat_kg_store_turn(const char *prompt, const char *response)
{
    if (!prompt)
        return;
    const char *resp = (response && response[0]) ? response : "";
    char        buf[32768];
    if (snprintf(buf, sizeof buf, "%s\n%s", prompt, resp) >= (int)sizeof buf)
        buf[sizeof buf - 1] = '\0';
    if (cos_kg_init() != 0)
        return;
    (void)cos_kg_extract_and_store(buf);
}

static int cos_chat_spike_snapshot_path(char *buf, size_t cap)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return -1;
    snprintf(buf, cap, "%s/.cos/spike_engine.snapshot", home);
    return 0;
}

static void chat_sigma_budget_defaults(struct cos_compute_budget *bu)
{
    bu->kernels_to_run    = 40;
    bu->max_rethinks      = 100;
    bu->enable_ttt        = 1;
    bu->enable_search     = 1;
    bu->enable_multimodal = 1;
    bu->time_budget_ms    = 30000.f;
    bu->energy_budget_mj  = 50.f;
}

static void chat_budget_apply(cos_pipeline_config_t *cfg,
                              const struct cos_compute_budget *bu,
                              chat_budget_snap_t *snap)
{
    snap->saved_max_rethink = cfg->max_rethink;
    snap->saved_ttt         = cfg->ttt_enabled;
    if (bu->max_rethinks < cfg->max_rethink)
        cfg->max_rethink = bu->max_rethinks;
    cfg->ttt_enabled = snap->saved_ttt && bu->enable_ttt;
}

static void chat_budget_restore(cos_pipeline_config_t *cfg,
                                const chat_budget_snap_t *snap)
{
    cfg->max_rethink = snap->saved_max_rethink;
    cfg->ttt_enabled = snap->saved_ttt;
}

static void chat_compute_line(double elapsed_ms,
                              const struct cos_compute_budget *bu)
{
    float est_mj = 0.f;

    if (bu != NULL && bu->time_budget_ms > 1e-6f) {
        est_mj = bu->energy_budget_mj
                 * (float)(elapsed_ms / (double)bu->time_budget_ms);
        if (est_mj > bu->energy_budget_mj)
            est_mj = bu->energy_budget_mj;
    }
    fprintf(stdout,
            "[compute: %d/40 kernels, %.2f ms, %.2f mJ]\n",
            bu != NULL ? bu->kernels_to_run : 40, elapsed_ms,
            (double)est_mj);
}

static int chat_sigma_layer_prepare(cos_pipeline_config_t *cfg,
                                    const chat_feat_t *feat,
                                    const uint64_t *bsc,
                                    uint64_t domain_h,
                                    chat_sigma_layer_t *out)
{
    memset(out, 0, sizeof(*out));
    chat_sigma_budget_defaults(&out->bu);
    out->spike_any = 1;

    out->need_predict =
        feat != NULL
        && (feat->speculative_sigma || feat->spike_mode || feat->adaptive_compute);
    if (!out->need_predict) {
        out->ps.predicted_sigma = 0.5f;
        out->skip_multi_verify  = 0;
        return 0;
    }

    out->ps = cos_predict_sigma(bsc, domain_h);
    if (out->ps.early_abstain && feat != NULL
        && (feat->speculative_sigma || feat->spike_mode
            || feat->adaptive_compute))
        return 1;

    if (feat != NULL && feat->adaptive_compute) {
        out->bu =
            cos_allocate_compute(out->ps.predicted_sigma,
                                 (feat->spike_mode && g_cos_spike_ready)
                                     ? &g_cos_spike
                                     : NULL);
        chat_budget_apply(cfg, &out->bu, &out->bsnap);
        out->did_budget = 1;
    }

    if (feat != NULL && feat->spike_mode && g_cos_spike_ready) {
        float chv[8];
        int   fired[8];
        cos_spike_fill_channels_from_speculative(out->ps.predicted_sigma,
                                                 out->ps.confidence, chv);
        cos_spike_check(&g_cos_spike, chv, fired, &out->spike_nf);
        out->spike_any = (out->spike_nf > 0);
    }

    out->skip_multi_verify =
        out->ps.skip_verification || (out->bu.kernels_to_run < 10);
    return 0;
}

static void chat_sigma_layer_restore(cos_pipeline_config_t *cfg,
                                     chat_sigma_layer_t *L)
{
    if (L != NULL && L->did_budget)
        chat_budget_restore(cfg, &L->bsnap);
}

static uint64_t chat_proof_kernel_mask(const chat_feat_t           *feat,
                                       const chat_sigma_layer_t *SL)
{
    int       k = 40;
    uint64_t bm;

    if (feat != NULL && feat->adaptive_compute != 0 && SL != NULL)
        k = SL->bu.kernels_to_run;
    if (k > 40)
        k = 40;
    if (k <= 0)
        return 0ULL;
    if (k >= 64)
        bm = ~(uint64_t)0;
    else
        bm = (1ULL << k) - 1ULL;
    return bm & 0xFFFFFFFFFFULL;
}

static char g_cos_chat_const_halt[768];

static void chat_turn_proof_emit(const cos_pipeline_config_t *cfg,
                                 const char *prompt,
                                 const cos_pipeline_result_t *r,
                                 const chat_multi_t *ms,
                                 int                       have_ms,
                                 const chat_feat_t        *feat,
                                 const chat_sigma_layer_t *SL,
                                 double                    elapsed_ms,
                                 const char *receipt_body,
                                 int         constitutional_valid,
                                 int         constitutional_compliant,
                                 int         constitutional_checks,
                                 int         constitutional_violations,
                                 int         constitutional_mandatory_halts)
{
    struct cos_proof_receipt               rec;
    struct cos_proof_receipt_options       opt;
    cos_multi_sigma_t               mc;
    cos_ultra_metacog_levels_t      lv;
    struct cos_error_attribution    attr;
    float sch[4], mch[4];
    uint64_t                        kbmp;
    char                            codex_ver[32];
    char                            model_buf[96];
    const char                     *resp;

    memset(&opt, 0, sizeof opt);
    if (receipt_body != NULL)
        resp = receipt_body;
    else
        resp = (r != NULL && r->response != NULL) ? r->response : "";

    if (have_ms && ms != NULL) {
        mc = ms->ens;
    } else {
        memset(&mc, 0, sizeof mc);
        mc.sigma_logprob = mc.sigma_entropy = mc.sigma_perplexity =
            (r != NULL ? r->sigma : 1.f);
        mc.sigma_consistency = 0.f;
        mc.sigma_combined    = (r != NULL ? r->sigma : 1.f);
    }

    chat_metacog_levels(prompt, r, cfg->max_rethink, &lv);
    mch[0] = lv.sigma_perception;
    mch[1] = lv.sigma_self;
    mch[2] = lv.sigma_social;
    mch[3] = lv.sigma_situational;

    sch[0] = mc.sigma_logprob;
    sch[1] = mc.sigma_entropy;
    sch[2] = mc.sigma_perplexity;
    sch[3] = mc.sigma_consistency;

    attr = cos_error_attribute(mc.sigma_logprob, mc.sigma_entropy,
                               mc.sigma_consistency, lv.sigma_perception);

    opt.codex_fnv64 = cfg->codex != NULL ? cfg->codex->hash_fnv1a64 : 0ULL;
    opt.injection_detected =
        (attr.source == COS_ERR_INJECTION) ? 1 : 0;
    opt.risk_level      = 0;
    opt.sovereign_brake = 0;
    opt.within_compute_budget = 1;
    if (feat != NULL && feat->adaptive_compute != 0 && SL != NULL) {
        opt.within_compute_budget =
            (elapsed_ms <= (double)SL->bu.time_budget_ms + 1e-3) ? 1 : 0;
        opt.kernels_run = SL->bu.kernels_to_run;
    } else {
        opt.kernels_run = 40;
    }

    snprintf(model_buf, sizeof model_buf, "%s", cos_chat_backend_label());
    opt.model_id = model_buf;

    codex_ver[0] = '\0';
    if (cfg->codex != NULL) {
        snprintf(codex_ver, sizeof codex_ver, "%s-ch%u-%llx",
                 cfg->codex->is_seed ? "seed" : "full",
                 (unsigned)cfg->codex->chapters_found,
                 (unsigned long long)(cfg->codex->hash_fnv1a64 & 0xffffULL));
    }
    opt.codex_version = codex_ver;

    if (constitutional_valid) {
        opt.constitutional_valid           = 1;
        opt.constitutional_compliant       = constitutional_compliant;
        opt.constitutional_checks          = constitutional_checks;
        opt.constitutional_violations      = constitutional_violations;
        opt.constitutional_mandatory_halts = constitutional_mandatory_halts;
    }

    kbmp = chat_proof_kernel_mask(feat, SL);

    cos_proof_receipt_generate_with(
        resp, mc.sigma_combined, sch, mch,
        (int)(r != NULL ? r->final_action : COS_SIGMA_ACTION_ABSTAIN),
        &attr, kbmp, &opt, &rec);

    (void)cos_proof_receipt_persist_chain(&rec);

    if (feat != NULL && feat->proof_receipt_echo != 0) {
        char *js = cos_proof_receipt_to_json(&rec);
        if (js != NULL) {
            fputs(js, stdout);
            fputc('\n', stdout);
            free(js);
        }
    }
}

static int run_chat_once(FILE *tout, cos_pipeline_config_t *cfg_rw,
                         const chat_feat_t *feat,
                         const char *prompt) {
    uint64_t ph = cos_engram_prompt_hash(prompt);
    float    tau_saved = cfg_rw->tau_accept;
    float    tl = cos_engram_get_local_tau(ph);
    if (tl > 0.f && tl < 1.f)
        cfg_rw->tau_accept = tl;

    uint64_t bsc_prompt[COS_INF_W];
    cos_inference_bsc_encode_prompt(prompt, bsc_prompt);
    uint64_t domain_h = cos_inference_prompt_fnv(prompt);

    cos_pipeline_result_t r;
    memset(&r, 0, sizeof(r));
    g_cos_chat_semantic_phase_ms = 0.0;

    chat_sigma_layer_t SL;
    int early_spec_abstain = 0;
    int prep =
        chat_sigma_layer_prepare(cfg_rw, feat, bsc_prompt, domain_h, &SL);
    if (prep != 0) {
        early_spec_abstain          = 1;
        r.response                  = COS_CHAT_PIPELINE_ABSTAIN;
        r.sigma                     = SL.ps.predicted_sigma;
        r.engram_hit                = 0;
        r.rethink_count             = 0;
        r.escalated                 = 0;
        r.cost_eur                  = 0.0;
        r.final_action              = COS_SIGMA_ACTION_ABSTAIN;
        r.agent_gate                = COS_AGENT_ALLOW;
        r.diagnostic                = "speculative_sigma_early_abstain";
        r.mode                      = cfg_rw->mode;
        r.codex_hash                = cfg_rw->codex != NULL
                                          ? cfg_rw->codex->hash_fnv1a64
                                          : 0ULL;
        r.ttt_applied               = 0;
    }

    int skip_multi_verify =
        early_spec_abstain ? 1 : SL.skip_multi_verify;
    int kernel_cap = SL.bu.kernels_to_run;

    int semantic_hit = 0;
    int cascade_done = 0;

    double w0 = 0.0, w1 = 0.0;
    double  elapsed_ms = 0.0;
    int     prc          = 0;

    if (!early_spec_abstain && !semantic_hit && feat != NULL
        && feat->semantic_lru) {
        uint64_t hlr = cos_semantic_cache_prompt_hash(prompt);
        float    cs  = cos_semantic_cache_lookup(
            hlr, g_cos_sem_lru_buf, sizeof g_cos_sem_lru_buf);
        if (cs >= 0.0f) {
            semantic_hit          = 1;
            r.response            = g_cos_sem_lru_buf;
            r.sigma               = cs;
            r.engram_hit          = 0;
            r.rethink_count       = 0;
            r.escalated           = 0;
            r.cost_eur            = 0.0;
            r.final_action        = COS_SIGMA_ACTION_ACCEPT;
            r.agent_gate          = COS_AGENT_ALLOW;
            r.diagnostic          = "semantic_lru_cache";
            r.mode                = cfg_rw->mode;
            r.codex_hash          = cfg_rw->codex != NULL
                                        ? cfg_rw->codex->hash_fnv1a64
                                        : 0ULL;
            r.ttt_applied         = 0;
            fprintf(stdout, "[CACHE_LRU] hit σ=%.3f (normalized prompt)\n",
                    (double)cs);
        }
    }

    if (!early_spec_abstain && !semantic_hit && feat != NULL
        && feat->semantic_cache) {
        struct cos_inference_cache_entry hit;
        if (cos_inference_cache_lookup(bsc_prompt, COS_INF_W, &hit) != 0) {
            semantic_hit          = 1;
            r.response            = hit.response;
            r.sigma               = hit.sigma;
            r.engram_hit          = 0;
            r.rethink_count       = 0;
            r.escalated           = 0;
            r.cost_eur            = 0.0;
            r.final_action        = COS_SIGMA_ACTION_ACCEPT;
            r.agent_gate          = COS_AGENT_ALLOW;
            r.diagnostic          = "semantic_inference_cache";
            r.mode                = cfg_rw->mode;
            r.codex_hash          = cfg_rw->codex != NULL
                                        ? cfg_rw->codex->hash_fnv1a64
                                        : 0ULL;
            r.ttt_applied         = 0;
            fprintf(stdout,
                    "[CACHE_SEMANTIC] hit  σ=%.3f  (BSC neighbour reuse)\n",
                    (double)hit.sigma);
        }
    }

    if (!early_spec_abstain && !semantic_hit && feat != NULL && feat->spike_mode
        && !SL.spike_any && g_spike_have_last) {
        float       hm = cos_inference_hamming_norm(bsc_prompt,
                                                    g_spike_last_bsc,
                                                    COS_INF_W);
        float       tau_h = 0.08f;
        const char *eh    = getenv("COS_SPIKE_HAMMING_MAX");
        if (eh != NULL && eh[0] != '\0')
            tau_h = (float)atof(eh);
        if (hm <= tau_h && SL.ps.predicted_sigma < 0.35f
            && g_spike_last_resp[0] != '\0') {
            semantic_hit = 1;
            snprintf(g_spike_ham_buf, sizeof g_spike_ham_buf, "%s",
                     g_spike_last_resp);
            r.response         = g_spike_ham_buf;
            r.sigma            = SL.ps.predicted_sigma;
            r.engram_hit       = 0;
            r.rethink_count    = 0;
            r.escalated        = 0;
            r.cost_eur         = 0.0;
            r.final_action     = COS_SIGMA_ACTION_ACCEPT;
            r.agent_gate       = COS_AGENT_ALLOW;
            r.diagnostic       = "spike_hamming_cache";
            r.mode             = cfg_rw->mode;
            r.codex_hash       = cfg_rw->codex != NULL
                                     ? cfg_rw->codex->hash_fnv1a64
                                     : 0ULL;
            r.ttt_applied      = 0;
            fprintf(stdout,
                    "[CACHE_SPIKE] hit  hamming_norm=%.4f  σ=%.3f\n",
                    (double)hm, (double)r.sigma);
        }
    }

    if (!early_spec_abstain && !semantic_hit && feat != NULL && feat->cascade
        && cos_cli_use_bitnet_http() != 0) {
        if (cfg_rw->engram != NULL) {
            cos_sigma_engram_tick(cfg_rw->engram);
            cos_sigma_engram_entry_t eg;
            uint64_t                 eh = cos_sigma_engram_hash(prompt);
            if (cos_sigma_engram_get(cfg_rw->engram, eh, &eg) == 1
                && eg.value != NULL) {
                const char *ev = (const char *)eg.value;
                snprintf(g_cos_eng_cascade_buf, sizeof g_cos_eng_cascade_buf,
                         "%s", ev);
                r.response       = g_cos_eng_cascade_buf;
                r.sigma          = eg.sigma_at_store;
                r.engram_hit     = 1;
                r.rethink_count  = 0;
                r.escalated      = 0;
                r.cost_eur       = 0.0;
                r.final_action   = COS_SIGMA_ACTION_ACCEPT;
                r.agent_gate     = COS_AGENT_ALLOW;
                r.diagnostic     = "engram_hit";
                r.mode           = cfg_rw->mode;
                r.codex_hash     = cfg_rw->codex != NULL ? cfg_rw->codex->hash_fnv1a64
                                                       : 0ULL;
                r.ttt_applied    = 0;
                semantic_hit     = 1;
            }
        }
        if (!semantic_hit) {
            int         port = 0;
            const char *pe   = getenv("COS_BITNET_SERVER_PORT");
            if (pe != NULL && pe[0] != '\0')
                port = atoi(pe);
            const char *sysp_c = NULL;
            if (cfg_rw->codex != NULL && cfg_rw->codex->bytes != NULL
                && cfg_rw->codex->size > 0)
                sysp_c = cfg_rw->codex->bytes;
            w0 = chat_wall_ms();
            float csig = 1.0f;
            int   tier = -1;
            int   cr   = cos_cascade_query(port, prompt, sysp_c, g_cos_cascade_buf,
                                         sizeof g_cos_cascade_buf, &csig,
                                         &tier);
            w1 = chat_wall_ms();
            if (cr != 0) {
                chat_emit_pipeline_failure_help();
                return 3;
            }
            prc = 0;
            r.response     = g_cos_cascade_buf;
            r.sigma        = csig;
            r.engram_hit   = 0;
            r.rethink_count = 0;
            r.escalated    = 0;
            r.cost_eur     = 0.0001 * (double)(tier >= 0 ? (tier + 1) : 4);
            r.final_action = cos_sigma_reinforce_round(
                csig, cfg_rw->tau_accept, cfg_rw->tau_rethink, r.rethink_count);
            r.agent_gate   = COS_AGENT_ALLOW;
            r.diagnostic   = (tier >= 0) ? "cascade_tier" : "cascade_abstain";
            r.mode         = cfg_rw->mode;
            r.codex_hash   = cfg_rw->codex != NULL ? cfg_rw->codex->hash_fnv1a64
                                                   : 0ULL;
            r.ttt_applied  = 0;
            cascade_done   = 1;
        }
    }

    if (!early_spec_abstain && !semantic_hit && !cascade_done) {
        w0 = chat_wall_ms();
        prc = cos_sigma_pipeline_run(cfg_rw, prompt, &r);
        w1 = chat_wall_ms();
    }

    if (SL.did_budget)
        chat_sigma_layer_restore(cfg_rw, &SL);

    cfg_rw->tau_accept = tau_saved;
    if (!early_spec_abstain && !semantic_hit && !cascade_done && prc != 0) {
        chat_emit_pipeline_failure_help();
        return 3;
    }
    if (!early_spec_abstain && !semantic_hit && !cascade_done) {
        elapsed_ms = w1 - w0;
        if (elapsed_ms < 0.0)
            elapsed_ms = 0.0;
    } else if (cascade_done && !semantic_hit) {
        elapsed_ms = w1 - w0;
        if (elapsed_ms < 0.0)
            elapsed_ms = 0.0;
    } else {
        elapsed_ms = 0.0;
    }

    if (feat != NULL && (feat->energy_report || feat->adaptive_compute))
        chat_compute_line(elapsed_ms, &SL.bu);

    if (!early_spec_abstain && feat != NULL && feat->semantic_cache
        && !semantic_hit && prc == 0 && r.response != NULL)
        (void)cos_inference_cache_store_latency(
            bsc_prompt, r.response, r.sigma,
            (int64_t)(elapsed_ms + 0.5));

    if (!early_spec_abstain && feat != NULL
        && (feat->speculative_sigma || feat->spike_mode
            || feat->adaptive_compute))
        cos_speculative_sigma_observe(domain_h, r.sigma);

    uint64_t      resp_cache_h = cos_response_cache_prompt_hash(prompt);
    int           skipped_resp_sig_cache = 0;
    if (!early_spec_abstain && !semantic_hit && prc == 0 && feat != NULL
        && (feat->semantic_sigma || feat->semantic_entropy)
        && !(feat->cascade)) {
        float c_sig = cos_response_cache_lookup(resp_cache_h, NULL, 0);
        if (c_sig >= 0.0f) {
            r.sigma         = c_sig;
            r.final_action  = cos_sigma_reinforce_round(
                c_sig, cfg_rw->tau_accept, cfg_rw->tau_rethink, r.rethink_count);
            skipped_resp_sig_cache = 1;
            fprintf(stdout,
                    "[response-σ-cache] hit σ=%.3f action=%s\n", (double)c_sig,
                    cos_sigma_action_label(r.final_action));
        }
    }

    if (!skipped_resp_sig_cache && !(feat != NULL && feat->cascade)) {
        if (feat != NULL && feat->semantic_sigma) {
            chat_maybe_semantic_sigma(cfg_rw, feat, prompt, &r,
                                      early_spec_abstain, semantic_hit, prc, 0);
        } else if (feat != NULL && feat->semantic_entropy)
            chat_maybe_semantic_entropy(cfg_rw, feat, prompt, &r,
                                        early_spec_abstain, semantic_hit, prc,
                                        0);
    }

    if (!early_spec_abstain && !semantic_hit && prc == 0
        && !skipped_resp_sig_cache && feat != NULL
        && (feat->semantic_sigma || feat->semantic_entropy)
        && r.response != NULL)
        cos_response_cache_store(resp_cache_h, r.sigma, r.response);

    if (!early_spec_abstain && feat != NULL && feat->semantic_lru && prc == 0
        && r.response != NULL
        && strcmp(r.response, COS_CHAT_PIPELINE_ABSTAIN) != 0
        && strncmp(r.response, "[ABSTAIN", 9) != 0
        && (r.diagnostic == NULL
            || strcmp(r.diagnostic, "semantic_lru_cache") != 0))
        cos_semantic_cache_store(cos_semantic_cache_prompt_hash(prompt),
                                 r.sigma, r.response);

    if (!early_spec_abstain && !semantic_hit && prc == 0 && r.response != NULL) {
        memcpy(g_spike_last_bsc, bsc_prompt, sizeof(g_spike_last_bsc));
        snprintf(g_spike_last_resp, sizeof g_spike_last_resp, "%s",
                 r.response);
        g_spike_have_last = 1;
    }

    if (feat != NULL && feat->spike_mode && g_cos_spike_ready) {
        char spath[512];
        if (cos_chat_spike_snapshot_path(spath, sizeof spath) == 0)
            (void)cos_spike_engine_snapshot_write(&g_cos_spike, spath);
    }

    const char *route = r.escalated ? "ESCALATE" : "LOCAL";

    /* FINAL-2 Phase C: metacog banner (verbose only; before the round
     * line so the diagnostic precedes the answer). */
    if (feat != NULL && feat->verbose) {
        cos_ultra_metacog_levels_t lv;
        chat_metacog_levels(prompt, &r, cfg_rw->max_rethink, &lv);
        fprintf(stdout,
                "[meta: perception=%.2f self=%.2f social=%.2f "
                "situational=%.2f]\n",
                (double)lv.sigma_perception, (double)lv.sigma_self,
                (double)lv.sigma_social, (double)lv.sigma_situational);
    }

    /* FINAL-2 Phase A: σ_combined shadow (does not change gating). */
    chat_multi_t ms;
    int have_ms = 0;
    memset(&ms, 0, sizeof(ms));
    {
        int allow_multi =
            feat != NULL && feat->multi_sigma && !skip_multi_verify
            && (!feat->adaptive_compute || kernel_cap >= 10);
        if (allow_multi) {
            int hmt = (cos_cli_use_bitnet_http() != 0)
                      && (feat != NULL)
                      && ((feat->semantic_sigma != 0)
                          || (feat->fast_sigma == 0));
            if (chat_multi_shadow(cfg_rw, hmt, prompt, &r, r.response, r.sigma,
                                  &ms) == 0) {
                have_ms = 1;
            }
        }
    }

    chat_agi_after_turn(cfg_rw, prompt, &r, feat, &ms, have_ms);

    if (feat != NULL && feat->verbose && r.ttt_applied != 0
        && cos_ttt_last_verbose_line()[0] != '\0')
        fprintf(stdout, "%s\n", cos_ttt_last_verbose_line());

    {
        const char *deliver_txt =
            (r.response != NULL) ? r.response : "";
        int c_valid = 0, c_compl = 1, c_checks = 0, c_viol = 0,
            c_mhalts = 0, c_mv = 0;
        char crep[1024];

        crep[0] = '\0';
        if (!early_spec_abstain && r.response != NULL
            && r.final_action != COS_SIGMA_ACTION_ABSTAIN) {
            struct cos_proof_receipt pr;
            memset(&pr, 0, sizeof pr);
            spektre_sha256((const uint8_t *)r.response, strlen(r.response),
                           pr.output_hash);
            pr.sigma_combined = r.sigma;
            pr.within_compute_budget = 1;
            if (feat != NULL && feat->adaptive_compute != 0) {
                pr.within_compute_budget =
                    (elapsed_ms <= (double)SL.bu.time_budget_ms + 1e-3) ? 1
                                                                        : 0;
            }
            (void)cos_constitution_check(r.response, r.sigma, &pr, &c_viol,
                                          &c_mv, crep, (int)sizeof crep);
            c_valid  = 1;
            c_checks = cos_constitution_get()->n_rules;
            c_compl  = (c_viol == 0) ? 1 : 0;
            c_mhalts = (c_mv > 0) ? 1 : 0;
            if (c_mv > 0) {
                snprintf(g_cos_chat_const_halt, sizeof g_cos_chat_const_halt,
                         "[CONSTITUTIONAL HALT] %s", crep);
                fprintf(stdout, "%s\n", g_cos_chat_const_halt);
                /* --multi-sigma benchmarks: σ was computed on the real model
                 * string; keep the visible answer + receipts on that text
                 * and only surface the halt as a warning line above. */
                if (feat == NULL || !feat->multi_sigma)
                    deliver_txt = g_cos_chat_const_halt;
            } else if (c_viol > 0) {
                fprintf(stdout, "[CONSTITUTIONAL WARNING] %s\n", crep);
            }
        }
        fprintf(stdout,
                "round 0  %s  [σ_peak=%.2f action=%s route=%s]\n",
                cos_chat_stream_env_on()
                    ? "(streamed above)"
                    : deliver_txt,
                (double)r.sigma,
                cos_sigma_action_label(r.final_action),
                route);

        /* NEXT-1 polished receipt (shared with REPL path).  Harness only
         * requires ACCEPT/RETHINK/ABSTAIN to appear somewhere in the
         * output; the "round 0" line already satisfied that, so the
         * receipt is free to adopt the visual format. */
        {
            float ttft_pass = -1.0f;
            if (cos_chat_stream_env_on()) {
                double g = cos_bitnet_server_last_ttft_ms();
                if (g >= 0.0)
                    ttft_pass = (float)g;
            }
            struct cos_speed_metrics spd = cos_speed_measure_ex(
                r.response != NULL ? r.response : "", (float)elapsed_ms,
                ttft_pass);
            cos_speed_print(&spd);
            fputc('\n', stdout);
            fprintf(stderr,
                    "[speed: %d tok | %.0fms | %.1f tok/s | σ_time %.0fms]\n",
                    spd.tokens_generated, (double)elapsed_ms,
                    (double)spd.tokens_per_second,
                    g_cos_chat_semantic_phase_ms);
            print_receipt_polished(&r, elapsed_ms,
                                   feat != NULL ? feat->conformal_active : 0,
                                   feat != NULL ? feat->conformal_alpha  : 0.0f,
                                   semantic_hit, &spd);
        }
        {
            const char *proof_body = deliver_txt;
            if (feat != NULL && feat->multi_sigma && c_mhalts != 0
                && r.response != NULL && r.response[0] != '\0')
                proof_body = r.response;
            chat_turn_proof_emit(cfg_rw, prompt, &r, &ms, have_ms, feat, &SL,
                                 elapsed_ms, proof_body, c_valid, c_compl,
                                 c_checks, c_viol, c_mhalts);
        }
    }

    if (have_ms) {
        fprintf(stdout,
                "[σ_combined=%.3f | σ_semantic=%.3f | σ_logprob=%.3f "
                "σ_entropy=%.3f σ_perplexity=%.3f σ_consistency=%.3f | k=%d]\n",
                (double)ms.ens.sigma_combined,
                (double)ms.ens.sigma_consistency,
                (double)ms.ens.sigma_logprob,
                (double)ms.ens.sigma_entropy,
                (double)ms.ens.sigma_perplexity,
                (double)ms.ens.sigma_consistency,
                ms.k_used);
    }

    /* FINAL-2 Phase D: add scalar σ to the session window if tracked. */
    if (feat != NULL && feat->coherence_active && feat->coh != NULL) {
        coh_push(feat->coh, r.sigma);
    }

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
        fprintf(tout,
                ",\"sigma_drift\":%d}\n",
                (g_ag_ledger_ready && g_ag_ledger.drift_detected) ? 1 : 0);
        fflush(tout);
    }
    if (feat != NULL && feat->knowledge_graph && r.response != NULL)
        chat_kg_store_turn(prompt, r.response);
    return 0;
}

/* HORIZON-1: run /bin/sh -c with argv-style -c string (no quoting hell). */
static int chat_sh_c_capture(const char *cmd, char *buf, size_t cap) {
    if (!cmd || !buf || cap < 2) return -1;
    int p[2];
    if (pipe(p) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        return -1;
    }
    if (pid == 0) {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        dup2(p[1], STDERR_FILENO);
        close(p[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(126);
    }
    close(p[1]);
    size_t n = 0;
    for (;;) {
        ssize_t r = read(p[0], buf + n, cap - 1 - n);
        if (r <= 0) break;
        n += (size_t)r;
        if (n >= cap - 1) break;
    }
    buf[n] = '\0';
    close(p[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    return -1;
}

static int chat_tools_confirm_yes(FILE *in) {
    const char *e = getenv("COS_TOOLS_ASSUME_YES");
    if (e != NULL && e[0] == '1') return 1;
    fprintf(stdout, "CONFIRM this command? [y/N] ");
    fflush(stdout);
    char b[32];
    if (fgets(b, sizeof b, in) == NULL) return 0;
    return (b[0] == 'y' || b[0] == 'Y');
}

/* One user line → gate line → optional execute. */
static int chat_tools_dispatch_line(const char *line, int dry_run, FILE *confirm_in) {
    float tl = 0.35f, th = 0.65f;
    cos_sigma_tool_thresholds_default(&tl, &th);
    cos_tool_gate_result_t g;
    if (cos_sigma_tool_gate(line, tl, th, &g) != 0) {
        fprintf(stdout, "[tool: (empty) | — | σ_tool=— | —]\n");
        return 0;
    }
    fprintf(stdout, "[tool: %s | %s | σ_tool=%.2f | %s]\n",
            g.expanded_cmd, g.risk_label, (double)g.sigma_tool, g.decision_label);
    if (g.decision == COS_TOOL_DEC_BLOCKED) {
        if (g.block_reason[0] != '\0')
            fprintf(stdout, "%s\n", g.block_reason);
        else
            fprintf(stdout, "High-risk operation blocked by σ-gate\n");
        return 0;
    }
    if (g.decision == COS_TOOL_DEC_CONFIRM) {
        if (!chat_tools_confirm_yes(confirm_in)) {
            fprintf(stdout, "[tool: not executed]\n");
            return 0;
        }
    }
    if (dry_run) {
        fprintf(stdout, "[tool: dry-run — not executing]\n");
        return 0;
    }
    enum { CAP = 16384 };
    char out[CAP];
    int ex = chat_sh_c_capture(g.expanded_cmd, out, sizeof out);
    if (out[0] != '\0') {
        fputs(out, stdout);
        size_t ol = strlen(out);
        if (ol > 0 && out[ol - 1] != '\n')
            fputc('\n', stdout);
    } else if (ex == 0) {
        fprintf(stdout, "Done.\n");
    }
    if (ex != 0)
        fprintf(stderr, "[tool: /bin/sh exit=%d]\n", ex);
    return 0;
}

int main(int argc, char **argv) {
    cos_ollama_autodetect_apply_env();

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
    int     want_icl           = 0;
    int     icl_k              = 3;
    int     icl_rethink        = 0;
    int     icl_user_disabled  = 0;

    /* FINAL-2 feature toggles (all off by default to keep existing
     * harnesses byte-identical; opt-in via flags or env). */
    int         multi_sigma        = 0;
    int         multi_sigma_cli    = 0; /* argv --multi-sigma only (not --verbose) */
    int         want_ttt           = 0;
    int         conformal_enabled  = 1;  /* auto-load bundle if present */
    int         coherence_enabled  = 1;  /* REPL-only; --once is a single
                                            turn so it has no trend.   */
    const char *calibration_path_arg = NULL;
    int         tools_mode       = 0;
    int         tools_self_test  = 0;
    int         tools_dry_run    = 0;
    int         semantic_cache   = 0;
    int         cascade          = 0;
    int         cross_model_sigma = 0;
    int         semantic_lru     = 0;
    int         speculative_sigma = 0;
    int         knowledge_graph  = 0;
    int         spike_mode        = 0;
    int         adaptive_compute   = 0;
    int         energy_report      = 0;
    int         want_proof_receipt = 0;
    int         semantic_entropy     = 0;
    int         no_semantic_entropy  = 0;
    int         semantic_sigma       = 0;
    int         no_semantic_sigma    = 0;
    int         stream_cli         = -1; /* unset: REPL on, --once off */
    int         tui_cli            = -1; /* unset: TTY REPL on, --once/pipe off */
    int         use_tui            = 0;
    int         spec_decode        = 0;
    int         fast_sigma         = 0;
    int         adaptive_tau_csv   = 0;
    const char *adaptive_csv_path = NULL;
    const char *domain             = "general";

    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--no-codex")   == 0) use_codex = 0;
        else if (strcmp(argv[i], "--codex-seed") == 0) { use_codex = 1; use_seed = 1; }
        else if (strcmp(argv[i], "--codex-path") == 0 && i + 1 < argc) codex_path = argv[++i];
        else if (strcmp(argv[i], "--local-only") == 0) local_only = 1;
        else if (strcmp(argv[i], "--swarm")      == 0) swarm_mode = 1;
        else if (strcmp(argv[i], "--verbose")    == 0) verbose = 1;
        else if (strcmp(argv[i], "--banner-only")== 0) banner_only = 1;
        else if (strcmp(argv[i], "--stream")     == 0) stream_cli = 1;
        else if (strcmp(argv[i], "--no-stream")  == 0) stream_cli = 0;
        else if (strcmp(argv[i], "--tui")       == 0) tui_cli = 1;
        else if (strcmp(argv[i], "--no-tui")    == 0) tui_cli = 0;
        else if (strcmp(argv[i], "--speculative-decode") == 0
                 || strcmp(argv[i], "--spec") == 0)
            spec_decode = 1;
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
        }         else if (strcmp(argv[i], "--tau-rethink")== 0 && i + 1 < argc) {
            tau_rethink = (float)atof(argv[++i]);
            have_tau_rethink = 1;
        } else if (strcmp(argv[i], "--icl") == 0) {
            want_icl = 1;
        } else if (strcmp(argv[i], "--no-icl") == 0) {
            want_icl = 0;
            icl_user_disabled = 1;
        } else if (strcmp(argv[i], "--icl-k") == 0 && i + 1 < argc) {
            icl_k = atoi(argv[++i]);
            if (icl_k < 1) icl_k = 1;
            if (icl_k > 8) icl_k = 8;
        } else if (strcmp(argv[i], "--icl-rethink-only") == 0) {
            icl_rethink = 1;
        }         else if (strcmp(argv[i], "--multi-sigma") == 0) {
            multi_sigma     = 1;
            multi_sigma_cli = 1;
        } else if (strcmp(argv[i], "--fast") == 0) {
            fast_sigma = 1;
        } else if (strcmp(argv[i], "--adaptive-tau") == 0) {
            adaptive_tau_csv = 1;
        } else if (strcmp(argv[i], "--adaptive-tau-csv") == 0
                   && i + 1 < argc) {
            adaptive_csv_path = argv[++i];
            adaptive_tau_csv  = 1;
        } else if (strcmp(argv[i], "--domain") == 0 && i + 1 < argc) {
            domain = argv[++i];
        } else if (strcmp(argv[i], "--ttt") == 0) {
            want_ttt = 1;
        } else if (strcmp(argv[i], "--no-multi-sigma") == 0) {
            multi_sigma     = 0;
            multi_sigma_cli = 0;
        } else if (strcmp(argv[i], "--semantic-entropy") == 0) {
            semantic_entropy  = 1;
            no_semantic_entropy = 0;
        } else if (strcmp(argv[i], "--no-semantic-entropy") == 0) {
            semantic_entropy    = 0;
            no_semantic_entropy = 1;
        } else if (strcmp(argv[i], "--semantic-sigma") == 0) {
            semantic_sigma      = 1;
            semantic_entropy      = 0;
            no_semantic_sigma     = 0;
        } else if (strcmp(argv[i], "--no-semantic-sigma") == 0) {
            semantic_sigma     = 0;
            no_semantic_sigma   = 1;
        } else if (strcmp(argv[i], "--conformal") == 0) {
            conformal_enabled = 1;
        } else if (strcmp(argv[i], "--no-conformal") == 0) {
            conformal_enabled = 0;
        } else if (strcmp(argv[i], "--coherence") == 0) {
            coherence_enabled = 1;
        } else if (strcmp(argv[i], "--no-coherence") == 0) {
            coherence_enabled = 0;
        } else if (strcmp(argv[i], "--calibration-path") == 0 && i + 1 < argc) {
            calibration_path_arg = argv[++i];
            conformal_enabled = 1;
        } else if (strcmp(argv[i], "--tools") == 0) {
            tools_mode = 1;
        } else if (strcmp(argv[i], "--tools-self-test") == 0) {
            tools_self_test = 1;
        } else if (strcmp(argv[i], "--tools-dry-run") == 0) {
            tools_dry_run = 1;
        } else if (strcmp(argv[i], "--semantic-cache") == 0) {
            semantic_cache = 1;
        } else if (strcmp(argv[i], "--cascade") == 0) {
            cascade = 1;
        } else if (strcmp(argv[i], "--cross-model-sigma") == 0) {
            cross_model_sigma = 1;
        } else if (strcmp(argv[i], "--semantic-lru") == 0) {
            semantic_lru = 1;
        } else if (strcmp(argv[i], "--speculative") == 0) {
            speculative_sigma = 1;
        } else if (strcmp(argv[i], "--graph") == 0) {
            knowledge_graph = 1;
        } else if (strcmp(argv[i], "--spike") == 0) {
            spike_mode = 1;
        } else if (strcmp(argv[i], "--adaptive") == 0) {
            adaptive_compute = 1;
        } else if (strcmp(argv[i], "--energy") == 0) {
            energy_report = 1;
        } else if (strcmp(argv[i], "--receipt") == 0) {
            want_proof_receipt = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            fprintf(stdout,
                "cos chat — σ-gated interactive inference\n"
                "  --no-codex          start without Codex as system prompt\n"
                "  --codex-seed        load the compact seed Codex form\n"
                "  --codex-path PATH   load a user-supplied system prompt\n"
                "  --local-only        never escalate to cloud\n"
                "  --swarm             cloud tier uses swarm consensus (stub)\n"
                "  --verbose           print a per-turn diagnostic\n"
                "  --stream            stream tokens (default: on in REPL)\n"
                "  --no-stream         full response at once (default with --once)\n"
                "  --tui               ANSI live layout (default on interactive TTY)\n"
                "  --no-tui            plain transcript text\n"
                "  --spec|--speculative-decode  dual-model σ decode (2B draft / 9B verify)\n"
                "  --tau-accept  F     override τ_accept (default 0.40 interactive,\n"
                "                      0.30 for --once to match Python harness)\n"
                "  --tau-rethink F     override τ_rethink (default 0.60 / 0.70)\n"
                "  --once              one headless turn (needs --prompt)\n"
                "  --prompt TEXT       prompt for --once\n"
                "  --transcript PATH   JSONL transcript for --once\n"
                "  --no-transcript     skip JSONL for --once\n"
                "  --max-tokens N      accepted for argv parity (ignored)\n"
                "  --banner-only       print banner and exit (CI hook)\n"
                "  --icl               prepend few-shot exemplars from ~/.cos/engram.db\n"
                "  --no-icl            disable ICL even if COS_ENGRAM_ICL=1\n"
                "  --icl-k N           number of exemplars (default 3, max 8)\n"
                "  --icl-rethink-only  inject ICL only on RETHINK rounds (round>0)\n"
                "  --multi-sigma       σ_combined shadow + Jaccard semantic σ\n"
                "                      (primary + HTTP samples @ 0.7 / 1.2) unless --fast\n"
                "  --fast              with --multi-sigma + HTTP: logprob-only shadow\n"
                "                      (skip semantic multi-sample completes)\n"
                "  --ttt               ULTRA-8: test-time sketch on RETHINK (Qwen path)\n"
                "  --no-multi-sigma    disable the ensemble shadow (default)\n"
                "  --semantic-entropy  after inference: 3 temps → cluster → σ_se\n"
                "  --no-semantic-entropy  disable (overrides COS_CHAT_SEMANTIC_ENTROPY=1)\n"
                "  --semantic-sigma    force Jaccard semantic σ path (same as --multi-sigma\n"
                "                      without --fast); env COS_CHAT_SEMANTIC_SIGMA=1\n"
                "  --no-semantic-sigma disable (overrides COS_CHAT_SEMANTIC_SIGMA=1)\n"
                "  --conformal         load τ from ~/.cos/calibration.json (default)\n"
                "  --no-conformal      always use the static τ_accept flag\n"
                "  --calibration-path PATH  override the conformal bundle path\n"
                "  --adaptive-tau        load τ_accept from graded conformal CSV\n"
                "                        (COS_ADAPTIVE_TAU_CSV or repo path; ~90%% row)\n"
                "  --adaptive-tau-csv PATH  explicit conformal_thresholds.csv\n"
                "  --domain MODE         scale τ: medical|creative|code|general\n"
                "  --coherence         track per-session σ window (ULTRA-9, default)\n"
                "  --no-coherence      disable the session coherence summary\n"
                "  --tools             σ-gated shell tool REPL (HORIZON-1)\n"
                "  --tools-self-test   20-case classifier regression (exit 0/1)\n"
                "  --tools-dry-run     with --tools: gate only, never exec\n"
                "  --semantic-cache    BSC Hamming semantic inference cache (~/.cos)\n"
                "  --semantic-lru      exact-match LRU on normalized prompt (1h TTL)\n"
                "  --cascade           tiered Ollama models (0.6b→4b→3b) + intra-tier σ\n"
                "  --cross-model-sigma three-model Jaccard σ (replaces same-model samples)\n"
                "  --speculative       predict σ early (skip-verify / early abstain)\n"
                "  --spike             neuromorphic σ-channel spike layer (stable σ → cheap path)\n"
                "  --adaptive          σ-guided compute budget (kernels / rethink / TTT caps)\n"
                "  --energy            print [compute: … kernels … mJ] receipt line\n"
                "  --receipt           print SHA-256 proof receipt JSON for each output\n"
                "  --graph             after each turn, extract relations to σ-knowledge graph\n"
                "  --help              this text\n"
                "\n"
                "  CREATION_OS_BITNET_EXE  optional path to local inference binary\n"
                "                          (see src/import/bitnet_spawn.h).\n"
                "  COS_ENGRAM_ICL=1        same as --icl (overridden by --no-icl)\n"
                "  COS_ENGRAM_ICL_N=3      exemplar count\n"
                "  COS_ENGRAM_ICL_RETHINK=1   same as --icl-rethink-only\n"
                "  COS_ENGRAM_ICL_TAU=0.35    max σ for an exemplar row (default τ_accept)\n"
                "  COS_TOOL_TAU_LOW/HIGH      tool gate thresholds (default 0.35 / 0.65)\n"
                "  COS_TOOLS_ASSUME_YES=1     auto-answer CONFIRM as yes\n"
                "  COS_INFERENCE_CACHE_CAP    ring slots for semantic cache (default 512)\n"
                "  COS_SEMANTIC_LRU_CACHE=0   disable --semantic-lru store/lookup\n"
                "  COS_CROSS_MODEL_A/B/C    override cross-model ids (defaults Gemma/Llama/Qwen)\n"
                "  COS_CASCADE_MODEL_0/1/2  override cascade tier models\n"
                "  COS_CASCADE_TAU_0/1/2     per-tier accept thresholds\n"
                "  COS_KG_ENABLE=1            same as --graph (per-turn graph extract)\n"
                "  COS_CHAT_SEMANTIC_ENTROPY=1  same as --semantic-entropy (unless --no-…)\n"
                "  COS_SEMANTIC_ENTROPY_MODE   replace | blend | max (default replace)\n"
                "  COS_SEMANTIC_ENTROPY_METRIC jaccard (default) or bsc (BSC + Hamming)\n"
                "  COS_CHAT_SEMANTIC_SIGMA=1   same as --semantic-sigma (wins over entropy)\n"
                "  COS_ADAPTIVE_TAU=1          same as --adaptive-tau (graded CSV τ)\n"
                "  COS_ADAPTIVE_TAU_CSV        path to conformal_thresholds.csv\n"
                "  CREATION_OS_ROOT            repo root for default graded CSV path\n"
                "  COS_ADAPTIVE_TAU_TEMPERATURE_BLEND=1  nudge τ using graded temperature.txt\n"
                "  COS_ADAPTIVE_TEMPERATURE_TXT  override path to temperature.txt\n"
                "  COS_ADAPTIVE_TAU_PATTERNS=1   τ from ~/.cos/patterns.json by prompt domain\n"
                "  COS_CHAT_TOKEN_JSONL=1        append tokens/query JSONL to ~/.cos/logs/chat_tokens.jsonl\n"
                "  COS_SIGMA_DRIFT_BASELINE_MEAN / _STD  optional σ drift z-score baseline\n");
            return 0;
        }
    }

    {
        int stream_on =
            (stream_cli >= 0) ? stream_cli : (once_mode ? 0 : 1);
        (void)setenv("COS_BITNET_STREAM", stream_on ? "1" : "0", 1);
    }

    {
        int tty_in  = isatty(STDIN_FILENO);
        int tty_out = isatty(STDOUT_FILENO);
        int def_tui = tty_in && tty_out && !once_mode;
        use_tui     = (tui_cli >= 0) ? tui_cli : def_tui;
        (void)setenv("COS_CHAT_TUI", use_tui ? "1" : "0", 1);
    }

    if (spec_decode) {
        (void)setenv("COS_SPEC_DECODE", "1", 1);
        cos_spec_decode_init(NULL);
    } else {
        (void)unsetenv("COS_SPEC_DECODE");
    }

    {
        const char *kge = getenv("COS_KG_ENABLE");
        if (kge != NULL && kge[0] == '1')
            knowledge_graph = 1;
    }

    if (tools_self_test) {
        int trc = cos_sigma_tools_self_test();
        if (trc != 0)
            fprintf(stderr, "cos chat: --tools-self-test FAILED (rc=%d)\n", trc);
        return trc != 0 ? 1 : 0;
    }

    if (tools_mode) {
        float tl = 0.35f, th = 0.65f;
        cos_sigma_tool_thresholds_default(&tl, &th);
        if (banner_only) {
            fprintf(stdout,
                    "Creation OS · σ-gated chat\n"
                    "  HORIZON-1 · σ_tool gate  ·  τ_low=%.2f  τ_high=%.2f\n"
                    "  assert(declared == realized);\n"
                    "  1 = 1.\n",
                    (double)tl, (double)th);
            return 0;
        }
        if (once_mode) {
            if (once_prompt == NULL || once_prompt[0] == '\0') {
                fprintf(stderr, "cos chat: --tools --once requires --prompt\n");
                return 2;
            }
            chat_tools_dispatch_line(once_prompt, tools_dry_run, stdin);
            return 0;
        }
        fprintf(stdout,
                "Creation OS · σ-gated tools (HORIZON-1)\n"
                "  Natural language:  \"List files in /tmp\"\n"
                "                     \"Delete all files in /home\"\n"
                "                     \"Write hello to /tmp/test.txt\"\n"
                "  Or type a shell command.  exit | quit to end.\n"
                "  CONFIRM tier: answer y/N (or COS_TOOLS_ASSUME_YES=1).\n");
        char line[2048];
        for (;;) {
            fprintf(stdout, "\n> ");
            fflush(stdout);
            if (fgets(line, sizeof line, stdin) == NULL) break;
            size_t n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                line[--n] = '\0';
            if (n == 0) continue;
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
            if (strcmp(line, "help") == 0) {
                fprintf(stdout,
                        "  σ_tool gate: READ < WRITE < DELETE < EXEC < NETWORK\n"
                        "  AUTO / CONFIRM / BLOCKED from τ_low / τ_high\n");
                continue;
            }
            chat_tools_dispatch_line(line, tools_dry_run, stdin);
        }
        fprintf(stdout, "\n  assert(declared == realized);\n  1 = 1.\n");
        return 0;
    }

    if (once_mode) {
        if (!have_tau_accept)  tau_accept  = 0.30f;
        if (!have_tau_rethink) tau_rethink = 0.70f;
    }

    /* NEXT-1: --verbose implies the σ_combined shadow ensemble in both
     * REPL and --once paths so the full ULTRA stack is visible with one
     * flag.  Users who want the ensemble without the verbose banner can
     * still pass --multi-sigma explicitly. */
    if (verbose) multi_sigma = 1;

    if (!no_semantic_entropy) {
        const char *se = getenv("COS_CHAT_SEMANTIC_ENTROPY");
        if (se != NULL && se[0] == '1' && se[1] == '\0')
            semantic_entropy = 1;
    }
    if (!no_semantic_sigma) {
        const char *ss = getenv("COS_CHAT_SEMANTIC_SIGMA");
        if (ss != NULL && ss[0] == '1' && ss[1] == '\0')
            semantic_sigma = 1;
    }
    if (semantic_sigma)
        semantic_entropy = 0;

    if (cross_model_sigma) {
        semantic_sigma     = 1;
        semantic_entropy   = 0;
        no_semantic_sigma  = 0;
    }

    if (multi_sigma_cli && !fast_sigma && !no_semantic_sigma) {
        semantic_sigma     = 1;
        semantic_entropy = 0;
    }

    /* FINAL-2 Phase B: resolve conformal τ (overrides static defaults
     * unless the user explicitly passed --tau-accept). */
    int   conformal_active = 0;
    float conformal_alpha  = 0.0f;
    if (conformal_enabled) {
        char path[1024];
        int have_path = 0;
        if (calibration_path_arg != NULL && calibration_path_arg[0] != '\0') {
            snprintf(path, sizeof(path), "%s", calibration_path_arg);
            have_path = 1;
        } else if (calibration_default_path(path, sizeof(path)) == 0) {
            have_path = 1;
        }
        if (have_path) {
            float ct = 0.0f, ca = 0.0f;
            char  cd[COS_CONFORMAL_NAME_MAX];
            cd[0] = '\0';
            if (load_conformal_tau(path, &ct, &ca, cd, sizeof(cd)) == 0) {
                conformal_active = 1;
                conformal_alpha  = ca;
                /* Only override τ_accept when the user did not pin it
                 * from the CLI; otherwise an explicit flag wins. */
                if (!have_tau_accept) tau_accept = ct;
            }
        }
    }

    {
        const char *atau = getenv("COS_ADAPTIVE_TAU");
        if (atau != NULL && atau[0] == '1' && atau[1] == '\0')
            adaptive_tau_csv = 1;
    }

    if (adaptive_tau_csv && !have_tau_accept) {
        const char *usep = adaptive_csv_path;
        char        default_csv[1024];

        if (usep == NULL || usep[0] == '\0') {
            if (cos_adaptive_tau_default_csv_path(default_csv,
                                                  sizeof(default_csv))
                == 0)
                usep = default_csv;
        }
        if (usep != NULL && usep[0] != '\0') {
            tau_accept = cos_adaptive_tau_accept_from_csv(usep, 0.3f);
            if (!have_tau_rethink)
                tau_rethink =
                    cos_adaptive_tau_rethink_from_accept(tau_accept);
            conformal_active = 0;
        } else {
            fprintf(stderr,
                    "cos chat: adaptive τ enabled but no CSV path "
                    "(use --adaptive-tau-csv PATH or COS_ADAPTIVE_TAU_CSV / "
                    "CREATION_OS_ROOT)\n");
        }
    }

    if (!adaptive_tau_csv && !have_tau_accept) {
        char        csv[1024];
        struct stat st;
        if (cos_adaptive_tau_default_csv_path(csv, sizeof csv) == 0
            && stat(csv, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            tau_accept = cos_adaptive_tau_accept_from_csv(csv, 0.3f);
            if (!have_tau_rethink)
                tau_rethink =
                    cos_adaptive_tau_rethink_from_accept(tau_accept);
        }
    }

    if (!icl_user_disabled) {
        const char *env_icl = getenv("COS_ENGRAM_ICL");
        if (env_icl != NULL && env_icl[0] == '1') want_icl = 1;
    }

    /* Codex load (optional). */
    cos_sigma_codex_t codex;
    memset(&codex, 0, sizeof(codex));
    int have_codex = 0;
    if (use_codex) {
        int rc;
        if (codex_path != NULL)
            rc = cos_sigma_codex_load(codex_path, &codex);
        else {
            const char *ce = getenv("COS_CODEX_PATH");
            if (ce != NULL && ce[0] != '\0')
                rc = cos_sigma_codex_load(ce, &codex);
            else if (use_seed)
                rc = cos_sigma_codex_load_seed(&codex);
            else
                rc = cos_sigma_codex_load(NULL, &codex);
        }
        if (rc == 0) have_codex = 1;
        else fprintf(stderr, "warning: codex load failed (rc=%d) — "
                            "continuing without soul\n", rc);
    }

    (void)cos_constitution_init("data/codex/atlantean_codex_compact.txt");

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

    /* OpenAI-compat HTTP (Ollama / llama-server): the primary path already
     * uses cos_bitnet_server_complete.  HYBRID + no API keys would call
     * cos_cli_stub_escalate and replace the real model output with a fake
     * cloud string — force LOCAL_ONLY unless swarm (cloud consensus). */
    if (cos_cli_use_bitnet_http() != 0 && !swarm_mode)
        local_only = 1;

    if (domain == NULL || domain[0] == '\0')
        domain = "general";
    if (strcmp(domain, "medical") != 0 && strcmp(domain, "creative") != 0
        && strcmp(domain, "code") != 0 && strcmp(domain, "general") != 0) {
        fprintf(stderr,
                "cos chat: unknown --domain \"%s\" "
                "(use medical|creative|code|general)\n",
                domain);
        domain = "general";
    }
    if (strcmp(domain, "medical") == 0) {
        tau_accept *= 0.5f;
        tau_rethink *= 0.7f;
    } else if (strcmp(domain, "creative") == 0) {
        tau_accept *= 1.5f;
        tau_rethink *= 1.3f;
    } else if (strcmp(domain, "code") == 0) {
        tau_accept *= 0.625f;
        tau_rethink *= 0.85f;
    }
    if (tau_accept < 0.01f)
        tau_accept = 0.01f;
    if (tau_accept > 0.99f)
        tau_accept = 0.99f;
    if (tau_rethink <= tau_accept)
        tau_rethink = tau_accept + 0.01f;
    if (tau_rethink > 0.99f)
        tau_rethink = 0.99f;

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
    /* DEV-6 + AGI-1: always pass a tagged `cos_cli_generate_ctx_t` so
     * the backend can attach Codex + optional engram-driven ICL. */
    cos_cli_generate_ctx_t genctx;
    memset(&genctx, 0, sizeof(genctx));
    genctx.magic                   = COS_CLI_GENERATE_CTX_MAGIC;
    genctx.codex                   = have_codex ? &codex : NULL;
    genctx.persist                 = persist;
    genctx.runtime_engram          = &engram;
    genctx.pipeline_local_only     = local_only ? 1 : 0;
    genctx.icl_exemplar_max_sigma  = tau_accept;
    const char *icl_tau_env = getenv("COS_ENGRAM_ICL_TAU");
    if (icl_tau_env != NULL && icl_tau_env[0] != '\0')
        genctx.icl_exemplar_max_sigma = (float)atof(icl_tau_env);
    genctx.icl_k                   = 0;
    genctx.icl_rethink_only        = icl_rethink;
    genctx.icl_compose             = chat_icl_compose;
    genctx.icl_ctx                 = persist;
    const char *env_icln = getenv("COS_ENGRAM_ICL_N");
    if (env_icln != NULL && env_icln[0] != '\0') {
        int n = atoi(env_icln);
        if (n >= 1 && n <= 8) icl_k = n;
    }
    const char *env_iclr = getenv("COS_ENGRAM_ICL_RETHINK");
    if (env_iclr != NULL && env_iclr[0] == '1') genctx.icl_rethink_only = 1;
    if (want_icl && persist != NULL) {
        genctx.icl_k = icl_k;
    } else {
        genctx.icl_k = 0;
    }
    cfg.generate_ctx = &genctx;
    cfg.escalate_ctx   = &genctx;
    /* DEV-5: dispatch via the API escalation module.  When
     * COS_ESCALATION_BACKEND / CREATION_OS_ESCALATION_PROVIDER + keys
     * are set, this reaches out to Claude / OpenAI / DeepSeek over
     * HTTPS and appends a (student → teacher) distill pair to
     * ~/.cos/distill_pairs.jsonl.  If COS_DEEPSEEK_API_KEY is set and
     * no explicit backend is chosen, DeepSeek V4 (deepseek-chat) is
     * the default teacher.  With no keys, cos_cli_escalate_api falls
     * through to cos_cli_stub_escalate so CI stays deterministic. */
    cfg.escalate     = cos_cli_escalate_api;
    if (persist != NULL) {
        cfg.on_engram_store     = cos_engram_persist_store;
        cfg.on_engram_store_ctx = persist;
    }

    cfg.ttt_enabled = want_ttt ? 1 : 0;
    if (want_ttt) {
        setenv("COS_CHAT_TTT", "1", 1);
        cos_ttt_set_verbose(verbose ? 1 : 0);
    } else {
        unsetenv("COS_CHAT_TTT");
        cos_ttt_set_verbose(0);
    }

    if (semantic_cache) {
        int cap = 512;
        const char *ce = getenv("COS_INFERENCE_CACHE_CAP");
        if (ce != NULL && ce[0] != '\0')
            cap = atoi(ce);
        if (cap < 32)
            cap = 32;
        if (cos_inference_cache_init(cap) != 0)
            fprintf(stderr,
                    "warning: semantic inference cache init failed — "
                    "CACHE_SEMANTIC disabled this session\n");
    }

    if (spike_mode || adaptive_compute) {
        cos_spike_engine_init(&g_cos_spike);
        {
            char sspath[512];
            if (cos_chat_spike_snapshot_path(sspath, sizeof sspath) == 0)
                (void)cos_spike_engine_snapshot_read(&g_cos_spike, sspath);
        }
        g_cos_spike_ready = 1;
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
        const char *backend = "stub";
        if (exe != NULL && exe[0] != '\0')
            backend = "bridge";
        else if (cos_cli_use_bitnet_http() != 0)
            backend = "external";
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
        chat_feat_t feat;
        memset(&feat, 0, sizeof(feat));
        feat.multi_sigma        = multi_sigma;
        feat.conformal_active   = conformal_active;
        feat.conformal_alpha    = conformal_alpha;
        feat.verbose              = verbose;
        feat.coherence_active     = 0; /* single-turn: no trend to report */
        feat.coh                  = NULL;
        feat.semantic_cache       = semantic_cache;
        feat.speculative_sigma    = speculative_sigma;
        feat.knowledge_graph     = knowledge_graph;
        feat.spike_mode          = spike_mode;
        feat.adaptive_compute     = adaptive_compute;
        feat.energy_report        = energy_report;
        feat.proof_receipt_echo   = want_proof_receipt;
        feat.semantic_entropy     = semantic_entropy;
        feat.semantic_sigma       = semantic_sigma;
        feat.fast_sigma           = fast_sigma;
        feat.cascade              = cascade;
        feat.cross_model_sigma    = cross_model_sigma;
        feat.semantic_lru         = semantic_lru;
        int rc = run_chat_once(tout, &cfg, &feat, once_prompt);
        if (tout != NULL)
            fclose(tout);
        if (g_ag_ledger_ready) {
            char slp[512];
            if (cos_state_ledger_default_path(slp, sizeof(slp)) == 0)
                cos_state_ledger_persist(&g_ag_ledger, slp);
            cos_engram_consolidate(50000);
        }
        cos_sigma_pipeline_free_engram_values(&engram);
        cos_sigma_codex_free(&codex);
        cos_engram_persist_close(persist);
        return rc;
    }

    if (!use_tui) {
        print_banner(&cfg, have_codex ? &codex : NULL);
        if (conformal_active) {
            fprintf(stdout,
                    "  conformal: τ=%.4f @ α=%.2f (from ~/.cos/calibration.json)\n",
                    (double)tau_accept, (double)conformal_alpha);
        }
        if (multi_sigma) {
            fprintf(stdout,
                    "  σ-ensemble: shadow (SCI-5), K_regen=%d per turn\n",
                    COS_CHAT_MULTI_K_REGEN);
        }
        if (strcmp(domain, "general") != 0) {
            fprintf(stdout, "  domain: %s (τ scaled)\n", domain);
        }
        if (adaptive_tau_csv && !have_tau_accept) {
            fprintf(stdout,
                    "  adaptive τ: graded conformal CSV (~90%% target row)\n");
        }
    } else {
        cos_tui_init();
        cos_tui_clear();
        cos_tui_header(0.f, 1.f);
        cos_tui_status_bar(0, 0, 0.f, 0.f);
    }

    chat_coh_t coh;
    coh_reset(&coh);

    char   line[2048];
    int    turn              = 0;
    float  sigma_sess_sum    = 0.f;
    int    sigma_sess_n      = 0;
    int    cache_sess_hits   = 0;
    int    sig_tty = (isatty(fileno(stdin)) != 0);
    if (sig_tty) {
        g_cos_chat_prev_sigint = signal(SIGINT, cos_chat_sigint_handler);
        g_cos_chat_sigint      = 0;
    }
    for (;;) {
        if (!use_tui) {
            fprintf(stdout, "\n> ");
            fflush(stdout);
        } else {
            fputs("\033[0m│ You: ", stdout);
            fflush(stdout);
        }
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

        if (strcmp(line, "help") == 0) {
            fprintf(stdout,
                "  commands:\n"
                "    exit | quit         end session + summary\n"
                "    cost                print sovereign ledger\n"
                "    status              print config + engram stats\n"
                "    help                this text\n"
                "  receipt glossary:\n"
                "    ACCEPT / RETHINK ×N / ABSTAIN   terminal action\n"
                "    CACHE / FRESH                   engram hit or miss\n"
                "    LOCAL / CLOUD                   where inference ran\n"
                "    conformal@α=…                   SCI-1 bundle active\n");
            continue;
        }

        turn++;
        uint64_t tph = cos_engram_prompt_hash(line);
        float    tau_sv = cfg.tau_accept;
        float    tloc = cos_engram_get_local_tau(tph);
        if (tloc > 0.f && tloc < 1.f)
            cfg.tau_accept = tloc;
        if (!have_tau_accept) {
            const char *tp = getenv("COS_ADAPTIVE_TAU_PATTERNS");
            if (tp != NULL && tp[0] == '1' && tp[1] == '\0') {
                float pt = cos_adaptive_tau_for_prompt_domain(line,
                                                              cfg.tau_accept);
                if (pt > 0.f && pt < 1.f)
                    cfg.tau_accept = pt;
            }
        }

        uint64_t bsc_prompt[COS_INF_W];
        cos_inference_bsc_encode_prompt(line, bsc_prompt);
        uint64_t domain_h = cos_inference_prompt_fnv(line);

        chat_feat_t rfeat;
        memset(&rfeat, 0, sizeof(rfeat));
        rfeat.multi_sigma       = (multi_sigma || verbose) ? 1 : 0;
        rfeat.verbose           = verbose;
        rfeat.conformal_active  = conformal_active;
        rfeat.conformal_alpha   = conformal_alpha;
        rfeat.semantic_cache    = semantic_cache;
        rfeat.speculative_sigma = speculative_sigma;
        rfeat.knowledge_graph   = knowledge_graph;
        rfeat.spike_mode        = spike_mode;
        rfeat.adaptive_compute  = adaptive_compute;
        rfeat.energy_report        = energy_report;
        rfeat.proof_receipt_echo   = want_proof_receipt;
        rfeat.semantic_entropy     = semantic_entropy;
        rfeat.semantic_sigma       = semantic_sigma;
        rfeat.fast_sigma           = fast_sigma;
        rfeat.cascade              = cascade;
        rfeat.cross_model_sigma    = cross_model_sigma;
        rfeat.semantic_lru         = semantic_lru;

        cos_pipeline_result_t r;
        memset(&r, 0, sizeof(r));

        chat_sigma_layer_t SL;
        int early_spec_abstain = 0;
        int prep =
            chat_sigma_layer_prepare(&cfg, &rfeat, bsc_prompt, domain_h, &SL);
        if (prep != 0) {
            early_spec_abstain          = 1;
            r.response                  = COS_CHAT_PIPELINE_ABSTAIN;
            r.sigma                     = SL.ps.predicted_sigma;
            r.engram_hit                = 0;
            r.rethink_count             = 0;
            r.escalated                 = 0;
            r.cost_eur                  = 0.0;
            r.final_action              = COS_SIGMA_ACTION_ABSTAIN;
            r.agent_gate                = COS_AGENT_ALLOW;
            r.diagnostic                = "speculative_sigma_early_abstain";
            r.mode                      = cfg.mode;
            r.codex_hash                = cfg.codex != NULL ? cfg.codex->hash_fnv1a64
                                                          : 0ULL;
            r.ttt_applied               = 0;
        }

        int skip_multi_verify =
            early_spec_abstain ? 1 : SL.skip_multi_verify;
        int kernel_cap = SL.bu.kernels_to_run;

        int semantic_hit = 0;
        int cascade_done = 0;
        g_cos_chat_semantic_phase_ms = 0.0;

        double w0 = 0.0, w1 = 0.0;
        double  elapsed_ms = 0.0;
        int     prc        = 0;

        if (!early_spec_abstain && !semantic_hit && semantic_lru) {
            uint64_t hlr = cos_semantic_cache_prompt_hash(line);
            float    cs  = cos_semantic_cache_lookup(
                hlr, g_cos_sem_lru_buf, sizeof g_cos_sem_lru_buf);
            if (cs >= 0.0f) {
                semantic_hit          = 1;
                r.response            = g_cos_sem_lru_buf;
                r.sigma               = cs;
                r.engram_hit          = 0;
                r.rethink_count       = 0;
                r.escalated           = 0;
                r.cost_eur            = 0.0;
                r.final_action        = COS_SIGMA_ACTION_ACCEPT;
                r.agent_gate          = COS_AGENT_ALLOW;
                r.diagnostic          = "semantic_lru_cache";
                r.mode                = cfg.mode;
                r.codex_hash          = cfg.codex != NULL ? cfg.codex->hash_fnv1a64
                                                         : 0ULL;
                r.ttt_applied         = 0;
                fprintf(use_tui ? stderr : stdout,
                        "[CACHE_LRU] hit σ=%.3f (normalized prompt)\n",
                        (double)cs);
            }
        }

        if (!early_spec_abstain && !semantic_hit && semantic_cache) {
            struct cos_inference_cache_entry hit;
            if (cos_inference_cache_lookup(bsc_prompt, COS_INF_W, &hit) != 0) {
                semantic_hit       = 1;
                r.response         = hit.response;
                r.sigma            = hit.sigma;
                r.engram_hit       = 0;
                r.rethink_count    = 0;
                r.escalated        = 0;
                r.cost_eur         = 0.0;
                r.final_action     = COS_SIGMA_ACTION_ACCEPT;
                r.agent_gate       = COS_AGENT_ALLOW;
                r.diagnostic       = "semantic_inference_cache";
                r.mode             = cfg.mode;
                r.codex_hash       = cfg.codex != NULL
                                         ? cfg.codex->hash_fnv1a64
                                         : 0ULL;
                r.ttt_applied      = 0;
                fprintf(use_tui ? stderr : stdout,
                        "[CACHE_SEMANTIC] hit  σ=%.3f  (BSC neighbour reuse)\n",
                        (double)hit.sigma);
            }
        }

        if (!early_spec_abstain && !semantic_hit && spike_mode && !SL.spike_any
            && g_spike_have_last) {
            float       hm = cos_inference_hamming_norm(bsc_prompt,
                                                        g_spike_last_bsc,
                                                        COS_INF_W);
            float       tau_h = 0.08f;
            const char *eh    = getenv("COS_SPIKE_HAMMING_MAX");
            if (eh != NULL && eh[0] != '\0')
                tau_h = (float)atof(eh);
            if (hm <= tau_h && SL.ps.predicted_sigma < 0.35f
                && g_spike_last_resp[0] != '\0') {
                semantic_hit = 1;
                snprintf(g_spike_ham_buf, sizeof g_spike_ham_buf, "%s",
                         g_spike_last_resp);
                r.response         = g_spike_ham_buf;
                r.sigma            = SL.ps.predicted_sigma;
                r.engram_hit       = 0;
                r.rethink_count    = 0;
                r.escalated        = 0;
                r.cost_eur         = 0.0;
                r.final_action     = COS_SIGMA_ACTION_ACCEPT;
                r.agent_gate       = COS_AGENT_ALLOW;
                r.diagnostic       = "spike_hamming_cache";
                r.mode             = cfg.mode;
                r.codex_hash       = cfg.codex != NULL ? cfg.codex->hash_fnv1a64
                                                       : 0ULL;
                r.ttt_applied      = 0;
                fprintf(use_tui ? stderr : stdout,
                        "[CACHE_SPIKE] hit  hamming_norm=%.4f  σ=%.3f\n",
                        (double)hm, (double)r.sigma);
            }
        }

        if (!early_spec_abstain && !semantic_hit && cascade
            && cos_cli_use_bitnet_http() != 0) {
            if (cfg.engram != NULL) {
                cos_sigma_engram_tick(cfg.engram);
                cos_sigma_engram_entry_t eg;
                uint64_t                 eh = cos_sigma_engram_hash(line);
                if (cos_sigma_engram_get(cfg.engram, eh, &eg) == 1
                    && eg.value != NULL) {
                    snprintf(g_cos_eng_cascade_buf, sizeof g_cos_eng_cascade_buf,
                             "%s", (const char *)eg.value);
                    r.response       = g_cos_eng_cascade_buf;
                    r.sigma          = eg.sigma_at_store;
                    r.engram_hit     = 1;
                    r.rethink_count  = 0;
                    r.escalated      = 0;
                    r.cost_eur       = 0.0;
                    r.final_action   = COS_SIGMA_ACTION_ACCEPT;
                    r.agent_gate     = COS_AGENT_ALLOW;
                    r.diagnostic     = "engram_hit";
                    r.mode           = cfg.mode;
                    r.codex_hash     = cfg.codex != NULL ? cfg.codex->hash_fnv1a64
                                                       : 0ULL;
                    r.ttt_applied    = 0;
                    semantic_hit     = 1;
                }
            }
            if (!semantic_hit) {
                if (use_tui && cos_chat_stream_env_on())
                    cos_tui_begin_ai_line();
                g_cos_chat_sigint = 0;
                cos_bitnet_server_io_cancel_clear();
                int         port = 0;
                const char *pe   = getenv("COS_BITNET_SERVER_PORT");
                if (pe != NULL && pe[0] != '\0')
                    port = atoi(pe);
                const char *sysp_c = NULL;
                if (cfg.codex != NULL && cfg.codex->bytes != NULL
                    && cfg.codex->size > 0)
                    sysp_c = cfg.codex->bytes;
                w0 = chat_wall_ms();
                float csig = 1.0f;
                int   tier = -1;
                int   cr   = cos_cascade_query(port, line, sysp_c, g_cos_cascade_buf,
                                             sizeof g_cos_cascade_buf, &csig,
                                             &tier);
                w1 = chat_wall_ms();
                if (cr != 0) {
                    chat_emit_pipeline_failure_help();
                    cfg.tau_accept = tau_sv;
                    if (SL.did_budget)
                        chat_sigma_layer_restore(&cfg, &SL);
                    continue;
                }
                prc            = 0;
                r.response     = g_cos_cascade_buf;
                r.sigma        = csig;
                r.engram_hit   = 0;
                r.rethink_count = 0;
                r.escalated    = 0;
                r.cost_eur     = 0.0001 * (double)(tier >= 0 ? (tier + 1) : 4);
                r.final_action = cos_sigma_reinforce_round(
                    csig, cfg.tau_accept, cfg.tau_rethink, r.rethink_count);
                r.agent_gate   = COS_AGENT_ALLOW;
                r.diagnostic   = (tier >= 0) ? "cascade_tier" : "cascade_abstain";
                r.mode         = cfg.mode;
                r.codex_hash   = cfg.codex != NULL ? cfg.codex->hash_fnv1a64
                                                   : 0ULL;
                r.ttt_applied  = 0;
                cascade_done   = 1;
            }
        }

        if (!early_spec_abstain && !semantic_hit && !cascade_done) {
            if (use_tui && cos_chat_stream_env_on())
                cos_tui_begin_ai_line();
            g_cos_chat_sigint = 0;
            cos_bitnet_server_io_cancel_clear();
            w0 = chat_wall_ms();
            prc = cos_sigma_pipeline_run(&cfg, line, &r);
            w1 = chat_wall_ms();
            if (g_cos_chat_sigint) {
                fprintf(use_tui ? stderr : stdout, "\n[cancelled]\n");
                g_cos_chat_sigint = 0;
                cos_bitnet_server_io_cancel_clear();
                cfg.tau_accept = tau_sv;
                if (SL.did_budget)
                    chat_sigma_layer_restore(&cfg, &SL);
                continue;
            }
        }

        if (use_tui && semantic_hit && r.response != NULL
            && r.final_action != COS_SIGMA_ACTION_ABSTAIN) {
            cos_tui_begin_ai_line();
            fputs(r.response, stdout);
            cos_tui_stream_newline();
        }

        if (SL.did_budget)
            chat_sigma_layer_restore(&cfg, &SL);

        cfg.tau_accept = tau_sv;

        if (!early_spec_abstain && !semantic_hit && !cascade_done && prc != 0) {
            chat_emit_pipeline_failure_help();
            continue;
        }

        if (!early_spec_abstain && !semantic_hit && !cascade_done) {
            elapsed_ms = w1 - w0;
            if (elapsed_ms < 0.0)
                elapsed_ms = 0.0;
        } else if (cascade_done && !semantic_hit) {
            elapsed_ms = w1 - w0;
            if (elapsed_ms < 0.0)
                elapsed_ms = 0.0;
        }

        if (energy_report || adaptive_compute)
            chat_compute_line(elapsed_ms, &SL.bu);

        if (!early_spec_abstain && semantic_cache && !semantic_hit && prc == 0
            && r.response != NULL)
            (void)cos_inference_cache_store_latency(
                bsc_prompt, r.response, r.sigma,
                (int64_t)(elapsed_ms + 0.5));

        if (!early_spec_abstain
            && (speculative_sigma || spike_mode || adaptive_compute))
            cos_speculative_sigma_observe(domain_h, r.sigma);

        uint64_t      resp_cache_h2 = cos_response_cache_prompt_hash(line);
        int           skipped_resp_sig_cache2 = 0;
        if (!early_spec_abstain && !semantic_hit && prc == 0
            && (rfeat.semantic_sigma || rfeat.semantic_entropy)
            && !rfeat.cascade) {
            float c2 = cos_response_cache_lookup(resp_cache_h2, NULL, 0);
            if (c2 >= 0.0f) {
                r.sigma        = c2;
                r.final_action = cos_sigma_reinforce_round(
                    c2, cfg.tau_accept, cfg.tau_rethink, r.rethink_count);
                skipped_resp_sig_cache2 = 1;
                fprintf(use_tui ? stderr : stdout,
                        "[response-σ-cache] hit σ=%.3f action=%s\n",
                        (double)c2, cos_sigma_action_label(r.final_action));
            }
        }

        if (!skipped_resp_sig_cache2 && !rfeat.cascade) {
            if (rfeat.semantic_sigma) {
                chat_maybe_semantic_sigma(&cfg, &rfeat, line, &r,
                                          early_spec_abstain, semantic_hit,
                                          prc, use_tui ? 1 : 0);
            } else if (rfeat.semantic_entropy)
                chat_maybe_semantic_entropy(&cfg, &rfeat, line, &r,
                                            early_spec_abstain, semantic_hit,
                                            prc, use_tui ? 1 : 0);
        }

        if (!early_spec_abstain && !semantic_hit && prc == 0
            && !skipped_resp_sig_cache2
            && (rfeat.semantic_sigma || rfeat.semantic_entropy)
            && r.response != NULL)
            cos_response_cache_store(resp_cache_h2, r.sigma, r.response);

        if (!early_spec_abstain && semantic_lru && prc == 0 && r.response != NULL
            && strcmp(r.response, COS_CHAT_PIPELINE_ABSTAIN) != 0
            && strncmp(r.response, "[ABSTAIN", 9) != 0
            && (r.diagnostic == NULL
                || strcmp(r.diagnostic, "semantic_lru_cache") != 0))
            cos_semantic_cache_store(cos_semantic_cache_prompt_hash(line),
                                     r.sigma, r.response);

        if (!early_spec_abstain && !semantic_hit && prc == 0 && r.response != NULL) {
            memcpy(g_spike_last_bsc, bsc_prompt, sizeof(g_spike_last_bsc));
            snprintf(g_spike_last_resp, sizeof g_spike_last_resp, "%s",
                     r.response);
            g_spike_have_last = 1;
        }

        if (spike_mode && g_cos_spike_ready) {
            char spath[512];
            if (cos_chat_spike_snapshot_path(spath, sizeof spath) == 0)
                (void)cos_spike_engine_snapshot_write(&g_cos_spike, spath);
        }

        /* FINAL-2 Phase C: metacog emits BEFORE the answer so the user
         * can read the awareness breakdown alongside the reply. */
        if (verbose) {
            cos_ultra_metacog_levels_t lv;
            FILE *mv = use_tui ? stderr : stdout;
            chat_metacog_levels(line, &r, cfg.max_rethink, &lv);
            fprintf(mv,
                    "[meta: perception=%.2f self=%.2f social=%.2f "
                    "situational=%.2f]\n",
                    (double)lv.sigma_perception, (double)lv.sigma_self,
                    (double)lv.sigma_social, (double)lv.sigma_situational);
        }

        /* FINAL-2 Phase A: optional σ_combined shadow ensemble.
         * NEXT-1 auto-enables this under --verbose so the full ULTRA
         * stack shows without requiring a second flag. */
        chat_feat_t xf;
        memset(&xf, 0, sizeof(xf));
        xf.multi_sigma        = (multi_sigma || verbose) ? 1 : 0;
        xf.verbose            = verbose;
        xf.conformal_active   = conformal_active;
        xf.conformal_alpha    = conformal_alpha;
        xf.semantic_cache     = semantic_cache;
        xf.speculative_sigma  = speculative_sigma;
        xf.knowledge_graph   = knowledge_graph;
        xf.semantic_entropy  = semantic_entropy;
        xf.semantic_sigma    = semantic_sigma;
        xf.fast_sigma        = fast_sigma;
        xf.cascade           = cascade;
        xf.cross_model_sigma = cross_model_sigma;
        xf.semantic_lru      = semantic_lru;

        chat_multi_t ms;
        memset(&ms, 0, sizeof(ms));
        int ms_ok = 0;
        {
            int allow_ms = (multi_sigma || verbose) && !skip_multi_verify
                && (!adaptive_compute || kernel_cap >= 10);
            if (allow_ms) {
                int hmt = (cos_cli_use_bitnet_http() != 0)
                          && ((rfeat.semantic_sigma != 0)
                              || (rfeat.fast_sigma == 0));
                ms_ok = (chat_multi_shadow(&cfg, hmt, line, &r, r.response,
                                           r.sigma, &ms) == 0)
                            ? 1
                            : 0;
            }
        }
        chat_agi_after_turn(&cfg, line, &r, &xf, &ms, ms_ok);
        if (ms_ok) {
            FILE *msf = use_tui ? stderr : stdout;
            fprintf(msf,
                    "[σ_combined=%.3f | σ_semantic=%.3f | σ_logprob=%.3f "
                    "σ_entropy=%.3f σ_perplexity=%.3f σ_consistency=%.3f | k=%d]\n",
                    (double)ms.ens.sigma_combined,
                    (double)ms.ens.sigma_consistency,
                    (double)ms.ens.sigma_logprob,
                    (double)ms.ens.sigma_entropy,
                    (double)ms.ens.sigma_perplexity,
                    (double)ms.ens.sigma_consistency,
                    ms.k_used);
        }

        {
            const char *repl_deliver =
                (r.response != NULL) ? r.response : "";
            int rcv = 0, rc_compl = 1, rc_chk = 0, rc_viol = 0,
                rc_mhalts = 0, rc_mv = 0;
            char rcrep[1024];

            rcrep[0] = '\0';
            if (!early_spec_abstain && r.response != NULL
                && r.final_action != COS_SIGMA_ACTION_ABSTAIN) {
                struct cos_proof_receipt pr;
                memset(&pr, 0, sizeof pr);
                spektre_sha256((const uint8_t *)r.response, strlen(r.response),
                               pr.output_hash);
                pr.sigma_combined = r.sigma;
                pr.within_compute_budget = 1;
                if (adaptive_compute) {
                    pr.within_compute_budget =
                        (elapsed_ms <= (double)SL.bu.time_budget_ms + 1e-3)
                            ? 1
                            : 0;
                }
                (void)cos_constitution_check(r.response, r.sigma, &pr,
                                              &rc_viol, &rc_mv, rcrep,
                                              (int)sizeof rcrep);
                rcv       = 1;
                rc_chk    = cos_constitution_get()->n_rules;
                rc_compl  = (rc_viol == 0) ? 1 : 0;
                rc_mhalts = (rc_mv > 0) ? 1 : 0;
                if (rc_mv > 0) {
                    snprintf(g_cos_chat_const_halt,
                             sizeof g_cos_chat_const_halt,
                             "[CONSTITUTIONAL HALT] %s", rcrep);
                    fprintf(use_tui ? stderr : stdout, "%s\n",
                            g_cos_chat_const_halt);
                    if (!(multi_sigma && rc_mv > 0))
                        repl_deliver = g_cos_chat_const_halt;
                } else if (rc_viol > 0) {
                    fprintf(use_tui ? stderr : stdout,
                            "[CONSTITUTIONAL WARNING] %s\n", rcrep);
                }
            }

            if (r.final_action == COS_SIGMA_ACTION_ABSTAIN) {
                if (use_tui)
                    fputs("│ (no answer — abstained)\n", stdout);
                else
                    fprintf(stdout, "(no answer — abstained)\n");
            } else if (!cos_chat_stream_env_on()) {
                if (semantic_hit && use_tui)
                    ;
                else if (repl_deliver[0] != '\0') {
                    if (use_tui) {
                        cos_tui_begin_ai_line();
                        fputs(repl_deliver, stdout);
                        cos_tui_stream_newline();
                    } else {
                        fprintf(stdout, "%s\n", repl_deliver);
                    }
                }
            }

            {
                float ttft_pass = -1.0f;
                if (cos_chat_stream_env_on()) {
                    double g = cos_bitnet_server_last_ttft_ms();
                    if (g >= 0.0)
                        ttft_pass = (float)g;
                }
                struct cos_speed_metrics spd = cos_speed_measure_ex(
                    r.response != NULL ? r.response : "", (float)elapsed_ms,
                    ttft_pass);
                if (!use_tui) {
                    cos_speed_print(&spd);
                    fputc('\n', stdout);
                    fprintf(stderr,
                            "[speed: %d tok | %.0fms | %.1f tok/s | σ_time "
                            "%.0fms]\n",
                            spd.tokens_generated, (double)elapsed_ms,
                            (double)spd.tokens_per_second,
                            g_cos_chat_semantic_phase_ms);
                    print_receipt_polished(&r, elapsed_ms,
                                           conformal_active, conformal_alpha,
                                           semantic_hit, &spd);
                } else {
                    cos_tui_print_receipt(
                        r.sigma, cos_sigma_action_label(r.final_action),
                        spd.tokens_per_second, spd.ttft_ms, (float)r.cost_eur);
                    sigma_sess_sum += r.sigma;
                    sigma_sess_n++;
                    if (semantic_hit || r.engram_hit)
                        cache_sess_hits++;
                    {
                        float smean = sigma_sess_n
                                          ? sigma_sess_sum / (float)sigma_sess_n
                                          : 0.f;
                        float k_eff_h = 1.f - smean;
                        if (coherence_enabled && coh.n >= 1) {
                            float mn = 0.f, sl = 0.f;
                            coh_summary(&coh, &mn, &sl);
                            (void)sl;
                            k_eff_h = 1.f - mn;
                        }
                        cos_tui_status_bar(turn, cache_sess_hits,
                                           0.001f * (float)turn, smean);
                        cos_tui_header(smean, k_eff_h);
                    }
                }
                {
                    const char *repl_proof = repl_deliver;
                    if (multi_sigma && rc_mhalts != 0 && r.response != NULL
                        && r.response[0] != '\0')
                        repl_proof = r.response;
                    chat_turn_proof_emit(&cfg, line, &r, &ms, ms_ok, &rfeat,
                                         &SL, elapsed_ms, repl_proof, rcv,
                                         rc_compl, rc_chk, rc_viol,
                                         rc_mhalts);
                }
            }
        }

        /* FINAL-2 Phase D: session window for coherence. */
        if (coherence_enabled) coh_push(&coh, r.sigma);

        /* NEXT-1: running coherence one-liner under --verbose, starting
         * at 2+ samples so dσ/dt has a meaningful slope. */
        if (verbose && coherence_enabled && coh.n >= 2) {
            float mean = 0.0f, slope = 0.0f;
            coh_summary(&coh, &mean, &slope);
            float k_eff = 1.0f - mean;
            const char *state = (mean < 0.5f && fabsf(slope) < 0.05f) ? "STABLE"
                              : (mean < 0.7f)                         ? "DRIFTING"
                              :                                         "AT_RISK";
            fprintf(stdout,
                "[coherence: dσ/dt=%+.3f/turn | K_eff=%.2f | %s | n=%d]\n",
                (double)slope, (double)k_eff, state, coh.n);
        }

        if (verbose && r.diagnostic != NULL) {
            fprintf(stdout, "  diag: %s\n", r.diagnostic);
        }
        if (knowledge_graph && r.response != NULL)
            chat_kg_store_turn(line, r.response);

        if (!early_spec_abstain && prc == 0) {
            struct cos_speed_metrics spd_tok = cos_speed_measure_ex(
                r.response != NULL ? r.response : "", 0.0, -1.0f);
            int tok_est = spd_tok.tokens_generated + r.rethink_count * 48;
            if (tok_est < 1)
                tok_est = 1;
            cos_chat_jsonl_turn(
                line, &r, elapsed_ms,
                (g_ag_ledger_ready && g_ag_ledger.drift_detected) ? 1 : 0,
                tok_est);
            cos_chat_token_session_note(tok_est);
        }
    }

    if (sig_tty)
        signal(SIGINT, g_cos_chat_prev_sigint);

    fprintf(stdout, "\n─── Session summary ───\n");
    fprintf(stdout, "  turns: %d\n", turn);
    cos_chat_token_trend_footer(stdout);
    print_cost(&sv);
    if (coherence_enabled && coh.n > 0) {
        float mean = 0.0f, slope = 0.0f;
        coh_summary(&coh, &mean, &slope);
        /* Present dσ/dt per turn; ULTRA-9 report scales to "per hour"
         * under the convention that a single turn ≈ an hour of usage
         * for demo purposes. */
        cos_ultra_coherence_emit_report(stdout, mean, slope, 2.0f, 1.0f);
    }
    fprintf(stdout, "  assert(declared == realized);\n  1 = 1.\n");

    if (g_ag_ledger_ready) {
        char sp[512];
        if (cos_state_ledger_default_path(sp, sizeof(sp)) == 0)
            cos_state_ledger_persist(&g_ag_ledger, sp);
        cos_engram_consolidate(50000);
    }

    cos_sigma_pipeline_free_engram_values(&engram);
    cos_sigma_codex_free(&codex);
    cos_engram_persist_close(persist);
    return 0;
}
