/*
 * v197 σ-Theory-of-Mind — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "tom.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void cos_v197_init(cos_v197_state_full_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x197A07E9ULL;
    s->tau_adapt = 0.40f;
}

/* ---- canonical (state → response-mode) mapping ---- */

static int mode_for(int state) {
    switch (state) {
    case COS_V197_STATE_FOCUSED:    return COS_V197_MODE_DIRECT;
    case COS_V197_STATE_EXPLORING:  return COS_V197_MODE_BROAD;
    case COS_V197_STATE_FRUSTRATED: return COS_V197_MODE_CONCRETE_FIX;
    case COS_V197_STATE_HURRIED:    return COS_V197_MODE_TERSE;
    case COS_V197_STATE_CREATIVE:   return COS_V197_MODE_ASSOCIATIVE;
    case COS_V197_STATE_ANALYTICAL: return COS_V197_MODE_RIGOROUS;
    }
    return COS_V197_MODE_NEUTRAL;
}

/* 3 samples per state (18 total) + embedded firmware-manipulation
 * probe samples.  Values chosen to map cleanly to state classifier. */
void cos_v197_build(cos_v197_state_full_t *s) {
    static const struct {
        int state;
        int len;
        int edits;
        float typing;
        float var;
        int history[COS_V197_HISTORY];
        int manipulation;
    } CAT[COS_V197_N_SAMPLES] = {
        /* FOCUSED: short, few edits, steady topic. */
        { 0,  80, 0, 3.2f, 0.08f, {1,1,1,1,1,1}, 0 },
        { 0, 110, 1, 3.0f, 0.10f, {2,2,2,2,2,2}, 0 },
        { 0,  90, 0, 3.3f, 0.05f, {3,3,3,3,3,3}, 0 },

        /* EXPLORING: long, topic-hopping. */
        { 1, 420, 2, 2.1f, 0.72f, {1,2,4,5,2,6}, 0 },
        { 1, 500, 3, 1.9f, 0.80f, {3,7,2,4,1,5}, 0 },
        { 1, 380, 1, 2.2f, 0.65f, {8,3,6,2,7,4}, 0 },

        /* FRUSTRATED: many edits, mid-length, repeated corrections. */
        { 2, 220, 8, 1.4f, 0.20f, {9,9,9,9,9,9}, 1 },  /* + probe */
        { 2, 180, 9, 1.2f, 0.18f, {5,5,5,5,5,5}, 0 },
        { 2, 240,10, 1.5f, 0.22f, {3,3,3,3,3,3}, 0 },

        /* HURRIED: very short, fast, single topic. */
        { 3,  35, 0, 4.8f, 0.05f, {7,7,7,7,7,7}, 0 },
        { 3,  28, 0, 5.0f, 0.04f, {7,7,7,7,7,7}, 0 },
        { 3,  42, 1, 4.6f, 0.07f, {7,7,7,7,7,7}, 0 },

        /* CREATIVE: long, high variance, moderate typing. */
        { 4, 600, 4, 2.6f, 0.85f, {1,9,3,8,2,7}, 0 },
        { 4, 550, 3, 2.7f, 0.90f, {4,1,9,5,2,8}, 0 },
        { 4, 620, 5, 2.5f, 0.88f, {6,2,8,3,9,1}, 1 },  /* + probe */

        /* ANALYTICAL: long, low variance, slow careful typing. */
        { 5, 480, 2, 2.0f, 0.12f, {4,4,4,4,4,4}, 0 },
        { 5, 520, 1, 1.8f, 0.10f, {6,6,6,6,6,6}, 0 },
        { 5, 510, 2, 1.9f, 0.14f, {8,8,8,8,8,8}, 0 }
    };

    for (int i = 0; i < COS_V197_N_SAMPLES; ++i) {
        cos_v197_sample_t *p = &s->samples[i];
        memset(p, 0, sizeof(*p));
        p->id              = i;
        p->msg_len         = CAT[i].len;
        p->edits           = CAT[i].edits;
        p->typing_speed    = CAT[i].typing;
        p->topic_variance  = CAT[i].var;
        for (int h = 0; h < COS_V197_HISTORY; ++h)
            p->history[h] = CAT[i].history[h];
        p->firmware_manipulation = CAT[i].manipulation != 0;
        p->user_state      = CAT[i].state;
    }
    s->n_samples = COS_V197_N_SAMPLES;
}

/* Mode-of-history intent prediction. */
static int intent_from_history(const int *hist, int n) {
    /* Topic ids expected in [1..10]; small-N mode. */
    int cnt[32] = {0};
    int best = hist[0], bestc = 0;
    for (int i = 0; i < n; ++i) {
        int t = hist[i];
        if (t < 0 || t >= 32) continue;
        cnt[t]++;
        if (cnt[t] > bestc) { bestc = cnt[t]; best = t; }
    }
    return best;
}

static float sigma_from_obs(int state, const cos_v197_sample_t *p) {
    /* σ_tom is small when observables align strongly with the
     * canonical pattern for `state`, larger when ambiguous. */
    switch (state) {
    case COS_V197_STATE_FOCUSED:
        return (p->edits <= 1 && p->topic_variance < 0.2f) ? 0.15f : 0.45f;
    case COS_V197_STATE_EXPLORING:
        return (p->msg_len > 300 && p->topic_variance > 0.5f) ? 0.20f : 0.50f;
    case COS_V197_STATE_FRUSTRATED:
        return (p->edits >= 6) ? 0.18f : 0.55f;
    case COS_V197_STATE_HURRIED:
        return (p->msg_len < 60 && p->typing_speed > 4.0f) ? 0.12f : 0.60f;
    case COS_V197_STATE_CREATIVE:
        return (p->topic_variance > 0.8f && p->msg_len > 400) ? 0.22f : 0.55f;
    case COS_V197_STATE_ANALYTICAL:
        return (p->topic_variance < 0.2f && p->msg_len > 400) ? 0.25f : 0.50f;
    }
    return 0.70f;
}

void cos_v197_run(cos_v197_state_full_t *s) {
    memset(s->state_counts, 0, sizeof(s->state_counts));
    s->n_adapted = 0;
    s->n_manipulation_rejected = 0;

    uint64_t prev = 0x1977074ULL;
    for (int i = 0; i < s->n_samples; ++i) {
        cos_v197_sample_t *p = &s->samples[i];
        p->sigma_tom    = sigma_from_obs(p->user_state, p);
        p->intent_topic = intent_from_history(p->history, COS_V197_HISTORY);
        s->state_counts[p->user_state]++;

        /* Anti-manipulation: any firmware-manipulation probe is
         * unconditionally rejected, adaptation cleared. */
        if (p->firmware_manipulation) {
            p->rejected_by_constitution = true;
            p->response_mode            = COS_V197_MODE_NEUTRAL;
            p->adaptation               = false;
            s->n_manipulation_rejected++;
        } else if (p->sigma_tom < s->tau_adapt) {
            p->response_mode = mode_for(p->user_state);
            p->adaptation    = true;
            s->n_adapted++;
        } else {
            p->response_mode = COS_V197_MODE_NEUTRAL;
            p->adaptation    = false;
        }

        p->hash_prev = prev;
        struct { int id, state, mode, intent;
                 int rej, adapt; float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = p->id;
        rec.state  = p->user_state;
        rec.mode   = p->response_mode;
        rec.intent = p->intent_topic;
        rec.rej    = p->rejected_by_constitution ? 1 : 0;
        rec.adapt  = p->adaptation ? 1 : 0;
        rec.sig    = p->sigma_tom;
        rec.prev   = prev;
        p->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev         = p->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x1977074ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v197_sample_t *p = &s->samples[i];
        if (p->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, state, mode, intent;
                 int rej, adapt; float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = p->id;
        rec.state  = p->user_state;
        rec.mode   = p->response_mode;
        rec.intent = p->intent_topic;
        rec.rej    = p->rejected_by_constitution ? 1 : 0;
        rec.adapt  = p->adaptation ? 1 : 0;
        rec.sig    = p->sigma_tom;
        rec.prev   = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != p->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v197_to_json(const cos_v197_state_full_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v197\","
        "\"n_samples\":%d,"
        "\"tau_adapt\":%.3f,"
        "\"state_counts\":[%d,%d,%d,%d,%d,%d],"
        "\"n_adapted\":%d,"
        "\"n_manipulation_rejected\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"samples\":[",
        s->n_samples, s->tau_adapt,
        s->state_counts[0], s->state_counts[1], s->state_counts[2],
        s->state_counts[3], s->state_counts[4], s->state_counts[5],
        s->n_adapted, s->n_manipulation_rejected,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v197_sample_t *p = &s->samples[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"state\":%d,\"sigma_tom\":%.3f,"
            "\"intent\":%d,\"mode\":%d,\"adapt\":%s,"
            "\"rejected\":%s,\"firmware\":%s}",
            i == 0 ? "" : ",", p->id, p->user_state, p->sigma_tom,
            p->intent_topic, p->response_mode,
            p->adaptation ? "true" : "false",
            p->rejected_by_constitution ? "true" : "false",
            p->firmware_manipulation ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v197_self_test(void) {
    cos_v197_state_full_t s;
    cos_v197_init(&s, 0x197A07E9ULL);
    cos_v197_build(&s);
    cos_v197_run(&s);

    if (s.n_samples != COS_V197_N_SAMPLES) return 1;
    for (int st = 0; st < COS_V197_N_STATES; ++st)
        if (s.state_counts[st] < 1)         return 2;
    if (s.n_manipulation_rejected < 1)      return 3;
    if (!s.chain_valid)                     return 4;

    /* Per-sample: high-confidence → adapt+mode; low-confidence →
     * neutral; manipulation → rejected+neutral. */
    for (int i = 0; i < s.n_samples; ++i) {
        const cos_v197_sample_t *p = &s.samples[i];
        if (p->firmware_manipulation) {
            if (!p->rejected_by_constitution)           return 5;
            if (p->response_mode != COS_V197_MODE_NEUTRAL) return 6;
            if (p->adaptation)                           return 7;
        } else if (p->sigma_tom < s.tau_adapt) {
            if (!p->adaptation)                          return 8;
            if (p->response_mode != mode_for(p->user_state)) return 9;
        } else {
            if (p->adaptation)                           return 10;
            if (p->response_mode != COS_V197_MODE_NEUTRAL) return 11;
        }
    }
    return 0;
}
