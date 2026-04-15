/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

void gda_worldmodel_m4_demo(uint64_t seed)
{
    uint64_t rng = seed;
    uint64_t a[COS_W], b[COS_W];
    cos_hv_random(&rng, a);
    cos_hv_random(&rng, b);
    printf("gda_worldmodel_m4 sigma=%.4f\n", cos_hv_sigma(a, b));
}
