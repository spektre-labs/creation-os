/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-prioritized compression — scaffold.
 */
#include "compress.h"

#include <stdlib.h>
#include <string.h>

float v53_compress_score(const v53_message_meta_t *m)
{
    if (!m) {
        return 0.0f;
    }
    /* Weights — auditable, tune-able:
     *   +2.0  per resolved-σ moment (learning)
     *   +1.5  if references invariant (creation.md)
     *   +1.0  per tool call made (grounded action)
     *   +peak_sigma (informative difficulty)
     *   -0.5  for pure filler (no tools, σ small, no learning)
     */
    float s = 0.0f;
    s += 2.0f * (float)(m->sigma_resolved ? 1 : 0);
    s += 1.5f * (float)(m->contains_invariant ? 1 : 0);
    s += 1.0f * (float)(m->tool_calls_made > 0 ? m->tool_calls_made : 0);
    s += (m->peak_sigma > 0.0f ? m->peak_sigma : 0.0f);
    if (m->tool_calls_made == 0 && m->peak_sigma < 0.10f &&
        !m->sigma_resolved && !m->contains_invariant) {
        s -= 0.5f;
    }
    return s;
}

v53_compress_decision_t v53_compress_decide(const v53_message_meta_t *m,
                                            float drop_below)
{
    v53_compress_decision_t d = { 0.0f, 1 };
    d.score = v53_compress_score(m);
    d.keep  = (d.score >= drop_below) ? 1 : 0;
    return d;
}

static int v53_cmp_float_asc(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

void v53_compress_batch(const v53_message_meta_t *ms, int n,
                        float target_drop_fraction,
                        v53_compress_decision_t *out)
{
    if (!ms || !out || n <= 0) {
        return;
    }
    if (target_drop_fraction < 0.0f) target_drop_fraction = 0.0f;
    if (target_drop_fraction > 1.0f) target_drop_fraction = 1.0f;

    /* Compute scores. */
    float *scores = (float *)calloc((size_t)n, sizeof(float));
    if (!scores) {
        for (int i = 0; i < n; i++) {
            out[i].score = v53_compress_score(&ms[i]);
            out[i].keep  = 1;
        }
        return;
    }
    for (int i = 0; i < n; i++) {
        scores[i] = v53_compress_score(&ms[i]);
    }

    /* Sort a copy ascending, pick percentile as threshold. */
    float *sorted = (float *)calloc((size_t)n, sizeof(float));
    if (!sorted) {
        for (int i = 0; i < n; i++) {
            out[i].score = scores[i];
            out[i].keep  = 1;
        }
        free(scores);
        return;
    }
    memcpy(sorted, scores, (size_t)n * sizeof(float));
    qsort(sorted, (size_t)n, sizeof(float), v53_cmp_float_asc);

    int drop_count = (int)((float)n * target_drop_fraction + 0.5f);
    if (drop_count < 0) drop_count = 0;
    if (drop_count > n) drop_count = n;
    float threshold = (drop_count > 0 && drop_count < n)
                      ? sorted[drop_count - 1]
                      : ((drop_count == 0) ? sorted[0] - 1.0f
                                           : sorted[n - 1] + 1.0f);

    for (int i = 0; i < n; i++) {
        out[i].score = scores[i];
        /* Keep strictly-greater than threshold; ties survive when we drop 0. */
        out[i].keep  = (scores[i] > threshold) ? 1 : (drop_count == 0 ? 1 : 0);
    }

    free(scores);
    free(sorted);
}
