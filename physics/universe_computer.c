/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 8;
    uint64_t u[COS_W];
    cos_hv_random(&rng, u);
    printf("universe_computer: state_bits=%d\n", COS_D);
    (void)u;
    return 0;
}
