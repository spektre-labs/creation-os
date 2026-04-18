/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v141 σ-Curriculum — implementation.
 */
#include "curriculum.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_topic(cos_v141_topic_t *t, const char *name,
                      float sigma, int n_obs) {
    memset(t, 0, sizeof *t);
    strncpy(t->name, name, COS_V141_NAME_CAP - 1);
    if (sigma <= 0.0f) sigma = 1e-6f;
    if (sigma >  1.0f) sigma = 1.0f;
    t->sigma = sigma;
    t->n_measured = n_obs;
}

void cos_v141_init_default(cos_v141_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof *s);
    /* Roster matches the user spec's example distribution:
     *   math: σ=0.65   (weak)
     *   code: σ=0.32   (strong)
     *   history: σ=0.71 (weakest)
     *   language: σ=0.45
     *   logic: σ=0.28  (strong)                                   */
    s->n_topics = 5;
    set_topic(&s->topics[0], "math",     0.65f, 120);
    set_topic(&s->topics[1], "code",     0.32f, 430);
    set_topic(&s->topics[2], "history",  0.71f,  88);
    set_topic(&s->topics[3], "language", 0.45f, 200);
    set_topic(&s->topics[4], "logic",    0.28f, 310);
}

int cos_v141_load(cos_v141_state_t *s,
                  const char *const *names,
                  const float *sigmas, int n) {
    if (!s || !names || !sigmas || n <= 0 || n > COS_V141_MAX_TOPICS)
        return -1;
    memset(s, 0, sizeof *s);
    s->n_topics = n;
    for (int i = 0; i < n; ++i)
        set_topic(&s->topics[i], names[i] ? names[i] : "t", sigmas[i], 0);
    return 0;
}

int cos_v141_weakest(const cos_v141_state_t *s) {
    if (!s || s->n_topics <= 0) return -1;
    int  best = 0;
    float mx = s->topics[0].sigma;
    for (int i = 1; i < s->n_topics; ++i) {
        if (s->topics[i].sigma > mx) { mx = s->topics[i].sigma; best = i; }
    }
    return best;
}

int cos_v141_cycle(cos_v141_state_t *s, float alpha,
                   cos_v141_cycle_report_t *out) {
    if (!s || s->n_topics <= 0 || !out) return -1;
    if (alpha <= 0.0f || alpha > 1.0f)   return -1;
    memset(out, 0, sizeof *out);

    /* 1) snapshot. */
    for (int i = 0; i < s->n_topics; ++i)
        s->sigma_before[i] = s->topics[i].sigma;

    /* 2) identify weakness. */
    int w = cos_v141_weakest(s);
    if (w < 0) return -2;
    out->weakest_index = w;
    strncpy(out->weakest_name, s->topics[w].name, COS_V141_NAME_CAP - 1);
    out->sigma_before  = s->sigma_before[w];

    /* 3) "micro-tune" the weak topic — deterministic σ decay.  This
     * stands in for one LoRA adapter step trained on synthetic pairs
     * produced by v141.1 via v114 swarm.  The σ-decay model is the
     * standard assumption: per-cycle improvement is proportional to
     * current σ (the larger the error, the more room to shrink it). */
    float new_sigma = s->topics[w].sigma * (1.0f - alpha);
    if (new_sigma < 1e-6f) new_sigma = 1e-6f;
    s->topics[w].sigma = new_sigma;
    s->topics[w].n_measured += 1;
    out->sigma_after = s->topics[w].sigma;
    out->improvement = out->sigma_before - out->sigma_after;

    /* 4) check no forgetting on strong topics.  In v141.0 we simply
     * leave non-targeted topics untouched; the assertion is a
     * contract that any future "real" trainer must also honour. */
    out->strong_topics_stable = 1;
    out->max_forgetting       = 0.0f;
    for (int i = 0; i < s->n_topics; ++i) {
        if (i == w) continue;
        float delta = s->topics[i].sigma - s->sigma_before[i];
        if (delta > out->max_forgetting) out->max_forgetting = delta;
        /* ε tolerance for float drift if a non-identity update ever
         * lands here. */
        if (delta > 1e-6f) out->strong_topics_stable = 0;
    }
    return 0;
}

int cos_v141_cycle_to_json(const cos_v141_cycle_report_t *r,
                           const cos_v141_state_t *s,
                           char *buf, size_t cap) {
    if (!r || !s || !buf || cap == 0) return -1;
    size_t off = 0;
    int rc = snprintf(buf + off, cap - off,
        "{\"weakest\":\"%s\",\"weakest_index\":%d,"
        "\"sigma_before\":%.6f,\"sigma_after\":%.6f,"
        "\"improvement\":%.6f,\"strong_topics_stable\":%s,"
        "\"max_forgetting\":%.6f,\"topics\":[",
        r->weakest_name, r->weakest_index,
        (double)r->sigma_before, (double)r->sigma_after,
        (double)r->improvement,
        r->strong_topics_stable ? "true" : "false",
        (double)r->max_forgetting);
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    for (int i = 0; i < s->n_topics; ++i) {
        rc = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma\":%.6f,\"n\":%d}",
            i ? "," : "",
            s->topics[i].name, (double)s->topics[i].sigma,
            s->topics[i].n_measured);
        if (rc < 0 || (size_t)rc >= cap - off) return -1;
        off += (size_t)rc;
    }
    rc = snprintf(buf + off, cap - off, "]}");
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    return (int)off;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v141 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v141_self_test(void) {
    cos_v141_state_t st;
    cos_v141_init_default(&st);

    /* --- A. Weakness detection --------------------------------- *
     * The default roster's weakest topic is "history" (σ=0.71).   */
    int w = cos_v141_weakest(&st);
    fprintf(stderr, "check-v141: weakest=%s σ=%.4f\n",
            st.topics[w].name, (double)st.topics[w].sigma);
    _CHECK(w == 2, "weakest index = history");
    _CHECK(strcmp(st.topics[w].name, "history") == 0,
           "weakest name = history");

    /* --- B. Single cycle ---------------------------------------
     * α = 0.40 → σ_history goes 0.71 → 0.426 ≈ 40% reduction.
     * Strong topics must stay *exactly* constant.                */
    cos_v141_cycle_report_t rep;
    _CHECK(cos_v141_cycle(&st, 0.40f, &rep) == 0, "cycle ok");
    fprintf(stderr, "  weakest=%s σ %.4f → %.4f improvement=%.4f\n",
            rep.weakest_name,
            (double)rep.sigma_before, (double)rep.sigma_after,
            (double)rep.improvement);
    _CHECK(rep.weakest_index == 2, "weakest still history");
    _CHECK(rep.improvement > 0.25f,
           "σ improves by ≥0.25 with α=0.40 on σ=0.71");
    _CHECK(rep.strong_topics_stable == 1,
           "no forgetting on strong topics");
    _CHECK(fabsf(rep.max_forgetting) < 1e-6f,
           "max_forgetting == 0 for untouched topics");

    /* --- C. Multi-cycle: curriculum progresses -----------------
     * After 3 more cycles the weakest topic should rotate away
     * from history (whose σ keeps shrinking) once history drops
     * below the next-weakest (language, σ=0.45).                */
    fprintf(stderr, "check-v141: 3 more cycles\n");
    for (int k = 0; k < 3; ++k) {
        cos_v141_cycle_report_t r;
        _CHECK(cos_v141_cycle(&st, 0.40f, &r) == 0, "cycle");
        fprintf(stderr,
            "  cycle %d: weakest=%s σ_before=%.4f σ_after=%.4f\n",
            k, r.weakest_name,
            (double)r.sigma_before, (double)r.sigma_after);
    }
    /* After several cycles on history, it should no longer be the
     * weakest.  Either math (0.65, untouched) or language (0.45,
     * untouched) should now be the weakest. */
    int final_w = cos_v141_weakest(&st);
    fprintf(stderr, "  final weakest=%s σ=%.4f\n",
            st.topics[final_w].name,
            (double)st.topics[final_w].sigma);
    _CHECK(final_w != 2 || st.topics[2].sigma < 0.30f,
           "weakness rotates away from history once below 0.30");
    _CHECK(fabsf(st.topics[4].sigma - 0.28f) < 1e-6f,
           "strong topic 'logic' σ exactly preserved");

    /* --- D. JSON shape ----------------------------------------- */
    char js[1024];
    int jw = cos_v141_cycle_to_json(&rep, &st, js, sizeof js);
    _CHECK(jw > 0, "json emit");
    _CHECK(strstr(js, "\"weakest\":\"history\"") != NULL,
           "json weakest = history");
    _CHECK(strstr(js, "\"strong_topics_stable\":true") != NULL,
           "json reports no forgetting");

    fprintf(stderr,
        "check-v141: OK (weakness → cycle → improvement → no forgetting)\n");
    return 0;
}
