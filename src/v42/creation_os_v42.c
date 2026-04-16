/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v42 — σ-guided self-play lab (challenger/solver/replay scaffold)
 *
 * Not merge-gate. Evidence class: lab demo (C). No in-tree GSM8K self-play curve without harness.
 */
#include "challenger.h"
#include "experience_buffer.h"
#include "self_play.h"
#include "solver.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_challenge(void)
{
    challenge_t c;
    memset(&c, 0, sizeof c);
    if (v42_generate_challenge(0.55f, "math", &c) != 0) {
        return 1;
    }
    if (!c.problem || !c.domain) {
        return 1;
    }
    v42_challenge_free(&c);
    fprintf(stderr, "[v42 self-test] OK challenger\n");
    return 0;
}

static int test_solver(void)
{
    challenge_t c;
    memset(&c, 0, sizeof c);
    if (v42_generate_challenge(0.5f, "code", &c) != 0) {
        return 1;
    }
    solve_result_t r;
    memset(&r, 0, sizeof r);
    if (v42_solve_with_sigma_reward_lab(&c, &r) != 0) {
        v42_challenge_free(&c);
        return 1;
    }
    if (!r.solution) {
        v42_challenge_free(&c);
        return 1;
    }
    float cons = v42_measure_consistency_lab(c.problem, 5);
    if (cons < 0.0f || cons > 1.0f + 1e-3f) {
        v42_solve_result_free(&r);
        v42_challenge_free(&c);
        fprintf(stderr, "[v42 self-test] FAIL consistency range\n");
        return 1;
    }
    v42_solve_result_free(&r);
    v42_challenge_free(&c);
    fprintf(stderr, "[v42 self-test] OK solver (consistency=%g)\n", (double)cons);
    return 0;
}

static int test_buffer_sampling(void)
{
    v42_experience_buffer_t b;
    memset(&b, 0, sizeof b);
    v42_buffer_init(&b, 8);
    sigma_decomposed_t s0 = {0};
    v42_buffer_append(&b, "p0", "s0", 0.9f, &s0, 0.5f, 1);
    v42_buffer_append(&b, "p1", "s1", -0.5f, &s0, 0.5f, 2);
    v42_buffer_append(&b, "p2", "s2", 0.1f, &s0, 0.51f, 3);

    v42_experience_t *batch = NULL;
    int n = v42_buffer_sample_batch(&b, 2, 0.5f, &batch);
    if (n != 2 || !batch) {
        v42_buffer_free(&b);
        return 1;
    }
    /* Expect hallucination row (-0.5) to rank highly. */
    int saw_halluc = 0;
    for (int i = 0; i < n; i++) {
        if (fabsf(batch[i].reward + 0.5f) < 1e-3f) {
            saw_halluc = 1;
        }
        v42_experience_free(&batch[i]);
    }
    free(batch);
    v42_buffer_free(&b);
    if (!saw_halluc) {
        fprintf(stderr, "[v42 self-test] FAIL expected prioritized -0.5 row in batch\n");
        return 1;
    }
    fprintf(stderr, "[v42 self-test] OK experience buffer sampling\n");
    return 0;
}

static int test_self_play(void)
{
    v42_self_play_state_t st;
    v42_self_play_init(&st, 0.5f);
    v42_experience_buffer_t buf;
    memset(&buf, 0, sizeof buf);
    v42_buffer_init(&buf, 16);
    for (int i = 0; i < 8; i++) {
        if (v42_self_play_step(&st, &buf) != 0) {
            v42_buffer_free(&buf);
            return 1;
        }
    }
    if (st.step != 8) {
        v42_buffer_free(&buf);
        return 1;
    }
    v42_buffer_free(&buf);
    fprintf(stderr, "[v42 self-test] OK self-play loop (external_requests=%d)\n", st.external_data_requests);
    return 0;
}

static int self_test(void)
{
    if (test_challenge() != 0) {
        return 1;
    }
    if (test_solver() != 0) {
        return 1;
    }
    if (test_buffer_sampling() != 0) {
        return 1;
    }
    if (test_self_play() != 0) {
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
    fprintf(stderr, "creation_os_v42: pass --self-test\n");
    return 2;
}
