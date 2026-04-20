/* σ-Merge primitive (LINEAR / SLERP / TIES / TASK_ARITH + α-sweep). */

#include "merge.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void merge_linear(const float *a, const float *b, int n,
                         float alpha, float *out)
{
    for (int i = 0; i < n; ++i)
        out[i] = alpha * a[i] + (1.0f - alpha) * b[i];
}

static void merge_slerp(const float *a, const float *b, int n,
                        float alpha, float *out)
{
    /* Compute Ω = acos(<a,b>/(|a||b|)).  Fallback to LINEAR when
     * Ω is tiny (vectors ~parallel) or when either vector is zero. */
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < n; ++i) {
        dot += (double)a[i] * (double)b[i];
        na  += (double)a[i] * (double)a[i];
        nb  += (double)b[i] * (double)b[i];
    }
    double na_s = sqrt(na), nb_s = sqrt(nb);
    if (na_s < 1e-12 || nb_s < 1e-12) { merge_linear(a, b, n, alpha, out); return; }
    double cosom = dot / (na_s * nb_s);
    if (cosom > 1.0)  cosom = 1.0;
    if (cosom < -1.0) cosom = -1.0;
    double omega = acos(cosom);
    double s_om  = sin(omega);
    if (fabs(s_om) < 1e-6) { merge_linear(a, b, n, alpha, out); return; }
    double wa = sin((1.0 - (double)alpha) * omega) / s_om;
    double wb = sin((double)alpha          * omega) / s_om;
    for (int i = 0; i < n; ++i)
        out[i] = (float)(wa * (double)a[i] + wb * (double)b[i]);
}

/* Helper: absolute value for qsort key. */
static int cmp_abs_desc(const void *xp, const void *yp) {
    float x = fabsf(*(const float *)xp);
    float y = fabsf(*(const float *)yp);
    if (x < y) return 1;
    if (x > y) return -1;
    return 0;
}

static void apply_top_k_mask(float *v, int n, int k) {
    if (k <= 0 || k >= n) return;
    /* Copy abs values, sort desc, take k-th, threshold original. */
    float *buf = (float *)malloc(sizeof(float) * (size_t)n);
    if (!buf) return;
    for (int i = 0; i < n; ++i) buf[i] = v[i];
    qsort(buf, (size_t)n, sizeof(float), cmp_abs_desc);
    float thresh = fabsf(buf[k - 1]);
    for (int i = 0; i < n; ++i) if (fabsf(v[i]) < thresh) v[i] = 0.0f;
    free(buf);
}

static void merge_ties(const float *a, const float *b, int n,
                       float alpha, int top_k, float *out)
{
    /* TIES-Merging (simplified): per coordinate,
     *   1. trim each vector to top-k |values|,
     *   2. sign-consensus: if signs agree, take α-linear combination,
     *      else drop to zero.                                       */
    float *A = (float *)malloc(sizeof(float) * (size_t)n);
    float *B = (float *)malloc(sizeof(float) * (size_t)n);
    if (!A || !B) { free(A); free(B); merge_linear(a,b,n,alpha,out); return; }
    memcpy(A, a, sizeof(float) * (size_t)n);
    memcpy(B, b, sizeof(float) * (size_t)n);
    apply_top_k_mask(A, n, top_k);
    apply_top_k_mask(B, n, top_k);
    for (int i = 0; i < n; ++i) {
        float x = A[i], y = B[i];
        if (x == 0.0f && y == 0.0f)     { out[i] = 0.0f; continue; }
        if (x == 0.0f)                  { out[i] = (1.0f - alpha) * y; continue; }
        if (y == 0.0f)                  { out[i] = alpha * x; continue; }
        if ((x >= 0) == (y >= 0))       out[i] = alpha * x + (1.0f - alpha) * y;
        else                            out[i] = 0.0f;   /* sign conflict */
    }
    free(A); free(B);
}

static void merge_task_arith(const float *a, const float *b, int n,
                             float alpha, const float *base, float *out)
{
    if (!base) { merge_linear(a, b, n, alpha, out); return; }
    for (int i = 0; i < n; ++i) {
        float da = a[i] - base[i];
        float db = b[i] - base[i];
        out[i] = base[i] + alpha * da + (1.0f - alpha) * db;
    }
}

int cos_sigma_merge(const float *a, const float *b, int n,
                    const cos_merge_config_t *cfg, float *out)
{
    if (!a || !b || !out || !cfg || n <= 0) return -1;
    if (!(cfg->alpha >= 0.0f && cfg->alpha <= 1.0f)) return -2;
    switch (cfg->method) {
        case COS_MERGE_LINEAR:     merge_linear(a, b, n, cfg->alpha, out); break;
        case COS_MERGE_SLERP:      merge_slerp (a, b, n, cfg->alpha, out); break;
        case COS_MERGE_TIES:
            if (cfg->ties_top_k < 0) return -3;
            merge_ties(a, b, n, cfg->alpha, cfg->ties_top_k, out);
            break;
        case COS_MERGE_TASK_ARITH:
            if (!cfg->base) return -4;
            merge_task_arith(a, b, n, cfg->alpha, cfg->base, out);
            break;
        default: return -5;
    }
    return 0;
}

int cos_sigma_merge_evaluate(const float *a, const float *b, int n,
                             const cos_merge_config_t *cfg,
                             cos_merge_sigma_oracle_fn oracle, void *oracle_ctx,
                             float *scratch_out,
                             cos_merge_result_t *out)
{
    if (!a || !b || !cfg || !oracle || !scratch_out || !out || n <= 0) return -1;
    int rc = cos_sigma_merge(a, b, n, cfg, scratch_out);
    if (rc != 0) return rc;
    float sa = clamp01f(oracle(a, n, oracle_ctx));
    float sb = clamp01f(oracle(b, n, oracle_ctx));
    float sm = clamp01f(oracle(scratch_out, n, oracle_ctx));
    out->sigma_before_a = sa;
    out->sigma_before_b = sb;
    out->sigma_after    = sm;
    out->best_alpha     = cfg->alpha;
    out->method         = cfg->method;
    float minab = (sa < sb) ? sa : sb;
    out->constructive = (sm < minab) ? 1 : 0;
    return 0;
}

int cos_sigma_merge_alpha_sweep(const float *a, const float *b, int n,
                                cos_merge_method_t method,
                                const float *base,
                                int ties_top_k,
                                const float *alphas, int n_alphas,
                                cos_merge_sigma_oracle_fn oracle,
                                void *oracle_ctx,
                                float *scratch,
                                float *out_best,
                                cos_merge_result_t *out_result)
{
    if (!a || !b || !oracle || !scratch || !out_best || !out_result ||
        n <= 0) return -1;
    static const float canon[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    const float *grid = alphas;
    int g = n_alphas;
    if (!grid || g <= 0) { grid = canon; g = 5; }

    float sa = clamp01f(oracle(a, n, oracle_ctx));
    float sb = clamp01f(oracle(b, n, oracle_ctx));
    float best_sigma = 2.0f;     /* sentinel */
    float best_alpha = 0.5f;
    for (int i = 0; i < g; ++i) {
        float al = grid[i];
        if (!(al >= 0.0f && al <= 1.0f)) continue;
        cos_merge_config_t cfg = {.method = method, .alpha = al,
                                  .base = base, .ties_top_k = ties_top_k};
        int rc = cos_sigma_merge(a, b, n, &cfg, scratch);
        if (rc != 0) continue;
        float sm = clamp01f(oracle(scratch, n, oracle_ctx));
        int replace = 0;
        if (sm < best_sigma - 1e-6f) replace = 1;
        else if (fabsf(sm - best_sigma) <= 1e-6f) {
            /* tie-break: prefer α closest to 0.5 */
            if (fabsf(al - 0.5f) < fabsf(best_alpha - 0.5f)) replace = 1;
        }
        if (replace) {
            best_sigma = sm;
            best_alpha = al;
            memcpy(out_best, scratch, sizeof(float) * (size_t)n);
        }
    }
    float minab = (sa < sb) ? sa : sb;
    out_result->sigma_before_a = sa;
    out_result->sigma_before_b = sb;
    out_result->sigma_after    = best_sigma;
    out_result->best_alpha     = best_alpha;
    out_result->method         = method;
    out_result->constructive   = (best_sigma < minab) ? 1 : 0;
    return 0;
}

/* --------------------- self-test -------------------------------- */

/* Oracle 1: σ = distance to target, normalized. */
typedef struct {
    const float *target;
    float        scale;
} dist_ctx_t;

static float oracle_dist(const float *w, int n, void *ctx) {
    dist_ctx_t *d = (dist_ctx_t *)ctx;
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        double diff = (double)w[i] - (double)d->target[i];
        s += diff * diff;
    }
    float r = (float)(sqrt(s) / (double)d->scale);
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return r;
}

static int check_linear(void) {
    float a[4] = {1, 2, 3, 4}, b[4] = {5, 6, 7, 8};
    float out[4];
    cos_merge_config_t cfg = {.method = COS_MERGE_LINEAR, .alpha = 0.5f};
    if (cos_sigma_merge(a, b, 4, &cfg, out) != 0) return 10;
    for (int i = 0; i < 4; ++i) {
        float exp = 0.5f * a[i] + 0.5f * b[i];
        if (fabsf(out[i] - exp) > 1e-5f) return 11 + i;
    }
    /* α=0 returns b */
    cfg.alpha = 0.0f;
    cos_sigma_merge(a, b, 4, &cfg, out);
    for (int i = 0; i < 4; ++i) if (fabsf(out[i] - b[i]) > 1e-5f) return 20;
    /* α=1 returns a */
    cfg.alpha = 1.0f;
    cos_sigma_merge(a, b, 4, &cfg, out);
    for (int i = 0; i < 4; ++i) if (fabsf(out[i] - a[i]) > 1e-5f) return 21;
    return 0;
}

static int check_slerp(void) {
    /* Parallel vectors → slerp should equal linear (fallback). */
    float a[3] = {1, 0, 0}, b[3] = {2, 0, 0}, out[3];
    cos_merge_config_t cfg = {.method = COS_MERGE_SLERP, .alpha = 0.5f};
    cos_sigma_merge(a, b, 3, &cfg, out);
    if (fabsf(out[0] - 1.5f) > 1e-3f) return 30;
    /* Orthogonal unit vectors at α=0.5 → both 0.707... */
    float u[2] = {1, 0}, v[2] = {0, 1}, m[2];
    cos_sigma_merge(u, v, 2, &cfg, m);
    float expected = (float)(sqrt(2.0) / 2.0);
    if (fabsf(m[0] - expected) > 1e-4f) return 31;
    if (fabsf(m[1] - expected) > 1e-4f) return 32;
    return 0;
}

static int check_ties(void) {
    /* Sign conflict at idx 0 → zero.  Same sign at 1,2 → α-linear. */
    float a[3] = { 1.0f,  2.0f,  3.0f};
    float b[3] = {-1.0f,  4.0f,  6.0f};
    float out[3];
    cos_merge_config_t cfg = {.method = COS_MERGE_TIES, .alpha = 0.5f,
                              .ties_top_k = 3};
    cos_sigma_merge(a, b, 3, &cfg, out);
    if (fabsf(out[0] - 0.0f)  > 1e-5f) return 40;
    if (fabsf(out[1] - 3.0f)  > 1e-5f) return 41;
    if (fabsf(out[2] - 4.5f)  > 1e-5f) return 42;
    return 0;
}

static int check_task_arith(void) {
    float base[2]  = {10, 20};
    float a[2]     = {12, 22};  /* delta_a = +2, +2 */
    float b[2]     = { 8, 24};  /* delta_b = −2, +4 */
    float out[2];
    cos_merge_config_t cfg = {.method = COS_MERGE_TASK_ARITH, .alpha = 0.5f,
                              .base = base};
    cos_sigma_merge(a, b, 2, &cfg, out);
    /* base + 0.5*(+2) + 0.5*(-2) = 10;  base + 0.5*2 + 0.5*4 = 23 */
    if (fabsf(out[0] - 10.0f) > 1e-5f) return 50;
    if (fabsf(out[1] - 23.0f) > 1e-5f) return 51;
    /* reject when base is NULL */
    cos_merge_config_t cfg2 = {.method = COS_MERGE_TASK_ARITH, .alpha = 0.5f};
    if (cos_sigma_merge(a, b, 2, &cfg2, out) != -4) return 52;
    return 0;
}

static int check_evaluate_constructive(void) {
    /* target = midpoint; a,b are symmetric around target, so the
     * halfway merge should beat both.                             */
    float target[4] = {0, 0, 0, 0};
    float a[4] = { 1, 1, 1, 1};
    float b[4] = {-1,-1,-1,-1};
    float out[4];
    dist_ctx_t ctx = {.target = target, .scale = 4.0f};
    cos_merge_config_t cfg = {.method = COS_MERGE_LINEAR, .alpha = 0.5f};
    cos_merge_result_t r;
    if (cos_sigma_merge_evaluate(a, b, 4, &cfg, oracle_dist, &ctx,
                                 out, &r) != 0) return 60;
    if (!r.constructive)                   return 61;
    if (!(r.sigma_after < r.sigma_before_a - 0.1f)) return 62;
    return 0;
}

static int check_evaluate_destructive(void) {
    /* target = (0,0); a=target; b = far.  α=0 = b (bad); α=1 = a (best).
     * Midpoint merge at α=0.5 is WORSE than a alone.              */
    float target[2] = {0, 0};
    float a[2] = {0, 0};
    float b[2] = {2, 2};
    float out[2];
    dist_ctx_t ctx = {.target = target, .scale = 3.0f};
    cos_merge_config_t cfg = {.method = COS_MERGE_LINEAR, .alpha = 0.5f};
    cos_merge_result_t r;
    cos_sigma_merge_evaluate(a, b, 2, &cfg, oracle_dist, &ctx, out, &r);
    if (r.constructive)                    return 70;  /* NOT constructive */
    return 0;
}

static int check_alpha_sweep(void) {
    float target[2] = {0, 0};
    float a[2] = {0, 0};
    float b[2] = {2, 2};
    float out[2], scratch[2];
    dist_ctx_t ctx = {.target = target, .scale = 3.0f};
    cos_merge_result_t r;
    int rc = cos_sigma_merge_alpha_sweep(a, b, 2, COS_MERGE_LINEAR,
                                         NULL, 0, NULL, 0,
                                         oracle_dist, &ctx,
                                         scratch, out, &r);
    if (rc != 0) return 80;
    /* Best α picks pure-a (α=1 → output = a = target). */
    if (fabsf(r.best_alpha - 1.0f) > 1e-5f) return 81;
    if (r.sigma_after > 1e-4f)              return 82;
    /* Strict <: σ_after (0) cannot beat σ_a (0) itself.  The sweep
     * still returns the best vector, just not "constructive". */
    if (r.constructive)                     return 83;
    /* Symmetric constructive case: a=(1,0), b=(-1,0), target=(0,0).
     * At α=0.5 the merged vector is (0,0) → σ = 0, strictly less
     * than min(σ_a, σ_b) = 0.5.                                   */
    float a2[2] = { 1, 0};
    float b2[2] = {-1, 0};
    float target2[2] = {0, 0};
    dist_ctx_t ctx2 = {.target = target2, .scale = 2.0f};
    cos_sigma_merge_alpha_sweep(a2, b2, 2, COS_MERGE_LINEAR, NULL, 0,
                                NULL, 0, oracle_dist, &ctx2,
                                scratch, out, &r);
    if (fabsf(r.best_alpha - 0.5f) > 1e-5f) return 84;
    if (!r.constructive)                    return 85;
    return 0;
}

int cos_sigma_merge_self_test(void) {
    int rc;
    if ((rc = check_linear()))                return rc;
    if ((rc = check_slerp()))                 return rc;
    if ((rc = check_ties()))                  return rc;
    if ((rc = check_task_arith()))            return rc;
    if ((rc = check_evaluate_constructive())) return rc;
    if ((rc = check_evaluate_destructive()))  return rc;
    if ((rc = check_alpha_sweep()))           return rc;
    return 0;
}
