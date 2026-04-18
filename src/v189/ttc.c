/*
 * v189 σ-TTC — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ttc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- init ------------------------------------------------------ */

void cos_v189_init(cos_v189_state_t *s, uint64_t seed, int mode) {
    memset(s, 0, sizeof(*s));
    s->seed           = seed ? seed : 0x189C7CAFULL;
    s->mode           = mode;
    s->tau_easy       = 0.20f;
    s->tau_hard       = 0.50f;
    s->tau_stop       = 0.15f;
    s->max_depth      = 8;
    s->max_paths_deep = 8;
    s->budget_total   = 128;
    s->n_queries      = COS_V189_N_QUERIES;
}

/* ---- fixture --------------------------------------------------- */

static int class_from_sigma(const cos_v189_state_t *s, float sigma) {
    if (sigma <  s->tau_easy) return COS_V189_CLASS_EASY;
    if (sigma >= s->tau_hard) return COS_V189_CLASS_HARD;
    return COS_V189_CLASS_MEDIUM;
}

void cos_v189_build(cos_v189_state_t *s) {
    uint64_t r = s->seed;
    for (int i = 0; i < s->n_queries; ++i) {
        cos_v189_query_t *q = &s->queries[i];
        q->id = i;

        /* Deterministic class rotation: 16 easy / 16 medium / 16 hard. */
        int bucket = i / 16;
        float base;
        switch (bucket) {
        case 0: base = 0.05f + 0.10f * frand(&r); break;   /* easy  < 0.20 */
        case 1: base = 0.25f + 0.20f * frand(&r); break;   /* medium */
        default: base = 0.55f + 0.35f * frand(&r); break;  /* hard ≥ 0.50 */
        }
        q->sigma_first_token = clampf(base, 0.02f, 0.95f);
        q->class_             = class_from_sigma(s, q->sigma_first_token);

        /* Token count and per-token σ. Hard queries have more tokens
         * and wider σ distribution. */
        int n_tokens = (q->class_ == COS_V189_CLASS_HARD)   ? 16 :
                       (q->class_ == COS_V189_CLASS_MEDIUM) ?  8 : 4;
        q->n_tokens = n_tokens;
        for (int t = 0; t < n_tokens; ++t) {
            float jitter = (frand(&r) - 0.5f) * 0.20f;
            q->sigma_token[t] = clampf(q->sigma_first_token + jitter,
                                       0.01f, 0.99f);
        }
    }
}

/* ---- σ → compute allocation ----------------------------------- */

static int paths_for(cos_v189_state_t *s, const cos_v189_query_t *q) {
    /* Mode caps. */
    int cap = (s->mode == COS_V189_MODE_FAST)     ? 1 :
              (s->mode == COS_V189_MODE_BALANCED) ? 3 :
                                                     s->max_paths_deep;

    int want;
    switch (q->class_) {
    case COS_V189_CLASS_EASY:   want = 1; break;
    case COS_V189_CLASS_MEDIUM: want = 3; break;
    default:                    want = s->max_paths_deep; break;
    }
    /* In DEEP mode floor paths at max_paths_deep regardless of class. */
    if (s->mode == COS_V189_MODE_DEEP && want < s->max_paths_deep)
        want = s->max_paths_deep;
    if (want > cap) want = cap;
    if (want < 1)   want = 1;
    return want;
}

static int depth_for(cos_v189_state_t *s, float sigma) {
    if (sigma < s->tau_stop) return 1;
    /* Linear in σ above τ_stop, clipped to max_depth. */
    float span = 1.0f - s->tau_stop;
    float u    = (sigma - s->tau_stop) / span;
    int   d    = 1 + (int)(u * (s->max_depth - 1) + 0.5f);
    if (d < 1)              d = 1;
    if (d > s->max_depth)   d = s->max_depth;
    /* FAST mode caps recurrent depth at 1. */
    if (s->mode == COS_V189_MODE_FAST) d = 1;
    /* BALANCED caps at 3. */
    if (s->mode == COS_V189_MODE_BALANCED && d > 3) d = 3;
    return d;
}

void cos_v189_run(cos_v189_state_t *s) {
    s->n_easy = s->n_medium = s->n_hard = 0;
    s->sum_compute_easy = s->sum_compute_medium = s->sum_compute_hard = 0.0;
    for (int i = 0; i < s->n_queries; ++i) {
        cos_v189_query_t *q = &s->queries[i];
        q->n_paths = paths_for(s, q);

        int total_depth = 0;
        for (int t = 0; t < q->n_tokens; ++t) {
            int d = depth_for(s, q->sigma_token[t]);
            /* Easy class → guarantee 1 iter (idle-kernel contract). */
            if (q->class_ == COS_V189_CLASS_EASY) d = 1;
            q->depth_token[t] = d;
            total_depth += d;
        }

        bool deep_heavy = (q->class_ == COS_V189_CLASS_HARD) &&
                          (s->mode != COS_V189_MODE_FAST);
        q->used_debate    = deep_heavy;
        q->used_symbolic  = deep_heavy;
        q->used_reflect   = deep_heavy;

        /* Total compute: paths × per-path token depth + plugin cost. */
        int plugin_cost = (q->used_debate    ? 12 : 0) +
                          (q->used_symbolic  ?  8 : 0) +
                          (q->used_reflect   ?  6 : 0);
        q->compute_units = q->n_paths * total_depth + plugin_cost;

        switch (q->class_) {
        case COS_V189_CLASS_EASY:
            s->n_easy++;   s->sum_compute_easy   += q->compute_units; break;
        case COS_V189_CLASS_MEDIUM:
            s->n_medium++; s->sum_compute_medium += q->compute_units; break;
        default:
            s->n_hard++;   s->sum_compute_hard   += q->compute_units; break;
        }
    }
    s->mean_compute_easy   = s->n_easy   ? s->sum_compute_easy   / s->n_easy   : 0.0;
    s->mean_compute_medium = s->n_medium ? s->sum_compute_medium / s->n_medium : 0.0;
    s->mean_compute_hard   = s->n_hard   ? s->sum_compute_hard   / s->n_hard   : 0.0;
    s->ratio_hard_over_easy =
        (s->mean_compute_easy > 0.0) ?
        (float)(s->mean_compute_hard / s->mean_compute_easy) : 0.0f;
}

/* ---- JSON ------------------------------------------------------ */

static const char *mode_name(int m) {
    switch (m) {
    case COS_V189_MODE_FAST:     return "fast";
    case COS_V189_MODE_BALANCED: return "balanced";
    case COS_V189_MODE_DEEP:     return "deep";
    default:                     return "?";
    }
}

size_t cos_v189_to_json(const cos_v189_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v189\",\"mode\":\"%s\","
        "\"n_queries\":%d,"
        "\"n_easy\":%d,\"n_medium\":%d,\"n_hard\":%d,"
        "\"tau_easy\":%.3f,\"tau_hard\":%.3f,\"tau_stop\":%.3f,"
        "\"max_depth\":%d,\"max_paths_deep\":%d,"
        "\"mean_compute_easy\":%.3f,"
        "\"mean_compute_medium\":%.3f,"
        "\"mean_compute_hard\":%.3f,"
        "\"ratio_hard_over_easy\":%.3f,"
        "\"queries\":[",
        mode_name(s->mode),
        s->n_queries, s->n_easy, s->n_medium, s->n_hard,
        s->tau_easy, s->tau_hard, s->tau_stop,
        s->max_depth, s->max_paths_deep,
        s->mean_compute_easy, s->mean_compute_medium,
        s->mean_compute_hard, s->ratio_hard_over_easy);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_queries; ++i) {
        const cos_v189_query_t *q = &s->queries[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"class\":%d,"
            "\"sigma_first\":%.3f,\"n_paths\":%d,"
            "\"compute\":%d,\"debate\":%s,\"symbolic\":%s,"
            "\"reflect\":%s}",
            i == 0 ? "" : ",", q->id, q->class_,
            q->sigma_first_token, q->n_paths, q->compute_units,
            q->used_debate   ? "true" : "false",
            q->used_symbolic ? "true" : "false",
            q->used_reflect  ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test ------------------------------------------------- */

int cos_v189_self_test(void) {
    cos_v189_state_t bal;
    cos_v189_init(&bal, 0x189C7CAFULL, COS_V189_MODE_BALANCED);
    cos_v189_build(&bal);
    cos_v189_run(&bal);

    /* 1. class partition. */
    if (bal.n_easy + bal.n_medium + bal.n_hard != bal.n_queries) return 1;
    if (bal.n_easy < 1 || bal.n_medium < 1 || bal.n_hard < 1)    return 2;

    /* 2. Monotone compute: hard > medium > easy. */
    if (!(bal.mean_compute_hard   >= 2.0 * bal.mean_compute_medium)) return 3;
    if (!(bal.mean_compute_medium >= 2.0 * bal.mean_compute_easy))   return 4;

    /* 3. Hard / Easy ratio > 4× (balanced mode). */
    if (bal.ratio_hard_over_easy <= 4.0f) return 5;

    /* 4. Easy queries: exactly 1 path, 1 recurrent iter per token. */
    for (int i = 0; i < bal.n_queries; ++i) {
        const cos_v189_query_t *q = &bal.queries[i];
        if (q->class_ == COS_V189_CLASS_EASY) {
            if (q->n_paths != 1) return 6;
            for (int t = 0; t < q->n_tokens; ++t)
                if (q->depth_token[t] != 1) return 7;
        }
        if (q->class_ == COS_V189_CLASS_HARD) {
            if (q->n_paths < 3) return 8;  /* balanced cap=3 */
        }
    }

    /* 5. FAST mode caps paths at 1 and depth at 1. */
    cos_v189_state_t fast;
    cos_v189_init(&fast, 0x189C7CAFULL, COS_V189_MODE_FAST);
    cos_v189_build(&fast);
    cos_v189_run(&fast);
    for (int i = 0; i < fast.n_queries; ++i) {
        const cos_v189_query_t *q = &fast.queries[i];
        if (q->n_paths != 1) return 9;
        for (int t = 0; t < q->n_tokens; ++t)
            if (q->depth_token[t] != 1) return 10;
        if (q->used_debate || q->used_symbolic || q->used_reflect) return 11;
    }

    /* 6. DEEP mode: every hard query uses 8 paths + all plugins. */
    cos_v189_state_t deep;
    cos_v189_init(&deep, 0x189C7CAFULL, COS_V189_MODE_DEEP);
    cos_v189_build(&deep);
    cos_v189_run(&deep);
    for (int i = 0; i < deep.n_queries; ++i) {
        const cos_v189_query_t *q = &deep.queries[i];
        if (q->n_paths != deep.max_paths_deep) return 12;
        if (q->class_ == COS_V189_CLASS_HARD) {
            if (!(q->used_debate && q->used_symbolic && q->used_reflect))
                return 13;
        }
    }
    if (deep.ratio_hard_over_easy <= 4.0f) return 14;

    return 0;
}
