/* Self-test driver for src/sigma/adaptive_compute.c (6+ cases). */
#include "adaptive_compute.h"
#include <stdio.h>
#include <string.h>

#define AC(name, cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", name); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    if (cos_allocate_compute_self_test() != 0)
        return 1;

    {
        struct cos_compute_budget b =
            cos_allocate_compute(0.049f, NULL);
        AC("minimal_k", b.kernels_to_run == 3);
        AC("minimal_re", b.max_rethinks == 0);
        AC("minimal_tt", b.enable_ttt == 0);
        AC("minimal_tm", b.time_budget_ms <= 101.f);
    }

    {
        struct cos_compute_budget b = cos_allocate_compute(0.15f, NULL);
        AC("std_k", b.kernels_to_run == 10);
        AC("std_mr", b.max_rethinks == 1);
    }

    {
        struct cos_compute_budget b = cos_allocate_compute(0.71f, NULL);
        AC("full_k40", b.kernels_to_run == 40);
        AC("full_se", b.enable_search != 0);
        AC("full_mr5", b.max_rethinks == 5);
    }

    return 0;
}
