/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-pipeline: conformal selective-prediction calibration. */
#include "conformal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Hoeffding upper bound on an empirical binary mean.
 *
 *     UCB(p̂, n, δ) = p̂ + sqrt( log(1/δ) / (2 n) )
 *
 * For n ≥ 1, this dominates the true mean with probability ≥ 1 − δ
 * (one-sided).  n == 0 collapses to +∞ (no evidence → no guarantee).
 * ======================================================================== */
static float hoeffding_ucb(float phat, int n, float delta) {
    if (n <= 0) return 1.0f;
    if (delta <= 0.0f || delta >= 1.0f) return 1.0f;
    float radius = (float)sqrt(log(1.0 / (double)delta) / (2.0 * (double)n));
    float ucb = phat + radius;
    if (ucb < 0.0f) ucb = 0.0f;
    if (ucb > 1.0f) ucb = 1.0f;
    return ucb;
}

/* qsort comparator for ascending σ. */
static int cmp_sigma_asc(const void *a, const void *b) {
    float sa = ((const cos_conformal_sample_t *)a)->sigma;
    float sb = ((const cos_conformal_sample_t *)b)->sigma;
    if (sa < sb) return -1;
    if (sa > sb) return  1;
    return 0;
}

/* ========================================================================
 * cos_conformal_calibrate — scan sorted σ's for the largest τ whose
 * UCB on selective risk is ≤ α, with at least MIN_SUPPORT accepted.
 * ======================================================================== */
int cos_conformal_calibrate(const cos_conformal_sample_t *samples, int n,
                            float alpha, float delta,
                            cos_conformal_report_t *out) {
    if (!samples || n <= 0 || !out) return -1;
    if (alpha <= 0.0f || alpha >= 1.0f) return -1;
    if (delta <= 0.0f || delta >= 1.0f) return -1;

    /* Copy scored rows only; we want a clean sort buffer. */
    cos_conformal_sample_t *buf = (cos_conformal_sample_t *)
        calloc((size_t)n, sizeof(*buf));
    if (!buf) return -1;

    int n_scored = 0;
    for (int i = 0; i < n; ++i) {
        if (samples[i].correct < 0) continue;
        buf[n_scored++] = samples[i];
    }

    memset(out, 0, sizeof(*out));
    out->alpha     = alpha;
    out->delta     = delta;
    out->n_total   = n;
    out->n_scored  = n_scored;
    /* default conservative: abstain-by-default at τ = smallest σ. */
    out->tau       = 0.0f;
    out->valid     = 0;
    snprintf(out->domain, sizeof(out->domain), "%s", "all");

    if (n_scored < COS_CONFORMAL_MIN_SUPPORT) {
        free(buf);
        return 0; /* valid=0 */
    }

    qsort(buf, (size_t)n_scored, sizeof(*buf), cmp_sigma_asc);

    /* Running sums as we sweep τ from the smallest σ upward. */
    int best_i = -1;
    float best_ucb = 2.0f; /* sentinel, larger than any valid UCB */
    int running_accept = 0;
    int running_errors = 0;

    for (int i = 0; i < n_scored; ++i) {
        running_accept += 1;
        if (buf[i].correct == 0) running_errors += 1;

        /* If the next sample has the same σ, we must not evaluate yet;
         * the τ we're testing accepts ties, so advance the running
         * sums across the tied block first. */
        if (i + 1 < n_scored && buf[i + 1].sigma == buf[i].sigma) continue;

        if (running_accept < COS_CONFORMAL_MIN_SUPPORT) continue;

        float phat = (float)running_errors / (float)running_accept;
        float ucb  = hoeffding_ucb(phat, running_accept, delta);
        if (ucb <= alpha) {
            /* Track the largest τ (equivalently, largest i) that passes. */
            best_i   = i;
            best_ucb = ucb;
        }
    }

    if (best_i >= 0) {
        out->tau               = buf[best_i].sigma;
        out->n_accepted        = best_i + 1; /* sorted ascending */
        /* Re-compute error count up to and including best_i. */
        int err = 0;
        for (int k = 0; k <= best_i; ++k) {
            if (buf[k].correct == 0) err += 1;
        }
        out->n_errors_accepted = err;
        out->empirical_risk    = (out->n_accepted > 0)
            ? (float)err / (float)out->n_accepted : 0.0f;
        out->risk_ucb          = best_ucb;
        out->coverage          = (float)out->n_accepted / (float)n_scored;
        out->valid             = 1;
    } else {
        /* No τ achieves UCB ≤ α: abstain-by-default. */
        out->tau               = buf[0].sigma;
        out->n_accepted        = 0;
        out->n_errors_accepted = 0;
        out->empirical_risk    = 0.0f;
        out->risk_ucb          = 1.0f;
        out->coverage          = 0.0f;
        out->valid             = 0;
    }

    free(buf);
    return 0;
}

/* ========================================================================
 * cos_conformal_per_domain — partition by sample.domain and calibrate
 * each partition independently, plus one "all" report.
 * ======================================================================== */
int cos_conformal_per_domain(const cos_conformal_sample_t *samples, int n,
                             float alpha, float delta,
                             cos_conformal_bundle_t *bundle) {
    if (!samples || n <= 0 || !bundle) return -1;
    memset(bundle, 0, sizeof(*bundle));

    /* Report 0: "all". */
    cos_conformal_report_t r;
    if (cos_conformal_calibrate(samples, n, alpha, delta, &r) != 0) return -1;
    snprintf(r.domain, sizeof(r.domain), "%s", "all");
    bundle->reports[0] = r;
    bundle->n_reports  = 1;

    /* Collect distinct non-empty domain names. */
    char names[COS_CONFORMAL_DOMAIN_MAX][COS_CONFORMAL_NAME_MAX];
    int  n_names = 0;
    for (int i = 0; i < n && n_names < COS_CONFORMAL_DOMAIN_MAX; ++i) {
        if (samples[i].domain[0] == '\0') continue;
        int found = 0;
        for (int j = 0; j < n_names; ++j) {
            if (strncmp(names[j], samples[i].domain,
                        COS_CONFORMAL_NAME_MAX) == 0) { found = 1; break; }
        }
        if (!found) {
            strncpy(names[n_names], samples[i].domain,
                    COS_CONFORMAL_NAME_MAX - 1);
            names[n_names][COS_CONFORMAL_NAME_MAX - 1] = '\0';
            n_names += 1;
        }
    }

    /* Per-domain: gather + calibrate. */
    cos_conformal_sample_t *sub = (cos_conformal_sample_t *)
        calloc((size_t)n, sizeof(*sub));
    if (!sub) return -1;

    for (int d = 0; d < n_names &&
                    bundle->n_reports < COS_CONFORMAL_DOMAIN_MAX; ++d) {
        int m = 0;
        for (int i = 0; i < n; ++i) {
            if (strncmp(samples[i].domain, names[d],
                        COS_CONFORMAL_NAME_MAX) == 0) sub[m++] = samples[i];
        }
        cos_conformal_report_t rd;
        if (cos_conformal_calibrate(sub, m, alpha, delta, &rd) != 0) continue;
        snprintf(rd.domain, sizeof(rd.domain), "%s", names[d]);
        bundle->reports[bundle->n_reports++] = rd;
    }

    free(sub);
    return 0;
}

/* ========================================================================
 * cos_conformal_eval_at — coverage-curve helper (SCI-3).
 * ======================================================================== */
int cos_conformal_eval_at(const cos_conformal_sample_t *samples, int n,
                          float tau, float delta,
                          cos_conformal_report_t *out) {
    if (!samples || n <= 0 || !out) return -1;
    if (delta <= 0.0f || delta >= 1.0f) return -1;

    memset(out, 0, sizeof(*out));
    out->tau     = tau;
    out->delta   = delta;
    out->n_total = n;
    snprintf(out->domain, sizeof(out->domain), "%s", "all");

    int n_scored = 0, n_accept = 0, n_err = 0;
    for (int i = 0; i < n; ++i) {
        if (samples[i].correct < 0) continue;
        n_scored += 1;
        if (samples[i].sigma <= tau) {
            n_accept += 1;
            if (samples[i].correct == 0) n_err += 1;
        }
    }
    out->n_scored          = n_scored;
    out->n_accepted        = n_accept;
    out->n_errors_accepted = n_err;
    out->empirical_risk    = (n_accept > 0)
        ? (float)n_err / (float)n_accept : 0.0f;
    out->risk_ucb = hoeffding_ucb(out->empirical_risk, n_accept, delta);
    out->coverage = (n_scored > 0)
        ? (float)n_accept / (float)n_scored : 0.0f;
    out->valid = 1;
    return 0;
}

/* ========================================================================
 * JSON bundle I/O (leaf, no external dep).
 * ======================================================================== */
static void json_write_report(FILE *fp, const cos_conformal_report_t *r) {
    fprintf(fp,
        "    {\n"
        "      \"domain\": \"%s\",\n"
        "      \"tau\": %.6f,\n"
        "      \"alpha\": %.6f,\n"
        "      \"delta\": %.6f,\n"
        "      \"n_total\": %d,\n"
        "      \"n_scored\": %d,\n"
        "      \"n_accepted\": %d,\n"
        "      \"n_errors_accepted\": %d,\n"
        "      \"empirical_risk\": %.6f,\n"
        "      \"risk_ucb\": %.6f,\n"
        "      \"coverage\": %.6f,\n"
        "      \"valid\": %d\n"
        "    }",
        r->domain, r->tau, r->alpha, r->delta, r->n_total,
        r->n_scored, r->n_accepted, r->n_errors_accepted,
        r->empirical_risk, r->risk_ucb, r->coverage, r->valid);
}

int cos_conformal_write_bundle_json(const char *path,
                                    const cos_conformal_bundle_t *b) {
    if (!path || !b) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    float alpha = (b->n_reports > 0) ? b->reports[0].alpha : 0.0f;
    float delta = (b->n_reports > 0) ? b->reports[0].delta : 0.0f;
    fprintf(fp,
        "{\n"
        "  \"schema\": \"cos.conformal.v1\",\n"
        "  \"alpha\": %.6f,\n"
        "  \"delta\": %.6f,\n"
        "  \"per_domain\": [\n", alpha, delta);
    for (int i = 0; i < b->n_reports; ++i) {
        json_write_report(fp, &b->reports[i]);
        fprintf(fp, "%s\n", (i + 1 < b->n_reports) ? "," : "");
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return 0;
}

/* Minimal JSON reader tailored to the shape we write above.  Not a
 * general parser.  Looks up each "key": value pair per report via
 * strstr, within a "per_domain" block delimited by '[' and ']'.       */

/* Advance *p past whitespace; return 1 if more chars, 0 on end. */
static int jp_skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
    return **p != '\0';
}

static int jp_get_string(const char *buf, const char *key,
                         char *out, int cap) {
    const char *p = strstr(buf, key);
    if (!p) return -1;
    p = strchr(p + strlen(key), ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return -1;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

static int jp_get_number(const char *buf, const char *key, double *out) {
    const char *p = strstr(buf, key);
    if (!p) return -1;
    p = strchr(p + strlen(key), ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return -1;
    *out = v;
    return 0;
}

int cos_conformal_read_bundle_json(const char *path,
                                   cos_conformal_bundle_t *b) {
    if (!path || !b) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > (1 << 20)) { fclose(fp); return -1; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return -1; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[got] = '\0';

    memset(b, 0, sizeof(*b));

    /* Walk per_domain array by splitting on '{' ... '}' pairs. */
    const char *arr = strstr(buf, "\"per_domain\"");
    if (!arr) { free(buf); return -1; }
    arr = strchr(arr, '[');
    if (!arr) { free(buf); return -1; }
    const char *end = strchr(arr, ']');
    if (!end) { free(buf); return -1; }

    const char *p = arr;
    while (p < end && b->n_reports < COS_CONFORMAL_DOMAIN_MAX) {
        const char *ob = strchr(p, '{');
        if (!ob || ob >= end) break;
        const char *oe = strchr(ob, '}');
        if (!oe || oe >= end) break;

        /* Extract this object into its own nul-terminated window.    */
        size_t olen = (size_t)(oe - ob + 1);
        char *obj = (char *)malloc(olen + 1);
        if (!obj) { free(buf); return -1; }
        memcpy(obj, ob, olen);
        obj[olen] = '\0';

        cos_conformal_report_t r; memset(&r, 0, sizeof(r));
        if (jp_get_string(obj, "\"domain\"", r.domain,
                          COS_CONFORMAL_NAME_MAX) != 0) {
            snprintf(r.domain, sizeof(r.domain), "%s", "all");
        }
        double v;
        if (jp_get_number(obj, "\"tau\"",               &v) == 0) r.tau = (float)v;
        if (jp_get_number(obj, "\"alpha\"",             &v) == 0) r.alpha = (float)v;
        if (jp_get_number(obj, "\"delta\"",             &v) == 0) r.delta = (float)v;
        if (jp_get_number(obj, "\"n_total\"",           &v) == 0) r.n_total = (int)v;
        if (jp_get_number(obj, "\"n_scored\"",          &v) == 0) r.n_scored = (int)v;
        if (jp_get_number(obj, "\"n_accepted\"",        &v) == 0) r.n_accepted = (int)v;
        if (jp_get_number(obj, "\"n_errors_accepted\"", &v) == 0) r.n_errors_accepted = (int)v;
        if (jp_get_number(obj, "\"empirical_risk\"",    &v) == 0) r.empirical_risk = (float)v;
        if (jp_get_number(obj, "\"risk_ucb\"",          &v) == 0) r.risk_ucb = (float)v;
        if (jp_get_number(obj, "\"coverage\"",          &v) == 0) r.coverage = (float)v;
        if (jp_get_number(obj, "\"valid\"",             &v) == 0) r.valid = (int)v;

        b->reports[b->n_reports++] = r;
        free(obj);
        p = oe + 1;
    }

    /* Silence unused-static-fn warning in builds where jp_skip_ws is
     * not folded in; reference it once. */
    (void)jp_skip_ws;

    free(buf);
    return (b->n_reports > 0) ? 0 : -1;
}

/* ========================================================================
 * JSONL loader for truthfulqa-style detail files.
 * ======================================================================== */
static int jp_line_get_string(const char *line, const char *key,
                              char *out, int cap) {
    return jp_get_string(line, key, out, cap);
}

static int jp_line_get_number(const char *line, const char *key, double *v) {
    return jp_get_number(line, key, v);
}

static int jp_line_get_bool_or_null(const char *line, const char *key,
                                    int *out) {
    const char *p = strstr(line, key);
    if (!p) return -1;
    p = strchr(p + strlen(key), ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "true",  4) == 0) { *out =  1; return 0; }
    if (strncmp(p, "false", 5) == 0) { *out =  0; return 0; }
    if (strncmp(p, "null",  4) == 0) { *out = -1; return 0; }
    return -1;
}

int cos_conformal_load_jsonl_ex(const char *path, const char *mode_filter,
                                int classify,
                                cos_conformal_sample_t *out, int cap) {
    if (!path || !out || cap <= 0) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    int n_out = 0;
    char *line = NULL;
    size_t cap_line = 0;
    ssize_t got;
    while ((got = getline(&line, &cap_line, fp)) != -1 && n_out < cap) {
        if (got < 2) continue;
        if (mode_filter && *mode_filter) {
            char mode[COS_CONFORMAL_NAME_MAX];
            if (jp_line_get_string(line, "\"mode\"", mode,
                                   COS_CONFORMAL_NAME_MAX) != 0) continue;
            if (strcmp(mode, mode_filter) != 0) continue;
        }
        double sigma;
        if (jp_line_get_number(line, "\"sigma\"", &sigma) != 0) continue;
        int correct = -1;
        (void)jp_line_get_bool_or_null(line, "\"correct\"", &correct);

        cos_conformal_sample_t s; memset(&s, 0, sizeof(s));
        s.sigma   = (float)sigma;
        s.correct = correct;

        if (classify) {
            /* Best-effort prompt extraction; classifier is deterministic
             * on the string, "other" on NULL.  The kernel's line parser
             * handles escaped quotes by stopping at the first closing
             * quote, which is good enough for TruthfulQA prompts. */
            char prompt[512] = {0};
            (void)jp_line_get_string(line, "\"prompt\"", prompt, sizeof(prompt));
            const char *cat = cos_conformal_classify_prompt(prompt);
            snprintf(s.domain, sizeof(s.domain), "%s", cat);
        } else {
            /* category -> domain (optional). */
            (void)jp_line_get_string(line, "\"category\"", s.domain,
                                     COS_CONFORMAL_NAME_MAX);
        }
        out[n_out++] = s;
    }
    free(line);
    fclose(fp);
    return n_out;
}

int cos_conformal_load_jsonl(const char *path, const char *mode_filter,
                             cos_conformal_sample_t *out, int cap) {
    return cos_conformal_load_jsonl_ex(path, mode_filter, 0, out, cap);
}

/* ========================================================================
 * SCI-2: prompt classifier (factual / code / reasoning / other).
 *
 * The goal is not a clever model; it is a deterministic, auditable
 * tag that the calibration pipeline can partition on, so `cos
 * calibrate --classify-prompts` produces one τ per category even
 * when the dataset did not ship its own "category" field.
 *
 * Rules, in priority order:
 *   code       — triggers if the prompt contains any of {"```",
 *                "def ", "function ", "class ", "return ",
 *                "import ", "#include", "SELECT ", "=>", "printf("}.
 *   reasoning  — triggers on {"why ", "explain", "derive",
 *                "prove", "step by step", "therefore", "because",
 *                "reason "}.
 *   factual    — triggers on {"what ", "who ", "when ", "where ",
 *                "which ", "capital", "define", "how many"}.
 *   other      — fallback.
 *
 * Matching is case-insensitive over an ASCII lowercase copy.  An
 * empty or NULL prompt returns "other".
 * ======================================================================== */
static int contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle) return 0;
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl == 0 || nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; ++i) {
        size_t k = 0;
        while (k < nl) {
            char a = (char)tolower((unsigned char)hay[i + k]);
            char b = (char)tolower((unsigned char)needle[k]);
            if (a != b) break;
            ++k;
        }
        if (k == nl) return 1;
    }
    return 0;
}

const char *cos_conformal_classify_prompt(const char *prompt) {
    if (!prompt || !*prompt) return "other";

    static const char *code_keys[] = {
        "```", "def ", "function ", "class ", "return ",
        "import ", "#include", "select ", "=>", "printf(",
        NULL
    };
    for (int i = 0; code_keys[i]; ++i) {
        if (contains_ci(prompt, code_keys[i])) return "code";
    }

    static const char *reasoning_keys[] = {
        "why ", "explain", "derive", "prove", "step by step",
        "therefore", "because", "reason ", "how does",
        NULL
    };
    for (int i = 0; reasoning_keys[i]; ++i) {
        if (contains_ci(prompt, reasoning_keys[i])) return "reasoning";
    }

    static const char *factual_keys[] = {
        "what ", "who ", "when ", "where ", "which ", "capital",
        "define ", "how many", "how much", "how long",
        NULL
    };
    for (int i = 0; factual_keys[i]; ++i) {
        if (contains_ci(prompt, factual_keys[i])) return "factual";
    }

    return "other";
}

/* ========================================================================
 * SCI-2: Gibbs-Candès ACI update.
 *   τ_{t+1} = clamp(τ_t + η · (err_t − α_target), 0, 1)
 *   err_t  = 1 if σ_t > τ_t  (rejection event)   else 0
 * ======================================================================== */
float cos_conformal_aci_update(float sigma, float tau_in,
                               float alpha_target, float eta) {
    if (!(alpha_target > 0.0f && alpha_target < 1.0f)) return tau_in;
    if (!(eta > 0.0f && eta < 1.0f))                   return tau_in;
    float err = (sigma > tau_in) ? 1.0f : 0.0f;
    float tau_out = tau_in + eta * (err - alpha_target);
    if (tau_out < 0.0f) tau_out = 0.0f;
    if (tau_out > 1.0f) tau_out = 1.0f;
    return tau_out;
}

/* ========================================================================
 * Self-test: synthetic calibration with a known ground-truth ordering.
 * We build 200 samples where correctness flips at σ = 0.5, so the
 * "ideal" τ at α = 0.10 should be close to 0.5.  We only sanity-check
 * monotonicity and UCB shape, not an exact value, because Hoeffding is
 * conservative.
 * ======================================================================== */
int cos_conformal_self_test(void) {
    const int N = 200;
    cos_conformal_sample_t *s = (cos_conformal_sample_t *)
        calloc((size_t)N, sizeof(*s));
    if (!s) return -1;
    for (int i = 0; i < N; ++i) {
        s[i].sigma   = (float)i / (float)N;  /* 0.000 .. 0.995 */
        s[i].correct = (s[i].sigma < 0.5f) ? 1 : 0;
    }

    /* Hoeffding @ N=200 gives a radius of ≈ 0.10-0.12 at δ=0.05, so
     * α must exceed that floor for the UCB to pass at all.  We pick
     * (0.20, 0.40) to test monotonicity comfortably inside the regime
     * where both thresholds are findable. */
    cos_conformal_report_t r_tight, r_loose;
    int rc = 0;
    if (cos_conformal_calibrate(s, N, 0.20f, 0.05f, &r_tight) != 0) rc = -1;
    if (cos_conformal_calibrate(s, N, 0.40f, 0.05f, &r_loose) != 0) rc = -1;

    /* With a perfect monotone relationship, a tighter α must yield a
     * smaller-or-equal τ and never a larger one. */
    if (rc == 0) {
        if (!r_tight.valid || !r_loose.valid) rc = -1;
        else if (r_tight.tau > r_loose.tau + 1e-6f) rc = -1;
        else if (r_tight.risk_ucb > 0.20f + 1e-6f) rc = -1;
        else if (r_loose.risk_ucb > 0.40f + 1e-6f) rc = -1;
    }

    /* eval_at at τ = 0 → no acceptances. */
    cos_conformal_report_t r_zero;
    if (rc == 0 && cos_conformal_eval_at(s, N, 0.0f, 0.05f, &r_zero) != 0) rc = -1;
    if (rc == 0 && r_zero.n_accepted != 1) rc = -1; /* σ_0 == 0.0 */

    /* SCI-2 ACI convergence: drive τ toward the (1−α)-quantile of a
     * stationary stream.  We draw σ's uniformly on [0,1]; with
     * α_target = 0.10 the asymptotic τ should be ≈ 0.90. */
    if (rc == 0) {
        float tau = 0.5f;
        unsigned seed = 0xC0FFEEu;
        for (int i = 0; i < 20000 && rc == 0; ++i) {
            seed = seed * 1103515245u + 12345u;
            float u = (float)((seed >> 16) & 0x7FFF) / 32767.0f;
            tau = cos_conformal_aci_update(u, tau, 0.10f, 0.01f);
        }
        /* Slack of ±0.05 accommodates the stochastic tail. */
        if (tau < 0.85f || tau > 0.95f) rc = -1;
    }

    /* SCI-2 classifier smoke: at least one routing per category. */
    if (rc == 0) {
        if (strcmp(cos_conformal_classify_prompt(
                   "What is the capital of France?"), "factual") != 0) rc = -1;
        if (rc == 0 && strcmp(cos_conformal_classify_prompt(
                   "Why is the sky blue?"), "reasoning") != 0) rc = -1;
        if (rc == 0 && strcmp(cos_conformal_classify_prompt(
                   "```python\ndef foo():\n    return 42\n```"),
                              "code") != 0) rc = -1;
        if (rc == 0 && strcmp(cos_conformal_classify_prompt(""),
                              "other") != 0) rc = -1;
    }

    free(s);
    return rc;
}
