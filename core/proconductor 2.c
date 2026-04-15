/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os.h"
#include <stdio.h>

void cos_proconductor_pair(uint64_t seed)
{
    uint64_t rng = seed;
    uint64_t m1[COS_W], m2[COS_W];
    cos_hv_random(&rng, m1);
    cos_hv_random(&rng, m2);
    printf("proconductor: sigma(model_a, model_b) = %.4f\n", cos_hv_sigma(m1, m2));
}
