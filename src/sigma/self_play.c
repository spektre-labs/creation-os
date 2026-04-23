/*
 * Self-play harness — challenge → answer → second sample → BSC agreement.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "self_play.h"

#include "inference_cache.h"

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_episodic.h"
#include "engram_persist.h"
#include "stub_gen.h"
#include "escalation.h"
#include "error_attribution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    cos_pipeline_config_t      cfg;
    cos_sigma_engram_entry_t   slots[64];
    cos_sigma_engram_t         engram;
    cos_sigma_sovereign_t      sovereign;
    cos_sigma_agent_t          agent;
    cos_sigma_codex_t          codex;
    cos_cli_generate_ctx_t     genctx;
    cos_engram_persist_t      *persist;
    int                        have_codex;
} cos_self_play_sess_t;

static int sp_genctx_minimal(cos_cli_generate_ctx_t *g,
                             cos_sigma_codex_t *codex,
                             cos_engram_persist_t *persist, int have_codex)
{
    memset(g, 0, sizeof(*g));
    g->magic                  = COS_CLI_GENERATE_CTX_MAGIC;
    g->codex                  = have_codex ? codex : NULL;
    g->persist                = persist;
    g->icl_exemplar_max_sigma = 0.35f;
    g->icl_k                  = 0;
    g->icl_rethink_only       = 0;
    g->icl_compose            = NULL;
    g->icl_ctx                = NULL;
    return 0;
}

static int self_play_sess_init(cos_self_play_sess_t *S, int max_rethink,
                               float tau_a, float tau_r)
{
    memset(S, 0, sizeof(*S));
    if (cos_sigma_engram_init(&S->engram, S->slots, 64, 0.25f, 200, 20) != 0)
        return -1;
    cos_sigma_sovereign_init(&S->sovereign, 0.85f);
    cos_sigma_agent_init(&S->agent, 0.80f, 0.10f);

    const char *no_persist = getenv("COS_ENGRAM_DISABLE");
    if (no_persist == NULL || no_persist[0] != '1') {
        if (cos_engram_persist_open(NULL, &S->persist) != 0)
            S->persist = NULL;
        if (S->persist != NULL)
            (void)cos_engram_persist_load(S->persist, &S->engram, 64);
    }

    if (cos_sigma_codex_load_seed(&S->codex) == 0)
        S->have_codex = 1;

    sp_genctx_minimal(&S->genctx, &S->codex, S->persist, S->have_codex);

    cos_sigma_pipeline_config_defaults(&S->cfg);
    S->cfg.tau_accept   = tau_a;
    S->cfg.tau_rethink  = tau_r;
    S->cfg.max_rethink  = max_rethink;
    S->cfg.mode         = COS_PIPELINE_MODE_LOCAL_ONLY;
    S->cfg.codex        = S->have_codex ? &S->codex : NULL;
    S->cfg.engram       = &S->engram;
    S->cfg.sovereign    = &S->sovereign;
    S->cfg.agent        = &S->agent;
    S->cfg.generate     = cos_cli_chat_generate;
    S->cfg.generate_ctx = &S->genctx;
    S->cfg.escalate     = cos_cli_escalate_api;
    if (S->persist != NULL) {
        S->cfg.on_engram_store     = cos_engram_persist_store;
        S->cfg.on_engram_store_ctx = S->persist;
    }
    return 0;
}

static void self_play_sess_shutdown(cos_self_play_sess_t *S)
{
    cos_sigma_pipeline_free_engram_values(&S->engram);
    if (S->have_codex)
        cos_sigma_codex_free(&S->codex);
    cos_engram_persist_close(S->persist);
}

int cos_self_play_run(const char *domain, int n_rounds,
                      struct cos_self_play_round *results)
{
    if (!domain || !domain[0] || n_rounds <= 0 || !results)
        return -1;
    if (n_rounds > 256)
        n_rounds = 256;

    cos_self_play_sess_t S;
    if (self_play_sess_init(&S, 3, 0.35f, 0.70f) != 0)
        return -2;

    int fail = 0;

    for (int r = 0; r < n_rounds; r++) {
        struct cos_self_play_round *row = &results[r];
        memset(row, 0, sizeof(*row));

        char qbp[1536];
        snprintf(qbp, sizeof(qbp),
                 "Generate exactly one difficult technical question about "
                 "\"%s\". Output only the question text on one line.",
                 domain);

        const char *q_raw = NULL;
        float       qs    = 1.0f;
        double      qc    = 0.0;
        if (cos_cli_chat_generate(qbp, 0, &S.genctx, &q_raw, &qs, &qc) != 0
            || q_raw == NULL) {
            snprintf(row->prompt, sizeof row->prompt,
                     "Fallback question about %s?", domain);
        } else {
            snprintf(row->prompt, sizeof row->prompt, "%s", q_raw);
        }

        cos_pipeline_result_t pr;
        if (cos_sigma_pipeline_run(&S.cfg, row->prompt, &pr) != 0) {
            snprintf(row->answer, sizeof row->answer, "[pipeline error]");
            row->sigma    = 1.0f;
            row->verified = -1;
            fail++;
            continue;
        }

        const char *a1 = pr.response ? pr.response : "";
        snprintf(row->answer, sizeof row->answer, "%s", a1);
        row->sigma = pr.sigma;

        const char *a2 = NULL;
        float       s2 = 1.0f;
        double      c2 = 0.0;
        if (cos_cli_chat_generate(row->prompt, 1, &S.genctx, &a2, &s2, &c2)
                != 0
            || a2 == NULL) {
            row->verified = -1;
        } else {
            uint64_t h1[COS_INF_W], h2[COS_INF_W];
            cos_inference_bsc_encode_prompt(a1, h1);
            cos_inference_bsc_encode_prompt(a2, h2);
            float dist =
                cos_inference_hamming_norm(h1, h2, COS_INF_W);
            if (dist < 0.30f)
                row->verified = 1;
            else
                row->verified = 0;
        }

        struct cos_engram_episode ep;
        ep.timestamp_ms   = (int64_t)time(NULL) * 1000LL;
        ep.prompt_hash    = cos_engram_prompt_hash(row->prompt);
        ep.sigma_combined = row->sigma;
        ep.action         = (int)pr.final_action;
        ep.was_correct    = row->verified;
        ep.attribution    = COS_ERR_NONE;
        ep.ttt_applied    = 0;
        (void)cos_engram_episode_store(&ep);
    }

    (void)cos_engram_consolidate(500);

    self_play_sess_shutdown(&S);
    return fail > 0 ? -3 : 0;
}

void cos_self_play_print_report(const struct cos_self_play_round *results,
                                int n)
{
    if (!results || n <= 0)
        return;
    printf("self-play rounds: %d\n", n);
    for (int i = 0; i < n; i++) {
        const struct cos_self_play_round *z = &results[i];
        printf("--- [%d] σ=%.3f verified=%d\n", i + 1, (double)z->sigma,
               z->verified);
        printf("  Q: %s\n", z->prompt);
        printf("  A: %.200s%s\n", z->answer,
               strlen(z->answer) > 200 ? "..." : "");
    }
}

int cos_self_play_self_test(void)
{
    uint64_t a[COS_INF_W], b[COS_INF_W];
    cos_inference_bsc_encode_prompt("identical answer text", a);
    cos_inference_bsc_encode_prompt("identical answer text", b);
    float d = cos_inference_hamming_norm(a, b, COS_INF_W);
    if (d > 1e-5f)
        return 1;

    cos_inference_bsc_encode_prompt("foo bar baz alpha", a);
    cos_inference_bsc_encode_prompt("xyz qq ww ee rr tt yy", b);
    float d2 = cos_inference_hamming_norm(a, b, COS_INF_W);
    if (d2 < 0.01f)
        return 2;

    struct cos_self_play_round rr[2];
    memset(rr, 0, sizeof(rr));
    if (cos_self_play_run("", 1, rr) != -1)
        return 3;
    if (cos_self_play_run("ok-domain", 0, rr) != -1)
        return 4;
    if (cos_self_play_run("ok-domain", 1, NULL) != -1)
        return 5;

    return 0;
}
