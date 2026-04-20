/*
 * σ-Stream implementation.  See stream.h for contracts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "stream.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float stream_clip01(float x) {
    if (!(x == x)) return 0.0f;
    if (x < 0.0f)  return 0.0f;
    if (x > 1.0f)  return 1.0f;
    return x;
}

void cos_sigma_stream_trace_init(cos_sigma_stream_trace_t *tr) {
    if (!tr) return;
    memset(tr, 0, sizeof *tr);
}

void cos_sigma_stream_trace_free(cos_sigma_stream_trace_t *tr) {
    if (!tr) return;
    free(tr->text);            tr->text = NULL;
    free(tr->sigma_per_token); tr->sigma_per_token = NULL;
    memset(tr, 0, sizeof *tr);
}

/* ------------------------------------------------------------------ *
 *  Buffer growth                                                     *
 * ------------------------------------------------------------------ */

static int stream_reserve_sigma(cos_sigma_stream_trace_t *tr,
                                int needed, int cap_tokens) {
    /* Grow the float buffer to hold at least `needed` entries.
     * Capacity tracked implicitly via `tr->n_tokens + 1` after
     * every successful growth — we always bump to the next grain
     * boundary so subsequent callers with needed ≤ current don't
     * reallocate. */
    static const int GRAIN = 128;
    if (needed <= 0)              return 0;
    if (needed > cap_tokens)      needed = cap_tokens;
    /* Target capacity: round `needed` up to the next multiple of
     * GRAIN (clamped by cap_tokens). */
    int want = ((needed + GRAIN - 1) / GRAIN) * GRAIN;
    if (want > cap_tokens) want = cap_tokens;

    if (!tr->sigma_per_token) {
        tr->sigma_per_token = (float *)calloc((size_t)want, sizeof *tr->sigma_per_token);
        return tr->sigma_per_token ? 0 : -1;
    }
    /* Already-allocated block holds at least
     * `((n_tokens + GRAIN - 1) / GRAIN) * GRAIN` entries because
     * every growth stays on the grain; `needed` ≤ that value means
     * no reallocation is required. */
    int have = ((tr->n_tokens + GRAIN - 1) / GRAIN) * GRAIN;
    if (have < GRAIN) have = GRAIN;
    if (needed <= have) return 0;
    float *n2 = (float *)realloc(tr->sigma_per_token, (size_t)want * sizeof *n2);
    if (!n2) return -1;
    tr->sigma_per_token = n2;
    return 0;
}

static int stream_append_text(cos_sigma_stream_trace_t *tr,
                              const char *s, size_t n,
                              size_t cap_text) {
    if (n == 0) return 0;
    size_t room = cap_text > tr->text_len ? cap_text - tr->text_len : 0;
    size_t take = n < room ? n : room;
    if (take < n) tr->truncated_text = 1;
    if (take == 0) return 0;

    size_t want = tr->text_len + take + 1;
    char  *nb   = (char *)realloc(tr->text, want);
    if (!nb) return -1;
    tr->text = nb;
    memcpy(tr->text + tr->text_len, s, take);
    tr->text_len += take;
    tr->text[tr->text_len] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */

int cos_sigma_stream_run(const cos_sigma_stream_config_t *cfg,
                         cos_sigma_stream_gen_t           gen,
                         void                            *gen_state,
                         cos_sigma_stream_callback_t      cb,
                         void                            *user,
                         cos_sigma_stream_trace_t        *trace) {
    if (!gen) return COS_STREAM_ERR_ARG;

    float tau   = (cfg && cfg->tau_rethink > 0) ? cfg->tau_rethink
                                                : COS_STREAM_TAU_RETHINK_DEFAULT;
    tau = stream_clip01(tau);
    int cap_tokens = (cfg && cfg->max_tokens > 0) ? cfg->max_tokens
                                                  : COS_STREAM_MAX_TOKENS;
    size_t cap_text = (cfg && cfg->max_text > 0) ? cfg->max_text
                                                 : COS_STREAM_MAX_TEXT;
    int suppress_empty = cfg ? cfg->suppress_zero_len : 0;

    cos_sigma_stream_trace_t local;
    cos_sigma_stream_trace_t *tr = trace;
    if (!tr) { tr = &local; cos_sigma_stream_trace_init(tr); }

    int rethink_pending = 0;
    float last_sigma    = 0.0f;

    for (;;) {
        const char *tok = NULL; float s = 0.0f; int done = 0;
        int rc = gen(gen_state, &tok, &s, &done);
        if (rc < 0) {
            tr->gen_error = 1;
            if (!trace) cos_sigma_stream_trace_free(&local);
            return COS_STREAM_ERR_GEN;
        }
        if (done) break;

        s = stream_clip01(s);
        last_sigma = s;

        /* Rethink marker fires *before* the token is emitted so the
         * caller sees "pause, rethink, then the revised token". */
        if (s >= tau) {
            if (cb) cb(NULL, s, 1, user);
            tr->n_rethink++;
            rethink_pending = 1;
        }

        size_t tlen = tok ? strlen(tok) : 0;
        if (tlen == 0 && suppress_empty) continue;

        if (tr->n_tokens >= cap_tokens) {
            tr->truncated_tokens = 1;
            continue;
        }
        if (stream_reserve_sigma(tr, tr->n_tokens + 1, cap_tokens) < 0) {
            if (!trace) cos_sigma_stream_trace_free(&local);
            return COS_STREAM_ERR_OOM;
        }
        tr->sigma_per_token[tr->n_tokens] = s;
        tr->n_tokens++;

        if (tlen > 0) {
            if (stream_append_text(tr, tok, tlen, cap_text) < 0) {
                if (!trace) cos_sigma_stream_trace_free(&local);
                return COS_STREAM_ERR_OOM;
            }
        }
        tr->sigma_sum    += s;
        tr->sigma_sum_sq += s * s;
        if (s > tr->sigma_max) tr->sigma_max = s;
        if (cb) cb(tok ? tok : "", s, 0, user);
        rethink_pending = 0;
        (void)rethink_pending;
    }
    (void)last_sigma;

    if (tr->n_tokens > 0) {
        tr->sigma_mean = tr->sigma_sum / (float)tr->n_tokens;
        float var = tr->sigma_sum_sq / (float)tr->n_tokens
                  - tr->sigma_mean * tr->sigma_mean;
        if (var < 0.0f) var = 0.0f;
        tr->sigma_std = sqrtf(var);
    } else {
        tr->sigma_mean = 0.0f;
        tr->sigma_std  = 0.0f;
    }

    if (!trace) cos_sigma_stream_trace_free(&local);
    return COS_STREAM_OK;
}

/* ------------------------------------------------------------------ *
 *  Self-test                                                         *
 * ------------------------------------------------------------------ */

typedef struct {
    const char *const *tokens;
    const float        *sigmas;
    int                 n;
    int                 idx;
} st_gen_t;

static int st_gen_fn(void *s, const char **out_tok, float *out_sig, int *done) {
    st_gen_t *g = (st_gen_t *)s;
    if (g->idx >= g->n) { *done = 1; return 0; }
    *out_tok = g->tokens[g->idx];
    *out_sig = g->sigmas[g->idx];
    *done    = 0;
    g->idx++;
    return 0;
}

typedef struct {
    int tokens, rethinks;
} st_count_t;

static void st_cb(const char *t, float s, int rethink, void *u) {
    (void)t; (void)s;
    st_count_t *c = (st_count_t *)u;
    if (rethink) c->rethinks++;
    else         c->tokens++;
}

int cos_sigma_stream_self_test(void) {
    const char *TOK[]   = {"Quantum ", "entanglement ", "is ",
                           "a ", "phenomenon ",
                           "where ", "particles ", "become ", "correlated."};
    const float SIG[]   = { 0.05f,     0.08f,           0.03f,
                            0.02f,     0.55f,
                            0.04f,     0.60f,           0.06f,   0.09f };
    const int N = (int)(sizeof TOK / sizeof TOK[0]);

    st_gen_t g = { .tokens=TOK, .sigmas=SIG, .n=N, .idx=0 };
    st_count_t c = {0};

    cos_sigma_stream_config_t cfg = { .tau_rethink = 0.50f };
    cos_sigma_stream_trace_t  tr; cos_sigma_stream_trace_init(&tr);
    if (cos_sigma_stream_run(&cfg, st_gen_fn, &g, st_cb, &c, &tr) != COS_STREAM_OK)
        return -1;

    if (tr.n_tokens  != N) { cos_sigma_stream_trace_free(&tr); return -2; }
    /* Two tokens exceed τ = 0.50 → two rethinks fired. */
    if (tr.n_rethink != 2) { cos_sigma_stream_trace_free(&tr); return -3; }
    if (c.tokens     != N) { cos_sigma_stream_trace_free(&tr); return -4; }
    if (c.rethinks   != 2) { cos_sigma_stream_trace_free(&tr); return -5; }
    if (tr.sigma_max < 0.60f - 1e-6f ||
        tr.sigma_max > 0.60f + 1e-6f) { cos_sigma_stream_trace_free(&tr); return -6; }
    if (tr.truncated_tokens || tr.truncated_text) {
        cos_sigma_stream_trace_free(&tr); return -7;
    }
    if (!tr.text || strncmp(tr.text, "Quantum ", 8) != 0) {
        cos_sigma_stream_trace_free(&tr); return -8;
    }
    cos_sigma_stream_trace_free(&tr);
    return 0;
}
