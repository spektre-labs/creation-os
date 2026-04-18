/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v150 σ-Swarm-Intelligence — implementation.  See swarm.h.
 */
#include "swarm.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- deterministic RNG ----------------------------------- */
static uint64_t splitmix(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand(uint64_t *s) {
    return (float)(splitmix(s) >> 40) / 16777216.0f;
}

/* ---------- topic vector ---------------------------------------- */
/* Axes in the 4-D topic space:
 *   [0] = symbolic/math
 *   [1] = code/software
 *   [2] = bio/physical
 *   [3] = narrative/linguistic
 * Each specialist's affinity is a unit vector along one axis. */
static const struct { const char *key; int axis; } k_topic_map[] = {
    { "math",      0 }, { "algebra",   0 }, { "logic",     0 },
    { "code",      1 }, { "software",  1 }, { "debug",     1 },
    { "bio",       2 }, { "physics",   2 }, { "world",     2 },
    { "language",  3 }, { "narrative", 3 }, { "prose",     3 },
};
static const int k_topic_map_n =
    (int)(sizeof(k_topic_map) / sizeof(k_topic_map[0]));

static int topic_axis(const char *t) {
    if (!t) return 0;
    for (int i = 0; i < k_topic_map_n; ++i) {
        if (strcasecmp(t, k_topic_map[i].key) == 0) return k_topic_map[i].axis;
    }
    /* fallback — hash string into [0..3]. */
    uint32_t h = 0x811c9dc5u;
    for (const char *p = t; *p; ++p) {
        h ^= (unsigned char)tolower((unsigned char)*p);
        h *= 16777619u;
    }
    return (int)(h & 3u);
}

void cos_v150_topic_vec(const char *topic,
                        float out[COS_V150_N_TOPIC_DIM]) {
    if (!out) return;
    for (int i = 0; i < COS_V150_N_TOPIC_DIM; ++i) out[i] = 0.0f;
    int ax = topic_axis(topic);
    if (ax < 0 || ax >= COS_V150_N_TOPIC_DIM) ax = 0;
    out[ax] = 1.0f;
}

/* cosine similarity on unit vectors == dot product. */
static float cosine(const float *a, const float *b, int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double)a[i] * (double)b[i];
    if (s < 0.0) s = 0.0;
    if (s > 1.0) s = 1.0;
    return (float)s;
}

/* ---------- swarm setup ----------------------------------------- */
void cos_v150_swarm_init(cos_v150_swarm_t *s, uint64_t seed) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->prng = seed ? seed : 0x15005A1AULL;
}

static void set_specialist(cos_v150_specialist_t *sp,
                           const char *name, const char *topic) {
    memset(sp, 0, sizeof(*sp));
    strncpy(sp->name,  name,  COS_V150_NAME_MAX  - 1);
    strncpy(sp->topic, topic, COS_V150_TOPIC_MAX - 1);
    cos_v150_topic_vec(topic, sp->affinity);
}

void cos_v150_seed_defaults(cos_v150_swarm_t *s) {
    if (!s) return;
    set_specialist(&s->spec[0], "atlas",  "math");
    set_specialist(&s->spec[1], "crow",   "code");
    set_specialist(&s->spec[2], "lynx",   "bio");
}

/* ---------- σ synthesis ----------------------------------------- */
/* Produce a per-specialist σ for this question.
 * σ = (1 − cos(affinity, topic)) * 0.7 + jitter * 0.3.
 * Jitter is seeded from (global, specialist_id, round). */
static float synth_sigma(const cos_v150_specialist_t *sp,
                         const float topic_tag[COS_V150_N_TOPIC_DIM],
                         uint64_t seed, int spec_id, int round) {
    float cos_score = cosine(sp->affinity, topic_tag,
                             COS_V150_N_TOPIC_DIM);
    float mismatch  = 1.0f - cos_score;  /* low for matched topic */
    uint64_t s = seed ^ ((uint64_t)(spec_id + 1) * 0x12345ULL)
                      ^ ((uint64_t)(round   + 1) * 0xABCDULL);
    float jitter = urand(&s);           /* 0..1                   */
    float sigma  = mismatch * 0.70f + jitter * 0.30f;
    if (sigma < 0.02f) sigma = 0.02f;
    if (sigma > 0.98f) sigma = 0.98f;
    return sigma;
}

static void synth_answer(char *out, size_t cap,
                         const cos_v150_specialist_t *sp,
                         const char *question, int round) {
    snprintf(out, cap, "[%s/r%d] %s→answer(%s)",
             sp->name, round, sp->topic, question);
}

/* ---------- debate ---------------------------------------------- */
int cos_v150_debate_run(cos_v150_swarm_t *s,
                        const char *question,
                        const char *topic,
                        cos_v150_debate_t *d) {
    if (!s || !question || !topic || !d) return -1;
    memset(d, 0, sizeof(*d));
    strncpy(d->question, question, COS_V150_QUESTION_MAX - 1);
    strncpy(d->topic,    topic,    COS_V150_TOPIC_MAX    - 1);
    cos_v150_topic_vec(topic, d->topic_tag);

    /* Seed the debate PRNG from the global swarm PRNG so repeated
     * debates on the same swarm still differ — but each individual
     * debate is deterministic given its own seed.                */
    d->seed = splitmix(&s->prng);

    /* ---- Round 0: independent answers + σ ----                 */
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        cos_v150_answer_t *a = &d->rounds[0][i];
        a->specialist_id = i;
        a->round         = 0;
        a->adopted_from  = -1;
        a->sigma         = synth_sigma(&s->spec[i], d->topic_tag,
                                       d->seed, i, 0);
        synth_answer(a->text, sizeof(a->text),
                     &s->spec[i], question, 0);
    }

    /* ---- Round 1: look at neighbours, adopt better answer ---- */
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        int best = i;
        for (int j = 0; j < COS_V150_N_SPECIALISTS; ++j) {
            if (d->rounds[0][j].sigma < d->rounds[0][best].sigma)
                best = j;
        }
        cos_v150_answer_t *a = &d->rounds[1][i];
        a->specialist_id = i;
        a->round         = 1;
        if (best != i) {
            a->adopted_from = best;
            strncpy(a->text, d->rounds[0][best].text,
                    COS_V150_ANSWER_MAX - 1);
            /* Inherit σ with a small epistemic penalty — the
             * originator still holds argmin σ; the adopter's
             * confidence is strictly worse than the source's. */
            float inherited = d->rounds[0][best].sigma + 0.01f;
            if (inherited > 0.98f) inherited = 0.98f;
            a->sigma = inherited;
        } else {
            a->adopted_from = -1;
            strncpy(a->text, d->rounds[0][i].text,
                    COS_V150_ANSWER_MAX - 1);
            a->sigma = d->rounds[0][i].sigma;
        }
    }

    /* ---- Round 2: vote.  Each specialist emits the final
     *               argmin-σ answer from round 1, with own σ. -- */
    int argmin = 0;
    for (int i = 1; i < COS_V150_N_SPECIALISTS; ++i) {
        if (d->rounds[1][i].sigma < d->rounds[1][argmin].sigma)
            argmin = i;
    }
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        cos_v150_answer_t *a = &d->rounds[2][i];
        a->specialist_id = i;
        a->round         = 2;
        a->adopted_from  = argmin;
        strncpy(a->text, d->rounds[1][argmin].text,
                COS_V150_ANSWER_MAX - 1);
        /* Final σ per specialist: own round-1 σ, shrunken by the
         * fraction of peers agreeing.  With 3/3 agreement we
         * apply a √N shrinkage factor (per v104 aggregation).    */
        float agree_factor = 1.0f / sqrtf((float)COS_V150_N_SPECIALISTS);
        a->sigma = d->rounds[1][i].sigma * agree_factor;
        if (a->sigma < 0.01f) a->sigma = 0.01f;
    }

    /* Populate sigma_final (before adversarial verify). */
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        d->sigma_final[i] = d->rounds[2][i].sigma;
    }

    d->best_specialist  = argmin;
    d->sigma_consensus  = cos_v150_sigma_consensus(d);
    d->sigma_collective = cos_v150_sigma_collective(d);
    d->converged        = (d->sigma_consensus < 0.50f) ? 1 : 0;

    /* Emergent specialisation tally: the argmin specialist for
     * this topic axis gets a +1 win-count.                       */
    int ax = topic_axis(topic);
    if (ax >= 0 && ax < COS_V150_N_TOPIC_DIM)
        s->win_count[argmin][ax]++;
    s->debates++;
    return 0;
}

/* ---------- adversarial verify ---------------------------------- */
int cos_v150_adversarial_verify(cos_v150_debate_t *d,
                                float tau_rebuttal) {
    if (!d) return -1;
    if (tau_rebuttal <= 0.0f) tau_rebuttal = 0.30f;
    d->n_critiques = 0;
    int defender = d->best_specialist;
    if (defender < 0 || defender >= COS_V150_N_SPECIALISTS) return 0;
    for (int adv = 0; adv < COS_V150_N_SPECIALISTS; ++adv) {
        if (adv == defender) continue;
        cos_v150_critique_t *c = &d->critiques[d->n_critiques++];
        c->adversary_id = adv;
        c->defender_id  = defender;
        /* σ_critique derived from adversary round-1 σ — a
         * confident adversary issues a sharper critique.         */
        uint64_t s = d->seed
                     ^ ((uint64_t)(adv + 1) * 0xA5A5U)
                     ^ ((uint64_t)(defender + 1) * 0x5A5AU);
        float base_crit = d->rounds[1][adv].sigma;
        float jitter    = urand(&s) * 0.10f;
        c->sigma_critique = base_crit + jitter;
        if (c->sigma_critique > 0.98f) c->sigma_critique = 0.98f;
        c->rebuttal_found = (c->sigma_critique <= tau_rebuttal) ? 1 : 0;

        /* Update defender σ: rebuttal found → σ rises by
         * (τ − σ_critique); vindicated → σ drops by (σ_critique − τ). */
        float delta = c->rebuttal_found
                    ? (tau_rebuttal - c->sigma_critique) * 0.5f
                    : (tau_rebuttal - c->sigma_critique) * 0.2f;
        float after = d->sigma_final[defender] - delta;
        if (after < 0.01f) after = 0.01f;
        if (after > 0.98f) after = 0.98f;
        d->sigma_final[defender] = after;
        c->defender_sigma_after  = after;
    }
    /* Refresh aggregate metrics after verify. */
    d->sigma_consensus  = cos_v150_sigma_consensus(d);
    d->sigma_collective = cos_v150_sigma_collective(d);
    return d->n_critiques;
}

/* ---------- σ aggregates ---------------------------------------- */
float cos_v150_sigma_collective(const cos_v150_debate_t *d) {
    if (!d) return 1.0f;
    double acc = 0.0;
    int n = 0;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        float s = d->sigma_final[i];
        if (s < 1e-6f) s = 1e-6f;
        acc += log((double)s);
        n++;
    }
    if (n == 0) return 1.0f;
    return (float)exp(acc / (double)n);
}

float cos_v150_sigma_consensus(const cos_v150_debate_t *d) {
    if (!d) return 1.0f;
    double mean = 0.0;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        mean += (double)d->sigma_final[i];
    }
    mean /= (double)COS_V150_N_SPECIALISTS;
    if (mean < 1e-6) mean = 1e-6;
    double var = 0.0;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        double delta = (double)d->sigma_final[i] - mean;
        var += delta * delta;
    }
    var /= (double)COS_V150_N_SPECIALISTS;
    double sd = sqrt(var);
    return (float)(sd / mean);
}

/* ---------- routing -------------------------------------------- */
int cos_v150_route_by_topic(const cos_v150_swarm_t *s,
                            const char *topic) {
    if (!s || !topic) return 0;
    float t[COS_V150_N_TOPIC_DIM];
    cos_v150_topic_vec(topic, t);
    int best = 0;
    float best_cos = -1.0f;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        float c = cosine(s->spec[i].affinity, t,
                         COS_V150_N_TOPIC_DIM);
        if (c > best_cos) { best_cos = c; best = i; }
    }
    return best;
}

/* ---------- JSON ------------------------------------------------- */
static int append(char **p, char *end, const char *fmt, ...) {
    if (*p >= end) return -1;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(*p, (size_t)(end - *p), fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if (n >= (int)(end - *p)) { *p = end; return -1; }
    *p += n;
    return 0;
}

static int emit_answer(char **p, char *end,
                       const cos_v150_answer_t *a) {
    return append(p, end,
        "{\"sid\":%d,\"r\":%d,\"adopted\":%d,"
        "\"sigma\":%.4f,\"text\":\"%s\"}",
        a->specialist_id, a->round, a->adopted_from,
        a->sigma, a->text);
}

int cos_v150_debate_to_json(const cos_v150_debate_t *d,
                            char *buf, size_t cap) {
    if (!d || !buf || cap < 128) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end,
        "{\"question\":\"%s\",\"topic\":\"%s\",\"seed\":%llu,"
        "\"sigma_consensus\":%.4f,\"sigma_collective\":%.4f,"
        "\"converged\":%s,\"best\":%d,\"rounds\":[",
        d->question, d->topic, (unsigned long long)d->seed,
        d->sigma_consensus, d->sigma_collective,
        d->converged ? "true" : "false",
        d->best_specialist) < 0) return -1;
    for (int r = 0; r < COS_V150_N_ROUNDS; ++r) {
        if (append(&p, end, "%s[", r ? "," : "") < 0) return -1;
        for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
            if (append(&p, end, "%s", i ? "," : "") < 0) return -1;
            if (emit_answer(&p, end, &d->rounds[r][i]) < 0) return -1;
        }
        if (append(&p, end, "]") < 0) return -1;
    }
    if (append(&p, end, "],\"final_sigma\":[") < 0) return -1;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        if (append(&p, end, "%s%.4f",
                   i ? "," : "", d->sigma_final[i]) < 0) return -1;
    }
    if (append(&p, end, "],\"critiques\":[") < 0) return -1;
    for (int i = 0; i < d->n_critiques; ++i) {
        const cos_v150_critique_t *c = &d->critiques[i];
        if (append(&p, end,
            "%s{\"adv\":%d,\"def\":%d,\"sigma_crit\":%.4f,"
            "\"rebuttal\":%s,\"def_sigma_after\":%.4f}",
            i ? "," : "", c->adversary_id, c->defender_id,
            c->sigma_critique,
            c->rebuttal_found ? "true" : "false",
            c->defender_sigma_after) < 0) return -1;
    }
    if (append(&p, end, "]}") < 0) return -1;
    return (int)(p - buf);
}

int cos_v150_swarm_to_json(const cos_v150_swarm_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf || cap < 64) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end,
        "{\"debates\":%d,\"specialists\":[", s->debates) < 0) return -1;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        if (append(&p, end,
            "%s{\"id\":%d,\"name\":\"%s\",\"topic\":\"%s\",\"wins\":[",
            i ? "," : "", i, s->spec[i].name,
            s->spec[i].topic) < 0) return -1;
        for (int k = 0; k < COS_V150_N_TOPIC_DIM; ++k) {
            if (append(&p, end, "%s%d",
                       k ? "," : "", s->win_count[i][k]) < 0) return -1;
        }
        if (append(&p, end, "]}") < 0) return -1;
    }
    if (append(&p, end, "]}") < 0) return -1;
    return (int)(p - buf);
}

/* ---------- Self-test ------------------------------------------- */
int cos_v150_self_test(void) {
    cos_v150_swarm_t s;
    cos_v150_swarm_init(&s, 0x1500);
    cos_v150_seed_defaults(&s);

    /* S1: math debate routes best to atlas (specialist 0).       */
    cos_v150_debate_t d;
    if (cos_v150_debate_run(&s, "solve 2x+3=7", "math", &d) != 0) return 1;
    if (d.best_specialist != 0) return 2;
    if (!d.converged)          return 3;

    /* S2: the debate's σ_collective is not worse than the worst
     *     round-0 σ — i.e. running the debate never makes the
     *     ensemble strictly less confident than the noisiest
     *     independent answer.                                   */
    float worst_r0 = 0.0f;
    for (int i = 0; i < COS_V150_N_SPECIALISTS; ++i) {
        if (d.rounds[0][i].sigma > worst_r0) worst_r0 = d.rounds[0][i].sigma;
    }
    if (d.sigma_collective > worst_r0 + 1e-3f) return 4;

    /* S3: adversarial verify emits N-1 critiques and at least
     *     one of them shifts the defender's σ.                   */
    float pre = d.sigma_final[d.best_specialist];
    int n_crit = cos_v150_adversarial_verify(&d, 0.30f);
    if (n_crit != COS_V150_N_SPECIALISTS - 1) return 5;
    float post = d.sigma_final[d.best_specialist];
    if (fabsf(post - pre) < 1e-5f) return 6;

    /* S4: code question routes to crow (specialist 1).           */
    int route = cos_v150_route_by_topic(&s, "code");
    if (route != 1) return 7;

    /* S5: emergent specialisation tally increments.              */
    int before = s.debates;
    cos_v150_debate_t d2;
    if (cos_v150_debate_run(&s, "implement quicksort", "code", &d2) != 0)
        return 8;
    if (s.debates != before + 1) return 9;
    if (d2.best_specialist != 1) return 10;

    /* S6: JSON serialisation succeeds. */
    char buf[4096];
    if (cos_v150_debate_to_json(&d, buf, sizeof(buf)) <= 0) return 11;
    if (cos_v150_swarm_to_json (&s, buf, sizeof(buf)) <= 0) return 12;

    return 0;
}
