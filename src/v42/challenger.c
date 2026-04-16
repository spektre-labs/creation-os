/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "challenger.h"

#include "v42_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void v42_challenge_free(challenge_t *c)
{
    if (!c) {
        return;
    }
    free(c->problem);
    free(c->domain);
    c->problem = NULL;
    c->domain = NULL;
}

int v42_generate_challenge(float target_sigma, const char *domain, challenge_t *out)
{
    if (!out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    const char *dom = (domain && domain[0]) ? domain : "reasoning";

    for (int attempt = 0; attempt < 10; attempt++) {
        char buf[1024];
        snprintf(buf, sizeof buf, "v42|domain=%s|target=%g|attempt=%d|salt=%x", dom, (double)target_sigma, attempt,
            (unsigned)(attempt * 1315423911u));
        float e = v42_epistemic_from_text(buf);
        if (fabsf(e - target_sigma) < 0.12f) {
            out->problem = strdup(buf);
            out->domain = strdup(dom);
            if (!out->problem || !out->domain) {
                v42_challenge_free(out);
                return -1;
            }
            out->expected_difficulty = e;
            return 0;
        }
    }

    /* Fall back: accept best-effort last attempt (still deterministic). */
    char buf[1024];
    snprintf(buf, sizeof buf, "v42|domain=%s|target=%g|fallback", dom, (double)target_sigma);
    out->problem = strdup(buf);
    out->domain = strdup(dom);
    if (!out->problem || !out->domain) {
        v42_challenge_free(out);
        return -1;
    }
    out->expected_difficulty = v42_epistemic_from_text(buf);
    return 0;
}
