/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "self_play.h"

#include "challenger.h"
#include "solver.h"

#include <math.h>
#include <string.h>

void v42_self_play_init(v42_self_play_state_t *s, float start_target)
{
    if (!s) {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->target_sigma = start_target;
    s->cumulative_reward = 0.0f;
    s->external_data_requests = 0;
    s->step = 0;
}

void v42_request_external_data(v42_self_play_state_t *st, const char *domain)
{
    if (!st) {
        return;
    }
    (void)domain;
    st->external_data_requests++;
}

int v42_self_play_step(v42_self_play_state_t *st, v42_experience_buffer_t *buf)
{
    if (!st || !buf) {
        return -1;
    }

    challenge_t c;
    memset(&c, 0, sizeof(c));
    if (v42_generate_challenge(st->target_sigma, "reasoning", &c) != 0) {
        return -1;
    }

    solve_result_t r;
    memset(&r, 0, sizeof(r));
    if (v42_solve_with_sigma_reward_lab(&c, &r) != 0) {
        v42_challenge_free(&c);
        return -1;
    }

    v42_buffer_append(buf, c.problem, r.solution, r.reward, &r.sigma, st->target_sigma, st->step);

    st->cumulative_reward = 0.9f * st->cumulative_reward + 0.1f * r.reward;
    if (st->cumulative_reward > 0.6f) {
        st->target_sigma += 0.05f;
    } else if (st->cumulative_reward < 0.2f) {
        st->target_sigma -= 0.05f;
    }
    st->target_sigma = fmaxf(0.1f, fminf(0.9f, st->target_sigma));

    if (st->target_sigma > 0.85f && st->cumulative_reward < 0.1f) {
        v42_request_external_data(st, c.domain ? c.domain : "reasoning");
        st->target_sigma = 0.6f;
    }

    v42_challenge_free(&c);
    v42_solve_result_free(&r);
    st->step++;
    return 0;
}
