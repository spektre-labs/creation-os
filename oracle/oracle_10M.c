/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "../core/creation_os.h"
#include <stdio.h>

int main(void)
{
    uint64_t rng = 6;
    uint64_t a[COS_W], b[COS_W];
    cos_hv_random(&rng, a);
    cos_hv_random(&rng, b);
    volatile uint32_t acc = 0;
    int n = 1000000;
    for (int t = 0; t < n; t++) {
        cos_hv_random(&rng, b);
        acc += cos_hv_hamming(a, b);
    }
    printf("oracle_10M: %d sigma_samples acc=%u\n", n, (unsigned)acc);
    return 0;
}
