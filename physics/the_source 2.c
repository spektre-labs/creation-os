/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 7;
    uint64_t sm[COS_W];
    cos_hv_random(&rng, sm);
    printf("the_source: hv_energy_sigma_self=%.6f\n", cos_hv_sigma(sm, sm));
    return 0;
}
