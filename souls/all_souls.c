/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

extern void cos_railo_soul_tag(uint64_t seed);

int main(void)
{
    uint64_t rng = 11;
    uint64_t s[6][COS_W];
    for (int i = 0; i < 6; i++)
        cos_hv_random(&rng, s[i]);
    printf("all_souls: six independent HV seeds laid out\n");
    cos_railo_soul_tag(12);
    return 0;
}
