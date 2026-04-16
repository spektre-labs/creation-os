/* SPDX-License-Identifier: AGPL-3.0-or-later
 * VERIFIED σ-KERNEL (v47 lab)
 *
 * Every exported function carries ACSL contracts that state the intended proof
 * obligations for Frama-C/WP + RTE. Full discharge depends on toolchain model
 * libraries (floats, math.h); CI may SKIP `frama-c` when absent.
 *
 * Proof targets: no undefined behavior on valid inputs, bounded loops, no
 * divide-by-zero on the hot path, structural invariants on outputs.
 */
#include "sigma_kernel_verified.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/*@ requires n >= 1;
    requires n <= V47_VERIFIED_MAX_N;
    requires \valid_read(logits + (0 .. n-1));
    assigns \nothing;
*/
float v47_verified_max_logit(const float *logits, int n)
{
    float maxv = logits[0];
    /*@
      loop invariant 1 <= i <= n;
      loop invariant \valid_read(logits + (0 .. n-1));
      loop assigns i, maxv;
      loop variant n - i;
    */
    for (int i = 1; i < n; i++) {
        if (logits[i] > maxv) {
            maxv = logits[i];
        }
    }
    return maxv;
}

/*@ requires n >= 1;
    requires n <= V47_VERIFIED_MAX_N;
    requires \valid_read(logits + (0 .. n-1));
    assigns \nothing;
*/
static float v47_verified_sumexp_stable(const float *logits, int n, float maxv)
{
    float sum = 0.0f;
    /*@
      loop invariant 0 <= i <= n;
      loop invariant \valid_read(logits + (0 .. n-1));
      loop assigns i, sum;
      loop variant n - i;
    */
    for (int i = 0; i < n; i++) {
        float t = logits[i] - maxv;
        sum += expf(t);
    }
    return sum;
}

/*@ requires n >= 1;
    requires n <= V47_VERIFIED_MAX_N;
    requires \valid_read(logits + (0 .. n-1));
    assigns \nothing;
*/
float v47_verified_softmax_entropy(const float *logits, int n)
{
    float maxv = v47_verified_max_logit(logits, n);
    float sum = v47_verified_sumexp_stable(logits, n, maxv);
    if (!(sum > 0.0f) || !isfinite(sum)) {
        return 0.0f;
    }
    float inv = 1.0f / sum;
    float H = 0.0f;
    /*@
      loop invariant 0 <= i <= n;
      loop assigns i, H;
      loop variant n - i;
    */
    for (int i = 0; i < n; i++) {
        float t = logits[i] - maxv;
        float alpha = expf(t);
        float p = alpha * inv;
        if (p > 1e-12f) {
            H -= p * logf(p);
        }
    }
    if (!isfinite(H) || H < 0.0f) {
        H = 0.0f;
    }
    return H;
}

/*@ requires n >= 2;
    requires n <= V47_VERIFIED_MAX_N;
    requires \valid_read(logits + (0 .. n-1));
    assigns \nothing;
    ensures \result >= 0.0;
    ensures \result <= 1.0;
*/
float v47_verified_top_margin01(const float *logits, int n)
{
    float top1 = logits[0];
    float top2 = logits[1];
    if (top2 > top1) {
        float tmp = top1;
        top1 = top2;
        top2 = tmp;
    }
    /*@
      loop invariant 2 <= i <= n;
      loop invariant top1 >= top2;
      loop assigns i, top1, top2;
      loop variant n - i;
    */
    for (int i = 2; i < n; i++) {
        float v = logits[i];
        if (v > top1) {
            top2 = top1;
            top1 = v;
        } else if (v > top2) {
            top2 = v;
        }
    }
    float margin = top1 - top2;
    if (margin < 0.0f) {
        margin = 0.0f;
    }
    /* Scale logits margin into [0,1] without claiming probabilistic calibration. */
    margin = margin * (1.0f / 32.0f);
    if (margin > 1.0f) {
        margin = 1.0f;
    }
    return margin;
}

/*@ requires n >= 1;
    requires n <= V47_VERIFIED_MAX_N;
    requires \valid_read(logits + (0 .. n-1));
    requires \valid(out);
    assigns *out;
*/
void v47_verified_dirichlet_decompose(const float *logits, int n, sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!logits || n <= 0) {
        return;
    }
    float maxv = v47_verified_max_logit(logits, n);
    float suma = v47_verified_sumexp_stable(logits, n, maxv);
    if (!(suma > 0.0f) || !isfinite(suma)) {
        return;
    }
    float K = (float)n;
    float S = suma;
    out->aleatoric = K / S;
    out->epistemic = K * (K - 1.0f) / (S * (S + 1.0f));
    out->total = out->aleatoric + out->epistemic;
}
