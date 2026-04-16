/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "solver.h"

#include "v42_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void v42_solve_result_free(solve_result_t *r)
{
    if (!r) {
        return;
    }
    free(r->solution);
    r->solution = NULL;
}

static float pair_sim_prefix(const char *a, const char *b)
{
    size_t na = strlen(a);
    size_t nb = strlen(b);
    size_t n = na < nb ? na : nb;
    if (n == 0) {
        return 0.0f;
    }
    size_t eq = 0;
    for (size_t i = 0; i < n; i++) {
        if (a[i] == b[i]) {
            eq++;
        }
    }
    return (float)eq / (float)n;
}

float v42_measure_consistency_lab(const char *prompt, int n)
{
    if (!prompt || n <= 1) {
        return 0.0f;
    }
    if (n > 16) {
        n = 16;
    }
    char *ans[16];
    for (int i = 0; i < n; i++) {
        char buf[768];
        snprintf(buf, sizeof buf, "%s|sample=%d", prompt, i);
        ans[i] = strdup(buf);
        if (!ans[i]) {
            for (int j = 0; j < i; j++) {
                free(ans[j]);
            }
            return 0.0f;
        }
    }
    int agreements = 0;
    int pairs = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            pairs++;
            if (pair_sim_prefix(ans[i], ans[j]) > 0.85f) {
                agreements++;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        free(ans[i]);
    }
    if (pairs <= 0) {
        return 0.0f;
    }
    return (float)agreements / (float)pairs;
}

static void decompose_solution(const char *solution, sigma_decomposed_t *out)
{
    enum { N = 32 };
    float log[N];
    v42_scratch_logits_from_text(solution, log, N);
    sigma_decompose_dirichlet_evidence(log, N, out);
}

int v42_solve_with_sigma_reward_lab(const challenge_t *challenge, solve_result_t *out)
{
    if (!challenge || !out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (!challenge->problem) {
        return -1;
    }
    size_t plen = strlen(challenge->problem);
    out->solution = malloc(plen + 32);
    if (!out->solution) {
        return -1;
    }
    if (snprintf(out->solution, plen + 32, "%s::solver", challenge->problem) <= 0) {
        free(out->solution);
        out->solution = NULL;
        return -1;
    }
    out->n_attempts = 1;
    decompose_solution(out->solution, &out->sigma);

    float consistency = v42_measure_consistency_lab(challenge->problem, 5);
    float e = out->sigma.epistemic;

    if (e < 0.3f && consistency > 0.8f) {
        out->reward = 1.0f;
    } else if (e > 0.7f && consistency < 0.4f) {
        out->reward = -1.0f;
    } else if (e > 0.5f && consistency > 0.7f) {
        out->reward = 0.5f;
    } else if (e < 0.3f && consistency < 0.5f) {
        out->reward = -0.5f;
    } else {
        out->reward = 0.0f;
    }
    return 0;
}
