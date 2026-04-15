/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 14;
    uint64_t h[COS_W];
    cos_hv_random(&rng, h);
    printf("gpu_cluster_hv: host_reference_sigma_self=%.6f\n", cos_hv_sigma(h, h));
    return 0;
}
