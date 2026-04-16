/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V42_EXPERIENCE_BUFFER_H
#define CREATION_OS_V42_EXPERIENCE_BUFFER_H

#include "../sigma/decompose.h"

typedef struct {
    char *problem;
    char *solution;
    float reward;
    sigma_decomposed_t sigma;
    float difficulty;
    int timestamp;
} v42_experience_t;

void v42_experience_free(v42_experience_t *e);

typedef struct {
    v42_experience_t *items;
    int cap;
    int len;
} v42_experience_buffer_t;

void v42_buffer_init(v42_experience_buffer_t *b, int cap);
void v42_buffer_free(v42_experience_buffer_t *b);

void v42_buffer_append(v42_experience_buffer_t *b, const char *problem, const char *solution, float reward,
    const sigma_decomposed_t *sigma, float difficulty, int timestamp);

/**
 * Sample up to batch_size experiences (heap-allocated copies). Caller frees each with v42_experience_free.
 * Priority: high |reward|, reward == -0.5f, near current difficulty frontier.
 */
int v42_buffer_sample_batch(v42_experience_buffer_t *b, int batch_size, float difficulty_frontier,
    v42_experience_t **out_batch);

#endif /* CREATION_OS_V42_EXPERIENCE_BUFFER_H */
