/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "budget_forcing.h"

#include "../sigma/decompose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

v41_budget_action_t v41_budget_next_action(int think_tokens, int is_end_of_thought, float sigma_epistemic,
    const budget_config_t *cfg)
{
    if (!cfg) {
        return V41_BF_STOP;
    }
    if (think_tokens >= cfg->max_think_tokens) {
        return V41_BF_STOP;
    }
    if (is_end_of_thought && think_tokens < cfg->min_think_tokens) {
        return V41_BF_INJECT_WAIT;
    }
    if (is_end_of_thought && sigma_epistemic > cfg->sigma_continue_threshold) {
        return V41_BF_INJECT_WAIT;
    }
    if (!is_end_of_thought && sigma_epistemic < cfg->sigma_stop_threshold && think_tokens >= cfg->min_think_tokens) {
        return V41_BF_STOP;
    }
    if (is_end_of_thought && sigma_epistemic <= cfg->sigma_stop_threshold && think_tokens >= cfg->min_think_tokens) {
        return V41_BF_STOP;
    }
    return V41_BF_APPEND;
}

static int append_tok(char **buf, size_t *cap, size_t *len, int tok)
{
    char tmp[64];
    int n = snprintf(tmp, sizeof tmp, "%d,", tok);
    if (n <= 0 || (size_t)n >= sizeof tmp) {
        return -1;
    }
    size_t need = *len + (size_t)n + 1;
    if (need > *cap) {
        size_t ncap = *cap ? (*cap * 2) : 4096;
        while (ncap < need) {
            ncap *= 2;
        }
        char *nb = realloc(*buf, ncap);
        if (!nb) {
            return -1;
        }
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, tmp, (size_t)n);
    *len += (size_t)n;
    (*buf)[*len] = '\0';
    return 0;
}

char *cos_v41_generate_with_budget_forcing(cos_v41_engine_t *eng, const char *prompt, budget_config_t *cfg)
{
    if (!eng || !cfg || !eng->forward_logits || !eng->free_logits || !eng->sample_token
        || !eng->token_is_end_of_thought) {
        return NULL;
    }

    char *out = calloc(1, 65536);
    if (!out) {
        return NULL;
    }
    size_t cap = 65536;
    size_t len = 0;

    for (int think = 0; think < cfg->max_think_tokens; think++) {
        int vn = 0;
        float *logits = eng->forward_logits(eng->user, prompt, out, &vn);
        if (!logits || vn <= 0) {
            break;
        }
        sigma_decomposed_t s;
        sigma_decompose_dirichlet_evidence(logits, vn, &s);

        int next_tok = eng->sample_token(eng->user, logits, vn);
        int is_eot = eng->token_is_end_of_thought(eng->user, next_tok) ? 1 : 0;

        v41_budget_action_t a = v41_budget_next_action(think, is_eot, s.epistemic, cfg);
        eng->free_logits(eng->user, logits);

        if (a == V41_BF_STOP) {
            break;
        }
        if (a == V41_BF_INJECT_WAIT) {
            cfg->wait_tokens_injected++;
            if (append_tok(&out, &cap, &len, eng->wait_token_id) != 0) {
                break;
            }
            continue;
        }
        if (append_tok(&out, &cap, &len, next_tok) != 0) {
            break;
        }
    }

    return out;
}
