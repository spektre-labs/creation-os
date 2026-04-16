/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "experience_buffer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void v42_experience_free(v42_experience_t *e)
{
    if (!e) {
        return;
    }
    free(e->problem);
    free(e->solution);
    e->problem = NULL;
    e->solution = NULL;
}

void v42_buffer_init(v42_experience_buffer_t *b, int cap)
{
    if (!b) {
        return;
    }
    memset(b, 0, sizeof(*b));
    if (cap <= 0) {
        cap = 16;
    }
    b->cap = cap;
    b->items = calloc((size_t)cap, sizeof(v42_experience_t));
}

void v42_buffer_free(v42_experience_buffer_t *b)
{
    if (!b || !b->items) {
        return;
    }
    for (int i = 0; i < b->len; i++) {
        v42_experience_free(&b->items[i]);
    }
    free(b->items);
    memset(b, 0, sizeof(*b));
}

void v42_buffer_append(v42_experience_buffer_t *b, const char *problem, const char *solution, float reward,
    const sigma_decomposed_t *sigma, float difficulty, int timestamp)
{
    if (!b || !problem || !solution || !sigma) {
        return;
    }
    if (!b->items || b->cap <= 0) {
        v42_buffer_init(b, 16);
    }
    if (b->len >= b->cap) {
        int ncap = b->cap * 2;
        if (ncap < 32) {
            ncap = 32;
        }
        void *nb = realloc(b->items, (size_t)ncap * sizeof(v42_experience_t));
        if (!nb) {
            return;
        }
        b->items = (v42_experience_t *)nb;
        /* Zero the newly allocated tail so no garbage pointers. */
        memset(&b->items[b->cap], 0, (size_t)(ncap - b->cap) * sizeof(v42_experience_t));
        b->cap = ncap;
    }
    v42_experience_t *e = &b->items[b->len];
    memset(e, 0, sizeof(*e));
    e->problem = strdup(problem);
    e->solution = strdup(solution);
    e->reward = reward;
    e->sigma = *sigma;
    e->difficulty = difficulty;
    e->timestamp = timestamp;
    if (e->problem && e->solution) {
        b->len++;
    } else {
        v42_experience_free(e);
        memset(e, 0, sizeof(*e));
    }
}

typedef struct {
    int idx;
    float score;
} pick_t;

static int cmp_pick_desc(const void *a, const void *b)
{
    const pick_t *x = (const pick_t *)a;
    const pick_t *y = (const pick_t *)b;
    if (x->score < y->score) {
        return 1;
    }
    if (x->score > y->score) {
        return -1;
    }
    return 0;
}

int v42_buffer_sample_batch(v42_experience_buffer_t *b, int batch_size, float difficulty_frontier,
    v42_experience_t **out_batch)
{
    if (!b || !out_batch || batch_size <= 0) {
        return 0;
    }
    *out_batch = NULL;
    if (!b->items || b->len <= 0) {
        return 0;
    }
    if (batch_size > b->len) {
        batch_size = b->len;
    }
    pick_t *picks = calloc((size_t)b->len, sizeof(pick_t));
    if (!picks) {
        return 0;
    }
    for (int i = 0; i < b->len; i++) {
        const v42_experience_t *e = &b->items[i];
        float ar = fabsf(e->reward);
        float near = 1.0f - fabsf(e->difficulty - difficulty_frontier);
        if (near < 0.0f) {
            near = 0.0f;
        }
        float halluc = (fabsf(e->reward + 0.5f) < 1e-3f) ? 10.0f : 0.0f;
        float score = 3.0f * ar + halluc + 0.25f * near;
        picks[i].idx = i;
        picks[i].score = score;
    }
    qsort(picks, (size_t)b->len, sizeof(pick_t), cmp_pick_desc);

    v42_experience_t *batch = calloc((size_t)batch_size, sizeof(v42_experience_t));
    if (!batch) {
        free(picks);
        return 0;
    }
    int outn = 0;
    for (int k = 0; k < batch_size; k++) {
        const v42_experience_t *src = &b->items[picks[k].idx];
        batch[outn].problem = strdup(src->problem ? src->problem : "");
        batch[outn].solution = strdup(src->solution ? src->solution : "");
        if (!batch[outn].problem || !batch[outn].solution) {
            for (int t = 0; t <= outn; t++) {
                v42_experience_free(&batch[t]);
            }
            free(batch);
            free(picks);
            return 0;
        }
        batch[outn].reward = src->reward;
        batch[outn].sigma = src->sigma;
        batch[outn].difficulty = src->difficulty;
        batch[outn].timestamp = src->timestamp;
        outn++;
    }
    free(picks);
    *out_batch = batch;
    return outn;
}
