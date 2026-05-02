/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/inference/ternary_engine.h"

#include <stdio.h>
#include <string.h>

static void fill_chain(cos_infer_ternary_layer_packed_t *ch, uint8_t pq[3][32], int32_t scale1)
{
    int    i;
    int    L;
    int8_t sq[64];

    for (L = 0; L < 3; L++) {
        for (i = 0; i < 64; i++)
            sq[i] = (int8_t)((i % 3) - 1);
        memset(pq[L], 0, sizeof(pq[L]));
        cos_infer_ternary_pack_int8(sq, pq[L], 64);
        ch[L].packed_weights = pq[L];
        ch[L].scale_q16      = (L == 1) ? scale1 : (COS_SIGMA_Q16 / 16);
        ch[L].rows           = 8;
        ch[L].cols           = 8;
    }
}

int main(void)
{
    int8_t               wflat[64];
    uint8_t              pk[32];
    int8_t               xin[8];
    int32_t              x32[8];
    int32_t              ypack[8];
    int32_t              yref[8];
    cos_infer_ternary_layer_packed_t layer;
    cos_infer_ternary_layer_packed_t chain[3];
    uint8_t              pq[3][32];
    int32_t              ws[256];
    cos_sigma_q16_state_t gate;
    int32_t              out[32];
    int32_t              outr;
    int32_t              rc;
    int                  i;
    float                sp;

    for (i = 0; i < 64; i++)
        wflat[i] = (int8_t)(((i + 3) % 3) - 1);
    memset(pk, 0, sizeof(pk));
    cos_infer_ternary_pack_int8(wflat, pk, 64);
    sp = cos_infer_ternary_sparsity_packed(pk, 64);
    if (sp < 0.2f) {
        fputs("ternary_packed: expected some zero weights\n", stderr);
        return 1;
    }

    for (i = 0; i < 8; i++) {
        int32_t v;

        v        = (int32_t)(i - 3);
        x32[i]   = v;
        if (v > 127)
            v = 127;
        if (v < -128)
            v = -128;
        xin[i] = (int8_t)v;
    }

    memset(yref, 0, sizeof(yref));
    ternary_matmul(wflat, xin, yref, 8, 8);

    layer.packed_weights = pk;
    layer.scale_q16      = COS_SIGMA_Q16;
    layer.rows           = 8;
    layer.cols           = 8;
    ternary_matmul_packed(pk, x32, ypack, 8, 8, COS_SIGMA_Q16);
    for (i = 0; i < 8; i++) {
        if (ypack[i] != yref[i]) {
            fprintf(stderr, "ternary_packed: row %d pack %d ref %d\n", i, (int)ypack[i],
                    (int)yref[i]);
            return 2;
        }
    }

    fill_chain(chain, pq, COS_SIGMA_Q16 / 16);
    for (i = 0; i < 8; i++)
        x32[i] = (int32_t)i - 2;
    cos_sigma_q16_init(&gate);
    rc = cos_infer_ternary_stack_forward(chain, 3, ws, 256, x32, out, &outr, &gate,
                                        (int32_t)((int64_t)9 * COS_SIGMA_Q16 / 10), 0);
    if (rc != 3) {
        fprintf(stderr, "ternary_stack: expected 3 layers got %d\n", (int)rc);
        return 3;
    }

    fill_chain(chain, pq, COS_SIGMA_Q16 / 16);
    cos_sigma_q16_init(&gate);
    rc = cos_infer_ternary_stack_forward(chain, 3, ws, 256, x32, out, &outr, &gate, 0, 0);
    if (rc != 0) {
        fprintf(stderr, "ternary_stack: expected ABSTAIN at layer 0 (k_raw=0), got %d\n", (int)rc);
        return 4;
    }

    fill_chain(chain, pq, COS_SIGMA_Q16 / 16);
    cos_sigma_q16_init(&gate);
    rc = cos_infer_ternary_stack_forward(chain, 3, ws, 256, x32, out, &outr, &gate,
                                        (int32_t)((int64_t)9 * COS_SIGMA_Q16 / 10),
                                        COS_SIGMA_Q16 / 4);
    if (rc >= 0) {
        fprintf(stderr, "ternary_stack: expected confident early exit (negative rc)\n");
        return 5;
    }

    puts("check_ternary_packed_main: OK");
    return 0;
}
