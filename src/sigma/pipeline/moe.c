/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-MoE primitive implementation + self-test. */

#include "moe.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

const cos_moe_cutoffs_t COS_MOE_CUTOFFS_DEFAULT = {
    .c_quarter        = 0.15f,
    .c_half           = 0.30f,
    .c_three_quarter  = 0.50f,
};

const char *cos_sigma_moe_width_label(cos_moe_width_t w) {
    switch (w) {
        case COS_MOE_WIDTH_Q:  return "Q";
        case COS_MOE_WIDTH_H:  return "H";
        case COS_MOE_WIDTH_TQ: return "TQ";
        case COS_MOE_WIDTH_F:  return "F";
    }
    return "?";
}

float cos_sigma_moe_width_to_frac(cos_moe_width_t w) {
    switch (w) {
        case COS_MOE_WIDTH_Q:  return 0.25f;
        case COS_MOE_WIDTH_H:  return 0.50f;
        case COS_MOE_WIDTH_TQ: return 0.75f;
        case COS_MOE_WIDTH_F:  return 1.00f;
    }
    return 1.00f;
}

static cos_moe_width_t pick_width(float sigma,
                                  const cos_moe_cutoffs_t *c) {
    if (!c) c = &COS_MOE_CUTOFFS_DEFAULT;
    /* NaN σ or out-of-range → FULL (safe default). */
    if (sigma != sigma)         return COS_MOE_WIDTH_F;
    if (!(sigma >= 0.0f))       return COS_MOE_WIDTH_F;
    if (sigma  < c->c_quarter)         return COS_MOE_WIDTH_Q;
    if (sigma  < c->c_half)            return COS_MOE_WIDTH_H;
    if (sigma  < c->c_three_quarter)   return COS_MOE_WIDTH_TQ;
    return COS_MOE_WIDTH_F;
}

/* Finite-safe argmax with tie-break-by-lowest-index. */
static int argmax_finite(const float *logits, int n, float *out_score) {
    int   best_i = -1;
    float best_v = -1e30f;
    for (int i = 0; i < n; ++i) {
        float v = logits[i];
        if (v != v)                    continue;  /* NaN */
        if (v ==  (1.0f / 0.0f))       continue;  /* +inf — allow */
        if (v == -(1.0f / 0.0f))       continue;  /* -inf — skip  */
        if (v > best_v) { best_v = v; best_i = i; }
    }
    if (best_i < 0) {
        if (out_score) *out_score = 0.0f / 0.0f;
        return 0;            /* fall through: expert 0, NaN score  */
    }
    if (out_score) *out_score = best_v;
    return best_i;
}

cos_moe_activation_t cos_sigma_moe_route(
    float sigma,
    const float *router_logits, int num_experts,
    const cos_moe_cutoffs_t *cutoffs)
{
    cos_moe_activation_t out = {0};
    if (!router_logits || num_experts <= 0) {
        out.expert_id  = -1;
        out.width      = COS_MOE_WIDTH_F;
        out.width_frac = 1.00f;
        out.score      = 0.0f / 0.0f;
        return out;
    }
    float score = 0.0f;
    int   idx = argmax_finite(router_logits, num_experts, &score);
    out.expert_id  = idx;
    out.width      = pick_width(sigma, cutoffs);
    out.width_frac = cos_sigma_moe_width_to_frac(out.width);
    out.score      = score;
    return out;
}

/* Partial selection-sort to grab the K largest finite logits.
 * Stable for tie → lowest index. */
int cos_sigma_moe_top_k_route(
    float sigma,
    const float *router_logits, int num_experts,
    int k,
    const cos_moe_cutoffs_t *cutoffs,
    cos_moe_activation_t *out)
{
    if (!router_logits || !out) return -1;
    if (num_experts <= 0)       return -2;
    if (k <= 0)                 return -3;
    if (k > num_experts)        k = num_experts;

    /* Visited mask on the caller stack.  For reasonable expert counts
     * (≤ 256) this is fine; if we ever want K > 256 experts we can
     * switch to a heap. */
    if (num_experts > 1024)     return -4;
    unsigned char visited[1024];
    memset(visited, 0, sizeof visited);

    cos_moe_width_t w = pick_width(sigma, cutoffs);
    float wf = cos_sigma_moe_width_to_frac(w);

    for (int slot = 0; slot < k; ++slot) {
        int best_i = -1;
        float best_v = -1e30f;
        for (int i = 0; i < num_experts; ++i) {
            if (visited[i]) continue;
            float v = router_logits[i];
            if (v != v)                    continue;
            if (v == -(1.0f / 0.0f))       continue;
            if (v > best_v) { best_v = v; best_i = i; }
        }
        if (best_i < 0) return slot;    /* no more finite values */
        visited[best_i] = 1;
        out[slot].expert_id  = best_i;
        out[slot].width      = w;
        out[slot].width_frac = wf;
        out[slot].score      = best_v;
    }
    return k;
}

float cos_sigma_moe_compute_saved(const cos_moe_activation_t *acts,
                                  int n)
{
    if (!acts || n <= 0) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += (1.0 - (double)acts[i].width_frac);
    return (float)(sum / (double)n);
}

/* --- self-test --------------------------------------------------------- */

static int check_width_gate(void) {
    float logits[3] = {0.1f, 0.5f, 0.2f};
    struct { float s; cos_moe_width_t w; float f; } rows[] = {
        {0.00f, COS_MOE_WIDTH_Q,  0.25f},   /* below c_quarter         */
        {0.14f, COS_MOE_WIDTH_Q,  0.25f},
        {0.15f, COS_MOE_WIDTH_H,  0.50f},   /* AT c_quarter → next     */
        {0.29f, COS_MOE_WIDTH_H,  0.50f},
        {0.30f, COS_MOE_WIDTH_TQ, 0.75f},
        {0.49f, COS_MOE_WIDTH_TQ, 0.75f},
        {0.50f, COS_MOE_WIDTH_F,  1.00f},
        {0.99f, COS_MOE_WIDTH_F,  1.00f},
        {1.00f, COS_MOE_WIDTH_F,  1.00f},
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
        cos_moe_activation_t a = cos_sigma_moe_route(
            rows[i].s, logits, 3, NULL);
        if (a.expert_id != 1)           return 100 + (int)i;      /* argmax = 1 */
        if (a.width     != rows[i].w)   return 200 + (int)i;
        if (a.width_frac != rows[i].f)  return 300 + (int)i;
        if (a.score != 0.5f)            return 400 + (int)i;
    }
    return 0;
}

static int check_monotonic(void) {
    /* Width must be monotonically non-decreasing in σ. */
    float logits[2] = {1.0f, 0.0f};
    cos_moe_width_t prev = COS_MOE_WIDTH_Q;
    for (int i = 0; i <= 100; ++i) {
        float s = (float)i / 100.0f;
        cos_moe_activation_t a = cos_sigma_moe_route(s, logits, 2, NULL);
        if ((int)a.width < (int)prev) return 500 + i;
        prev = a.width;
    }
    return 0;
}

static int check_nan_and_degenerate(void) {
    float nan_v = 0.0f / 0.0f;
    float logits[3] = {nan_v, 0.2f, nan_v};

    /* NaN σ → FULL width (safe default). */
    cos_moe_activation_t a = cos_sigma_moe_route(nan_v, logits, 3, NULL);
    if (a.width != COS_MOE_WIDTH_F) return 600;
    if (a.expert_id != 1)           return 601;    /* only finite */

    /* All-NaN logits → expert_id == 0, score == NaN. */
    float all_nan[3] = {nan_v, nan_v, nan_v};
    cos_moe_activation_t b = cos_sigma_moe_route(0.5f, all_nan, 3, NULL);
    if (b.expert_id != 0)           return 602;
    if (b.score == b.score)         return 603;    /* expect NaN */

    /* num_experts == 0 → expert_id == -1, width FULL. */
    cos_moe_activation_t c = cos_sigma_moe_route(0.5f, logits, 0, NULL);
    if (c.expert_id != -1)          return 604;
    if (c.width != COS_MOE_WIDTH_F) return 605;
    return 0;
}

static int check_top_k(void) {
    float logits[5] = {0.1f, 0.9f, 0.3f, 0.7f, 0.5f};
    cos_moe_activation_t acts[5];
    int n = cos_sigma_moe_top_k_route(0.10f, logits, 5, 3, NULL, acts);
    if (n != 3) return 700;

    /* Descending order. */
    if (acts[0].expert_id != 1) return 701;  /* 0.9 */
    if (acts[1].expert_id != 3) return 702;  /* 0.7 */
    if (acts[2].expert_id != 4) return 703;  /* 0.5 */

    /* All three share the σ=0.10 width = Q. */
    for (int i = 0; i < 3; ++i) {
        if (acts[i].width      != COS_MOE_WIDTH_Q) return 710 + i;
        if (acts[i].width_frac != 0.25f)           return 720 + i;
    }

    /* k == N returns all. */
    int n2 = cos_sigma_moe_top_k_route(0.8f, logits, 5, 5, NULL, acts);
    if (n2 != 5) return 730;

    /* k > N clamped. */
    int n3 = cos_sigma_moe_top_k_route(0.8f, logits, 5, 99, NULL, acts);
    if (n3 != 5) return 731;
    return 0;
}

static int check_compute_saved(void) {
    cos_moe_activation_t a[4] = {
        {.width_frac = 0.25f}, {.width_frac = 0.50f},
        {.width_frac = 0.75f}, {.width_frac = 1.00f},
    };
    float saved = cos_sigma_moe_compute_saved(a, 4);
    /* Expected: mean(1 - 0.25, 1 - 0.50, 1 - 0.75, 1 - 1.00)
     *         = (0.75 + 0.50 + 0.25 + 0.00) / 4 = 0.375        */
    float want = 0.375f;
    if (saved < want - 1e-6f || saved > want + 1e-6f) return 800;
    if (cos_sigma_moe_compute_saved(NULL, 4)    != 0.0f) return 801;
    if (cos_sigma_moe_compute_saved(a, 0)       != 0.0f) return 802;
    return 0;
}

static int check_lcg_stress(void) {
    /* 10^4 LCG draws: argmax returned always matches the actual max
     * index; width matches the σ cutoff pick; no crashes. */
    enum { N = 16 };
    uint32_t state = 0xD15EA5Eu;
    for (int it = 0; it < 10000; ++it) {
        float logits[N];
        for (int i = 0; i < N; ++i) {
            state = state * 1664525u + 1013904223u;
            logits[i] = ((float)((state >> 8) & 0xFFFF) / 65535.0f)
                      * 2.0f - 1.0f;
        }
        state = state * 1664525u + 1013904223u;
        float sig = (float)((state >> 8) & 0xFFFF) / 65535.0f;

        cos_moe_activation_t a =
            cos_sigma_moe_route(sig, logits, N, NULL);

        /* Independent argmax. */
        int want = 0;
        for (int i = 1; i < N; ++i)
            if (logits[i] > logits[want]) want = i;

        if (a.expert_id != want) return 900;
        if (a.score < logits[want] - 1e-6f ||
            a.score > logits[want] + 1e-6f) return 901;
        /* width cutoff re-check. */
        cos_moe_width_t w = pick_width(sig, NULL);
        if (a.width != w) return 902;
    }
    return 0;
}

int cos_sigma_moe_self_test(void) {
    int rc;
    if ((rc = check_width_gate()))           return rc;
    if ((rc = check_monotonic()))            return rc;
    if ((rc = check_nan_and_degenerate()))   return rc;
    if ((rc = check_top_k()))                return rc;
    if ((rc = check_compute_saved()))        return rc;
    if ((rc = check_lcg_stress()))           return rc;
    return 0;
}
