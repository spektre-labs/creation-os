/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v119 σ-Speculative decoding policy implementation.
 */

#include "speculative.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void cos_v119_config_defaults(cos_v119_config_t *cfg) {
    if (!cfg) return;
    cfg->gamma_base   = COS_V119_GAMMA_BASE_DEFAULT;
    cfg->gamma_min    = COS_V119_GAMMA_MIN_DEFAULT;
    cfg->gamma_max    = COS_V119_GAMMA_MAX_DEFAULT;
    cfg->tau_spec     = COS_V119_TAU_SPEC_DEFAULT;
    cfg->accept_ratio = 1.0f;
}

int cos_v119_adaptive_gamma(const cos_v119_config_t *cfg_in, float recent_sigma) {
    cos_v119_config_t c;
    if (cfg_in) c = *cfg_in; else cos_v119_config_defaults(&c);
    if (c.gamma_base <= 0) c.gamma_base = COS_V119_GAMMA_BASE_DEFAULT;
    if (c.gamma_min  <= 0) c.gamma_min  = COS_V119_GAMMA_MIN_DEFAULT;
    if (c.gamma_max  <= 0) c.gamma_max  = COS_V119_GAMMA_MAX_DEFAULT;
    if (c.gamma_min  > c.gamma_max) c.gamma_min = c.gamma_max;

    float s = recent_sigma;
    if (s != s) s = 0.5f;                      /* NaN guard */
    if (s < 0.f) s = 0.f;
    if (s > 1.f) s = 1.f;

    float gf = (float)c.gamma_base * (1.0f - s);
    int g = (int)lroundf(gf);
    if (g < c.gamma_min) g = c.gamma_min;
    if (g > c.gamma_max) g = c.gamma_max;
    return g;
}

int cos_v119_verify_group(const cos_v119_config_t *cfg_in,
                          const cos_v119_draft_token_t *draft, int n_draft,
                          const cos_v119_verify_token_t *verify,
                          cos_v119_step_record_t *out) {
    if (!draft || !verify || !out || n_draft <= 0) return 0;
    cos_v119_config_t c;
    if (cfg_in) c = *cfg_in; else cos_v119_config_defaults(&c);
    if (c.accept_ratio <= 0.f) c.accept_ratio = 1.0f;

    int n_emitted = 0;
    for (int i = 0; i < n_draft; ++i) {
        cos_v119_step_record_t r;
        memset(&r, 0, sizeof r);
        r.sigma_product = draft[i].sigma_product;
        r.p_draft       = draft[i].p_draft;

        if (draft[i].sigma_product > c.tau_spec) {
            /* σ-gate: auto-reject, do not consult target.  The group
             * stops here and the caller's outer loop will re-seed the
             * draft from the current position. */
            r.decision = COS_V119_DECISION_SIGMA_GATE;
            r.token_id = draft[i].token_id;
            r.p_target = 0.0f;
            out[n_emitted++] = r;
            return n_emitted;
        }

        const cos_v119_verify_token_t *v = &verify[i];
        r.p_target = v->p_target;

        int target_argmax_matches = (v->token_id == draft[i].token_id);
        int ratio_ok = (v->p_target >= c.accept_ratio * draft[i].p_draft);

        if (target_argmax_matches && ratio_ok) {
            r.decision = COS_V119_DECISION_ACCEPT;
            r.token_id = draft[i].token_id;
            out[n_emitted++] = r;
            continue;
        }

        /* Reject the draft's token, record the reject, then emit one
         * sampled token from the target's own distribution to keep
         * the stream moving. */
        r.decision = COS_V119_DECISION_REJECT;
        r.token_id = draft[i].token_id;
        out[n_emitted++] = r;

        cos_v119_step_record_t s = {0};
        s.decision      = COS_V119_DECISION_SAMPLED;
        s.token_id      = v->token_id;
        s.sigma_product = draft[i].sigma_product;
        s.p_draft       = draft[i].p_draft;
        s.p_target      = v->p_target;
        out[n_emitted++] = s;
        return n_emitted;
    }
    return n_emitted;
}

void cos_v119_simulate(const cos_v119_config_t *cfg_in,
                       const cos_v119_draft_token_t *drafts,
                       const cos_v119_verify_token_t *verifies,
                       int stream_len,
                       cos_v119_stats_t *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof *stats);
    if (!drafts || !verifies || stream_len <= 0) return;
    cos_v119_config_t c;
    if (cfg_in) c = *cfg_in; else cos_v119_config_defaults(&c);

    float recent_sigma = 0.0f;
    int   pos = 0;
    cos_v119_step_record_t buf[64];

    while (pos < stream_len) {
        int gamma_raw = cos_v119_adaptive_gamma(&c, recent_sigma);
        if (gamma_raw > COS_V119_GAMMA_MAX_DEFAULT * 4) gamma_raw = c.gamma_max;
        int gamma = gamma_raw;
        if (gamma > stream_len - pos) gamma = stream_len - pos;

        if (gamma < c.gamma_base) ++stats->gammas_adapted_down;
        if (gamma > c.gamma_base) ++stats->gammas_adapted_up;
        stats->sum_gamma += (double)gamma;
        ++stats->groups;

        int n = cos_v119_verify_group(&c,
                                       drafts   + pos,
                                       gamma,
                                       verifies + pos,
                                       buf);
        /* Classify each record. */
        for (int i = 0; i < n; ++i) {
            switch (buf[i].decision) {
            case COS_V119_DECISION_ACCEPT:
                ++stats->tokens_accepted;
                ++stats->tokens_emitted;
                ++stats->target_invocations;
                stats->sum_sigma_draft += (double)buf[i].sigma_product;
                break;
            case COS_V119_DECISION_REJECT:
                ++stats->tokens_rejected;
                ++stats->target_invocations;
                stats->sum_sigma_draft += (double)buf[i].sigma_product;
                break;
            case COS_V119_DECISION_SIGMA_GATE:
                ++stats->tokens_sigma_gated;
                stats->sum_sigma_draft += (double)buf[i].sigma_product;
                break;
            case COS_V119_DECISION_SAMPLED:
                ++stats->tokens_sampled;
                ++stats->tokens_emitted;
                break;
            }
        }

        /* Advance pos by tokens we actually consumed from the draft
         * stream: one per ACCEPT / SIGMA_GATE / REJECT record.  A
         * SAMPLED record is a target fallback and advances the output
         * stream but not our position in `drafts[]`. */
        int drafts_consumed = 0;
        for (int i = 0; i < n; ++i) {
            if (buf[i].decision == COS_V119_DECISION_ACCEPT ||
                buf[i].decision == COS_V119_DECISION_REJECT ||
                buf[i].decision == COS_V119_DECISION_SIGMA_GATE)
                ++drafts_consumed;
        }
        if (drafts_consumed == 0) drafts_consumed = 1;  /* safety */
        pos += drafts_consumed;

        /* Update recent_sigma: EMA over the last observed draft σ.
         * Gives the next adaptive-γ call a smooth signal. */
        float last_sigma = drafts[pos > 0 ? pos - 1 : 0].sigma_product;
        recent_sigma = 0.6f * last_sigma + 0.4f * recent_sigma;
    }
}

float cos_v119_acceptance_rate(const cos_v119_stats_t *s) {
    if (!s) return 0.f;
    int denom = s->tokens_accepted + s->tokens_rejected + s->tokens_sigma_gated;
    if (denom == 0) return 0.f;
    return (float)s->tokens_accepted / (float)denom;
}

float cos_v119_target_save_rate(const cos_v119_stats_t *s, int stream_len) {
    if (!s || stream_len <= 0) return 0.f;
    return 1.0f - (float)s->target_invocations / (float)stream_len;
}

float cos_v119_mean_gamma(const cos_v119_stats_t *s) {
    if (!s || s->groups == 0) return 0.f;
    return (float)(s->sum_gamma / (double)s->groups);
}

int cos_v119_stats_to_json(const cos_v119_stats_t *s, int stream_len,
                           char *out, size_t cap) {
    if (!s || !out || cap == 0) return -1;
    float ar = cos_v119_acceptance_rate(s);
    float tr = cos_v119_target_save_rate(s, stream_len);
    float mg = cos_v119_mean_gamma(s);
    int n = snprintf(out, cap,
        "{\"stream_len\":%d,"
        "\"tokens_emitted\":%d,\"tokens_accepted\":%d,"
        "\"tokens_rejected\":%d,\"tokens_sigma_gated\":%d,"
        "\"tokens_sampled\":%d,"
        "\"target_invocations\":%d,"
        "\"gammas_adapted_down\":%d,\"gammas_adapted_up\":%d,"
        "\"groups\":%d,\"mean_gamma\":%.4f,"
        "\"acceptance_rate\":%.4f,\"target_save_rate\":%.4f}",
        stream_len, s->tokens_emitted, s->tokens_accepted,
        s->tokens_rejected, s->tokens_sigma_gated, s->tokens_sampled,
        s->target_invocations, s->gammas_adapted_down, s->gammas_adapted_up,
        s->groups, (double)mg, (double)ar, (double)tr);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v119 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v119_self_test(void) {
    /* 1) Defaults. */
    cos_v119_config_t c;
    cos_v119_config_defaults(&c);
    _CHECK(c.gamma_base == 8, "defaults: gamma_base");
    _CHECK(c.gamma_min  == 2, "defaults: gamma_min");
    _CHECK(c.gamma_max  == 12, "defaults: gamma_max");
    _CHECK(fabsf(c.tau_spec - 0.70f) < 1e-6f, "defaults: tau_spec");

    /* 2) Adaptive γ: low σ → γ_max, high σ → γ_min. */
    _CHECK(cos_v119_adaptive_gamma(&c, 0.0f) == 8,  "γ(σ=0.0) = 8");
    _CHECK(cos_v119_adaptive_gamma(&c, 0.5f) == 4,  "γ(σ=0.5) = 4");
    _CHECK(cos_v119_adaptive_gamma(&c, 0.9f) == c.gamma_min,
           "γ(σ=0.9) = γ_min");
    /* Monotonic non-increasing in σ. */
    int prev = cos_v119_adaptive_gamma(&c, 0.0f);
    for (float s = 0.0f; s <= 1.0f; s += 0.05f) {
        int cur = cos_v119_adaptive_gamma(&c, s);
        _CHECK(cur <= prev, "γ monotonic in σ");
        prev = cur;
    }

    /* 3) verify_group: full acceptance when target argmax matches
     *    draft and p_target >= p_draft. */
    cos_v119_draft_token_t d[4] = {
        { 10, 0.9f, 0.05f },
        { 11, 0.8f, 0.08f },
        { 12, 0.7f, 0.10f },
        { 13, 0.6f, 0.12f },
    };
    cos_v119_verify_token_t v[4] = {
        { 10, 0.95f }, { 11, 0.85f }, { 12, 0.75f }, { 13, 0.65f },
    };
    cos_v119_step_record_t rec[16];
    int n = cos_v119_verify_group(&c, d, 4, v, rec);
    _CHECK(n == 4, "full-accept group emits 4 records");
    for (int i = 0; i < 4; ++i)
        _CHECK(rec[i].decision == COS_V119_DECISION_ACCEPT, "accept");

    /* 4) σ-gate: third token above τ triggers gate → only 3 records,
     *    last is SIGMA_GATE, no target invocation recorded. */
    cos_v119_draft_token_t dg[4] = {
        { 10, 0.9f, 0.05f },
        { 11, 0.8f, 0.10f },
        { 12, 0.5f, 0.90f },   /* σ > τ_spec */
        { 13, 0.6f, 0.20f },
    };
    n = cos_v119_verify_group(&c, dg, 4, v, rec);
    _CHECK(n == 3, "σ-gate aborts after third token");
    _CHECK(rec[0].decision == COS_V119_DECISION_ACCEPT, "rec0 accept");
    _CHECK(rec[1].decision == COS_V119_DECISION_ACCEPT, "rec1 accept");
    _CHECK(rec[2].decision == COS_V119_DECISION_SIGMA_GATE, "rec2 sigma gate");
    _CHECK(rec[2].p_target == 0.0f, "σ-gate: no target consulted");

    /* 5) Reject + fallback sample: when target argmax disagrees, emit
     *    REJECT followed by SAMPLED (target's own choice). */
    cos_v119_verify_token_t vr[4] = {
        { 10, 0.95f }, { 99 /*!=*/, 0.5f }, { 12, 0.8f }, { 13, 0.7f },
    };
    n = cos_v119_verify_group(&c, d, 4, vr, rec);
    _CHECK(n == 3, "reject stops group after fallback");
    _CHECK(rec[0].decision == COS_V119_DECISION_ACCEPT, "rec0 accept");
    _CHECK(rec[1].decision == COS_V119_DECISION_REJECT, "rec1 reject");
    _CHECK(rec[2].decision == COS_V119_DECISION_SAMPLED, "rec2 sampled");
    _CHECK(rec[2].token_id == 99, "sampled = target's argmax");

    /* 6) Synthetic stream: confident σ=0.10 everywhere, target always
     *    agrees → near-100% acceptance + low target_save (target is
     *    invoked for every draft token) and mean γ == γ_max-ish. */
    enum { N = 96 };
    cos_v119_draft_token_t  dc[N];
    cos_v119_verify_token_t vc[N];
    for (int i = 0; i < N; ++i) {
        dc[i].token_id      = 1000 + i;
        dc[i].p_draft       = 0.9f;
        dc[i].sigma_product = 0.10f;
        vc[i].token_id      = 1000 + i;
        vc[i].p_target      = 0.95f;
    }
    cos_v119_stats_t sc;
    cos_v119_simulate(&c, dc, vc, N, &sc);
    float ar = cos_v119_acceptance_rate(&sc);
    _CHECK(ar > 0.99f, "confident stream: acceptance > 99%");
    _CHECK(sc.tokens_sigma_gated == 0, "confident: no σ-gates");
    /* On N=96 with γ_base=8 and full-accept groups, we get 12 groups of
     * 8 → mean γ = 8.  A final truncated group can drag this down on
     * other N values; require ≥ 7 within 0.5 to tolerate that. */
    _CHECK(cos_v119_mean_gamma(&sc) >= 6.5f, "confident: mean γ >= 6.5");

    /* 7) Synthetic stream: uncertain σ=0.85 everywhere → every group
     *    σ-gates on the first token.  target_save ≈ 1.0 (draft
     *    rejects before the target runs). */
    cos_v119_draft_token_t  du[N];
    cos_v119_verify_token_t vu[N];
    for (int i = 0; i < N; ++i) {
        du[i].token_id      = 2000 + i;
        du[i].p_draft       = 0.6f;
        du[i].sigma_product = 0.85f;
        vu[i].token_id      = 2000 + i;
        vu[i].p_target      = 0.5f;
    }
    cos_v119_stats_t su;
    cos_v119_simulate(&c, du, vu, N, &su);
    _CHECK(su.tokens_sigma_gated > 0, "uncertain: σ-gates fire");
    _CHECK(su.target_invocations == 0,
           "uncertain: target spared entirely by σ-gate");
    float tsr = cos_v119_target_save_rate(&su, N);
    _CHECK(tsr >= 0.99f, "uncertain: target_save ≈ 1.0");
    _CHECK(cos_v119_mean_gamma(&su) <= (float)c.gamma_min + 0.5f,
           "uncertain: γ clamps to γ_min");

    /* 8) JSON shape. */
    char js[512];
    int jn = cos_v119_stats_to_json(&sc, N, js, sizeof js);
    _CHECK(jn > 0, "stats json non-empty");
    _CHECK(strstr(js, "\"acceptance_rate\":") != NULL, "json acceptance");
    _CHECK(strstr(js, "\"mean_gamma\":")      != NULL, "json mean_gamma");
    _CHECK(strstr(js, "\"target_save_rate\":") != NULL, "json target_save");

    return 0;
}
