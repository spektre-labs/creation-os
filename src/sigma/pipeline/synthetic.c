/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Synthetic primitive — quality-gated generation with collapse guard. */

#include "synthetic.h"

#include <stdio.h>
#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

int cos_sigma_synth_init(cos_synth_config_t *cfg,
                         float tau_quality,
                         float tau_diversity,
                         float anchor_ratio,
                         int max_attempts)
{
    if (!cfg) return -1;
    if (!(tau_quality  > 0.0f && tau_quality  <= 1.0f)) return -2;
    if (!(tau_diversity > 0.0f && tau_diversity <= 1.0f)) return -3;
    if (!(anchor_ratio >= 0.0f && anchor_ratio <= 1.0f)) return -4;
    if (max_attempts <= 0) return -5;
    cfg->tau_quality    = tau_quality;
    cfg->tau_diversity  = tau_diversity;
    cfg->anchor_ratio   = anchor_ratio;
    cfg->max_attempts   = max_attempts;
    return 0;
}

/* Compare the first 8 bytes of two strings; NULL-safe. */
static int prefix_eq8(const char *a, const char *b) {
    if (!a || !b) return a == b;
    for (int i = 0; i < 8; ++i) {
        char ca = a[i], cb = b[i];
        if (ca != cb) return 0;
        if (ca == '\0') return 1;
    }
    return 1;
}

float cos_sigma_synth_diversity(const cos_synth_pair_t *pairs, int n) {
    if (!pairs || n <= 0) return 1.0f;
    int unique = 0;
    for (int i = 0; i < n; ++i) {
        int seen = 0;
        for (int j = 0; j < i; ++j) {
            if (prefix_eq8(pairs[i].output, pairs[j].output)) { seen = 1; break; }
        }
        if (!seen) unique++;
    }
    return (float)unique / (float)n;
}

int cos_sigma_synth_generate(const cos_synth_config_t *cfg,
                             const char *domain,
                             int n_target,
                             cos_synth_generate_fn generator,
                             void *ctx,
                             cos_synth_pair_t *out_pairs, int cap,
                             cos_synth_report_t *out)
{
    if (!cfg || !generator || !out_pairs || !out || n_target <= 0 || cap <= 0)
        return -1;
    if (n_target > cap) return -2;
    memset(out, 0, sizeof *out);
    out->n_target = n_target;

    int attempts  = 0;
    int accepted  = 0;
    int n_real    = 0;
    int n_synth   = 0;
    int rej_qual  = 0;
    int rej_anch  = 0;
    int collapsed = 0;
    int cap_hit   = 0;

    while (accepted < n_target) {
        if (attempts >= cfg->max_attempts) { cap_hit = 1; break; }
        cos_synth_pair_t cand;
        memset(&cand, 0, sizeof cand);
        if (generator(domain, attempts, ctx, &cand) != 0) {
            attempts++;
            continue;
        }
        attempts++;
        cand.sigma    = clamp01f(cand.sigma);
        cand.accepted = 0;

        /* Quality gate (real pairs bypass σ-gating — they are ground truth). */
        if (!cand.is_real && cand.sigma >= cfg->tau_quality) {
            rej_qual++;
            continue;
        }

        /* Anchor-ratio gate: cap total synth count at
         *   max_synth = n_target − ceil(anchor_ratio · n_target).
         * Simple, deterministic, and exactly enforces the
         * caller-requested real share at the end of the run. */
        if (!cand.is_real && cfg->anchor_ratio > 0.0f) {
            int required_real = (int)((double)cfg->anchor_ratio
                                      * (double)n_target + 0.9999);
            if (required_real > n_target) required_real = n_target;
            int max_synth = n_target - required_real;
            if (n_synth >= max_synth) {
                rej_anch++;
                continue;
            }
        }

        cand.accepted = 1;
        out_pairs[accepted] = cand;
        accepted++;
        if (cand.is_real) n_real++; else n_synth++;

        /* Recompute σ_diversity on the accepted set every step;
         * stop the run if it drops below τ_diversity. */
        float div = cos_sigma_synth_diversity(out_pairs, accepted);
        if (accepted >= 3 && div < cfg->tau_diversity) {
            collapsed = 1;
            break;
        }
    }

    out->attempts        = attempts;
    out->accepted        = accepted;
    out->n_real          = n_real;
    out->n_synthetic     = n_synth;
    out->rejected_quality = rej_qual;
    out->rejected_anchor  = rej_anch;
    out->diversity       = accepted > 0
        ? cos_sigma_synth_diversity(out_pairs, accepted) : 1.0f;
    out->acceptance_rate = attempts > 0
        ? (float)accepted / (float)attempts : 0.0f;
    out->collapsed       = collapsed;
    out->hit_attempt_cap = cap_hit;
    return 0;
}

/* ---------- self-test ---------- */

/* Deterministic generator that round-robins through a caller-supplied
 * script of (output, sigma, is_real) tuples. */
typedef struct {
    const char *const *outputs;
    const float       *sigmas;
    const int         *is_real;
    int                n;
    int                cursor;
} script_ctx_t;

static int script_gen(const char *domain, int seed, void *ctx,
                      cos_synth_pair_t *out) {
    script_ctx_t *s = (script_ctx_t *)ctx;
    (void)domain; (void)seed;
    if (s->cursor >= s->n) return -1;
    out->input   = "q";
    out->output  = s->outputs[s->cursor];
    out->sigma   = s->sigmas [s->cursor];
    out->is_real = s->is_real[s->cursor];
    s->cursor++;
    return 0;
}

static int check_init_guards(void) {
    cos_synth_config_t cfg;
    if (cos_sigma_synth_init(&cfg, 0.0f,  0.5f, 0.2f, 100) == 0) return 10;
    if (cos_sigma_synth_init(&cfg, 0.5f,  0.0f, 0.2f, 100) == 0) return 11;
    if (cos_sigma_synth_init(&cfg, 0.5f,  0.5f, -0.1f, 100) == 0) return 12;
    if (cos_sigma_synth_init(&cfg, 0.5f,  0.5f, 1.1f,  100) == 0) return 13;
    if (cos_sigma_synth_init(&cfg, 0.5f,  0.5f, 0.2f,   0)  == 0) return 14;
    return 0;
}

static int check_quality_filter(void) {
    const char *outs[]  = {"alpha", "beta", "gamma", "delta", "epsilon"};
    float sigmas[]      = {0.10f, 0.45f, 0.20f, 0.80f, 0.25f};
    int   real[]        = {0, 0, 0, 0, 0};
    script_ctx_t s = {outs, sigmas, real, 5, 0};
    cos_synth_config_t cfg;
    cos_sigma_synth_init(&cfg, 0.30f, 0.20f, 0.0f, 20);
    cos_synth_pair_t pairs[5];
    cos_synth_report_t r;
    /* Three σ<0.30 candidates: alpha 0.10, gamma 0.20, epsilon 0.25 */
    if (cos_sigma_synth_generate(&cfg, "d", 3, script_gen, &s,
                                 pairs, 5, &r) != 0)        return 20;
    if (r.accepted != 3)                                      return 21;
    if (r.rejected_quality != 2)                              return 22;
    if (strcmp(pairs[0].output, "alpha")    != 0)             return 23;
    if (strcmp(pairs[1].output, "gamma")    != 0)             return 24;
    if (strcmp(pairs[2].output, "epsilon")  != 0)             return 25;
    /* 3 accepts in 5 attempts = 0.6. */
    if (!(r.acceptance_rate > 0.59f && r.acceptance_rate < 0.61f)) return 26;
    return 0;
}

static int check_collapse_guard(void) {
    /* All outputs share prefix "sameprefix_" → diversity = 1/n after
     * first accept but 1/k then 1/k afterwards.  After 3 accepts
     * diversity = 1/3 = 0.333… < τ_diversity = 0.50 → collapse. */
    const char *outs[] = {
        "sameprefix_a", "sameprefix_b", "sameprefix_c",
        "sameprefix_d", "sameprefix_e"
    };
    float sigmas[]  = {0.10f, 0.10f, 0.10f, 0.10f, 0.10f};
    int   real[]    = {0, 0, 0, 0, 0};
    script_ctx_t s  = {outs, sigmas, real, 5, 0};
    cos_synth_config_t cfg;
    cos_sigma_synth_init(&cfg, 0.30f, 0.50f, 0.0f, 20);
    cos_synth_pair_t pairs[5];
    cos_synth_report_t r;
    cos_sigma_synth_generate(&cfg, "d", 5, script_gen, &s, pairs, 5, &r);
    if (!r.collapsed)            return 30;
    if (r.accepted != 3)         return 31;  /* stops after 3rd accept */
    return 0;
}

static int check_anchor_ratio(void) {
    /* 5-slot target; anchor_ratio 0.40 → need at least 2 real.
     * Script: synth, synth, real, synth, synth, real.
     * τ_quality 0.50 (all pass).  τ_diversity 0.10 (outputs unique).
     * With anchor_ratio 0.40 enforcement: first synth 1 accept (ok),
     * second synth accept → ratio 0/2=0, remaining=3, max_real=3,
     * best=3/5=0.60 ≥0.40 so reject; real accepts; then synth ok;
     * then synth; then real. */
    const char *outs[]  = {"syn_a", "syn_b", "REAL_c", "syn_d", "syn_e", "REAL_f"};
    float sigmas[]      = {0.10f, 0.15f, 0.05f, 0.20f, 0.22f, 0.05f};
    int   real[]        = {0, 0, 1, 0, 0, 1};
    script_ctx_t s = {outs, sigmas, real, 6, 0};
    cos_synth_config_t cfg;
    cos_sigma_synth_init(&cfg, 0.50f, 0.10f, 0.40f, 20);
    cos_synth_pair_t pairs[5];
    cos_synth_report_t r;
    if (cos_sigma_synth_generate(&cfg, "d", 5, script_gen, &s,
                                 pairs, 5, &r) != 0)            return 40;
    if (r.accepted != 5)                                         return 41;
    if (r.n_real   != 2)                                         return 42;
    if (r.n_synthetic != 3)                                      return 43;
    /* Expect at least one anchor rejection along the way. */
    if (r.rejected_anchor < 1)                                   return 44;
    return 0;
}

static int check_diversity_metric(void) {
    cos_synth_pair_t p[4];
    memset(p, 0, sizeof p);
    p[0].output = "aaa";     /* aaa */
    p[1].output = "aab";     /* aab (different at byte 2)  */
    p[2].output = "aaa";     /* duplicate → matches p[0] within 8 bytes */
    p[3].output = "zzzzzzzz"; /* unique */
    float d = cos_sigma_synth_diversity(p, 4);
    /* Unique prefixes: p[0] unique, p[1] unique, p[2] matches p[0], p[3] unique
     * → 3/4 = 0.75. */
    if (!(d > 0.749f && d < 0.751f)) return 50;
    return 0;
}

int cos_sigma_synth_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))       return rc;
    if ((rc = check_quality_filter()))    return rc;
    if ((rc = check_collapse_guard()))    return rc;
    if ((rc = check_anchor_ratio()))      return rc;
    if ((rc = check_diversity_metric()))  return rc;
    return 0;
}
