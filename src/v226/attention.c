/*
 * v226 σ-Attention — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "attention.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v226_init(cos_v226_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x226A77E9ULL;
    s->tau_useful = 0.40f;
    s->tau_noisy  = 0.70f;
    s->sigma_head_min = 1.0f;
    s->sigma_head_max = 0.0f;
}

/* Head temperature profiles.  Low τ = very sharp softmax
 * (low σ); high τ = nearly uniform (high σ). */
static const float HEAD_TEMP[COS_V226_N_HEADS] = {
    0.90f, /* 0 mixed        */
    0.20f, /* 1 factual      (sharp) */
    0.75f, /* 2 mixed        */
    2.50f, /* 3 diffuse      (noisy) */
    1.00f, /* 4 mixed        */
    2.20f, /* 5 diffuse      (noisy) */
    0.25f, /* 6 factual      (sharp) */
    0.85f  /* 7 mixed        */
};

/* Build deterministic Q·K^T logits.  For head h, token
 * t, key k the "matching score" is
 *      qk[t][k] = cos(π · f_h(t,k))
 * with a head-dependent phase generator f_h so that
 * different heads peak on different keys. */
void cos_v226_build(cos_v226_state_t *s) {
    for (int h = 0; h < COS_V226_N_HEADS; ++h) {
        cos_v226_head_t *hd = &s->heads[h];
        hd->head_id     = h;
        hd->temperature = HEAD_TEMP[h];
        for (int t = 0; t < COS_V226_N_TOKENS; ++t) {
            for (int k = 0; k < COS_V226_KEY_LEN; ++k) {
                float ideal_k   = (float)((t + h) % COS_V226_KEY_LEN);
                float dist      = fabsf((float)k - ideal_k);
                /* Logit: negative distance from each head's
                 * preferred key; temperature later. */
                hd->qk[t][k]    = -dist;
            }
        }
    }
}

static void softmax_row(const float *in, float *out, int n, float T) {
    float mx = in[0];
    for (int i = 1; i < n; ++i) if (in[i] > mx) mx = in[i];
    float den = 0.0f;
    for (int i = 0; i < n; ++i) {
        out[i] = expf((in[i] - mx) / T);
        den += out[i];
    }
    if (den <= 0.0f) den = 1e-12f;
    for (int i = 0; i < n; ++i) out[i] /= den;
}

static float entropy_norm(const float *p, int n) {
    float h = 0.0f;
    for (int i = 0; i < n; ++i)
        if (p[i] > 1e-12f) h -= p[i] * logf(p[i]);
    float max_h = logf((float)n);
    if (max_h <= 0.0f) return 0.0f;
    float sig = h / max_h;
    if (sig < 0.0f) sig = 0.0f;
    if (sig > 1.0f) sig = 1.0f;
    return sig;
}

void cos_v226_run(cos_v226_state_t *s) {
    s->n_valuable = s->n_flagged = 0;
    s->n_boost = s->n_prune = s->n_keep = 0;
    s->sigma_head_min = 1.0f;
    s->sigma_head_max = 0.0f;

    for (int h = 0; h < COS_V226_N_HEADS; ++h) {
        cos_v226_head_t *hd = &s->heads[h];
        float sum_sig = 0.0f;
        for (int t = 0; t < COS_V226_N_TOKENS; ++t) {
            softmax_row(hd->qk[t], hd->softmax[t],
                        COS_V226_KEY_LEN, hd->temperature);
            hd->sigma_token[t] =
                entropy_norm(hd->softmax[t], COS_V226_KEY_LEN);
            sum_sig += hd->sigma_token[t];
        }
        hd->sigma_head = sum_sig / (float)COS_V226_N_TOKENS;
        if (hd->sigma_head < s->sigma_head_min) s->sigma_head_min = hd->sigma_head;
        if (hd->sigma_head > s->sigma_head_max) s->sigma_head_max = hd->sigma_head;

        if      (hd->sigma_head > s->tau_noisy) {
            hd->action = (int)COS_V226_ACTION_PRUNE;
            s->n_prune++; s->n_flagged++;
        } else if (hd->sigma_head < s->tau_useful) {
            hd->action = (int)COS_V226_ACTION_BOOST;
            s->n_boost++; s->n_valuable++;
        } else {
            hd->action = (int)COS_V226_ACTION_KEEP;
            s->n_keep++;
        }
    }

    /* FNV-1a chain: head → per-(t,k) qk & softmax →
     * per-token σ → per-head summary → global summary. */
    uint64_t prev = 0x226C01A7ULL;
    for (int h = 0; h < COS_V226_N_HEADS; ++h) {
        const cos_v226_head_t *hd = &s->heads[h];
        for (int t = 0; t < COS_V226_N_TOKENS; ++t) {
            for (int k = 0; k < COS_V226_KEY_LEN; ++k) {
                struct { int h, t, k; float qk, sm; uint64_t prev; } rec;
                memset(&rec, 0, sizeof(rec));
                rec.h = h; rec.t = t; rec.k = k;
                rec.qk = hd->qk[t][k]; rec.sm = hd->softmax[t][k];
                rec.prev = prev;
                prev = fnv1a(&rec, sizeof(rec), prev);
            }
            struct { int h, t; float sig; uint64_t prev; } rec2;
            memset(&rec2, 0, sizeof(rec2));
            rec2.h = h; rec2.t = t;
            rec2.sig = hd->sigma_token[t];
            rec2.prev = prev;
            prev = fnv1a(&rec2, sizeof(rec2), prev);
        }
        struct { int h, action; float temp, sig; uint64_t prev; } rec3;
        memset(&rec3, 0, sizeof(rec3));
        rec3.h = h;
        rec3.action = hd->action;
        rec3.temp = hd->temperature;
        rec3.sig = hd->sigma_head;
        rec3.prev = prev;
        prev = fnv1a(&rec3, sizeof(rec3), prev);
    }
    {
        struct { int nv, nf, nb, np, nk;
                 float mn, mx; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.nv = s->n_valuable;
        rec.nf = s->n_flagged;
        rec.nb = s->n_boost;
        rec.np = s->n_prune;
        rec.nk = s->n_keep;
        rec.mn = s->sigma_head_min;
        rec.mx = s->sigma_head_max;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v226_to_json(const cos_v226_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v226\","
        "\"n_heads\":%d,\"n_tokens\":%d,\"key_len\":%d,"
        "\"tau_useful\":%.3f,\"tau_noisy\":%.3f,"
        "\"n_valuable\":%d,\"n_flagged\":%d,"
        "\"n_boost\":%d,\"n_prune\":%d,\"n_keep\":%d,"
        "\"sigma_head_min\":%.4f,\"sigma_head_max\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"heads\":[",
        COS_V226_N_HEADS, COS_V226_N_TOKENS, COS_V226_KEY_LEN,
        s->tau_useful, s->tau_noisy,
        s->n_valuable, s->n_flagged,
        s->n_boost, s->n_prune, s->n_keep,
        s->sigma_head_min, s->sigma_head_max,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int h = 0; h < COS_V226_N_HEADS; ++h) {
        const cos_v226_head_t *hd = &s->heads[h];
        const char *act = hd->action == (int)COS_V226_ACTION_PRUNE
            ? "prune" : (hd->action == (int)COS_V226_ACTION_BOOST
            ? "boost" : "keep");
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"temp\":%.3f,\"sigma\":%.4f,\"action\":\"%s\","
            "\"sigma_token\":[",
            h == 0 ? "" : ",", hd->head_id, hd->temperature,
            hd->sigma_head, act);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
        for (int t = 0; t < COS_V226_N_TOKENS; ++t) {
            int q = snprintf(buf + off, cap - off,
                "%s%.4f", t == 0 ? "" : ",", hd->sigma_token[t]);
            if (q < 0 || off + (size_t)q >= cap) return 0;
            off += (size_t)q;
        }
        int q2 = snprintf(buf + off, cap - off, "]}");
        if (q2 < 0 || off + (size_t)q2 >= cap) return 0;
        off += (size_t)q2;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v226_self_test(void) {
    cos_v226_state_t s;
    cos_v226_init(&s, 0x226A77E9ULL);
    cos_v226_build(&s);
    cos_v226_run(&s);
    if (!s.chain_valid) return 1;

    for (int h = 0; h < COS_V226_N_HEADS; ++h) {
        const cos_v226_head_t *hd = &s.heads[h];
        for (int t = 0; t < COS_V226_N_TOKENS; ++t) {
            float sum = 0.0f;
            for (int k = 0; k < COS_V226_KEY_LEN; ++k)
                sum += hd->softmax[t][k];
            if (fabsf(sum - 1.0f) > 1e-4f) return 2;
            if (hd->sigma_token[t] < 0.0f || hd->sigma_token[t] > 1.0f) return 3;
        }
        if (hd->sigma_head < 0.0f || hd->sigma_head > 1.0f) return 3;
    }
    if (s.n_valuable < 1) return 4;
    if (s.n_flagged  < 1) return 5;
    if ((s.sigma_head_max - s.sigma_head_min) < 0.30f) return 6;
    return 0;
}
