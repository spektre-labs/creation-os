/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v39 — σ_hardware lab: memristor-substrate noise term + full σ stack glue (POSIX)
 *
 * Not merge-gate. Ships: scalar σ_hardware estimator + σ_total composition on top of v34 Dirichlet σ.
 */
#include "../sigma/decompose.h"
#include "../sigma/decompose_v39.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(float a, float b)
{
    return fabsf(a - b) < 1e-4f;
}

static int self_test(void)
{
    float logits[4] = {1.0f, 0.0f, -0.5f, -1.0f};
    sigma_decomposed_t sw;
    memset(&sw, 0, sizeof sw);
    sigma_decompose_dirichlet_evidence(logits, 4, &sw);
    if (!(sw.total > 0.0f)) {
        fprintf(stderr, "[v39 self-test] FAIL decompose total\n");
        return 1;
    }

    float hw = sigma_hardware_estimate(10.0f, 9.0f, 300.0f);
    if (!(hw > 0.0f)) {
        fprintf(stderr, "[v39 self-test] FAIL hardware estimate should be > 0\n");
        return 1;
    }

    sigma_full_t full;
    memset(&full, 0, sizeof full);
    sigma_full_from_decomposed(&sw, hw, &full);
    float expect = sw.aleatoric + sw.epistemic + hw;
    if (!nearly_equal(full.total, expect)) {
        fprintf(stderr, "[v39 self-test] FAIL total combine (got %g expect %g)\n", (double)full.total, (double)expect);
        return 1;
    }

    fprintf(stderr, "[v39 self-test] OK sigma_total=%g (ale=%g epi=%g hw=%g)\n", (double)full.total,
        (double)full.aleatoric, (double)full.epistemic, (double)full.hardware);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    if (argv[1] && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    fprintf(stderr, "creation_os_v39: pass --self-test\n");
    return 2;
}
