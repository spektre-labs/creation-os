/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v48 — σ-armored red-team lab: anomaly detector + sandbox + defense stack.
 */
#include "defense_layers.h"
#include "sandbox.h"
#include "sigma_anomaly.h"

#include "../sigma/decompose.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *m)
{
    fprintf(stderr, "[v48 self-test] FAIL %s\n", m);
    return 1;
}

static int test_anomaly_confident_volatile(void)
{
    enum { n = 32 };
    sigma_decomposed_t s[n];
    memset(s, 0, sizeof s);
    for (int i = 0; i < n; i++) {
        /* Mostly low epistemic with rare spikes → low mean + high variance (“confident but volatile” lab toy). */
        s[i].epistemic = ((i % 8) == 7) ? 0.55f : 0.02f;
    }
    sigma_anomaly_t a = detect_anomaly(s, n, NULL, 0);
    if (!a.anomaly_detected) {
        return fail("expected anomaly on volatile low-mean epistemic");
    }
    fprintf(stderr, "[v48 self-test] OK anomaly volatile (mean=%g var=%g)\n", (double)a.sigma_mean, (double)a.sigma_variance);
    return 0;
}

static int test_anomaly_baseline_diverge(void)
{
    enum { n = 16 };
    sigma_decomposed_t s[n];
    float base[n];
    memset(s, 0, sizeof s);
    for (int i = 0; i < n; i++) {
        s[i].epistemic = 0.05f * (float)(i + 1);
        base[i] = 0.01f;
    }
    sigma_anomaly_t a = detect_anomaly(s, n, base, n);
    if (!a.anomaly_detected) {
        return fail("expected anomaly when profile diverges from baseline");
    }
    fprintf(stderr, "[v48 self-test] OK anomaly baseline distance=%g\n", (double)a.gap);
    return 0;
}

static int test_sandbox_deny_high_sigma(void)
{
    sandbox_config_t cfg = {0};
    cfg.sigma_threshold_for_action = 0.2f;
    sigma_decomposed_t hi = {0};
    hi.epistemic = 0.9f;
    sandbox_decision_t d = sandbox_check(&cfg, "read /etc/passwd", hi);
    if (d.allowed) {
        sandbox_decision_free(&d);
        return fail("sandbox should deny high epistemic");
    }
    sandbox_decision_free(&d);
    fprintf(stderr, "[v48 self-test] OK sandbox deny\n");
    return 0;
}

static int test_defense_fail_closed(void)
{
    enum { n = 8 };
    sigma_decomposed_t s[n];
    memset(s, 0, sizeof s);
    for (int i = 0; i < n; i++) {
        s[i].epistemic = 0.05f;
    }
    sandbox_config_t cfg = {0};
    cfg.sigma_threshold_for_action = 0.3f;
    defense_result_t r = run_all_defenses("hello", "ok", s, n, &cfg);
    if (!r.final_decision) {
        defense_result_free(&r);
        return fail("expected allow on calm profile");
    }
    defense_result_free(&r);

    defense_result_t r2 = run_all_defenses("", "ok", s, n, &cfg);
    if (r2.final_decision) {
        defense_result_free(&r2);
        return fail("expected block on empty input");
    }
    defense_result_free(&r2);
    fprintf(stderr, "[v48 self-test] OK defense fail-closed\n");
    return 0;
}

static int self_test(void)
{
    if (test_anomaly_confident_volatile() != 0) {
        return 1;
    }
    if (test_anomaly_baseline_diverge() != 0) {
        return 1;
    }
    if (test_sandbox_deny_high_sigma() != 0) {
        return 1;
    }
    if (test_defense_fail_closed() != 0) {
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
    fprintf(stderr, "creation_os_v48: pass --self-test\n");
    return 2;
}
