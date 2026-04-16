/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V42_SELF_PLAY_H
#define CREATION_OS_V42_SELF_PLAY_H

#include "experience_buffer.h"

typedef struct {
    float target_sigma;
    float cumulative_reward;
    int external_data_requests;
    int step;
} v42_self_play_state_t;

void v42_self_play_init(v42_self_play_state_t *s, float start_target);

/** “Limit breaking” hook — lab stub increments a counter (host may fetch external data). */
void v42_request_external_data(v42_self_play_state_t *st, const char *domain);

/** One iteration: challenger → solver → buffer → curriculum update. Returns 0 on success. */
int v42_self_play_step(v42_self_play_state_t *st, v42_experience_buffer_t *buf);

#endif /* CREATION_OS_V42_SELF_PLAY_H */
