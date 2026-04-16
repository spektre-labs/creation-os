/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 disagreement analyzer — scaffold.
 *
 * The design note suggests cosine similarity over embeddings. This
 * scaffold uses a **lexical-overlap** heuristic (token Jaccard
 * similarity on lowercased alphanumeric words) so the harness has
 * zero external dependencies and fully deterministic tests. A real
 * runtime plugs in an embedding similarity by replacing
 * v54_pairwise_similarity() with an embedding backend; the rest of
 * the dispatch pipeline is unchanged.
 */
#ifndef CREATION_OS_V54_DISAGREEMENT_H
#define CREATION_OS_V54_DISAGREEMENT_H

#include "dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   n;                       /* number of responses            */
    float mean_similarity;         /* in [0,1]                       */
    float mean_disagreement;       /* 1 − mean_similarity            */
    int   outlier_index;           /* response most distant from others, or -1 */
    float outlier_distance;        /* 1 − avg similarity to others    */
} v54_disagreement_t;

/* Return Jaccard similarity over lowercased alphanumeric tokens. */
float v54_pairwise_similarity(const char *a, const char *b);

/* Fill stats for n responses. n ≤ V54_MAX_AGENTS. */
void v54_disagreement_analyze(const v54_response_t *responses, int n,
                              v54_disagreement_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V54_DISAGREEMENT_H */
