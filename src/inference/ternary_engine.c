/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Ternary {-1,0,+1} row-matrix times int8 column vector.
 * For w in {-1,0,1}, (int16)(w * x) matches add/sub/skip semantics.
 */
#include "ternary_engine.h"

#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define COS_INF_HAVE_NEON 1
#endif

#ifdef __riscv_vector
#include <riscv_vector.h>
#define COS_INF_HAVE_RVV 1
#endif

void ternary_matmul(const int8_t *W, const int8_t *x, int32_t *out, int32_t M,
                    int32_t N)
{
    int32_t i;
    if (!W || !x || !out || M <= 0 || N <= 0)
        return;

#ifdef COS_INF_HAVE_RVV
    if (1) {
        for (i = 0; i < M; i++) {
            int32_t acc   = 0;
            int32_t j     = 0;
            const int8_t *row = W + (int32_t)i * N;
            while (j < N) {
                size_t vl = __riscv_vsetvl_e8m1((uint32_t)(N - j));
                vint8m1_t vw = __riscv_vle8_v_i8m1(row + j, vl);
                vint8m1_t vx = __riscv_vle8_v_i8m1(x + j, vl);
                vint16m2_t prod = __riscv_vwmul_vv_i16m2(vw, vx, vl);
                vint32m1_t acc0 = __riscv_vmv_s_x_i32m1(0, 1);
                acc0 = __riscv_vwredsum_vs_i16m2_i32m1(prod, acc0, vl);
                acc += (int32_t)__riscv_vmv_x_s_i32m1_i32(acc0);
                j += (int32_t)vl;
            }
            out[i] = acc;
        }
        return;
    }
#endif

#ifdef COS_INF_HAVE_NEON
    if (1) {
        for (i = 0; i < M; i++) {
            int32x4_t acc4 = vdupq_n_s32(0);
            int32_t j;
            const int8_t *row = W + (int32_t)i * N;
            for (j = 0; j + 15 < N; j += 16) {
                int8x16_t vw = vld1q_s8(row + j);
                int8x16_t vx = vld1q_s8(x + j);
                int8x16_t pos = vandq_s8(vx, vcgtq_s8(vw, vdupq_n_s8(0)));
                int8x16_t neg = vandq_s8(vx, vcltq_s8(vw, vdupq_n_s8(0)));
                int16x8_t d_lo  = vsubl_s8(vget_low_s8(pos), vget_low_s8(neg));
                int16x8_t d_hi  = vsubl_high_s8(pos, neg);
                acc4 = vaddq_s32(acc4, vpaddlq_s16(d_lo));
                acc4 = vaddq_s32(acc4, vpaddlq_s16(d_hi));
            }
            {
                int32_t acc = vaddvq_s32(acc4);
                for (; j < N; j++) {
                    int8_t w = row[j];
                    if (w == 1)
                        acc += (int32_t)x[j];
                    else if (w == -1)
                        acc -= (int32_t)x[j];
                }
                out[i] = acc;
            }
        }
        return;
    }
#endif

    for (i = 0; i < M; i++) {
        int32_t acc = 0;
        int32_t j;
        const int8_t *row = W + (int32_t)i * N;
        for (j = 0; j < N; j++) {
            int8_t w = row[j];
            if (w == 1)
                acc += (int32_t)x[j];
            else if (w == -1)
                acc -= (int32_t)x[j];
        }
        out[i] = acc;
    }
}

void cos_infer_ternary_pack_int8(const int8_t *w, uint8_t *packed, int32_t n_weights)
{
    int32_t nbytes;
    int32_t i;

    if (!w || !packed || n_weights <= 0)
        return;
    nbytes = (n_weights + 3) / 4;
    memset(packed, 0, (size_t)(unsigned)nbytes);

    for (i = 0; i < n_weights; i++) {
        int32_t shift = (i % 4) * 2;
        int32_t idx   = i / 4;
        uint8_t code  = 0;

        if (w[i] > 0)
            code = 1U;
        else if (w[i] < 0)
            code = 2U;
        packed[idx] = (uint8_t)(packed[idx] | (uint8_t)(code << shift));
    }
}

void ternary_matmul_packed(const uint8_t *packed, const int32_t *x, int32_t *out, int32_t M,
                           int32_t N, int32_t scale_q16)
{
    int32_t r;

    if (!packed || !x || !out || M <= 0 || N <= 0)
        return;

    for (r = 0; r < M; r++) {
        int64_t acc64;
        int32_t c;

        acc64 = 0;
        for (c = 0; c < N; c++) {
            int32_t lin   = r * N + c;
            int32_t bytei = lin / 4;
            int32_t shift = (lin % 4) * 2;
            uint8_t b     = packed[bytei];
            int32_t code  = (int32_t)((b >> shift) & 3);

            if (code == 1)
                acc64 += (int64_t)x[c];
            else if (code == 2)
                acc64 -= (int64_t)x[c];
        }
        {
            int64_t sc;
            int32_t y;

            sc = scale_q16 > 0 ? (int64_t)scale_q16 : (int64_t)COS_SIGMA_Q16;
            y  = (int32_t)((acc64 * sc) >> 16);
            out[r] = y;
        }
    }
}

float cos_infer_ternary_sparsity_packed(const uint8_t *packed, int32_t total_weights)
{
    int32_t zeros;
    int32_t i;

    if (!packed || total_weights <= 0)
        return 0.0f;
    zeros = 0;
    for (i = 0; i < total_weights; i++) {
        int32_t bytei = i / 4;
        int32_t shift = (i % 4) * 2;
        uint8_t b     = packed[bytei];
        int32_t code  = (int32_t)((b >> shift) & 3);

        if (code == 0)
            zeros++;
    }
    return (float)zeros / (float)total_weights;
}

int32_t cos_infer_ternary_layer_sigma_q16(const int32_t *act, int32_t len)
{
    int32_t mx;
    int32_t i;

    if (!act || len <= 0)
        return 0;
    mx = 0;
    for (i = 0; i < len; i++) {
        int32_t v = act[i];

        if (v < 0) {
            if (-v > mx)
                mx = -v;
        } else if (v > mx) {
            mx = v;
        }
    }
    return cos_sigma_q16_from_unit_q16(mx, mx + 4000);
}

int32_t cos_infer_ternary_stack_forward(const cos_infer_ternary_layer_packed_t *layers,
                                        int32_t n_layers, int32_t *workspace,
                                        int32_t workspace_words, const int32_t *input,
                                        int32_t *out_final, int32_t *out_rows,
                                        cos_sigma_q16_state_t *gate, int32_t k_raw_q16,
                                        int32_t confident_exit_sigma_q16)
{
    int32_t dmax;
    int32_t L;
    int32_t *a;
    int32_t *b;
    int32_t prev_rows;

    if (!layers || n_layers <= 0 || !workspace || !input || !out_final || !out_rows || !gate)
        return -1;

    dmax = layers[0].cols;
    for (L = 0; L < n_layers; L++) {
        if (layers[L].rows > dmax)
            dmax = layers[L].rows;
        if (layers[L].cols > dmax)
            dmax = layers[L].cols;
    }
    if (workspace_words < dmax * 2)
        return -2;

    a = workspace;
    b = workspace + dmax;
    memcpy(a, input, (size_t)layers[0].cols * sizeof(int32_t));

    prev_rows = layers[0].cols;
    for (L = 0; L < n_layers; L++) {
        int32_t rows = layers[L].rows;
        int32_t cols = layers[L].cols;
        int32_t sig_q;
        cos_sigma_verdict_t verdict;

        if (cols != prev_rows)
            return -3;

        ternary_matmul_packed(layers[L].packed_weights, a, b, rows, cols, layers[L].scale_q16);
        sig_q = cos_infer_ternary_layer_sigma_q16(b, rows);
        cos_sigma_q16_update(gate, sig_q, k_raw_q16);
        verdict = cos_sigma_mirror_verdict(gate);

        if (confident_exit_sigma_q16 > 0 && sig_q < confident_exit_sigma_q16) {
            memcpy(out_final, b, (size_t)rows * sizeof(int32_t));
            *out_rows = rows;
            return -(L + 1);
        }
        if (verdict == COS_SIGMA_VERDICT_ABSTAIN) {
            memcpy(out_final, b, (size_t)rows * sizeof(int32_t));
            *out_rows = rows;
            return L;
        }
        prev_rows = rows;
        if (L + 1 < n_layers)
            memcpy(a, b, (size_t)rows * sizeof(int32_t));
    }

    memcpy(out_final, b, (size_t)layers[n_layers - 1].rows * sizeof(int32_t));
    *out_rows = layers[n_layers - 1].rows;
    return n_layers;
}
