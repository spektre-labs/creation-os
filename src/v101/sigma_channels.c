/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v101 σ-channels on real logits.
 *
 * Eight scalar signals in [0, 1] computed directly from a single row of
 * float logits.  The intent is that every channel maps "0 = confident" and
 * "1 = uncertain" so that `mean(channels)` is a usable abstain signal.
 *
 * Numerical shape:
 *   - softmax is log-sum-exp stable (we subtract max logit).
 *   - partial top-k is via O(n_vocab * k) selection-sort (k <= 20, so for
 *     BitNet's n_vocab = 128256 this is ~2.5 M compares per row — fast
 *     enough that the dominant cost remains llama_decode).
 *   - all reductions are in double to keep the sum of ~128k float probs
 *     numerically clean.
 *
 * Dependencies: <math.h> only.  No NEON / no Accelerate (keeps the layer
 * trivially portable across the hosts the merge-gate must be green on).
 */
#include "bridge.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static inline double clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/* --- default σ-aggregator (v105) -------------------------------------- */

/* Process-wide default.  The initial value is resolved lazily from the
 * COS_V101_AGG env var on the first call to `cos_v101_get_default_aggregator`
 * or `cos_v101_sigma_from_logits`.  We store a sentinel `-1` to indicate
 * "uninitialised" so the env-var override only runs once.  Use of plain
 * `int` (instead of the enum typedef) is deliberate — we want a value
 * outside the enum range as the sentinel.
 */
static int s_default_agg = -1;  /* -1 = not yet resolved */

static int resolve_default_aggregator_env(void)
{
    const char *e = getenv("COS_V101_AGG");
    if (!e || !*e) return (int)COS_V101_AGG_PRODUCT;  /* v105 default */
    /* Case-insensitive compare on short strings. */
    if ((e[0] == 'm' || e[0] == 'M') &&
        (e[1] == 'e' || e[1] == 'E') &&
        (e[2] == 'a' || e[2] == 'A') &&
        (e[3] == 'n' || e[3] == 'N') &&
        e[4] == '\0') {
        return (int)COS_V101_AGG_MEAN;
    }
    if ((e[0] == 'p' || e[0] == 'P') &&
        (e[1] == 'r' || e[1] == 'R') &&
        (e[2] == 'o' || e[2] == 'O') &&
        (e[3] == 'd' || e[3] == 'D') &&
        (e[4] == 'u' || e[4] == 'U') &&
        (e[5] == 'c' || e[5] == 'C') &&
        (e[6] == 't' || e[6] == 'T') &&
        e[7] == '\0') {
        return (int)COS_V101_AGG_PRODUCT;
    }
    /* Unrecognised → v105 default. */
    return (int)COS_V101_AGG_PRODUCT;
}

cos_v101_sigma_agg_t cos_v101_get_default_aggregator(void)
{
    int cur = s_default_agg;
    if (cur == -1) {
        cur = resolve_default_aggregator_env();
        s_default_agg = cur;
    }
    return (cos_v101_sigma_agg_t)cur;
}

cos_v101_sigma_agg_t cos_v101_set_default_aggregator(cos_v101_sigma_agg_t agg)
{
    cos_v101_sigma_agg_t prev = cos_v101_get_default_aggregator();
    if (agg == COS_V101_AGG_MEAN || agg == COS_V101_AGG_PRODUCT) {
        s_default_agg = (int)agg;
    }
    return prev;
}

const char *cos_v101_aggregator_name(cos_v101_sigma_agg_t agg)
{
    switch (agg) {
    case COS_V101_AGG_MEAN:    return "mean";
    case COS_V101_AGG_PRODUCT: return "product";
    default:                   return "unknown";
    }
}

/* Partial top-k: return the indices / values of the top-k logits (by value)
 * in `idx[]` / `val[]`, largest first.  k <= COS_V101_TOPK_MAX.
 */
enum { COS_V101_TOPK_MAX = 20 };

static void partial_topk(const float *logits, int n_vocab, int k,
                         int *idx_out, float *val_out)
{
    if (k > COS_V101_TOPK_MAX) k = COS_V101_TOPK_MAX;
    for (int i = 0; i < k; i++) {
        idx_out[i] = -1;
        val_out[i] = -INFINITY;
    }
    for (int i = 0; i < n_vocab; i++) {
        float v = logits[i];
        if (v <= val_out[k - 1]) continue;
        int pos = k - 1;
        while (pos > 0 && v > val_out[pos - 1]) {
            val_out[pos] = val_out[pos - 1];
            idx_out[pos] = idx_out[pos - 1];
            pos--;
        }
        val_out[pos] = v;
        idx_out[pos] = i;
    }
}

int cos_v101_sigma_from_logits(const float *logits, int n_vocab, cos_v101_sigma_t *out)
{
    if (!logits || !out || n_vocab < 2) return -1;

    /* Pass 1: max + sum of logits, plus mean / variance. */
    double max_l = (double)logits[0];
    double sum_l = 0.0;
    double sum_l2 = 0.0;
    double min_l = (double)logits[0];
    for (int i = 0; i < n_vocab; i++) {
        double x = (double)logits[i];
        if (x > max_l) max_l = x;
        if (x < min_l) min_l = x;
        sum_l += x;
        sum_l2 += x * x;
    }
    double mean_l = sum_l / (double)n_vocab;
    double var_l = (sum_l2 / (double)n_vocab) - (mean_l * mean_l);
    if (var_l < 0.0) var_l = 0.0;
    double std_l = sqrt(var_l);

    /* Pass 2: softmax normalizer Z = sum exp(l - max). */
    double Z = 0.0;
    for (int i = 0; i < n_vocab; i++) {
        Z += exp((double)logits[i] - max_l);
    }
    if (Z <= 0.0) Z = 1e-300;
    double log_Z = log(Z) + max_l;  /* log-sum-exp */

    /* Pass 3: Shannon entropy H = -sum p log p, with p = exp(l - log_Z). */
    double H = 0.0;
    for (int i = 0; i < n_vocab; i++) {
        double log_p = (double)logits[i] - log_Z;
        double p = exp(log_p);
        H -= p * log_p;
    }
    if (H < 0.0) H = 0.0;
    double log_V = log((double)n_vocab);
    double H_norm = H / log_V;

    /* Top-20 probs from logits. */
    int topk_idx[COS_V101_TOPK_MAX];
    float topk_val[COS_V101_TOPK_MAX];
    partial_topk(logits, n_vocab, 20, topk_idx, topk_val);
    double p_top[COS_V101_TOPK_MAX];
    for (int i = 0; i < 20; i++) {
        p_top[i] = exp((double)topk_val[i] - log_Z);
    }

    double p_top1 = p_top[0];
    double p_top2 = (n_vocab >= 2) ? p_top[1] : 0.0;
    double margin = p_top1 - p_top2;
    double top5_sum = p_top[0] + p_top[1] + p_top[2] + p_top[3] + p_top[4];
    double top20_sum = 0.0;
    for (int i = 0; i < 20; i++) top20_sum += p_top[i];

    double tail_mass = 1.0 - top20_sum;
    if (tail_mass < 0.0) tail_mass = 0.0;
    if (tail_mass > 1.0) tail_mass = 1.0;

    /* Channel construction. */
    double std_safe = (std_l < 1e-9) ? 1e-9 : std_l;
    double centered_spread = (max_l - mean_l) / std_safe;  /* z-score of argmax */

    out->entropy_norm = (float)clamp01(H_norm);
    out->margin       = (float)clamp01(1.0 - margin);
    out->top_k_mass   = (float)clamp01(1.0 - top5_sum);
    out->tail_mass    = (float)tail_mass;
    out->logit_spread = (float)clamp01(1.0 - tanh(centered_spread / 8.0));
    out->p_max        = (float)clamp01(1.0 - p_top1);
    /* Effective support ratio.  exp(H) in [1, n_vocab]: high → uncertain. */
    out->n_effective  = (float)clamp01(exp(H) / (double)n_vocab);
    out->logit_std    = (float)clamp01(1.0 - tanh(std_l / 4.0));

    /* Arithmetic mean of the 8 channels (pre-v105 default). */
    double sigma_sum =
        (double)out->entropy_norm +
        (double)out->margin +
        (double)out->top_k_mass +
        (double)out->tail_mass +
        (double)out->logit_spread +
        (double)out->p_max +
        (double)out->n_effective +
        (double)out->logit_std;
    double arith_mean = sigma_sum / (double)COS_V101_SIGMA_CHANNELS;

    /* Geometric mean of the 8 channels (v105 default).  We floor each
     * channel at `EPS` before taking the log so a perfectly-confident
     * channel (x == 0) cannot collapse the whole product to zero.  EPS
     * matches `benchmarks/v104/operators.py::op_sigma_product`, which is
     * where this aggregator was validated as the cross-task winner. */
    const double EPS = 1e-6;
    double channels[COS_V101_SIGMA_CHANNELS] = {
        (double)out->entropy_norm,
        (double)out->margin,
        (double)out->top_k_mass,
        (double)out->tail_mass,
        (double)out->logit_spread,
        (double)out->p_max,
        (double)out->n_effective,
        (double)out->logit_std,
    };
    double log_sum = 0.0;
    for (int i = 0; i < COS_V101_SIGMA_CHANNELS; i++) {
        double v = channels[i];
        if (v < EPS) v = EPS;
        log_sum += log(v);
    }
    double geom_mean = exp(log_sum / (double)COS_V101_SIGMA_CHANNELS);

    out->sigma_arith_mean = (float)clamp01(arith_mean);
    out->sigma_product    = (float)clamp01(geom_mean);

    /* The public `sigma` field is whichever the current default aggregator
     * picks.  All existing consumers (abstention gate, max-per-token, CLI
     * JSON output, v103 sidecar) continue to read `s.sigma` and
     * transparently switch aggregator when the default is reconfigured. */
    cos_v101_sigma_agg_t agg = cos_v101_get_default_aggregator();
    out->sigma = (agg == COS_V101_AGG_MEAN)
                     ? out->sigma_arith_mean
                     : out->sigma_product;

    (void)min_l;  /* kept in case future channel wants it */
    return 0;
}
