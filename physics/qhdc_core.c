/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 9;
    uint64_t q[COS_W], c[COS_W];
    cos_hv_random(&rng, q);
    cos_hv_random(&rng, c);
    printf("qhdc_core: sigma_q_c=%.4f\n", cos_hv_sigma(q, c));
    return 0;
}
