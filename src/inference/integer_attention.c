/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Integer attention: QK^T with fixed scale + piecewise exp approximation.
 */
#include "cos_inference_limits.h"
#include "integer_attention.h"
#include <limits.h>
#include <stdint.h>

void integer_attention(const int32_t *Q, const int32_t *K, const int32_t *V,
                      int32_t *out, int32_t seq_len, int32_t head_dim)
{
    int32_t scores[COS_INF_MAX_SCORES];
    int32_t scale = 256; /* ~1/sqrt(64) in Q8 when scores use >>16 later */
    int32_t max_score;
    int64_t sum_exp;
    int32_t i;
    int32_t j;

    if (!Q || !K || !V || !out || seq_len <= 0 || head_dim <= 0
        || seq_len > COS_INF_MAX_SCORES || head_dim > COS_INF_MAX_HEAD_DIM)
        return;

    max_score = INT32_MIN;
    for (i = 0; i < seq_len; i++) {
        int64_t dot = 0;
        for (j = 0; j < head_dim; j++)
            dot += (int64_t)Q[j] * (int64_t)K[i * head_dim + j];
        scores[i] = (int32_t)((dot * scale) >> 16);
        if (scores[i] > max_score)
            max_score = scores[i];
    }

    sum_exp = 0;
    for (i = 0; i < seq_len; i++) {
        int32_t x;
        scores[i] -= max_score;
        x = scores[i];
        scores[i] =
            (1 << 16) + x + (int32_t)(((int64_t)x * (int64_t)x) >> 17);
        if (scores[i] < 1)
            scores[i] = 1;
        sum_exp += (int64_t)scores[i];
    }
    if (sum_exp < 1)
        sum_exp = 1;

    for (j = 0; j < head_dim; j++) {
        int64_t val = 0;
        for (i = 0; i < seq_len; i++)
            val += ((int64_t)scores[i] * (int64_t)V[i * head_dim + j]) / sum_exp;
        out[j] = (int32_t)val;
    }
}
