/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v45 — σ-introspection lab (calibration gap, doubt reward, internal probe placeholder)
 *
 * Not merge-gate. No in-tree TruthfulQA introspection scatter plot until harness + weights exist.
 */
#include "clap_sigma.h"
#include "doubt_reward.h"
#include "introspection.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m)
{
    fprintf(stderr, "[v45 self-test] FAIL %s\n", m);
    return 1;
}

static int test_introspection(void)
{
    v45_introspection_state_t s;
    memset(&s, 0, sizeof s);
    v45_measure_introspection_lab("q", "answer text", (float)NAN, &s);
    if (s.calibration_gap < 0.0f || s.calibration_gap > 1.0f + 1e-3f) {
        return fail("calibration_gap range");
    }
    if (s.meta_sigma < 0.0f || !isfinite(s.meta_sigma)) {
        return fail("meta_sigma");
    }
    fprintf(stderr, "[v45 self-test] OK introspection (gap=%g meta=%g)\n", (double)s.calibration_gap, (double)s.meta_sigma);
    return 0;
}

static int test_doubt_reward(void)
{
    float r1 = v45_doubt_reward("x", 1, 0.1f, 0.9f);
    if (fabsf(r1 - 1.0f) > 1e-3f) {
        return fail("doubt_reward correct confident");
    }
    float r2 = v45_doubt_reward("x", 0, 0.8f, 0.9f);
    if (fabsf(r2 - 0.5f) > 1e-3f) {
        return fail("doubt_reward wrong honest");
    }
    float r3 = v45_doubt_reward("x", 0, 0.1f, 0.99f);
    if (fabsf(r3 - (-1.0f)) > 1e-3f) {
        return fail("doubt_reward wrong overconfident");
    }
    fprintf(stderr, "[v45 self-test] OK doubt_reward\n");
    return 0;
}

static int test_clap(void)
{
    v45_internal_probe_t p;
    memset(&p, 0, sizeof p);
    v45_probe_internals_lab(0xC0FFEEu, 12, &p);
    if (p.sigma_internal < 0.0f || !isfinite(p.sigma_internal)) {
        return fail("sigma_internal");
    }
    if (p.layer_agreement <= 0.0f || !isfinite(p.layer_agreement)) {
        return fail("layer_agreement");
    }
    fprintf(stderr, "[v45 self-test] OK clap_sigma lab probe\n");
    return 0;
}

static int self_test(void)
{
    if (test_introspection() != 0) {
        return 1;
    }
    if (test_doubt_reward() != 0) {
        return 1;
    }
    if (test_clap() != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    if (argv[1] && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    fprintf(stderr, "creation_os_v45: pass --self-test\n");
    return 2;
}
