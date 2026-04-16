/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "self_verify.h"

#include <stdlib.h>
#include <string.h>

void v41_verification_result_free(verification_result_t *r)
{
    if (!r) {
        return;
    }
    free(r->initial_answer);
    free(r->revised_answer);
    r->initial_answer = NULL;
    r->revised_answer = NULL;
}

int v41_self_verify_from_totals(const char *initial_answer, float initial_sigma, const char *revised_answer,
    float revised_sigma, verification_result_t *out)
{
    if (!out) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (!initial_answer || !revised_answer) {
        return -1;
    }
    out->initial_answer = strdup(initial_answer);
    out->revised_answer = strdup(revised_answer);
    if (!out->initial_answer || !out->revised_answer) {
        v41_verification_result_free(out);
        return -1;
    }
    out->initial_sigma = initial_sigma;
    out->revised_sigma = revised_sigma;
    out->correction_improved = (revised_sigma < initial_sigma) ? 1 : 0;
    return 0;
}
