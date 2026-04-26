/* Continual learning cadence — statistics only in this layer. */

#include "continual_learning.h"

#include <string.h>

void cos_continual_learning_tick(cos_predictive_world_t *wm, int query_count)
{
    int i;

    if (wm == NULL || query_count <= 0)
        return;
    /* Medium cadence: gentle decay so unused domains slowly relax. */
    if (query_count % 10 == 0) {
        for (i = 0; i < 16; ++i) {
            if (wm->domain_query_count[i] == 0)
                wm->domain_sigma[i] =
                    0.98f * wm->domain_sigma[i] + 0.02f * 0.45f;
        }
    }
    /* Slow cadence: widen trend window field (placeholder for batch fit). */
    if (query_count % 100 == 0 && wm->trend_window < 4096)
        wm->trend_window += 32;
}

int cos_continual_learning_self_test(void)
{
    cos_predictive_world_t w;
    int                    i;

    cos_predictive_world_init(&w);
    cos_continual_learning_tick(&w, 10);
    for (i = 0; i < 16; ++i) {
        if (w.domain_sigma[i] < 0.f || w.domain_sigma[i] > 1.f)
            return 1;
    }
    cos_continual_learning_tick(&w, 100);
    if (w.trend_window < 64)
        return 2;
    return 0;
}
