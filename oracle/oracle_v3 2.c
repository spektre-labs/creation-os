/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 5;
    uint64_t p[COS_W], s[COS_W], v[COS_W];
    cos_hv_random(&rng, p);
    cos_hv_random(&rng, s);
    cos_hv_random(&rng, v);
    printf("oracle_v3 proposer/solver sigma %.4f %.4f\n", cos_hv_sigma(p, s), cos_hv_sigma(s, v));
    return 0;
}
