/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bitnet_sigma.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static float sigma_heuristic_text_only(const char *text)
{
    if (text == NULL || text[0] == '\0')
        return 1.0f;

    size_t n = strlen(text);
    while (n > 0u) {
        unsigned char c = (unsigned char)text[n - 1u];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
            --n;
        else
            break;
    }
    if (n == 0u)
        return 1.0f;

    int qmarks = 0;
    int alnum = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '?')
            ++qmarks;
        if (isalnum((int)c) != 0)
            ++alnum;
    }

    int short_numeric = (n <= 6u && alnum > 0);
    if (short_numeric) {
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)text[i];
            if (isdigit((int)c) == 0 && c != ' ' && c != '\t' && c != '-' && c != '+'
                && c != '.' && c != '/')
                short_numeric = 0;
        }
    }

    float base = 0.18f + (float)n * 0.00035f;
    if (short_numeric != 0)
        base *= 0.35f;
    base += 0.06f * (float)qmarks;

    if (base > 0.95f)
        base = 0.95f;
    return base;
}

float cos_bitnet_sigma_for_local_output(const char *text)
{
    const char *o = getenv("CREATION_OS_BITNET_SIGMA");
    if (o != NULL && o[0] != '\0') {
        char *end = NULL;
        float v = strtof(o, &end);
        if (end != o && v >= 0.0f && v <= 1.0f)
            return v;
    }
    return sigma_heuristic_text_only(text);
}

float cos_bitnet_sigma_for_prompt_and_output(const char *prompt, const char *text)
{
    /* DEV-8: the bitnet_ppl.c perplexity second-pass that used to live
     * here has been purged.  Real per-token σ now comes from
     * bitnet_server.c (logprobs).  The legacy subprocess path (called
     * from stub_gen.c when CREATION_OS_BITNET_EXE is set) still lands
     * here but falls through to the text-shape heuristic, which is
     * fine for that narrow fallback.
     *
     * The `prompt` argument is kept for API stability: earlier callers
     * used it to feed llama-perplexity the full prompt+completion
     * context.  It is intentionally unused in the heuristic. */
    (void)prompt;

    const char *o = getenv("CREATION_OS_BITNET_SIGMA");
    if (o != NULL && o[0] != '\0') {
        char *end = NULL;
        float v = strtof(o, &end);
        if (end != o && v >= 0.0f && v <= 1.0f)
            return v;
    }

    return sigma_heuristic_text_only(text);
}
