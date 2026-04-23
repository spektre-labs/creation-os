/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "speed_metrics.h"

#include <ctype.h>
#include <stdio.h>

struct cos_speed_metrics cos_speed_measure(const char *response,
                                           float       wall_ms)
{
    struct cos_speed_metrics m;
    m.tokens_generated        = 0;
    m.wall_time_ms            = wall_ms;
    m.tokens_per_second       = 0.0f;
    m.time_to_first_token_ms  = wall_ms;

    if (response != NULL) {
        int in_word = 0;
        for (const char *p = response; *p != '\0'; ++p) {
            unsigned char c = (unsigned char)*p;
            if (!isspace(c)) {
                if (!in_word) {
                    m.tokens_generated++;
                    in_word = 1;
                }
            } else {
                in_word = 0;
            }
        }
    }

    if (wall_ms > 0.5f && m.tokens_generated > 0)
        m.tokens_per_second =
            (float)m.tokens_generated / (wall_ms / 1000.0f);

    return m;
}

void cos_speed_print(const struct cos_speed_metrics *m)
{
    if (m == NULL)
        return;
    printf("[speed: %d tok | %.0fms | %.1f tok/s]",
           m->tokens_generated, (double)m->wall_time_ms,
           (double)m->tokens_per_second);
}
