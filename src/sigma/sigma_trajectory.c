/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "sigma_trajectory.h"

#include "semantic_entropy.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int next_chunk(const char *in, int *pos, char *out, size_t outcap)
{
    size_t w = 0;
    int    p = *pos;
    while (in[p] == ' ' || in[p] == '\t' || in[p] == '\n' || in[p] == '\r')
        p++;
    if (in[p] == '\0') {
        *pos = p;
        return 0;
    }
    for (; in[p] != '\0' && w + 2 < outcap; ++p) {
        out[w++] = in[p];
        if (in[p] == '.' || in[p] == '!' || in[p] == '?') {
            if (in[p + 1] == '\0' || in[p + 1] == ' ' || in[p + 1] == '\n'
                || in[p + 1] == '\r') {
                ++p;
                break;
            }
        }
    }
    while (w > 0 && (out[w - 1] == ' ' || out[w - 1] == '\t'))
        w--;
    out[w] = '\0';
    *pos   = p;
    return (out[0] != '\0') ? 1 : 0;
}

cos_sigma_trajectory_t cos_measure_trajectory(int port, const char *prompt,
                                              const char *model)
{
    cos_sigma_trajectory_t t;
    char                     work[4096];
    char                     chunk[512];
    int                      pos = 0;
    int                      ncl;
    memset(&t, 0, sizeof t);
    if (prompt == NULL || prompt[0] == '\0')
        return t;
    snprintf(work, sizeof work, "%s", prompt);
    const char *m =
        (model != NULL && model[0] != '\0') ? model : "gemma3:4b";
    while (t.n_steps < 64 && next_chunk(work, &pos, chunk, sizeof chunk) == 1) {
        ncl = 3;
        float se =
            cos_semantic_entropy_ex(chunk, NULL, port, m, 3, &ncl);
        t.sigma_per_step[t.n_steps++] = se;
    }
    if (t.n_steps == 0) {
        ncl = 3;
        float se = cos_semantic_entropy_ex(prompt, NULL, port, m, 3, &ncl);
        t.sigma_per_step[0] = se;
        t.n_steps           = 1;
    }
    t.sigma_final = t.sigma_per_step[t.n_steps - 1];
    t.sigma_max   = t.sigma_per_step[0];
    t.sigma_mean  = 0.f;
    for (int i = 0; i < t.n_steps; ++i) {
        if (t.sigma_per_step[i] > t.sigma_max)
            t.sigma_max = t.sigma_per_step[i];
        t.sigma_mean += t.sigma_per_step[i];
        if (i > 0 && t.sigma_per_step[i] > t.sigma_per_step[i - 1] + 0.12f)
            t.spike_count++;
    }
    t.sigma_mean /= (float)t.n_steps;
    if (t.n_steps >= 2) {
        t.sigma_slope =
            (t.sigma_per_step[t.n_steps - 1] - t.sigma_per_step[0])
            / (float)(t.n_steps - 1);
    } else
        t.sigma_slope = 0.f;
    return t;
}

const char *cos_trajectory_action(const cos_sigma_trajectory_t *t)
{
    if (t == NULL || t->n_steps <= 0)
        return "ABSTAIN";
    if (t->sigma_slope > 0.04f)
        return "ABSTAIN";
    if (t->spike_count >= 2)
        return "RETHINK";
    if (t->sigma_max < 0.35f && t->sigma_mean < 0.30f)
        return "ACCEPT";
    if (t->sigma_max < 0.55f)
        return "RETHINK";
    return "ABSTAIN";
}
