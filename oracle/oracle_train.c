/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 4;
    uint64_t w[COS_W], g[COS_W];
    cos_hv_random(&rng, w);
    memcpy(g, w, sizeof(g));
    /* one training step: noise then partial pull toward w */
    cos_hv_random(&rng, g);
    float e0 = cos_hv_sigma(w, g);
    for (int i = 0; i < COS_W; i++)
        g[i] ^= (g[i] ^ w[i]) & 0xFULL;
    float e1 = cos_hv_sigma(w, g);
    printf("oracle_train: sigma before %.4f after %.4f\n", e0, e1);
    return 0;
}
