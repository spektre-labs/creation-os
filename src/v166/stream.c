/*
 * v166 σ-Stream — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "stream.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------- */
/* SplitMix64 PRNG                                               */
/* ------------------------------------------------------------- */

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ------------------------------------------------------------- */
/* Tokenization                                                  */
/* ------------------------------------------------------------- */

static bool is_sep(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == ',' || c == '.' || c == ';'  || c == ':'  ||
           c == '?' || c == '!'  || c == '/' || c == '\\';
}

int cos_v166_tokenize(const char *prompt, cos_v166_stream_t *out) {
    if (!out) return 0;
    memset(out->frames, 0, sizeof(out->frames));
    out->n_tokens = 0;
    if (!prompt) return 0;

    int i = 0;
    while (prompt[i] && out->n_tokens < COS_V166_MAX_TOKENS) {
        while (prompt[i] && is_sep(prompt[i])) ++i;
        if (!prompt[i]) break;
        int j = i;
        int k = 0;
        while (prompt[j] && !is_sep(prompt[j]) &&
               k < COS_V166_MAX_TOKEN - 1) {
            out->frames[out->n_tokens].token[k++] = prompt[j++];
        }
        out->frames[out->n_tokens].token[k] = '\0';
        /* skip the rest of an over-length word */
        while (prompt[j] && !is_sep(prompt[j])) ++j;
        out->n_tokens++;
        i = j;
    }
    /* guarantee at least one synthetic token */
    if (out->n_tokens == 0) {
        strcpy(out->frames[0].token, "<empty>");
        out->n_tokens = 1;
    }
    return out->n_tokens;
}

/* ------------------------------------------------------------- */
/* σ generator                                                    */
/* ------------------------------------------------------------- */

static float geomean_channels(const float *ch, int n) {
    double acc = 0.0;
    for (int i = 0; i < n; ++i) {
        double v = ch[i] < 1e-6f ? 1e-6 : (double)ch[i];
        acc += log(v);
    }
    return (float)exp(acc / n);
}

/* per-token σ per channel: base = 0.05..0.25 jitter + slow drift. */
static void generate_sigma(uint64_t seed, int seq, float ch[8]) {
    for (int c = 0; c < 8; ++c) {
        uint64_t s = seed ^
                     ((uint64_t)seq * 0xC2B2AE3D27D4EB4FULL) ^
                     ((uint64_t)c * 0x165667B19E3779F9ULL);
        float base = 0.05f + 0.20f * frand(&s);
        /* slow-drift component adds realism */
        float drift = 0.05f * sinf(0.37f * (float)seq + 0.11f * (float)c);
        ch[c] = clampf(base + drift, 0.0f, 1.0f);
    }
}

/* ------------------------------------------------------------- */
/* Streaming loop                                                */
/* ------------------------------------------------------------- */

void cos_v166_run(cos_v166_stream_t *s,
                  const char *prompt,
                  uint64_t seed,
                  float tau_interrupt,
                  int inject_burst_seq) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x16601664A5A5A5A5ULL;
    s->tau_interrupt = tau_interrupt;
    cos_v166_tokenize(prompt, s);

    s->frames[0].kind = COS_V166_EVENT_START;
    /* fill σ per token; inject burst where requested */
    for (int t = 0; t < s->n_tokens; ++t) {
        cos_v166_frame_t *f = &s->frames[t];
        f->kind = COS_V166_EVENT_TOKEN;
        f->seq  = t;
        generate_sigma(s->seed, t, f->channels);

        /* inject a hot burst: drive all channels high */
        if (inject_burst_seq >= 0 && t == inject_burst_seq) {
            for (int c = 0; c < 8; ++c) f->channels[c] = 0.85f + 0.02f * (float)c;
        }

        f->sigma_product = geomean_channels(f->channels, 8);

        /* voice hint: delay_ms = 40 + 400*σ  (higher σ → slower) */
        f->audible_delay_ms = 40 + (int)(400.0f * f->sigma_product);
    }

    /* Emit / interrupt loop. */
    s->n_emitted = 0;
    s->was_interrupted = false;
    s->interrupt_seq   = -1;

    for (int t = 0; t < s->n_tokens; ++t) {
        cos_v166_frame_t *f = &s->frames[t];
        s->n_emitted++;
        if (f->sigma_product > tau_interrupt) {
            s->was_interrupted = true;
            s->interrupt_seq   = t;
            s->sigma_final     = f->sigma_product;
            /* overwrite this frame's kind so the consumer knows
             * it was the trigger */
            f->kind = COS_V166_EVENT_INTERRUPTED;
            f->interrupted = true;
            snprintf(f->reason, COS_V166_MAX_MSG,
                     "sigma_burst σ=%.3f > τ=%.3f",
                     (double)f->sigma_product, (double)tau_interrupt);
            return;
        }
    }
    /* Completed without interrupt */
    s->sigma_final = s->n_tokens > 0
        ? s->frames[s->n_tokens - 1].sigma_product
        : 0.0f;
}

/* ------------------------------------------------------------- */
/* JSON serializers                                              */
/* ------------------------------------------------------------- */

static const char *kind_name(cos_v166_event_kind_t k) {
    switch (k) {
        case COS_V166_EVENT_START:       return "start";
        case COS_V166_EVENT_TOKEN:       return "token";
        case COS_V166_EVENT_INTERRUPTED: return "interrupted";
        case COS_V166_EVENT_COMPLETE:    return "complete";
        default:                         return "?";
    }
}

size_t cos_v166_frame_to_json(const cos_v166_frame_t *f,
                              char *buf, size_t cap) {
    if (!f || !buf || cap < 2) return 0;
    /* channels */
    char ch[256];
    size_t u = 0;
    for (int i = 0; i < COS_V166_N_CHANNELS; ++i) {
        int n = snprintf(ch + u, sizeof(ch) - u,
                         "%s%.4f", i == 0 ? "" : ",", (double)f->channels[i]);
        if (n < 0 || (size_t)n >= sizeof(ch) - u) return 0;
        u += (size_t)n;
    }
    int n;
    if (f->kind == COS_V166_EVENT_INTERRUPTED) {
        n = snprintf(buf, cap,
            "{\"kind\":\"interrupted\",\"seq\":%d,\"token\":\"%s\","
            "\"sigma_product\":%.4f,\"channels\":[%s],"
            "\"reason\":\"%s\"}",
            f->seq, f->token, (double)f->sigma_product, ch, f->reason);
    } else {
        n = snprintf(buf, cap,
            "{\"kind\":\"%s\",\"seq\":%d,\"token\":\"%s\","
            "\"sigma_product\":%.4f,\"channels\":[%s],"
            "\"audible_delay_ms\":%d}",
            kind_name(f->kind), f->seq, f->token,
            (double)f->sigma_product, ch, f->audible_delay_ms);
    }
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

size_t cos_v166_stream_to_ndjson(const cos_v166_stream_t *s,
                                 char *buf, size_t cap) {
    if (!s || !buf || cap < 2) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kind\":\"start\",\"kernel\":\"v166\","
        "\"seed\":\"0x%016llx\",\"tau_interrupt\":%.4f,"
        "\"n_tokens\":%d}\n",
        (unsigned long long)s->seed, (double)s->tau_interrupt,
        s->n_tokens);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;

    for (int i = 0; i < s->n_emitted; ++i) {
        size_t k = cos_v166_frame_to_json(&s->frames[i],
                                          buf + used, cap - used);
        if (k == 0) return 0;
        used += k;
        if (used + 1 >= cap) return 0;
        buf[used++] = '\n';
    }

    const char *final_kind = s->was_interrupted ? "interrupted" : "complete";
    n = snprintf(buf + used, cap - used,
        "{\"kind\":\"%s\",\"n_emitted\":%d,"
        "\"interrupt_seq\":%d,\"sigma_final\":%.4f}\n",
        final_kind, s->n_emitted, s->interrupt_seq,
        (double)s->sigma_final);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    buf[used] = '\0';
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v166_self_test(void) {
    cos_v166_stream_t s;

    /* (1) tokenization: 5 tokens */
    int n = cos_v166_tokenize("hello cruel world of streaming", &s);
    if (n != 5) return 1;

    /* (2) baseline run: high tau → no interrupt */
    cos_v166_run(&s, "hello cruel world of streaming",
                 0x16600001ULL, 0.95f, -1);
    if (s.was_interrupted) return 2;
    if (s.n_emitted != 5)  return 3;

    /* (3) determinism */
    cos_v166_stream_t s2;
    cos_v166_run(&s2, "hello cruel world of streaming",
                 0x16600001ULL, 0.95f, -1);
    for (int i = 0; i < 5; ++i) {
        if (s.frames[i].sigma_product != s2.frames[i].sigma_product)
            return 4;
    }

    /* (4) injected burst → interrupt at seq 2 */
    cos_v166_run(&s, "hello cruel world of streaming",
                 0x16600001ULL, 0.50f, 2);
    if (!s.was_interrupted)   return 5;
    if (s.interrupt_seq != 2) return 6;
    if (s.n_emitted != 3)     return 7;  /* frames 0,1,2 emitted */
    if (s.sigma_final < 0.80f) return 8;

    /* (5) audible_delay_ms scales with σ_product */
    cos_v166_run(&s, "alpha beta gamma", 0x16600002ULL, 0.95f, -1);
    for (int i = 0; i < s.n_emitted; ++i) {
        int expected = 40 + (int)(400.0f * s.frames[i].sigma_product);
        if (s.frames[i].audible_delay_ms != expected) return 9;
    }

    return 0;
}
