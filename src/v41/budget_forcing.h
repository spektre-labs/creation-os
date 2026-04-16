/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v41 lab: σ-guided test-time compute — budget forcing scaffold (not merge-gate).
 *
 * Evidence class: **lab demo (C)** — deterministic policy on σ_epistemic; no in-tree
 * GSM8K/AIME harness row until an evaluator + weights bundle exists.
 */
#ifndef CREATION_OS_V41_BUDGET_FORCING_H
#define CREATION_OS_V41_BUDGET_FORCING_H

typedef struct {
    int min_think_tokens;
    int max_think_tokens;
    float sigma_continue_threshold;
    float sigma_stop_threshold;
    int wait_tokens_injected;
} budget_config_t;

typedef enum {
    V41_BF_APPEND = 0,
    V41_BF_INJECT_WAIT = 1,
    V41_BF_STOP = 2,
} v41_budget_action_t;

/** Policy step: given epistemic σ and whether the sampler emitted an end-of-thought token. */
v41_budget_action_t v41_budget_next_action(int think_tokens, int is_end_of_thought, float sigma_epistemic,
    const budget_config_t *cfg);

typedef struct {
    void *user;
    float *(*forward_logits)(void *user, const char *prompt, const char *partial_out, int *vocab_n);
    void (*free_logits)(void *user, float *logits);
    int (*sample_token)(void *user, const float *logits, int vocab_n);
    int (*token_is_end_of_thought)(void *user, int tok);
    int wait_token_id;
} cos_v41_engine_t;

/**
 * Toy integration loop: decompose logits each step, apply v41_budget_next_action, append or inject WAIT.
 * Caller supplies engine callbacks (stub LM or real host bridge). Output is NUL-terminated; caller frees.
 */
char *cos_v41_generate_with_budget_forcing(cos_v41_engine_t *eng, const char *prompt, budget_config_t *cfg);

#endif /* CREATION_OS_V41_BUDGET_FORCING_H */
