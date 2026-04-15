/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 13;
    uint64_t ctx[COS_W], tgt[COS_W];
    cos_hv_random(&rng, ctx);
    cos_hv_random(&rng, tgt);
    float e = cos_hv_sigma(ctx, tgt);
    printf("jepa_bsc: energy_sigma=%.4f\n", e);
    return 0;
}
