/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V41_SELF_VERIFY_H
#define CREATION_OS_V41_SELF_VERIFY_H

typedef struct {
    char *initial_answer;
    float initial_sigma;
    char *revised_answer;
    float revised_sigma;
    int correction_improved;
} verification_result_t;

void v41_verification_result_free(verification_result_t *r);

/**
 * Lab hook: compare σ_totals for two string answers (caller measures; no LM inside).
 * Sets correction_improved = (revised < initial) with strict inequality.
 */
int v41_self_verify_from_totals(const char *initial_answer, float initial_sigma, const char *revised_answer,
    float revised_sigma, verification_result_t *out);

#endif /* CREATION_OS_V41_SELF_VERIFY_H */
