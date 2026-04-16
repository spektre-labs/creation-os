/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v56 verifier — rule-based process reward model (VPRM-inspired).
 *
 * Framing: "Beyond Outcome Verification: Verifiable Process Reward
 * Models for Structured Reasoning" (arXiv:2601.17223, 2026) argues
 * deterministic, rule-based verifiers are up to 20 %F1 above neural
 * judges because they avoid the false-positive ceiling that limits
 * neural PRMs.  A separate line (ThinkPRM) shows that a generative
 * CoT verifier needs only 1 % of the process labels of a
 * discriminative PRM — i.e., the *rules themselves* carry most of
 * the information.
 *
 * v56 implements that rule-based floor directly: zero learned
 * parameters, zero training labels, deterministic per-step
 * PASS / FAIL, aggregate precision / recall / F1, and a
 * verifier-σ signal equal to (1 − precision) — because the paper
 * explicitly names precision as the higher-priority metric when
 * false positives build an asymptotic ceiling.
 *
 * Scope (tier C): this module is a **policy layer**.  It does not
 * run a model, does not tokenize, does not call out.  Caller
 * supplies steps as opaque strings + an optional per-step score,
 * plus a rule set.  The kernel is reproducible, falsifiable, and
 * NEON-friendly (the hot path is a branchless reduction over a
 * bitmask).
 */
#ifndef CREATION_OS_V56_VERIFIER_H
#define CREATION_OS_V56_VERIFIER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    V56_RULE_NONEMPTY      = 0,  /* step text is non-empty */
    V56_RULE_MIN_LENGTH    = 1,  /* strlen(step.text) >= int_param */
    V56_RULE_MAX_LENGTH    = 2,  /* strlen(step.text) <= int_param */
    V56_RULE_SCORE_BOUNDED = 3,  /* lo <= step.score <= hi */
    V56_RULE_SCORE_MONOTONE= 4,  /* step.score >= previous step.score */
    V56_RULE_NO_EXACT_REPEAT = 5,/* step.text != any prior step.text */
    V56_RULE_CHAR_ENTROPY  = 6,  /* normalized byte-entropy(step.text) <= hi (≠ gibberish) */
    V56_N_RULE_KINDS
} v56_rule_kind_t;

typedef struct {
    v56_rule_kind_t kind;
    float lo;
    float hi;
    int   int_param;
} v56_rule_t;

typedef struct {
    const char *text;
    float score;            /* caller-supplied PRM-style score; 0 if unused */
} v56_step_t;

typedef struct {
    int   n_steps;
    int   n_rules;
    int   n_pass_pairs;     /* rule × step pairs that passed */
    int   n_fail_pairs;
    int   n_steps_fully_passed;  /* steps where EVERY rule passed */
    float precision;        /* pass_pairs / (pass_pairs + fail_pairs) */
    float recall;           /* pass_pairs / total_pairs (total = n_rules × n_steps) */
    float f1;
    float sigma_verifier;   /* 1 − precision (paper: false-positive ceiling) */
    int   pass_overall;     /* 1 iff every (rule, step) passed */
} v56_verify_result_t;

/* Run the full rule set over the full step chain.  Deterministic,
 * single pass, no allocation. */
void v56_verify(const v56_step_t *steps, int n_steps,
                const v56_rule_t *rules, int n_rules,
                v56_verify_result_t *out);

/* Return 1 if rule r passes on step s (given the full chain for
 * context-sensitive rules like NO_EXACT_REPEAT and MONOTONE).
 * Exposed for unit testing individual rules. */
int v56_rule_pass(const v56_rule_t *r,
                  const v56_step_t *steps, int n_steps, int step_idx);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V56_VERIFIER_H */
