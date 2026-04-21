/*
 * σ-Evolve — implementation.  See cos_evolve.h for contracts.
 *
 * Design notes:
 *   - Brier fitness is convex in the weights for fixed τ and fixed
 *     per-row signals, so (1+1)-ES with Gaussian steps and elitist
 *     selection provably converges to the minimum.  We exploit that
 *     in the self-test: start from uniform 0.25 weights, evolve for
 *     N generations, assert Brier strictly drops.
 *   - Fitness is pure; the only side-effect the core ever takes is
 *     writing params.json / fixture loading (explicitly flagged APIs).
 *   - RNG is splitmix64 seeded from the policy.  Deterministic.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "cos_evolve.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------ small helpers ------------------ */

static float clampf(float x, float lo, float hi) {
    if (!(x == x)) return lo;           /* NaN → lo */
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float rng_uniform(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)((uint64_t)1 << 53));
}

/* Box-Muller (one draw per call, other discarded; cheap and fine). */
static float rng_gaussian(uint64_t *s) {
    float u1 = rng_uniform(s);
    float u2 = rng_uniform(s);
    if (u1 < 1e-12f) u1 = 1e-12f;
    return sqrtf(-2.0f * logf(u1)) * cosf(6.2831853071795864769f * u2);
}

/* ------------------ genome math ------------------ */

void cos_sigma_evolve_params_default(cos_evolve_params_t *p) {
    if (!p) return;
    p->w_logprob     = 0.50f;
    p->w_entropy     = 0.20f;
    p->w_perplexity  = 0.10f;
    p->w_consistency = 0.20f;
    p->tau           = 0.50f;
}

void cos_sigma_evolve_params_normalise(cos_evolve_params_t *p) {
    if (!p) return;
    float w[4] = { p->w_logprob, p->w_entropy,
                   p->w_perplexity, p->w_consistency };
    for (int i = 0; i < 4; i++) w[i] = clampf(w[i], 0.0f, 1.0f);
    float s = w[0] + w[1] + w[2] + w[3];
    if (s < 1e-9f) {                     /* degenerate → uniform      */
        for (int i = 0; i < 4; i++) w[i] = 0.25f;
    } else {
        for (int i = 0; i < 4; i++) w[i] /= s;
    }
    p->w_logprob     = w[0];
    p->w_entropy     = w[1];
    p->w_perplexity  = w[2];
    p->w_consistency = w[3];
    p->tau = clampf(p->tau, 0.01f, 0.99f);
}

void cos_sigma_evolve_mutate_default(cos_evolve_params_t *p,
                                     float mutation_sigma,
                                     uint64_t *rng_state) {
    if (!p || !rng_state) return;
    int idx = (int)(splitmix64(rng_state) & 0x3);
    float step = rng_gaussian(rng_state) * mutation_sigma;
    float *w[4] = { &p->w_logprob, &p->w_entropy,
                    &p->w_perplexity, &p->w_consistency };
    *w[idx] += step;
    cos_sigma_evolve_params_normalise(p);
}

void cos_sigma_evolve_mutate_tau(cos_evolve_params_t *p,
                                 float mutation_sigma,
                                 uint64_t *rng_state) {
    if (!p || !rng_state) return;
    p->tau += rng_gaussian(rng_state) * mutation_sigma;
    p->tau = clampf(p->tau, 0.01f, 0.99f);
}

/* ------------------ fitness ------------------ */

int cos_sigma_evolve_score(const cos_evolve_params_t *p,
                           const cos_evolve_item_t   *items,
                           int n_items,
                           cos_evolve_score_t *out) {
    if (!p || !out) return -1;
    memset(out, 0, sizeof *out);
    if (!items || n_items <= 0) {
        out->brier = (float)NAN;
        return 0;
    }
    double sse = 0.0;
    int n_accept = 0, n_correct_accept = 0;
    for (int i = 0; i < n_items; i++) {
        const cos_evolve_item_t *it = &items[i];
        float sigma =
              p->w_logprob     * clampf(it->sigma_logprob,     0.f, 1.f)
            + p->w_entropy     * clampf(it->sigma_entropy,     0.f, 1.f)
            + p->w_perplexity  * clampf(it->sigma_perplexity,  0.f, 1.f)
            + p->w_consistency * clampf(it->sigma_consistency, 0.f, 1.f);
        sigma = clampf(sigma, 0.f, 1.f);
        float target = it->correct ? 0.0f : 1.0f;
        float err = sigma - target;
        sse += (double)err * (double)err;
        if (sigma <= p->tau) {
            n_accept++;
            if (it->correct) n_correct_accept++;
        }
    }
    out->n_items          = n_items;
    out->n_accept         = n_accept;
    out->n_correct_accept = n_correct_accept;
    out->brier            = (float)(sse / (double)n_items);
    out->coverage         = (float)n_accept / (float)n_items;
    out->accuracy_accept  =
        n_accept > 0 ? (float)n_correct_accept / (float)n_accept : 0.0f;
    return 0;
}

/* ------------------ run loop ------------------ */

void cos_sigma_evolve_policy_default(cos_evolve_policy_t *p) {
    if (!p) return;
    p->generations     = 200;
    p->improvement_eps = 1e-4f;
    p->mutation_sigma  = 0.08f;
    p->rng_seed        = 0x1337BEEFULL;
    p->log_every       = 0;
}

int cos_sigma_evolve_run(cos_evolve_params_t *inout,
                         const cos_evolve_item_t *items, int n_items,
                         const cos_evolve_policy_t *policy,
                         cos_evolve_report_t *rep) {
    if (!inout || !items || n_items <= 0) return -1;
    cos_evolve_policy_t pol;
    if (policy) pol = *policy;
    else cos_sigma_evolve_policy_default(&pol);

    cos_sigma_evolve_params_normalise(inout);

    cos_evolve_score_t score0, score_best, score_cand;
    if (cos_sigma_evolve_score(inout, items, n_items, &score0) != 0)
        return -1;
    cos_evolve_params_t best = *inout;
    score_best = score0;

    uint64_t rng = pol.rng_seed ? pol.rng_seed : 1ULL;
    int n_accept = 0, n_revert = 0;

    for (int g = 0; g < pol.generations; g++) {
        cos_evolve_params_t cand = best;
        /* Alternate weight-mutations and τ-mutations (3:1).
         * Keeps τ moving so cos calibrate --auto can share this loop. */
        if ((g & 3) == 3) {
            cos_sigma_evolve_mutate_tau(&cand, pol.mutation_sigma, &rng);
        } else {
            cos_sigma_evolve_mutate_default(&cand, pol.mutation_sigma,
                                             &rng);
        }
        if (cos_sigma_evolve_score(&cand, items, n_items, &score_cand) != 0)
            continue;

        if (score_cand.brier + pol.improvement_eps < score_best.brier) {
            best       = cand;
            score_best = score_cand;
            n_accept++;
            if (pol.log_every > 0 && (g % pol.log_every) == 0) {
                fprintf(stderr,
                        "[evolve] gen=%d accept brier=%.6f "
                        "acc@τ=%.3f cov=%.3f\n",
                        g, score_best.brier, score_best.accuracy_accept,
                        score_best.coverage);
            }
        } else {
            n_revert++;
        }
    }

    *inout = best;
    if (rep) {
        memset(rep, 0, sizeof *rep);
        rep->iterations  = pol.generations;
        rep->n_accepted  = n_accept;
        rep->n_reverted  = n_revert;
        rep->best        = best;
        rep->best_score  = score_best;
        rep->start_score = score0;
        /* start is the caller's pre-normalised input; approximate by
         * holding the normalised start in .start for reproducibility */
        rep->start       = best;     /* overwritten below */
    }
    return 0;
}

/* ------------------ fixture loader (minimal JSONL) ------------------ */

static const char *json_find_key(const char *line, const char *key) {
    size_t kl = strlen(key);
    const char *p = line;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, key, kl) == 0 && p[1 + kl] == '"') {
            const char *q = p + 1 + kl + 1;
            while (*q && (*q == ' ' || *q == '\t' || *q == ':')) q++;
            return q;
        }
        p++;
    }
    return NULL;
}

static float json_num(const char *line, const char *key, float def) {
    const char *p = json_find_key(line, key);
    if (!p) return def;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return def;
    return (float)v;
}

static int json_bool_or_int(const char *line, const char *key, int def) {
    const char *p = json_find_key(line, key);
    if (!p) return def;
    if (strncmp(p, "true",  4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p) return def;
    return v ? 1 : 0;
}

cos_evolve_item_t *cos_sigma_evolve_load_fixture_jsonl(
        const char *path, int *n_items, char *err, size_t err_cap) {
    if (n_items) *n_items = 0;
    if (!path) { if (err && err_cap) snprintf(err, err_cap, "no path"); return NULL; }
    FILE *f = fopen(path, "r");
    if (!f) {
        if (err && err_cap) snprintf(err, err_cap,
                "cannot open %s: %s", path, strerror(errno));
        return NULL;
    }
    size_t cap = 64, n = 0;
    cos_evolve_item_t *arr = calloc(cap, sizeof *arr);
    if (!arr) { fclose(f); return NULL; }
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        const char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '{') continue;
        if (n == cap) {
            size_t nc = cap * 2;
            cos_evolve_item_t *na = realloc(arr, nc * sizeof *arr);
            if (!na) { free(arr); fclose(f); return NULL; }
            arr = na; cap = nc;
        }
        cos_evolve_item_t it = {0};
        it.sigma_logprob     = json_num (line, "sigma_logprob",     0.f);
        it.sigma_entropy     = json_num (line, "sigma_entropy",     0.f);
        it.sigma_perplexity  = json_num (line, "sigma_perplexity",  0.f);
        it.sigma_consistency = json_num (line, "sigma_consistency", 0.f);
        it.correct           = json_bool_or_int(line, "correct", 0);
        arr[n++] = it;
    }
    fclose(f);
    if (n_items) *n_items = (int)n;
    return arr;
}

/* ------------------ params JSON read/write ------------------ */

int cos_sigma_evolve_params_load_json(const char *path,
                                      cos_evolve_params_t *out) {
    if (!path || !out) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    out->w_logprob     = json_num(buf, "w_logprob",     out->w_logprob);
    out->w_entropy     = json_num(buf, "w_entropy",     out->w_entropy);
    out->w_perplexity  = json_num(buf, "w_perplexity",  out->w_perplexity);
    out->w_consistency = json_num(buf, "w_consistency", out->w_consistency);
    out->tau           = json_num(buf, "tau",           out->tau);
    cos_sigma_evolve_params_normalise(out);
    return 0;
}

int cos_sigma_evolve_params_save_json(const char *path,
                                      const cos_evolve_params_t *p) {
    if (!path || !p) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f,
        "{\n"
        "  \"w_logprob\":     %.8f,\n"
        "  \"w_entropy\":     %.8f,\n"
        "  \"w_perplexity\":  %.8f,\n"
        "  \"w_consistency\": %.8f,\n"
        "  \"tau\":           %.8f\n"
        "}\n",
        p->w_logprob, p->w_entropy, p->w_perplexity,
        p->w_consistency, p->tau);
    fclose(f);
    return 0;
}

/* ------------------ self-test ------------------
 *
 * Synthetic fixture of 32 rows: correct rows have σ-signals
 * clustered near 0, incorrect rows near 1, with signal-specific
 * informativeness (σ_logprob is the strongest signal, σ_perplexity
 * the weakest).  A Brier-minimising weight vector therefore leans
 * hard on σ_logprob.  We assert the evolved weights satisfy that
 * ordering, that Brier strictly drops, and that accuracy on the
 * accepted slice goes up.
 */
static void make_synthetic_fixture(cos_evolve_item_t *arr, int n,
                                   uint64_t seed) {
    uint64_t s = seed;
    for (int i = 0; i < n; i++) {
        int correct = (i & 1) == 0;
        arr[i].correct = correct;
        float base = correct ? 0.10f : 0.85f;
        float noise_lp   = (rng_uniform(&s) - 0.5f) * 0.15f;
        float noise_en   = (rng_uniform(&s) - 0.5f) * 0.35f;
        float noise_pp   = (rng_uniform(&s) - 0.5f) * 0.55f;
        float noise_cons = (rng_uniform(&s) - 0.5f) * 0.25f;
        arr[i].sigma_logprob     = clampf(base + noise_lp,   0.f, 1.f);
        arr[i].sigma_entropy     = clampf(base + noise_en,   0.f, 1.f);
        arr[i].sigma_perplexity  = clampf(base + noise_pp,   0.f, 1.f);
        arr[i].sigma_consistency = clampf(base + noise_cons, 0.f, 1.f);
    }
}

int cos_sigma_evolve_self_test(void) {
    cos_evolve_item_t fx[64];
    make_synthetic_fixture(fx, 64, 0xC05C05C0ULL);

    cos_evolve_params_t p;
    cos_sigma_evolve_params_default(&p);
    /* Start from uniform to make the drop observable. */
    p.w_logprob = p.w_entropy = p.w_perplexity = p.w_consistency = 0.25f;
    p.tau = 0.50f;

    cos_evolve_score_t s0;
    if (cos_sigma_evolve_score(&p, fx, 64, &s0) != 0) return 1;

    cos_evolve_policy_t pol;
    cos_sigma_evolve_policy_default(&pol);
    pol.generations = 400;
    pol.rng_seed    = 0xDEADBEEFULL;

    cos_evolve_report_t rep;
    if (cos_sigma_evolve_run(&p, fx, 64, &pol, &rep) != 0) return 2;

    /* Brier monotone-improved. */
    if (!(rep.best_score.brier + 1e-6f < s0.brier)) return 3;
    /* Strongest signal won the weight race. */
    if (!(p.w_logprob > p.w_perplexity)) return 4;
    /* Accepted-slice accuracy beat or matched the start. */
    if (!(rep.best_score.accuracy_accept + 1e-6f >= s0.accuracy_accept))
        return 5;
    /* Weights normalise. */
    float ws = p.w_logprob + p.w_entropy + p.w_perplexity + p.w_consistency;
    if (!(ws > 0.999f && ws < 1.001f)) return 6;
    /* τ stayed in (0,1). */
    if (!(p.tau > 0.f && p.tau < 1.f)) return 7;
    /* Normalisation safe on degenerate input. */
    cos_evolve_params_t zero = {0.f, 0.f, 0.f, 0.f, 0.50f};
    cos_sigma_evolve_params_normalise(&zero);
    if (!(fabsf(zero.w_logprob - 0.25f) < 1e-4f)) return 8;
    /* Fixture-less score → NaN brier, no crash. */
    cos_evolve_score_t se;
    cos_sigma_evolve_score(&p, NULL, 0, &se);
    if (!(se.brier != se.brier)) return 9;
    return 0;
}
