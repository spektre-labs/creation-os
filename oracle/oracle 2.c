/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 3;
    uint64_t a[COS_W], b[COS_W];
    cos_hv_random(&rng, a);
    cos_hv_random(&rng, b);
    printf("oracle: attention-as-sigma baseline %.6f\n", cos_hv_sigma(a, b));
    return 0;
}
