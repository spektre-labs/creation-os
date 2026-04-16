/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V42_CHALLENGER_H
#define CREATION_OS_V42_CHALLENGER_H

typedef struct {
    char *problem;
    float expected_difficulty;
    char *domain;
} challenge_t;

void v42_challenge_free(challenge_t *c);

/**
 * Lab challenger: synthesize a short problem string whose epistemic σ is near `target_sigma`.
 * Returns 0 on success (caller frees with v42_challenge_free).
 */
int v42_generate_challenge(float target_sigma, const char *domain, challenge_t *out);

#endif /* CREATION_OS_V42_CHALLENGER_H */
