/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "cos_sigma_mirror.h"
#include "sigma_quantize.h"
#include <stdio.h>
#include <string.h>

int32_t sigma_quant_symmetric_max_level(int8_t bits)
{
    if (bits < 2)
        bits = 2;
    if (bits > 8)
        bits = 8;
    return (1 << (bits - 1)) - 1;
}

void sigma_quant_layer_symmetric(const int32_t *weights_fp, int8_t *weights_q,
                                int32_t n, int8_t bits)
{
    int32_t i;
    int32_t max_abs;
    int32_t levels;
    int64_t scale;
    int32_t lv;
    if (!weights_fp || !weights_q || n <= 0)
        return;
    max_abs = 0;
    for (i = 0; i < n; i++) {
        int32_t aw = weights_fp[i];
        if (aw < 0)
            aw = -aw;
        if (aw > max_abs)
            max_abs = aw;
    }
    levels = sigma_quant_symmetric_max_level(bits);
    if (levels < 1)
        levels = 1;
    scale  = (max_abs > 0) ? ((int64_t)max_abs + levels - 1) / (int64_t)levels : 1;
    if (scale < 1)
        scale = 1;
    for (i = 0; i < n; i++) {
        int64_t q = (int64_t)weights_fp[i] / scale;
        if (q > levels)
            q = levels;
        if (q < -levels)
            q = -levels;
        lv = (int32_t)q;
        if (lv > 127)
            lv = 127;
        if (lv < -128)
            lv = -128;
        weights_q[i] = (int8_t)lv;
    }
}

int32_t sigma_quant_layer_distortion_q16(const int32_t *weights_fp,
                                        const int8_t *weights_q, int32_t n,
                                        int8_t bits)
{
    int32_t i;
    int64_t abs_err;
    int64_t abs_sum;
    int32_t levels;
    int32_t max_abs;
    int64_t scale;
    int64_t d;
    if (!weights_fp || !weights_q || n <= 0)
        return COS_SIGMA_Q16_ONE;
    max_abs = 0;
    for (i = 0; i < n; i++) {
        int32_t aw = weights_fp[i];
        if (aw < 0)
            aw = -aw;
        if (aw > max_abs)
            max_abs = aw;
    }
    levels = sigma_quant_symmetric_max_level(bits);
    if (levels < 1)
        levels = 1;
    scale  = (max_abs > 0) ? ((int64_t)max_abs + levels - 1) / (int64_t)levels : 1;
    abs_err = 0;
    abs_sum = 0;
    for (i = 0; i < n; i++) {
        int64_t recon = (int64_t)weights_q[i] * scale;
        int64_t e     = (int64_t)weights_fp[i] - recon;
        if (e < 0)
            e = -e;
        abs_err += e;
        {
            int64_t u = weights_fp[i];
            if (u < 0)
                u = -u;
            abs_sum += u;
        }
    }
    if (abs_sum < 1)
        abs_sum = 1;
    d = (abs_err * (int64_t)COS_SIGMA_Q16_ONE) / abs_sum;
    if (d < 0)
        d = 0;
    if (d > COS_SIGMA_Q16_ONE - 1)
        d = COS_SIGMA_Q16_ONE - 1;
    return (int32_t)d;
}

void sigma_quant_auto_layers(sigma_quant_config_t *cfg,
                            const int32_t *const *planes, const int32_t *sizes,
                            int32_t n_planes, int32_t max_distortion_q16)
{
    int32_t L;
    int8_t  bit_opts[4];
    int32_t d_q16;
    int8_t  qscratch[4096];
    if (!cfg || !planes || !sizes)
        return;
    if (n_planes <= 0)
        return;
    if (n_planes > COS_SIGMA_QUANT_MAX_LAYERS)
        n_planes = COS_SIGMA_QUANT_MAX_LAYERS;
    memset(cfg, 0, sizeof *cfg);
    cfg->n_layers = n_planes;
    bit_opts[0] = 2;
    bit_opts[1] = 4;
    bit_opts[2] = 8;
    bit_opts[3] = 8;

    for (L = 0; L < n_planes; L++) {
        const int32_t *w;
        int32_t sz;
        int32_t limit;
        int32_t bidx;
        w = planes[L];
        sz = sizes[L];
        if (!w || sz <= 0)
            continue;
        limit = (int32_t)(sizeof qscratch / sizeof qscratch[0]);
        if (sz > limit)
            sz = limit;
        for (bidx = 0; bidx < 4; bidx++) {
            int8_t b = bit_opts[bidx];
            sigma_quant_layer_symmetric(w, qscratch, sz, b);
            d_q16 = sigma_quant_layer_distortion_q16(w, qscratch, sz, b);
            cfg->layer_distortion_q16[L] = d_q16;
            if (d_q16 <= max_distortion_q16 || bidx == 3) {
                cfg->layer_bits[L] = b;
                break;
            }
        }
    }
}

void sigma_quant_print_summary(const sigma_quant_config_t *cfg)
{
    int32_t c2 = 0, c4 = 0, c8 = 0;
    int32_t i;
    if (!cfg)
        return;
    for (i = 0; i < cfg->n_layers; i++) {
        if (cfg->layer_bits[i] <= 2)
            c2++;
        else if (cfg->layer_bits[i] <= 4)
            c4++;
        else
            c8++;
    }
    printf("sigma_quant summary: Q2_layers=%d Q4_layers=%d Q8_layers=%d\n",
           (int)c2, (int)c4, (int)c8);
}
