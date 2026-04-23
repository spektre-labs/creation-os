/*
 * cos think — thin orchestration over cos_sigma_pipeline_run + local generator.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "cos_think.h"

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_persist.h"
#include "stub_gen.h"
#include "bitnet_server.h"
#include "escalation.h"
#include "multi_sigma.h"
#include "error_attribution.h"
#include "engram_episodic.h"

#include "../../core/cos_bsc.h"
#include "inference_cache.h"
#include "skill_distill.h"
#include "world_model.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "introspection.h"
#include "reinforce.h"
#include "state_ledger.h"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifndef COS_THINK_DEFAULT_SUBTASKS
#define COS_THINK_DEFAULT_SUBTASKS 3
#endif

#define COS_THINK_MULTI_TEXT_CAP 2048
#define COS_THINK_MULTI_K_REGEN  2

typedef struct {
    cos_pipeline_config_t    cfg;
    cos_sigma_engram_entry_t slots[64];
    cos_sigma_engram_t       engram;
    cos_sigma_sovereign_t    sovereign;
    cos_sigma_agent_t        agent;
    cos_sigma_codex_t        codex;
    cos_cli_generate_ctx_t     genctx;
    cos_engram_persist_t      *persist;
    int                        have_codex;
} cos_think_sess_t;

static int cos_think_genctx_minimal(cos_cli_generate_ctx_t *g,
                                    cos_sigma_codex_t *codex,
                                    cos_engram_persist_t *persist,
                                    int have_codex) {
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

static void think_first_word(const char *goal, char *out, size_t cap)
{
    if (out == NULL || cap < 2) {
        if (out && cap)
            out[0] = '\0';
        return;
    }
    out[0] = '\0';
    if (goal == NULL)
        return;
    while (*goal && isspace((unsigned char)*goal))
        ++goal;
    size_t i = 0;
    while (goal[i] && !isspace((unsigned char)goal[i]) && i + 1 < cap) {
        out[i] = goal[i];
        ++i;
    }
    out[i] = '\0';
}

static int think_goal_world_augment(const char *goal, char *dst, size_t cap)
{
    if (dst == NULL || cap < 2) {
        if (dst && cap)
            dst[0] = '\0';
        return 0;
    }
    dst[0] = '\0';
    const char *inj = getenv("COS_WORLD_INJECT");
    if (inj == NULL || inj[0] != '1')
        return 0;
    char tok[160];
    think_first_word(goal, tok, sizeof tok);
    if (tok[0] == '\0')
        return 0;
    struct cos_world_belief b[8];
    int                 nb = 0;
    if (cos_world_query_belief(tok, b, 8, &nb) != 0 || nb < 1)
        return 0;
    float wavg = cos_world_confidence(tok);
    int   n    = snprintf(dst, cap,
                       "[sigma-world head=%s sigma_avg=%.3f] ",
                       tok, (double)wavg);
    if (n < 0 || (size_t)n >= cap)
        return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < nb && i < 6 && off + 240 < cap; ++i) {
        int k = snprintf(dst + off, cap - off,
            "%s %s %s (sigma=%.3f); ",
            b[i].subject, b[i].predicate, b[i].object,
            (double)b[i].sigma);
        if (k < 0)
            break;
        off += (size_t)k;
    }
    snprintf(dst + off, cap - off, "\n%s", goal);
    return 1;
}

/** Route decomposition to a fast llama-server on COS_THINK_SMALL_PORT (e.g. 2B
 *  on :8089) while subtasks use COS_BITNET_SERVER_PORT (9B on :8088). */
static int think_cli_chat_generate_decompose(const char *prompt,
                                             cos_cli_generate_ctx_t *gctx,
                                             const char **text, float *sigma,
                                             double *cost_eur)
{
#if defined(__unix__) || defined(__APPLE__)
    const char *alt = getenv("COS_THINK_SMALL_PORT");
    if (alt != NULL && alt[0] != '\0') {
        const char *prev = getenv("COS_BITNET_SERVER_PORT");
        char        saved[24];
        int         had_prev = 0;
        if (prev != NULL && prev[0] != '\0') {
            snprintf(saved, sizeof saved, "%s", prev);
            had_prev = 1;
        }
        setenv("COS_BITNET_SERVER_PORT", alt, 1);
        cos_bitnet_server_invalidate_config();
        int rc =
            cos_cli_chat_generate(prompt, 0, gctx, text, sigma, cost_eur);
        if (had_prev)
            setenv("COS_BITNET_SERVER_PORT", saved, 1);
        else
            unsetenv("COS_BITNET_SERVER_PORT");
        cos_bitnet_server_invalidate_config();
        return rc;
    }
#endif
    return cos_cli_chat_generate(prompt, 0, gctx, text, sigma, cost_eur);
}

static void think_trim(char *s) {
    if (s == NULL) return;
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) ++i;
    if (i > 0)
        memmove(s, s + i, strlen(s + i) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        --n;
    }
}

static int think_parse_lines(const char *blob, char lines[][1024], int max_lines,
                             int *n_out) {
    if (!blob || !n_out || max_lines <= 0) return -1;
    *n_out = 0;
    const char *p = blob;
    while (*p && *n_out < max_lines) {
        while (*p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '\n' && *e != '\r') ++e;
        size_t len = (size_t)(e - p);
        if (len >= 1024) len = 1023;
        memcpy(lines[*n_out], p, len);
        lines[*n_out][len] = '\0';
        think_trim(lines[*n_out]);
        if (lines[*n_out][0] != '\0') {
            /* Strip leading "1." / "-" bullets */
            char *ln = lines[*n_out];
            while (*ln && (isdigit((unsigned char)*ln) || *ln == '.' || *ln == ')'
                   || *ln == '-' || *ln == '*' || isspace((unsigned char)*ln))) {
                if (*ln == '-' || *ln == '*' || *ln == ')') {
                    ++ln;
                    break;
                }
                ++ln;
            }
            think_trim(ln);
            if (ln != lines[*n_out])
                memmove(lines[*n_out], ln, strlen(ln) + 1);
            (*n_out)++;
        }
        p = e;
        if (*p == '\r' || *p == '\n') ++p;
    }
    return 0;
}

static void think_metacog_levels(const char *prompt,
                                 const cos_pipeline_result_t *r,
                                 int                          max_rethink,
                                 cos_ultra_metacog_levels_t *lv) {
    memset(lv, 0, sizeof(*lv));
    int words = 0, in = 0;
    if (prompt != NULL) {
        for (const char *s = prompt; *s; ++s) {
            int w = (*s > ' ');
            if (w && !in) {
                words++;
                in = 1;
            } else if (!w)
                in = 0;
        }
    }
    lv->sigma_perception = (words <= 3) ? 0.35f : 0.05f;
    lv->sigma_self       = (r != NULL) ? r->sigma : 1.0f;
    lv->sigma_social     = 0.0f;
    if (prompt != NULL) {
        size_t L = strlen(prompt);
        if (L > 0 && prompt[L - 1] == '?' && words <= 4)
            lv->sigma_social = 0.45f;
    }
    int budget = max_rethink > 0 ? max_rethink : 1;
    lv->sigma_situational =
        (r != NULL) ? ((float)r->rethink_count / (float)budget) : 0.0f;
    if (lv->sigma_situational > 1.0f) lv->sigma_situational = 1.0f;
}

typedef struct {
    cos_multi_sigma_t ens;
    int               have;
    int               k_used;
} think_multi_t;

static int think_multi_shadow(cos_pipeline_config_t *cfg, const char *input,
                              const char *primary_text, float primary_sigma,
                              think_multi_t *out) {
    if (out == NULL || cfg == NULL) return -1;
    memset(out, 0, sizeof(*out));
    const char *texts[1 + COS_THINK_MULTI_K_REGEN];
    char        copies[1 + COS_THINK_MULTI_K_REGEN][COS_THINK_MULTI_TEXT_CAP];
    int n = 0;
    if (primary_text != NULL) {
        snprintf(copies[n], sizeof(copies[n]), "%s", primary_text);
        texts[n] = copies[n];
        n++;
    }
    for (int k = 0; k < COS_THINK_MULTI_K_REGEN; ++k) {
        if (cfg->generate == NULL) break;
        const char *t = NULL;
        float       s = 1.0f;
        double      c = 0.0;
        if (cfg->generate(input, k + 1, cfg->generate_ctx, &t, &s, &c) != 0
            || t == NULL)
            break;
        snprintf(copies[n], sizeof(copies[n]), "%s", t);
        texts[n] = copies[n];
        n++;
    }
    float sigma_consistency =
        (n >= 2) ? cos_multi_sigma_consistency(texts, n) : 0.0f;
    if (sigma_consistency < 0.0f) sigma_consistency = 0.0f;
    float s_lp = primary_sigma;
    if (s_lp < 0.0f) s_lp = 0.0f;
    if (s_lp > 1.0f) s_lp = 1.0f;
    if (cos_multi_sigma_combine(s_lp, s_lp, s_lp, sigma_consistency, NULL,
                                &out->ens) == 0) {
        out->have   = 1;
        out->k_used = n;
        return 0;
    }
    return -1;
}

static void think_answer_to_hv(const char *text, uint64_t *hv) {
    uint64_t st = cos_engram_prompt_hash(text ? text : "");
    for (int i = 0; i < COS_W; ++i) {
        st = st * 6364136223846793005ULL + 1442695040888963407ULL
             + (uint64_t)(i + 3) * 0x9e3779b97f4a7c15ULL;
        hv[i] = st ^ (st << 13);
    }
}

static float think_pair_similarity(const char *a, const char *b) {
    uint64_t ha[COS_W], hb[COS_W];
    think_answer_to_hv(a, ha);
    think_answer_to_hv(b, hb);
    uint32_t d = cos_hv_hamming(ha, hb);
    return 1.0f - (float)d / (float)COS_D;
}

static int think_compute_consensus(const struct cos_think_result *res) {
    int n = res->n_subtasks;
    if (n < 2) return 1;
    double sum = 0.0;
    int    pairs = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            sum += (double)think_pair_similarity(res->subtasks[i].answer,
                                                  res->subtasks[j].answer);
            pairs++;
        }
    }
    if (pairs <= 0) return 0;
    float mean = (float)(sum / (double)pairs);
    return (mean > 0.70f) ? 1 : 0;
}

static int think_sess_init(cos_think_sess_t *S, int allow_cloud,
                           int max_rethink, float tau_a, float tau_r) {
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

    {
        const char *cp = getenv("COS_CODEX_PATH");
        if (cp != NULL && cp[0] != '\0') {
            if (cos_sigma_codex_load(cp, &S->codex) == 0)
                S->have_codex = 1;
        } else if (cos_sigma_codex_load(NULL, &S->codex) == 0) {
            S->have_codex = 1;
        }
    }

    cos_think_genctx_minimal(&S->genctx, &S->codex, S->persist, S->have_codex);

    cos_sigma_pipeline_config_defaults(&S->cfg);
    S->cfg.tau_accept   = tau_a;
    S->cfg.tau_rethink  = tau_r;
    S->cfg.max_rethink  = max_rethink;
    S->cfg.mode         = allow_cloud ? COS_PIPELINE_MODE_HYBRID
                                      : COS_PIPELINE_MODE_LOCAL_ONLY;
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

static void think_sess_shutdown(cos_think_sess_t *S) {
    cos_sigma_pipeline_free_engram_values(&S->engram);
    if (S->have_codex) cos_sigma_codex_free(&S->codex);
    cos_engram_persist_close(S->persist);
}

int cos_think_decompose(const char *goal, char subtasks[][1024], int *n_out) {
    if (!goal || !subtasks || !n_out) return -1;
    *n_out = 0;

    cos_cli_generate_ctx_t gctx;
    cos_sigma_codex_t      codex;
    memset(&codex, 0, sizeof(codex));
    int have_codex = 0;
    {
        const char *cp = getenv("COS_CODEX_PATH");
        if (cp != NULL && cp[0] != '\0')
            have_codex = (cos_sigma_codex_load(cp, &codex) == 0);
        else
            have_codex = (cos_sigma_codex_load(NULL, &codex) == 0);
    }
    cos_think_genctx_minimal(&gctx, &codex, NULL, have_codex);

    char prompt[3072];
    snprintf(prompt, sizeof(prompt),
             "Break this goal into %d specific, answerable questions:\n"
             "GOAL: %s\n"
             "Return ONLY the questions, one per line.",
             COS_THINK_DEFAULT_SUBTASKS, goal);

    const char *text = NULL;
    float       sigma = 1.0f;
    double      cost  = 0.0;
    if (think_cli_chat_generate_decompose(prompt, &gctx, &text, &sigma, &cost)
            != 0
        || text == NULL)
        goto fallback;

    if (sigma > 0.80f)
        goto fallback;

    int nlines = 0;
    think_parse_lines(text, subtasks, COS_THINK_MAX_SUBTASKS, &nlines);
    if (nlines <= 0)
        goto fallback;

    *n_out = nlines;
    if (have_codex) cos_sigma_codex_free(&codex);
    return 0;

fallback:
    snprintf(subtasks[0], 1024, "%s", goal);
    *n_out = 1;
    if (have_codex) cos_sigma_codex_free(&codex);
    return 0;
}

static void think_store_episode(const char *prompt, float sigma_combined,
                                enum cos_error_source src,
                                cos_sigma_action_t act, int ttt_applied) {
    struct cos_engram_episode ep;
    ep.timestamp_ms   = (int64_t)time(NULL) * 1000LL;
    ep.prompt_hash    = cos_engram_prompt_hash(prompt);
    ep.sigma_combined = sigma_combined;
    ep.action         = (int)act;
    ep.was_correct    = -1;
    ep.attribution    = src;
    ep.ttt_applied    = ttt_applied != 0 ? 1 : 0;
    (void)cos_engram_episode_store(&ep);
}

static int think_run_one_subtask(cos_think_sess_t *S, const char *prompt,
                                 struct cos_state_ledger *ledger,
                                 struct cos_think_subtask *st) {
    memset(st, 0, sizeof(*st));
    snprintf(st->prompt, sizeof(st->prompt), "%s", prompt);

    clock_t c0 = clock();
    cos_pipeline_result_t r;
    if (cos_sigma_pipeline_run(&S->cfg, prompt, &r) != 0) {
        st->final_action = 2; /* ABSTAIN — pipeline failure */
        return -1;
    }
    clock_t c1 = clock();
    st->latency_ms =
        (int64_t)((double)(c1 - c0) * 1000.0 / (double)CLOCKS_PER_SEC);
    if (st->latency_ms < 0) st->latency_ms = 0;

    const char *resp = r.response ? r.response : "";
    snprintf(st->answer, sizeof(st->answer), "%s", resp);

    think_multi_t ms;
    float sigma_use = r.sigma;
    if (think_multi_shadow(&S->cfg, prompt, resp, r.sigma, &ms) == 0
        && ms.have) {
        sigma_use = ms.ens.sigma_combined;
    }
    st->sigma_combined = sigma_use;

    cos_ultra_metacog_levels_t lv;
    think_metacog_levels(prompt, &r, S->cfg.max_rethink, &lv);
    struct cos_error_attribution attr =
        cos_error_attribute(ms.have ? ms.ens.sigma_logprob : sigma_use,
                           ms.have ? ms.ens.sigma_entropy : sigma_use,
                           ms.have ? ms.ens.sigma_consistency : 0.0f,
                           lv.sigma_perception);
    st->attribution = attr.source;

    st->backend = r.escalated ? 1 : 0;

    think_store_episode(prompt, st->sigma_combined, st->attribution,
                        r.final_action, r.ttt_applied);

    st->final_action = (int)r.final_action;

    if (ledger != NULL) {
        cos_multi_sigma_t mfill;
        if (ms.have) {
            cos_state_ledger_fill_multi(ledger, &ms.ens);
        } else {
            mfill.sigma_logprob      = sigma_use;
            mfill.sigma_entropy      = sigma_use;
            mfill.sigma_perplexity     = sigma_use;
            mfill.sigma_consistency  = 0.0f;
            mfill.sigma_combined     = sigma_use;
            cos_state_ledger_fill_multi(ledger, &mfill);
        }
        float meta[4] = { lv.sigma_perception, lv.sigma_self,
                           lv.sigma_social, lv.sigma_situational };
        cos_state_ledger_update(ledger, st->sigma_combined, meta,
                                (int)r.final_action);
        cos_state_ledger_add_cost(ledger, (float)r.cost_eur);
        if (r.engram_hit) cos_state_ledger_note_cache_hit(ledger);
    }
    return 0;
}

int cos_think_run(const char *goal, int max_rounds,
                  struct cos_think_result *result) {
    if (!goal || !goal[0] || !result) return -1;
    memset(result, 0, sizeof(*result));

    snprintf(result->goal, sizeof(result->goal), "%s", goal);
    result->timestamp = (int64_t)time(NULL);

    cos_skill_begin_session();

    int mr = max_rounds;
    if (mr < 1) mr = 3;
    if (mr > cos_sigma_reinforce_max_rounds())
        mr = cos_sigma_reinforce_max_rounds();

    float tau_a = 0.35f;
    float tau_r = 0.70f;
    const char *e_ta = getenv("COS_THINK_TAU_ACCEPT");
    const char *e_tr = getenv("COS_THINK_TAU_RETHINK");
    if (e_ta && e_ta[0]) tau_a = (float)atof(e_ta);
    if (e_tr && e_tr[0]) tau_r = (float)atof(e_tr);

    int allow_cloud = 1;
    const char *bl = getenv("COS_THINK_BACKENDS");
    if (bl != NULL && strstr(bl, "local") != NULL && strstr(bl, "cloud") == NULL)
        allow_cloud = 0;

    cos_think_sess_t S;
    if (think_sess_init(&S, allow_cloud, mr, tau_a, tau_r) != 0)
        return -2;

    struct cos_state_ledger ledger;
    cos_state_ledger_init(&ledger);

    clock_t t_all0 = clock();

    const char *sk_disable = getenv("COS_SKILL_DISABLE");
    int          use_skill  = (sk_disable == NULL || sk_disable[0] != '1');

    char sub[COS_THINK_MAX_SUBTASKS][1024];
    int    n_sub = 0;

    uint64_t goal_bsc[COS_INF_W];
    cos_inference_bsc_encode_prompt(goal, goal_bsc);
    float skill_thr = 0.35f;
    const char *skh = getenv("COS_SKILL_HAMMING_MAX");
    if (skh != NULL && skh[0] != '\0')
        skill_thr = (float)atof(skh);

    char              goal_wm[3072];
    const char       *goal_for_decompose = goal;
    if (think_goal_world_augment(goal, goal_wm, sizeof goal_wm))
        goal_for_decompose = goal_wm;

    struct cos_skill *sk_hit = NULL;
    if (use_skill)
        sk_hit = cos_skill_lookup(goal_bsc, skill_thr);
    if (sk_hit != NULL && sk_hit->n_steps > 0) {
        n_sub = sk_hit->n_steps;
        if (n_sub > COS_THINK_MAX_SUBTASKS)
            n_sub = COS_THINK_MAX_SUBTASKS;
        for (int i = 0; i < n_sub; i++)
            snprintf(sub[i], sizeof(sub[i]), "%s", sk_hit->steps[i]);
        cos_skill_free(sk_hit);
        sk_hit = NULL;
    } else {
        if (sk_hit) {
            cos_skill_free(sk_hit);
            sk_hit = NULL;
        }
        cos_think_decompose(goal_for_decompose, sub, &n_sub);
    }

    result->n_subtasks = n_sub;
    if (n_sub <= 0) {
        think_sess_shutdown(&S);
        return -3;
    }

    float smin = 1.0f, smax = 0.0f;
    double ssum = 0.0;
    int    best_i = 0;

    for (int i = 0; i < n_sub; ++i) {
        if (think_run_one_subtask(&S, sub[i], &ledger,
                                   &result->subtasks[i]) != 0) {
            snprintf(result->subtasks[i].answer,
                     sizeof(result->subtasks[i].answer),
                     "[pipeline error]");
            result->subtasks[i].sigma_combined = 1.0f;
            result->subtasks[i].attribution    = COS_ERR_REASONING;
            result->subtasks[i].final_action   = 2; /* ABSTAIN */
        }
        float s = result->subtasks[i].sigma_combined;
        ssum += (double)s;
        if (s < smin) {
            smin   = s;
            best_i = i;
        }
        if (s > smax) smax = s;
    }

    result->sigma_mean       = (float)(ssum / (double)n_sub);
    result->sigma_min        = smin;
    result->sigma_max        = smax;
    result->best_subtask_idx = best_i;
    result->consensus        = think_compute_consensus(result);

    clock_t t_all1 = clock();
    result->total_ms =
        (int64_t)((double)(t_all1 - t_all0) * 1000.0 / (double)CLOCKS_PER_SEC);

    if (use_skill) {
        uint64_t sh = cos_skill_last_lookup_hash();
        if (sh != 0ULL) {
            int succ = (result->sigma_mean < tau_a) ? 1 : 0;
            if (succ) {
                for (int i = 0; i < result->n_subtasks; ++i) {
                    if (result->subtasks[i].final_action != 0) {
                        succ = 0;
                        break;
                    }
                }
            }
            (void)cos_skill_update_reliability(sh, succ);
        }

        int distill_ok = (result->sigma_mean < tau_a);
        if (distill_ok) {
            for (int i = 0; i < result->n_subtasks; ++i) {
                if (result->subtasks[i].final_action != 0) {
                    distill_ok = 0;
                    break;
                }
            }
        }
        if (distill_ok) {
            struct cos_skill out;
            if (cos_skill_distill(result, &out) == 0
                && cos_skill_save(&out) != 0)
                fprintf(stderr,
                        "cos think: warning: skill save failed\n");
        }
        cos_skill_clear_lookup_hash();
    }

    think_sess_shutdown(&S);
    return 0;
}

static void json_esc_append(char *dst, size_t cap, size_t *w, const char *s) {
    if (!dst || !w) return;
    for (const char *p = s ? s : ""; *p && *w + 4 < cap; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            dst[(*w)++] = '\\';
            dst[(*w)++] = (char)c;
        } else if (c == '\n') {
            dst[(*w)++] = '\\';
            dst[(*w)++] = 'n';
        } else if (c == '\r') {
            dst[(*w)++] = '\\';
            dst[(*w)++] = 'r';
        } else if (c == '\t') {
            dst[(*w)++] = '\\';
            dst[(*w)++] = 't';
        } else if (c < 0x20) {
            int n = snprintf(dst + *w, cap - *w, "\\u%04x", c);
            if (n > 0) *w += (size_t)n;
        } else {
            dst[(*w)++] = (char)c;
        }
    }
    if (*w < cap) dst[*w] = '\0';
}

char *cos_think_report_to_json(const struct cos_think_result *r) {
    if (!r) return NULL;
    size_t cap = 65536;
    char  *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    size_t w = 0;
#define JAPP(...)                                                               \
    do {                                                                        \
        int _n = snprintf(buf + w, cap - w, __VA_ARGS__);                       \
        if (_n > 0) w += (size_t)_n;                                             \
    } while (0)

    JAPP("{\"goal\":\"");
    json_esc_append(buf, cap, &w, r->goal);
    JAPP("\",\"timestamp\":%lld,\"n_subtasks\":%d,"
         "\"sigma_mean\":%.6f,\"sigma_min\":%.6f,\"sigma_max\":%.6f,"
         "\"best_subtask_idx\":%d,\"consensus\":%d,\"total_ms\":%lld,"
         "\"subtasks\":[",
         (long long)r->timestamp, r->n_subtasks, (double)r->sigma_mean,
         (double)r->sigma_min, (double)r->sigma_max, r->best_subtask_idx,
         r->consensus, (long long)r->total_ms);

    static const char *srcnm[] = {"NONE", "EPISTEMIC", "ALEATORIC",
                                  "REASONING", "MEMORY", "NOVEL_DOMAIN"};
    for (int i = 0; i < r->n_subtasks; ++i) {
        const struct cos_think_subtask *st = &r->subtasks[i];
        JAPP("%s{\"prompt\":\"", i ? "," : "");
        json_esc_append(buf, cap, &w, st->prompt);
        JAPP("\",\"answer\":\"");
        json_esc_append(buf, cap, &w, st->answer);
        const char *sn = "NONE";
        int          ai = (int)st->attribution;
        if (ai >= 0 && ai < 6) sn = srcnm[ai];
        JAPP("\",\"sigma_combined\":%.6f,\"final_action\":%d,"
             "\"attribution\":\"%s\","
             "\"backend\":\"%s\",\"latency_ms\":%lld}",
             (double)st->sigma_combined, st->final_action, sn,
             st->backend ? "CLOUD" : "LOCAL",
             (long long)st->latency_ms);
    }
    JAPP("]}");
    if (w >= cap) w = cap - 1;
    buf[w] = '\0';
    return buf;
}

static int ensure_dir(const char *path) {
#if defined(__unix__) || defined(__APPLE__)
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
#endif
    return 0;
}

/** Create ~/.cos then ~/.cos/think under HOME. */
static int ensure_cos_think_dir(char *full_out, size_t full_cap, const char *home) {
    char base[512];
    snprintf(base, sizeof(base), "%s/.cos", home);
    if (ensure_dir(base) != 0) return -1;
    snprintf(full_out, full_cap, "%s/.cos/think", home);
    if (ensure_dir(full_out) != 0) return -1;
    return 0;
}

static void goal_slug(const char *goal, char *out, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; goal[i] && j + 1 < cap && i < 48; ++i) {
        unsigned char c = (unsigned char)goal[i];
        if (isalnum(c))
            out[j++] = (char)tolower((int)c);
        else if (c == ' ' || c == '_' || c == '-')
            out[j++] = '_';
    }
    if (j == 0) {
        snprintf(out, cap, "goal");
    } else {
        out[j] = '\0';
    }
}

int cos_think_persist(const struct cos_think_result *r) {
    if (!r) return -1;
    const char *home = getenv("HOME");
    if (!home || !home[0]) return -2;

    char dir[512];
    if (ensure_cos_think_dir(dir, sizeof(dir), home) != 0)
        return -3;

    char slug[128];
    goal_slug(r->goal, slug, sizeof(slug));

    char path[768];
    snprintf(path, sizeof(path), "%s/%lld_%s.json", dir,
             (long long)r->timestamp, slug);

    char *json = cos_think_report_to_json(r);
    if (!json) return -4;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        free(json);
        return -5;
    }
    fputs(json, fp);
    fclose(fp);
    free(json);
    return 0;
}

static const char *think_src_abbr(enum cos_error_source s) {
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

void cos_think_print_report(const struct cos_think_result *r) {
    if (!r) return;
    printf("%s\n", "┌─────────────────────────────────┐");
    printf("│ %-31s │\n", "cos think report");
    printf("│ goal: %-24s│\n", r->goal);
    printf("│ subtasks: %-21d │\n", r->n_subtasks);
    printf("│ σ_mean: %.2f  σ_min: %.2f      │\n", (double)r->sigma_mean,
           (double)r->sigma_min);
    printf("│ consensus: %-18s │\n", r->consensus ? "YES" : "NO");
    printf("│ best: subtask %d (σ=%.2f)        │\n",
           r->best_subtask_idx + 1,
           (double)r->subtasks[r->best_subtask_idx].sigma_combined);
    printf("│ time: %lld ms                    │\n", (long long)r->total_ms);
    printf("%s\n", "├─────────────────────────────────┤");
    for (int i = 0; i < r->n_subtasks; ++i) {
        const struct cos_think_subtask *st = &r->subtasks[i];
        printf("│ [%d] σ=%.2f %s %s      \n", i + 1, (double)st->sigma_combined,
               think_src_abbr(st->attribution),
               st->backend ? "CLOUD" : "LOCAL");
        printf("│ Q: %.30s\n", st->prompt);
        printf("│ A: %.30s\n", st->answer);
        printf("│                                 \n");
    }
    printf("%s\n", "└─────────────────────────────────┘");
}

static void print_usage(void) {
    fputs(
        "cos think — σ-orchestrated goal decomposition\n"
        "  --goal TEXT       required goal string\n"
        "  --rounds N        pipeline rethink budget per subtask (default 3)\n"
        "  --backends LIST   local | local,cloud (default: cloud allowed)\n"
        "  --tau-accept F    σ gate accept threshold (default 0.35)\n"
        "  --tau-rethink F   σ gate rethink ceiling (default 0.70)\n"
        "  --no-persist      skip ~/.cos/think/*.json\n"
        "  --quiet           suppress box report\n"
        "  --json            emit JSON only on stdout\n"
        "  env: COS_SKILL_DISABLE=1  — no BSC skill lookup / distillation\n"
        "  env: COS_SKILL_HAMMING_MAX  — match threshold (default 0.35)\n",
        stderr);
}

int cos_think_main(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0)
            return cos_think_self_test() != 0 ? 1 : 0;
    }

    const char *goal       = NULL;
    int         rounds     = 3;
    int         quiet      = 0;
    int         json_stdout = 0;
    int         no_persist = 0;
    float       tau_a      = 0.35f;
    float       tau_r      = 0.70f;
    char        backends[64] = "local,cloud";

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--goal") == 0 && i + 1 < argc) {
            goal = argv[++i];
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--backends") == 0 && i + 1 < argc) {
            snprintf(backends, sizeof(backends), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--tau-accept") == 0 && i + 1 < argc) {
            tau_a = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--tau-rethink") == 0 && i + 1 < argc) {
            tau_r = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_stdout = 1;
        } else if (strcmp(argv[i], "--no-persist") == 0) {
            no_persist = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    if (!goal || !goal[0]) {
        fputs("cos think: --goal required\n", stderr);
        print_usage();
        return 2;
    }

    setenv("COS_THINK_BACKENDS", backends, 1);
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "%g", (double)tau_a);
    setenv("COS_THINK_TAU_ACCEPT", tbuf, 1);
    snprintf(tbuf, sizeof(tbuf), "%g", (double)tau_r);
    setenv("COS_THINK_TAU_RETHINK", tbuf, 1);

    struct cos_think_result res;
    if (cos_think_run(goal, rounds, &res) != 0) {
        fputs("cos think: run failed\n", stderr);
        return 3;
    }

    if (!quiet && !json_stdout)
        cos_think_print_report(&res);

    if (!no_persist && cos_think_persist(&res) != 0)
        fputs("cos think: warning: persist failed\n", stderr);

    if (json_stdout) {
        char *js = cos_think_report_to_json(&res);
        if (js) {
            printf("%s\n", js);
            free(js);
        }
    }
    return 0;
}

#if defined(COS_THINK_MAIN)
int main(int argc, char **argv) { return cos_think_main(argc - 1, argv + 1); }
#endif

int cos_think_self_test(void) {
    int fails = 0;

    char lines[COS_THINK_MAX_SUBTASKS][1024];
    int  n = 0;
    think_parse_lines("alpha\nbeta\n", lines, COS_THINK_MAX_SUBTASKS, &n);
    if (n != 2) fails++;

    struct cos_think_result r;
    memset(&r, 0, sizeof(r));
    snprintf(r.goal, sizeof(r.goal), "t");
    r.n_subtasks              = 3;
    r.timestamp               = 1;
    snprintf(r.subtasks[0].answer, sizeof(r.subtasks[0].answer), "same");
    snprintf(r.subtasks[1].answer, sizeof(r.subtasks[1].answer), "same");
    snprintf(r.subtasks[2].answer, sizeof(r.subtasks[2].answer), "same");
    if (think_compute_consensus(&r) != 1) fails++;

    snprintf(r.subtasks[0].answer, sizeof(r.subtasks[0].answer), "aaa");
    snprintf(r.subtasks[1].answer, sizeof(r.subtasks[1].answer), "bbb");
    snprintf(r.subtasks[2].answer, sizeof(r.subtasks[2].answer), "ccc");
    if (think_compute_consensus(&r) != 0) fails++;

    char *js = cos_think_report_to_json(&r);
    if (!js || !strstr(js, "aaa")) fails++;
    free(js);

    /* Persistence smoke (best-effort). */
#if defined(__unix__) || defined(__APPLE__)
    {
        char tmpl[] = "/tmp/cos_think_selftest_XXXXXX";
        char *dir   = mkdtemp(tmpl);
        if (dir != NULL) {
            char old_home[512];
            old_home[0] = '\0';
            const char *oh0 = getenv("HOME");
            if (oh0)
                snprintf(old_home, sizeof(old_home), "%s", oh0);
            setenv("HOME", dir, 1);
            memset(&r, 0, sizeof(r));
            snprintf(r.goal, sizeof(r.goal), "persist_test");
            r.timestamp  = 42;
            r.n_subtasks = 1;
            if (cos_think_persist(&r) != 0) fails++;
            char path[512];
            snprintf(path, sizeof(path), "%s/.cos/think/42_persist_test.json", dir);
            FILE *fp = fopen(path, "r");
            if (!fp) fails++;
            else
                fclose(fp);
            if (old_home[0])
                setenv("HOME", old_home, 1);
            else
                unsetenv("HOME");
        }
    }
#endif

    char        home_save[512];
    home_save[0] = '\0';
    const char *hs = getenv("HOME");
    if (hs)
        snprintf(home_save, sizeof(home_save), "%s", hs);
    setenv("COS_ENGRAM_DISABLE", "1", 1);
    const char *sav_b = getenv("COS_BITNET_SERVER");
    const char *sav_e = getenv("COS_BITNET_SERVER_EXTERNAL");
    const char *sav_x = getenv("CREATION_OS_BITNET_EXE");
    char        buf_b[128], buf_e[128];
    char        buf_x[512];
    buf_b[0] = buf_e[0] = buf_x[0] = '\0';
    if (sav_b)
        snprintf(buf_b, sizeof(buf_b), "%s", sav_b);
    if (sav_e)
        snprintf(buf_e, sizeof(buf_e), "%s", sav_e);
    if (sav_x)
        snprintf(buf_x, sizeof(buf_x), "%s", sav_x);
    unsetenv("COS_BITNET_SERVER");
    unsetenv("COS_BITNET_SERVER_EXTERNAL");
    unsetenv("CREATION_OS_BITNET_EXE");
    struct cos_think_result rr;
    if (cos_think_run("low:harness_goal", 3, &rr) != 0)
        fails++;
    else if (rr.n_subtasks < 1)
        fails++;
    if (buf_b[0])
        setenv("COS_BITNET_SERVER", buf_b, 1);
    else
        unsetenv("COS_BITNET_SERVER");
    if (buf_e[0])
        setenv("COS_BITNET_SERVER_EXTERNAL", buf_e, 1);
    else
        unsetenv("COS_BITNET_SERVER_EXTERNAL");
    if (buf_x[0])
        setenv("CREATION_OS_BITNET_EXE", buf_x, 1);
    else
        unsetenv("CREATION_OS_BITNET_EXE");
    if (home_save[0])
        setenv("HOME", home_save, 1);
    unsetenv("COS_ENGRAM_DISABLE");

    unsetenv("COS_THINK_BACKENDS");
    unsetenv("COS_THINK_TAU_ACCEPT");
    unsetenv("COS_THINK_TAU_RETHINK");

    return fails;
}
