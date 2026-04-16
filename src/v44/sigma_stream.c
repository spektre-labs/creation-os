/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_stream.h"

#include <stdio.h>

int v44_stream_format_token_line(const char *token_utf8, const sigma_decomposed_t *s, char *out, size_t cap)
{
    if (!out || cap < 32u) {
        return -1;
    }
    const char *t = token_utf8 ? token_utf8 : "";
    float e = s ? s->epistemic : 0.0f;
    float a = s ? s->aleatoric : 0.0f;
    int n = snprintf(out, cap,
        "data: {\"token\":\"%s\",\"sigma\":{\"e\":%.4f,\"a\":%.4f}}\n\n",
        t, (double)e, (double)a);
    if (n < 0 || (size_t)n >= cap) {
        return -1;
    }
    return n;
}

int v44_stream_format_sigma_alert(const char *alert, const char *action, char *out, size_t cap)
{
    if (!out || cap < 32u) {
        return -1;
    }
    const char *al = alert ? alert : "sigma_alert";
    const char *ac = action ? action : "none";
    int n = snprintf(out, cap, "data: {\"sigma_alert\":\"%s\",\"action\":\"%s\"}\n\n", al, ac);
    if (n < 0 || (size_t)n >= cap) {
        return -1;
    }
    return n;
}
