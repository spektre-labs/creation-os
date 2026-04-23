/*
 * Sigma-guided adaptive compute allocation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "adaptive_compute.h"
#include <math.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

struct cos_compute_budget cos_allocate_compute(
    float speculative_sigma,
    const struct cos_spike_engine *spikes)
{
    struct cos_compute_budget b;
    float                     s = speculative_sigma;
    float                     bump = 0.f;

    memset(&b, 0, sizeof(b));

    if (s < 0.f)
        s = 0.f;
    if (s > 1.f)
        s = 1.f;

    if (spikes != NULL && spikes->energy_saved_ratio > 0.5f
        && spikes->total_suppressed > spikes->total_fires)
        bump = 0.02f;

    s = s + bump;
    if (s > 1.f)
        s = 1.f;

    if (s < 0.05f) {
        b.kernels_to_run     = 3;
        b.max_rethinks       = 0;
        b.enable_ttt         = 0;
        b.enable_search      = 0;
        b.enable_multimodal  = 0;
        b.time_budget_ms     = 100.f;
        b.energy_budget_mj   = 0.02f;
    } else if (s < 0.3f) {
        b.kernels_to_run     = 10;
        b.max_rethinks       = 1;
        b.enable_ttt         = 0;
        b.enable_search      = 0;
        b.enable_multimodal  = 0;
        b.time_budget_ms     = 1000.f;
        b.energy_budget_mj   = 1.f;
    } else if (s <= 0.7f) {
        b.kernels_to_run     = 25;
        b.max_rethinks       = 3;
        b.enable_ttt         = 1;
        b.enable_search      = 0;
        b.enable_multimodal  = 1;
        b.time_budget_ms     = 5000.f;
        b.energy_budget_mj   = 10.f;
    } else {
        b.kernels_to_run     = 40;
        b.max_rethinks       = 5;
        b.enable_ttt         = 1;
        b.enable_search      = 1;
        b.enable_multimodal  = 1;
        b.time_budget_ms     = 30000.f;
        b.energy_budget_mj   = 50.f;
    }

    return b;
}

#if CREATION_OS_ENABLE_SELF_TESTS
#include <stdio.h>
static int fail_ac(const char *msg)
{
    fprintf(stderr, "adaptive_compute self-test: %s\n", msg);
    return 1;
}
#endif

int cos_allocate_compute_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_compute_budget b;
    struct cos_spike_engine   sp;

    memset(&sp, 0, sizeof(sp));

    b = cos_allocate_compute(0.02f, NULL);
    if (b.kernels_to_run != 3 || b.max_rethinks != 0 || b.enable_ttt != 0)
        return fail_ac("minimal band");

    b = cos_allocate_compute(0.10f, NULL);
    if (b.kernels_to_run != 10 || b.max_rethinks != 1)
        return fail_ac("standard band");

    b = cos_allocate_compute(0.50f, NULL);
    if (b.kernels_to_run != 25 || b.enable_ttt != 1)
        return fail_ac("heavy band");

    b = cos_allocate_compute(0.85f, NULL);
    if (b.kernels_to_run != 40 || b.enable_search != 1)
        return fail_ac("full band");

    b = cos_allocate_compute(-1.f, NULL);
    if (b.kernels_to_run != 3)
        return fail_ac("clamp low");

    cos_spike_engine_init(&sp);
    sp.total_fires       = 1;
    sp.total_suppressed  = 10;
    sp.energy_saved_ratio = 0.9f;
    b = cos_allocate_compute(0.04f, &sp);
    if (b.kernels_to_run != 10)
        return fail_ac("spike bump crosses band");

    fprintf(stderr, "adaptive_compute self-test: OK\n");
#endif
    return 0;
}
