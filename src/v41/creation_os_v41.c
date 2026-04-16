/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v41 — σ-guided test-time compute lab (budget forcing + BoN + tree)
 *
 * Not merge-gate. Evidence class: lab demo (C). No vendored GSM8K/AIME harness here.
 */
#include "best_of_n.h"
#include "budget_forcing.h"
#include "reasoning_tree.h"
#include "self_verify.h"

#include "../sigma/decompose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_budget_policy(void)
{
    budget_config_t cfg = {.min_think_tokens = 2,
        .max_think_tokens = 16,
        .sigma_continue_threshold = 0.40f,
        .sigma_stop_threshold = 0.35f,
        .wait_tokens_injected = 0};
    if (v41_budget_next_action(0, 1, 0.9f, &cfg) != V41_BF_INJECT_WAIT) {
        fprintf(stderr, "[v41 self-test] FAIL early EOT should inject\n");
        return 1;
    }
    if (v41_budget_next_action(3, 1, 0.9f, &cfg) != V41_BF_INJECT_WAIT) {
        fprintf(stderr, "[v41 self-test] FAIL high epistemic EOT should inject\n");
        return 1;
    }
    if (v41_budget_next_action(4, 1, 0.10f, &cfg) != V41_BF_STOP) {
        fprintf(stderr, "[v41 self-test] FAIL low epistemic EOT should stop\n");
        return 1;
    }
    if (v41_budget_next_action(4, 0, 0.10f, &cfg) != V41_BF_STOP) {
        fprintf(stderr, "[v41 self-test] FAIL confident early stop on non-EOT\n");
        return 1;
    }
    fprintf(stderr, "[v41 self-test] OK budget policy\n");
    return 0;
}

typedef struct {
    int calls;
} mock_eng_t;

static float *mock_forward_logits(void *user, const char *prompt, const char *partial_out, int *vocab_n)
{
    (void)prompt;
    (void)partial_out;
    mock_eng_t *m = (mock_eng_t *)user;
    int v = 8;
    float *L = calloc((size_t)v, sizeof(float));
    if (!L) {
        return NULL;
    }
    /* High epistemic for the first two forwards, then a sharp peak -> low epistemic. */
    for (int i = 0; i < v; i++) {
        if (m->calls < 2) {
            L[i] = (i == 0) ? 2.0f : 0.0f;
        } else {
            L[0] = 12.0f;
            L[1] = -9.0f;
            for (int j = 2; j < v; j++) {
                L[j] = -30.0f;
            }
        }
    }
    m->calls++;
    *vocab_n = v;
    return L;
}

static void mock_free_logits(void *user, float *logits)
{
    (void)user;
    free(logits);
}

static int mock_sample(void *user, const float *logits, int vocab_n)
{
    (void)logits;
    (void)vocab_n;
    mock_eng_t *m = (mock_eng_t *)user;
    /* Always emit end-of-thought token id 5 until budget loop exits. */
    (void)m;
    return 5;
}

static int mock_is_eot(void *user, int tok)
{
    (void)user;
    return tok == 5;
}

static int test_budget_forcing_loop(void)
{
    mock_eng_t st;
    memset(&st, 0, sizeof st);
    cos_v41_engine_t eng = {.user = &st,
        .forward_logits = mock_forward_logits,
        .free_logits = mock_free_logits,
        .sample_token = mock_sample,
        .token_is_end_of_thought = mock_is_eot,
        .wait_token_id = 999};
    budget_config_t cfg = {.min_think_tokens = 2,
        .max_think_tokens = 12,
        /* Disable “high epistemic continue” path so this test only exercises min-think WAITs. */
        .sigma_continue_threshold = 1.0e9f,
        /* After min-think WAITs, allow stop on EOT for any finite toy epistemic. */
        .sigma_stop_threshold = 10.0f,
        .wait_tokens_injected = 0};
    char *out = cos_v41_generate_with_budget_forcing(&eng, "prompt:", &cfg);
    if (!out) {
        fprintf(stderr, "[v41 self-test] FAIL null output\n");
        return 1;
    }
    if (cfg.wait_tokens_injected != 2) {
        fprintf(stderr, "[v41 self-test] FAIL expected exactly 2 min-think WAIT injections (got %d)\n",
            cfg.wait_tokens_injected);
        free(out);
        return 1;
    }
    free(out);
    fprintf(stderr, "[v41 self-test] OK budget forcing loop (waits=%d)\n", cfg.wait_tokens_injected);
    return 0;
}

static int test_best_of_n(void)
{
    if (v41_compute_n_samples(0.1f) != 1) {
        return 1;
    }
    if (v41_compute_n_samples(0.25f) != 3) {
        return 1;
    }
    if (v41_compute_n_samples(0.5f) != 8) {
        return 1;
    }
    if (v41_compute_n_samples(0.7f) != 16) {
        return 1;
    }
    if (v41_compute_n_samples(0.9f) != 0) {
        return 1;
    }
    float s[4] = {0.8f, 0.2f, 0.5f, 0.4f};
    if (v41_argmin_float(s, 4) != 1) {
        return 1;
    }
    fprintf(stderr, "[v41 self-test] OK best-of-N policy\n");
    return 0;
}

static int test_self_verify(void)
{
    verification_result_t r;
    memset(&r, 0, sizeof r);
    if (v41_self_verify_from_totals("x", 0.9f, "y", 0.4f, &r) != 0) {
        return 1;
    }
    if (!r.correction_improved) {
        v41_verification_result_free(&r);
        fprintf(stderr, "[v41 self-test] FAIL expected improvement\n");
        return 1;
    }
    v41_verification_result_free(&r);
    fprintf(stderr, "[v41 self-test] OK self-verify bookkeeping\n");
    return 0;
}

static float branch_sigma(void *ud, const char *parent_text, int branch_idx, int child_depth)
{
    (void)ud;
    (void)parent_text;
    (void)child_depth;
    float scratch_logits[16];
    for (int i = 0; i < 16; i++) {
        scratch_logits[i] = (float)(branch_idx + 1) * 0.05f + (float)i * 0.001f;
    }
    sigma_decomposed_t s;
    sigma_decompose_dirichlet_evidence(scratch_logits, 16, &s);
    return s.epistemic;
}

static int test_reasoning_tree(void)
{
    reasoning_node_t *root = v41_reasoning_node_create("root:uncertain-branching", 0.75f, 0);
    if (!root) {
        return 1;
    }
    v41_reasoning_expand(root, 3, branch_sigma, NULL);
    float best_sigma = 1.0f;
    const char *leaf = v41_reasoning_best_leaf(root, &best_sigma);
    if (!leaf || root->n_children == 0) {
        v41_reasoning_tree_free(root);
        fprintf(stderr, "[v41 self-test] FAIL tree expansion\n");
        return 1;
    }
    if (!(best_sigma < 1.0f)) {
        v41_reasoning_tree_free(root);
        fprintf(stderr, "[v41 self-test] FAIL best sigma\n");
        return 1;
    }
    v41_reasoning_tree_free(root);
    fprintf(stderr, "[v41 self-test] OK reasoning tree (best_sigma=%g)\n", (double)best_sigma);
    return 0;
}

static int self_test(void)
{
    if (test_budget_policy() != 0) {
        return 1;
    }
    if (test_budget_forcing_loop() != 0) {
        return 1;
    }
    if (test_best_of_n() != 0) {
        fprintf(stderr, "[v41 self-test] FAIL best-of-N\n");
        return 1;
    }
    if (test_self_verify() != 0) {
        return 1;
    }
    if (test_reasoning_tree() != 0) {
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
    fprintf(stderr, "creation_os_v41: pass --self-test\n");
    return 2;
}
