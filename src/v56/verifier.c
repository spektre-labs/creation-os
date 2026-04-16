/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "verifier.h"

#include <math.h>
#include <string.h>

static int v56_strlen(const char *s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* Byte-level Shannon entropy of the text, normalized by log2(256) = 8.
 * Used as a cheap gibberish / randomness proxy: English text sits
 * around 0.5–0.7; random bytes approach 1.0; short repeats approach 0. */
static float v56_char_entropy_norm(const char *s)
{
    int count[256] = {0};
    int n = 0;
    if (!s) return 0.0f;
    for (int i = 0; s[i]; i++) {
        count[(unsigned char)s[i]]++;
        n++;
    }
    if (n <= 1) return 0.0f;
    double h = 0.0;
    for (int i = 0; i < 256; i++) {
        if (count[i] == 0) continue;
        double p = (double)count[i] / (double)n;
        h -= p * log2(p);
    }
    double norm = h / 8.0;   /* log2(256) = 8 */
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    return (float)norm;
}

int v56_rule_pass(const v56_rule_t *r,
                  const v56_step_t *steps, int n_steps, int step_idx)
{
    if (!r || !steps || step_idx < 0 || step_idx >= n_steps) return 0;
    const v56_step_t *s = &steps[step_idx];
    const char *t = s->text ? s->text : "";
    int slen = v56_strlen(t);

    switch (r->kind) {
    case V56_RULE_NONEMPTY:
        return slen > 0 ? 1 : 0;

    case V56_RULE_MIN_LENGTH:
        return slen >= r->int_param ? 1 : 0;

    case V56_RULE_MAX_LENGTH:
        return slen <= r->int_param ? 1 : 0;

    case V56_RULE_SCORE_BOUNDED:
        return (s->score >= r->lo && s->score <= r->hi) ? 1 : 0;

    case V56_RULE_SCORE_MONOTONE:
        if (step_idx == 0) return 1;  /* first step trivially monotone */
        return s->score >= steps[step_idx - 1].score ? 1 : 0;

    case V56_RULE_NO_EXACT_REPEAT:
        for (int i = 0; i < step_idx; i++) {
            const char *o = steps[i].text ? steps[i].text : "";
            if (strcmp(t, o) == 0) return 0;
        }
        return 1;

    case V56_RULE_CHAR_ENTROPY: {
        float he = v56_char_entropy_norm(t);
        return he <= r->hi ? 1 : 0;
    }

    default:
        return 0;
    }
}

void v56_verify(const v56_step_t *steps, int n_steps,
                const v56_rule_t *rules, int n_rules,
                v56_verify_result_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!steps || !rules || n_steps <= 0 || n_rules <= 0) return;

    int pass_pairs = 0;
    int fail_pairs = 0;
    int steps_fully_passed = 0;

    for (int si = 0; si < n_steps; si++) {
        int all_ok = 1;
        for (int ri = 0; ri < n_rules; ri++) {
            int ok = v56_rule_pass(&rules[ri], steps, n_steps, si);
            if (ok) pass_pairs++;
            else   { fail_pairs++; all_ok = 0; }
        }
        if (all_ok) steps_fully_passed++;
    }

    int total = pass_pairs + fail_pairs;
    out->n_steps = n_steps;
    out->n_rules = n_rules;
    out->n_pass_pairs = pass_pairs;
    out->n_fail_pairs = fail_pairs;
    out->n_steps_fully_passed = steps_fully_passed;
    out->pass_overall = (fail_pairs == 0) ? 1 : 0;
    out->precision = (total > 0) ? ((float)pass_pairs / (float)total) : 0.0f;
    /* recall is defined here against a "perfect verifier" denominator
     * of n_rules × n_steps; for rule-based this equals precision,
     * but we keep the slot open for caller extension. */
    out->recall = out->precision;
    float pr = out->precision;
    float rc = out->recall;
    out->f1 = (pr + rc > 0.0f) ? (2.0f * pr * rc / (pr + rc)) : 0.0f;
    out->sigma_verifier = 1.0f - out->precision;
    if (out->sigma_verifier < 0.0f) out->sigma_verifier = 0.0f;
    if (out->sigma_verifier > 1.0f) out->sigma_verifier = 1.0f;
}
