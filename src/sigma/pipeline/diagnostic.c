/* σ-Diagnostic primitive — explainable σ. */

#include "diagnostic.h"

#include <math.h>
#include <string.h>

static float clamp01(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static bool finite_lp(float x) { return x == x && x > -INFINITY && x < INFINITY; }

static cos_sigma_diagnostic_t empty_diag(float cf_target) {
    cos_sigma_diagnostic_t d;
    memset(&d, 0, sizeof d);
    d.sigma          = 1.0f;
    d.factor_entropy = 1.0f;
    d.factor_gap     = 1.0f;
    d.factor_maxprob = 1.0f;
    d.cf_target      = clamp01(cf_target == 0.0f ? 0.90f : cf_target);
    d.cf_sigma       = 1.0f;
    for (int i = 0; i < COS_SIGMA_DIAG_TOPK; ++i) {
        d.top_idx[i]  = -1;
        d.top_prob[i] = 0.0f;
    }
    return d;
}

/* Numerically-stable softmax over logprobs (inputs may be raw
 * logits OR natural-log probabilities — the shift by max makes
 * either work).  Drops NaN / inf entries. */
static int softmax(const float *lp, int n, float *p_out) {
    float mx = -INFINITY;
    for (int i = 0; i < n; ++i)
        if (finite_lp(lp[i]) && lp[i] > mx) mx = lp[i];
    if (!finite_lp(mx)) return -1;

    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        if (!finite_lp(lp[i])) { p_out[i] = 0.0f; continue; }
        double e = exp((double)(lp[i] - mx));
        p_out[i] = (float)e;
        sum += e;
    }
    if (sum <= 0.0) return -1;
    for (int i = 0; i < n; ++i) p_out[i] = (float)((double)p_out[i] / sum);
    return 0;
}

/* In-place top-3 selection (no allocation). */
static void top3(const float *p, int n, int *idx, float *prob) {
    for (int i = 0; i < COS_SIGMA_DIAG_TOPK; ++i) {
        idx[i]  = -1;
        prob[i] = -1.0f;
    }
    for (int i = 0; i < n; ++i) {
        float v = p[i];
        if (v > prob[0]) {
            prob[2] = prob[1]; idx[2] = idx[1];
            prob[1] = prob[0]; idx[1] = idx[0];
            prob[0] = v;       idx[0] = i;
        } else if (v > prob[1]) {
            prob[2] = prob[1]; idx[2] = idx[1];
            prob[1] = v;       idx[1] = i;
        } else if (v > prob[2]) {
            prob[2] = v;       idx[2] = i;
        }
    }
    for (int i = 0; i < COS_SIGMA_DIAG_TOPK; ++i)
        if (prob[i] < 0.0f) { prob[i] = 0.0f; idx[i] = -1; }
}

/* Counterfactual: what would σ_diag be if p_top1 were lifted to
 * ``target`` and the remaining mass squashed proportionally?  We
 * compute σ on the implied distribution without rebuilding the
 * full vector — only the entropy, gap and max_prob factors matter. */
static float counterfactual_sigma(const float *p, int n, int top1,
                                  float target, float ln_n)
{
    if (n <= 0 || top1 < 0 || top1 >= n) return 1.0f;
    if (target <= 0.0f) target = 0.0001f;
    if (target >= 1.0f) target = 0.9999f;

    float p_top1 = p[top1];
    float rest_mass_old = 1.0f - p_top1;
    float rest_mass_new = 1.0f - target;

    /* Rescale every other entry by rest_mass_new / rest_mass_old. */
    double scale = (rest_mass_old > 0.0f)
                       ? (double)rest_mass_new / (double)rest_mass_old
                       : 0.0;

    /* Entropy of the implied distribution: H = -Σ p_i ln p_i. */
    double H = 0.0;
    float top2_new = 0.0f;
    for (int i = 0; i < n; ++i) {
        double q = (i == top1) ? (double)target
                               : (double)p[i] * scale;
        if (q > 0.0) H -= q * log(q);
        if (i != top1 && (float)q > top2_new) top2_new = (float)q;
    }

    float fact_entropy = (float)(H / (double)((ln_n > 0.0f) ? ln_n : 1.0f));
    if (fact_entropy < 0.0f) fact_entropy = 0.0f;
    if (fact_entropy > 1.0f) fact_entropy = 1.0f;
    float fact_gap     = clamp01(1.0f - (target - top2_new));
    float fact_maxprob = clamp01(1.0f - target);
    return clamp01((fact_entropy + fact_gap + fact_maxprob) / 3.0f);
}

cos_sigma_diagnostic_t
cos_sigma_diagnostic_explain(const float *logprobs, int vocab_size,
                             float cf_target)
{
    if (!logprobs || vocab_size <= 0)
        return empty_diag(cf_target);

    /* Stack-bound buffer: vocab_size up to 65 536 → 256 KiB.  For
     * larger vocabs callers should subselect a top-k slice and
     * call us with that.  We refuse otherwise. */
    if (vocab_size > 65536) return empty_diag(cf_target);
    float p[65536];
    if (softmax(logprobs, vocab_size, p) != 0) return empty_diag(cf_target);

    /* Entropy in nats. */
    double H = 0.0;
    for (int i = 0; i < vocab_size; ++i)
        if (p[i] > 0.0f) H -= (double)p[i] * log((double)p[i]);
    float ln_n = (float)log((double)vocab_size);

    /* Top-3 (idx, prob). */
    int   idx [COS_SIGMA_DIAG_TOPK];
    float prob[COS_SIGMA_DIAG_TOPK];
    top3(p, vocab_size, idx, prob);

    cos_sigma_diagnostic_t d;
    memset(&d, 0, sizeof d);
    d.entropy        = (float)H;
    d.effective_k    = (int)(exp(H) + 0.5);
    d.max_prob       = prob[0];
    d.gap_top12      = prob[0] - prob[1];
    if (d.gap_top12 < 0.0f) d.gap_top12 = 0.0f;

    d.factor_entropy = clamp01((ln_n > 0.0f) ? (float)(H / (double)ln_n) : 0.0f);
    d.factor_gap     = clamp01(1.0f - d.gap_top12);
    d.factor_maxprob = clamp01(1.0f - d.max_prob);
    d.sigma          = clamp01((d.factor_entropy + d.factor_gap
                                + d.factor_maxprob) / 3.0f);

    for (int i = 0; i < COS_SIGMA_DIAG_TOPK; ++i) {
        d.top_idx[i]  = idx[i];
        d.top_prob[i] = prob[i];
    }

    d.cf_target = (cf_target > 0.0f && cf_target < 1.0f)
                      ? cf_target : 0.90f;
    d.cf_sigma  = counterfactual_sigma(p, vocab_size, idx[0],
                                       d.cf_target, ln_n);
    return d;
}

/* --- self-test --------------------------------------------------------- */

static int approx_eq(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

static int check_empty_inputs(void) {
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(NULL, 4, 0.9f);
    if (d.sigma != 1.0f) return 10;
    if (d.factor_entropy != 1.0f || d.factor_gap != 1.0f
        || d.factor_maxprob != 1.0f) return 11;
    float lp[2] = {0.0f, 0.0f};
    d = cos_sigma_diagnostic_explain(lp, 0, 0.9f);
    if (d.sigma != 1.0f) return 12;
    return 0;
}

static int check_sharp_low_sigma(void) {
    /* 4 tokens, log-probs {0, -10, -10, -10}.  Softmax peaks
     * sharply on token 0: p_top1 ≈ 0.99995, p_top2 ≈ 0.00005,
     * H ≈ 0 → all factors ≈ 0 → σ_diag ≈ 0. */
    float lp[4] = {0.0f, -10.0f, -10.0f, -10.0f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.9f);
    if (d.top_idx[0] != 0) return 20;
    if (!(d.max_prob > 0.999f)) return 21;
    if (!(d.sigma < 0.05f)) return 22;
    if (d.effective_k != 1) return 23;
    return 0;
}

static int check_three_way_tie(void) {
    /* 4 tokens, log-probs {0, 0, 0, -10}.  Softmax: p ≈ (1/3, 1/3,
     * 1/3, ε).  H ≈ ln(3) ≈ 1.099, ln(4) ≈ 1.386, factor_entropy
     * ≈ 0.793.  gap ≈ 0 → factor_gap ≈ 1.  max_prob ≈ 0.333 →
     * factor_maxprob ≈ 0.667.  σ_diag ≈ (0.793 + 1.0 + 0.667)/3
     * ≈ 0.820.  effective_k = round(exp(1.099)) = 3. */
    float lp[4] = {0.0f, 0.0f, 0.0f, -10.0f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.9f);
    if (d.effective_k != 3) return 30;
    if (!approx_eq(d.factor_entropy, 0.793f, 0.02f)) return 31;
    if (!approx_eq(d.factor_gap,     1.000f, 0.005f)) return 32;
    if (!approx_eq(d.factor_maxprob, 0.667f, 0.005f)) return 33;
    if (!approx_eq(d.sigma,          0.820f, 0.02f)) return 34;
    return 0;
}

static int check_two_way_close(void) {
    /* Logprobs {0, -0.1, -10, -10}.  p_top1 ≈ 0.5237,
     * p_top2 ≈ 0.4738, gap ≈ 0.05.  factor_gap ≈ 0.95.
     * factor_maxprob ≈ 0.476.  H ≈ 0.692 + small → factor_entropy
     * (over ln 4 = 1.386) ≈ 0.50.  σ ≈ (0.50 + 0.95 + 0.476)/3 ≈
     * 0.642. */
    float lp[4] = {0.0f, -0.1f, -10.0f, -10.0f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.9f);
    if (d.top_idx[0] != 0 || d.top_idx[1] != 1) return 40;
    if (!(d.factor_gap > 0.93f && d.factor_gap < 0.97f)) return 41;
    if (!(d.sigma > 0.60f && d.sigma < 0.68f)) return 42;
    return 0;
}

static int check_counterfactual(void) {
    /* On the three-way tie, lifting top-1 to 0.90 should drive
     * σ way down (entropy + max_prob + gap all collapse). */
    float lp[4] = {0.0f, 0.0f, 0.0f, -10.0f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.90f);
    if (!(d.cf_sigma < d.sigma)) return 50;
    if (!(d.cf_sigma < 0.30f)) return 51;
    return 0;
}

static int check_nan_inf_dropped(void) {
    float lp[4] = {1.0f, 0.0f / 0.0f, 1.0f / 0.0f, 0.5f};
    cos_sigma_diagnostic_t d = cos_sigma_diagnostic_explain(lp, 4, 0.9f);
    /* Top-1 should be index 0 (logprob 1.0).  NaN and +inf are
     * dropped (probability 0).  Index 3 (logprob 0.5) is the
     * runner-up. */
    if (d.top_idx[0] != 0) return 60;
    if (d.top_idx[1] != 3) return 61;
    return 0;
}

int cos_sigma_diagnostic_self_test(void) {
    int rc;
    if ((rc = check_empty_inputs()))    return rc;
    if ((rc = check_sharp_low_sigma())) return rc;
    if ((rc = check_three_way_tie()))   return rc;
    if ((rc = check_two_way_close()))   return rc;
    if ((rc = check_counterfactual())) return rc;
    if ((rc = check_nan_inf_dropped())) return rc;
    return 0;
}
