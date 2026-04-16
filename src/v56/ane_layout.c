/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ane_layout.h"
#include <string.h>

int v56_ane_round_up_spatial(int d)
{
    if (d <= 0) return V56_ANE_SPATIAL_MULT;
    int m = V56_ANE_SPATIAL_MULT;
    int r = d % m;
    int up = (r == 0) ? d : d + (m - r);
    if (up < m) up = m;
    return up;
}

int v56_ane_layout_validate(const v56_ane_layout_t *l)
{
    if (!l) return 0;
    int m = V56_ANE_SPATIAL_MULT;
    int h_ok = (l->spatial_h >= m) && (l->spatial_h % m == 0);
    int w_ok = (l->spatial_w >= m) && (l->spatial_w % m == 0);
    return (h_ok && w_ok) ? 1 : 0;
}

void v56_ane_layout_from_gemm(const v56_gemm_t *in, v56_ane_layout_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!in || in->M <= 0 || in->K <= 0 || in->N <= 0) {
        out->reason = "invalid GEMM dims";
        return;
    }

    /* matmul → 1×1 conv rewrite, NCHW: */
    out->input_shape[0]  = 1;
    out->input_shape[1]  = in->K;
    out->input_shape[2]  = 1;
    out->input_shape[3]  = in->N;

    out->weight_shape[0] = in->M;
    out->weight_shape[1] = in->K;
    out->weight_shape[2] = 1;
    out->weight_shape[3] = 1;

    out->spatial_h = 1;
    out->spatial_w = in->N;

    int m = V56_ANE_SPATIAL_MULT;
    int new_h = v56_ane_round_up_spatial(out->spatial_h);
    int new_w = v56_ane_round_up_spatial(out->spatial_w);

    out->pad_h = new_h - out->spatial_h;
    out->pad_w = new_w - out->spatial_w;

    /* pad bytes, in fp32: we need to grow the (H, W) plane of the
     * input tensor to (new_h, new_w), keeping K channels.  Existing
     * cells = spatial_h × spatial_w; new cells = new_h × new_w.
     * Added cells × K × 4 bytes. */
    long added = (long)new_h * (long)new_w
               - (long)out->spatial_h * (long)out->spatial_w;
    if (added < 0) added = 0;
    long bytes = added * (long)in->K * 4L;
    /* cap at INT_MAX to keep the struct int-clean */
    if (bytes > 2147483647L) bytes = 2147483647L;
    out->pad_bytes_fp32 = (int)bytes;

    out->ane_compatible = v56_ane_layout_validate(out);
    if (out->ane_compatible) {
        out->reason = "ANE-ready: spatial dims satisfy >=16 AND multiple-of-16";
    } else if (out->spatial_h < m) {
        out->reason = "spatial_h < 16 (conv rewrite needs reshape or batching)";
    } else if (out->spatial_w < m) {
        out->reason = "spatial_w < 16 (pad or batch N up to 16)";
    } else {
        out->reason = "spatial dim not a multiple of 16";
    }
}
