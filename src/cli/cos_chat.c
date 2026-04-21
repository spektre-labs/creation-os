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
#include "introspection.h"
#include "coherence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef COS_CHAT_VERSION_STRING
#define COS_CHAT_VERSION_STRING "v3.0.0"
#endif

/* NEXT-1 polish: backend detection + one-line action decorator. */
static const char *cos_chat_backend_label(void) {
    const char *exe = getenv("CREATION_OS_BITNET_EXE");
    if (exe != NULL && exe[0] != '\0') return "bridge · BitNet b1.58 2B · Local · CPU";
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

typedef struct {
    cos_multi_sigma_t ens;
    int   have;
    int   k_used;
} chat_multi_t;

static int chat_multi_shadow(cos_pipeline_config_t *cfg,
                             const char *input,
                             const char *primary_text,
                             float primary_sigma,
                             chat_multi_t *out) {
    if (out == NULL) return -1;
    memset(out, 0, sizeof(*out));

    /* Collect up to K+1 texts: [primary, regen_1, regen_2, ...]. */
    const int K = COS_CHAT_MULTI_K_REGEN;
    const char *texts[1 + COS_CHAT_MULTI_K_REGEN];
    char        copies[1 + COS_CHAT_MULTI_K_REGEN][COS_CHAT_MULTI_TEXT_CAP];
    int n = 0;
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
        /* Use round = k+1 so the generator seeds a different trajectory;
         * the K extra calls are local electricity only and are not
         * forwarded to the sovereign ledger (shadow mode). */
        if (cfg->generate(input, k + 1, cfg->generate_ctx,
                          &t, &s, &c) != 0 || t == NULL) break;
        snprintf(copies[n], sizeof(copies[n]), "%s", t);
        texts[n] = copies[n];
        n++;
    }

    float sigma_consistency =
        (n >= 2) ? cos_multi_sigma_consistency(texts, n) : 0.0f;
    if (sigma_consistency < 0.0f) sigma_consistency = 0.0f;

    /* Defensive clamp on the scalar. */
    float s_lp = primary_sigma;
    if (s_lp < 0.0f) s_lp = 0.0f;
    if (s_lp > 1.0f) s_lp = 1.0f;

    /* In the absence of per-token top-k logprobs we conservatively feed
     * the scalar into σ_entropy and σ_perplexity; the ensemble weights
     * (w_entropy=0.20, w_perplexity=0.10, w_consistency=0.20) keep the
     * ensemble under the scalar unless consistency genuinely diverges. */
    if (cos_multi_sigma_combine(s_lp, s_lp, s_lp,
                                sigma_consistency, NULL, &out->ens) == 0) {
        out->have   = 1;
        out->k_used = n;
        return 0;
    }
    return -1;
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
                                   float conformal_alpha) {
    const char *act   = cos_sigma_action_label(r->final_action);
    const char *src   = r->engram_hit ? "CACHE" : "FRESH";
    const char *route = r->escalated  ? "CLOUD" : "LOCAL";
    double ms = r->engram_hit ? 0.0 : elapsed_ms;

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

    if (r->final_action == COS_SIGMA_ACTION_ABSTAIN) {
        fprintf(stdout,
                "[σ=%.3f | ABSTAIN%s%s | %.0f ms | €%.4f]\n"
                "  (σ above τ_rethink — cannot guarantee accuracy at "
                "this confidence; reformulate or accept uncertainty.)\n",
                (double)r->sigma, conformal_tag, rethink_tag,
                ms, r->cost_eur);
    } else {
        fprintf(stdout,
                "[σ=%.3f | %s | %s | %s%s%s | %.0f ms | €%.4f]\n",
                (double)r->sigma, act, src, route,
                conformal_tag, rethink_tag, ms, r->cost_eur);
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

/* FINAL-2 per-turn toggles carried through to the --once / REPL paths.
 * Kept in a small config struct so the function signatures do not grow
 * an unbounded number of arguments. */
typedef struct {
    int   multi_sigma;          /* Phase A   */
    int   conformal_active;     /* Phase B on (τ loaded from bundle)   */
    float conformal_alpha;      /* Phase B pinned α (for receipt)     */
    int   verbose;              /* Phase C: emit metacog banner       */
    int   coherence_active;     /* Phase D: track σ in session window */
    chat_coh_t *coh;            /* Phase D window state               */
} chat_feat_t;

static int run_chat_once(FILE *tout, cos_pipeline_config_t *cfg_rw,
                         const chat_feat_t *feat,
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
    if (feat != NULL && feat->multi_sigma) {
        if (chat_multi_shadow(cfg_rw, prompt,
                              r.response, r.sigma, &ms) == 0) {
            have_ms = 1;
        }
    }

    fprintf(stdout,
            "round 0  %s  [σ_peak=%.2f action=%s route=%s]\n",
            r.response != NULL ? r.response : "",
            (double)r.sigma,
            cos_sigma_action_label(r.final_action),
            route);

    /* NEXT-1 polished receipt (shared with REPL path).  Harness only
     * requires ACCEPT/RETHINK/ABSTAIN to appear somewhere in the
     * output; the "round 0" line already satisfied that, so the
     * receipt is free to adopt the visual format. */
    print_receipt_polished(&r, elapsed_ms,
                           feat != NULL ? feat->conformal_active : 0,
                           feat != NULL ? feat->conformal_alpha  : 0.0f);
    if (have_ms) {
        fprintf(stdout,
                "[σ_combined=%.3f | σ_logprob=%.3f σ_entropy=%.3f "
                "σ_perplexity=%.3f σ_consistency=%.3f | k=%d]\n",
                (double)ms.ens.sigma_combined,
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
    int     want_icl           = 0;
    int     icl_k              = 3;
    int     icl_rethink        = 0;
    int     icl_user_disabled  = 0;

    /* FINAL-2 feature toggles (all off by default to keep existing
     * harnesses byte-identical; opt-in via flags or env). */
    int         multi_sigma        = 0;
    int         conformal_enabled  = 1;  /* auto-load bundle if present */
    int         coherence_enabled  = 1;  /* REPL-only; --once is a single
                                            turn so it has no trend.   */
    const char *calibration_path_arg = NULL;

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
        } else if (strcmp(argv[i], "--multi-sigma") == 0) {
            multi_sigma = 1;
        } else if (strcmp(argv[i], "--no-multi-sigma") == 0) {
            multi_sigma = 0;
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
                "  --icl               prepend few-shot exemplars from ~/.cos/engram.db\n"
                "  --no-icl            disable ICL even if COS_ENGRAM_ICL=1\n"
                "  --icl-k N           number of exemplars (default 3, max 8)\n"
                "  --icl-rethink-only  inject ICL only on RETHINK rounds (round>0)\n"
                "  --multi-sigma       emit σ_combined (SCI-5) shadow per turn\n"
                "  --no-multi-sigma    disable the ensemble shadow (default)\n"
                "  --conformal         load τ from ~/.cos/calibration.json (default)\n"
                "  --no-conformal      always use the static τ_accept flag\n"
                "  --calibration-path PATH  override the conformal bundle path\n"
                "  --coherence         track per-session σ window (ULTRA-9, default)\n"
                "  --no-coherence      disable the session coherence summary\n"
                "  --help              this text\n"
                "\n"
                "  CREATION_OS_BITNET_EXE  optional path to local inference binary\n"
                "                          (see src/import/bitnet_spawn.h).\n"
                "  COS_ENGRAM_ICL=1        same as --icl (overridden by --no-icl)\n"
                "  COS_ENGRAM_ICL_N=3      exemplar count\n"
                "  COS_ENGRAM_ICL_RETHINK=1   same as --icl-rethink-only\n"
                "  COS_ENGRAM_ICL_TAU=0.35    max σ for an exemplar row (default τ_accept)\n");
            return 0;
        }
    }

    if (once_mode) {
        if (!have_tau_accept)  tau_accept  = 0.30f;
        if (!have_tau_rethink) tau_rethink = 0.70f;
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
    /* DEV-6 + AGI-1: always pass a tagged `cos_cli_generate_ctx_t` so
     * the backend can attach Codex + optional engram-driven ICL. */
    cos_cli_generate_ctx_t genctx;
    memset(&genctx, 0, sizeof(genctx));
    genctx.magic                   = COS_CLI_GENERATE_CTX_MAGIC;
    genctx.codex                   = have_codex ? &codex : NULL;
    genctx.persist                 = persist;
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
        chat_feat_t feat;
        memset(&feat, 0, sizeof(feat));
        feat.multi_sigma        = multi_sigma;
        feat.conformal_active   = conformal_active;
        feat.conformal_alpha    = conformal_alpha;
        feat.verbose            = verbose;
        feat.coherence_active   = 0; /* single-turn: no trend to report */
        feat.coh                = NULL;
        int rc = run_chat_once(tout, &cfg, &feat, once_prompt);
        if (tout != NULL)
            fclose(tout);
        cos_sigma_pipeline_free_engram_values(&engram);
        cos_sigma_codex_free(&codex);
        cos_engram_persist_close(persist);
        return rc;
    }

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

    chat_coh_t coh;
    coh_reset(&coh);

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
        clock_t t0 = clock();
        cos_pipeline_result_t r;
        cos_sigma_pipeline_run(&cfg, line, &r);
        clock_t t1 = clock();
        double elapsed_ms = (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
        if (elapsed_ms < 0.0) elapsed_ms = 0.0;

        /* FINAL-2 Phase C: metacog emits BEFORE the answer so the user
         * can read the awareness breakdown alongside the reply. */
        if (verbose) {
            cos_ultra_metacog_levels_t lv;
            chat_metacog_levels(line, &r, cfg.max_rethink, &lv);
            fprintf(stdout,
                    "[meta: perception=%.2f self=%.2f social=%.2f "
                    "situational=%.2f]\n",
                    (double)lv.sigma_perception, (double)lv.sigma_self,
                    (double)lv.sigma_social, (double)lv.sigma_situational);
        }

        if (r.final_action == COS_SIGMA_ACTION_ABSTAIN) {
            fprintf(stdout, "(no answer — abstained)\n");
        } else {
            fprintf(stdout, "%s\n", r.response);
        }

        /* NEXT-1 polished receipt: encodes action/cache/route/rethink
         * count/conformal/wall ms/cost in one scannable line. */
        print_receipt_polished(&r, elapsed_ms,
                               conformal_active, conformal_alpha);

        /* FINAL-2 Phase A: optional σ_combined shadow ensemble.
         * NEXT-1 auto-enables this under --verbose so the full ULTRA
         * stack shows without requiring a second flag. */
        if (multi_sigma || verbose) {
            chat_multi_t ms;
            if (chat_multi_shadow(&cfg, line, r.response, r.sigma, &ms) == 0) {
                fprintf(stdout,
                    "[σ_combined=%.3f | σ_logprob=%.3f σ_entropy=%.3f "
                    "σ_perplexity=%.3f σ_consistency=%.3f | k=%d]\n",
                    (double)ms.ens.sigma_combined,
                    (double)ms.ens.sigma_logprob,
                    (double)ms.ens.sigma_entropy,
                    (double)ms.ens.sigma_perplexity,
                    (double)ms.ens.sigma_consistency,
                    ms.k_used);
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
    }

    fprintf(stdout, "\n─── Session summary ───\n");
    fprintf(stdout, "  turns: %d\n", turn);
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

    cos_sigma_pipeline_free_engram_values(&engram);
    cos_sigma_codex_free(&codex);
    cos_engram_persist_close(persist);
    return 0;
}
