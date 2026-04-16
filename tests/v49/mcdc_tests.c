/* SPDX-License-Identifier: AGPL-3.0-or-later
 * MC/DC-oriented unit tests for v47 σ-kernel surface + v49 abstention gate.
 */
#include "../../src/v47/sigma_kernel_verified.h"
#include "../../src/v49/sigma_gate.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_entropy_basic(void)
{
    float logits[8];
    for (int i = 0; i < 8; i++) {
        logits[i] = -1.0f - 0.1f * (float)i;
    }
    float H = v47_verified_softmax_entropy(logits, 8);
    assert(isfinite(H));
    assert(H >= 0.0f);
}

static void test_margin_basic(void)
{
    float logits[4] = {-1.0f, -0.5f, -2.0f, -3.0f};
    float m = v47_verified_top_margin01(logits, 4);
    assert(m >= 0.0f && m <= 1.0f);
}

static void test_decomp_sum(void)
{
    float logits[16];
    for (int i = 0; i < 16; i++) {
        logits[i] = (i == 3) ? 1.0f : -0.2f * (float)i;
    }
    sigma_decomposed_t s = {0};
    v47_verified_dirichlet_decompose(logits, 16, &s);
    assert(isfinite(s.total) && isfinite(s.aleatoric) && isfinite(s.epistemic));
    assert(fabsf(s.total - (s.aleatoric + s.epistemic)) < 1e-3f);
}

static void test_gate_mcdc(void)
{
    /* MC/DC on v49_sigma_gate_calibrated_epistemic:
     * Path A: non-finite epistemic -> ABSTAIN
     * Path B: finite but epistemic > threshold -> ABSTAIN
     * Path C: finite and epistemic <= threshold -> EMIT
     */
    assert(v49_sigma_gate_calibrated_epistemic(nanf(""), 0.5f) == V49_ACTION_ABSTAIN);
    assert(v49_sigma_gate_calibrated_epistemic(0.8f, 0.7f) == V49_ACTION_ABSTAIN);
    assert(v49_sigma_gate_calibrated_epistemic(0.5f, 0.7f) == V49_ACTION_EMIT);
}

int main(void)
{
    test_entropy_basic();
    test_margin_basic();
    test_decomp_sum();
    test_gate_mcdc();
    puts("mcdc_tests: OK");
    return 0;
}
