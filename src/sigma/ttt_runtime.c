/*
 * ULTRA-8 — TTT runtime (prompt-level fast adaptation for RETHINK).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ttt_runtime.h"

#include "bitnet_server.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float g_active[256];
static int   g_have_active;
static float g_saved_before;
static float g_failed_sigma_ref;
static int   g_applied_turn;
static int   g_reverted_flag;
static int   g_verbose;
static char  g_verbose_line[512];

static uint64_t ttt_fnv1a64(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    uint64_t             h = 14695981039346656037ULL;
    while (p && *p) {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static float uncertain_mass(void) {
    double s = 0.0;
    for (int i = 0; i < 256; ++i) s += (double)g_active[i];
    return (float)(s / 256.0);
}

void cos_ttt_turn_begin(void) {
    memset(g_active, 0, sizeof(g_active));
    g_have_active      = 0;
    g_saved_before     = 1.0f;
    g_failed_sigma_ref = 1.0f;
    g_applied_turn     = 0;
    g_reverted_flag    = 0;
    g_verbose_line[0]  = '\0';
}

void cos_ttt_set_verbose(int on) { g_verbose = on ? 1 : 0; }

char *cos_ttt_last_verbose_line(void) { return g_verbose_line; }

int cos_ttt_episode_applied_flag(void) { return g_applied_turn; }

int cos_ttt_was_reverted(void) { return g_reverted_flag; }

int cos_ttt_should_revert(float new_sigma) {
    return g_have_active && (new_sigma > g_failed_sigma_ref + 1e-5f);
}

float cos_ttt_measure_improvement(float sigma_before, float sigma_after) {
    return sigma_before - sigma_after;
}

int cos_ttt_extract_signal(const char *response, const float *token_sigma,
                           int n_tokens, struct cos_ttt_update *update) {
    if (update == NULL) return -1;
    memset(update, 0, sizeof(*update));
    update->prompt_hash = ttt_fnv1a64(response ? response : "");
    update->sigma_before = g_saved_before;

    if (token_sigma != NULL && n_tokens > 0) {
        for (int b = 0; b < 256; ++b) {
            int start = (int)((int64_t)b * (int64_t)n_tokens / 256);
            int end   = (int)((int64_t)(b + 1) * (int64_t)n_tokens / 256);
            if (start >= n_tokens) {
                update->delta_weights[b] = 0.0f;
                continue;
            }
            if (end > n_tokens) end = n_tokens;
            double acc = 0.0;
            int    cnt = 0;
            for (int t = start; t < end; ++t) {
                acc += (double)token_sigma[t];
                cnt++;
            }
            update->delta_weights[b] =
                cnt > 0 ? (float)(acc / (double)cnt) : 0.0f;
        }
    } else {
        /* Text-only fallback: spread byte statistics across 256 bins. */
        const unsigned char *p =
            (const unsigned char *)(response ? response : "");
        unsigned long counts[256];
        memset(counts, 0, sizeof(counts));
        size_t total = 0;
        for (; *p; ++p) {
            counts[*p]++;
            total++;
        }
        if (total == 0) {
            memset(update->delta_weights, 0, sizeof(update->delta_weights));
            return 0;
        }
        for (int i = 0; i < 256; ++i)
            update->delta_weights[i] = (float)counts[i] / (float)total;
    }
    return 0;
}

int cos_ttt_apply_fast_weights(struct cos_ttt_update *update) {
    if (update == NULL) return -1;
    memcpy(g_active, update->delta_weights, sizeof(g_active));
    g_have_active = 1;
    update->applied = 1;
    g_saved_before = update->sigma_before;
    return 0;
}

int cos_ttt_revert(struct cos_ttt_update *update) {
    (void)update;
    memset(g_active, 0, sizeof(g_active));
    g_have_active   = 0;
    g_reverted_flag = 1;
    return 0;
}

int cos_ttt_prepare_rethink(const char *input, const char *failed_response,
                            float failed_sigma) {
    (void)input;
    float        sig[4096];
    int          n_sig = 0;
    cos_bitnet_server_copy_last_token_sigmas(sig, 4096, &n_sig);

    struct cos_ttt_update up;
    g_saved_before       = failed_sigma;
    g_failed_sigma_ref   = failed_sigma;
    up.sigma_before      = failed_sigma;
    cos_ttt_extract_signal(failed_response, n_sig > 0 ? sig : NULL, n_sig,
                           &up);
    cos_ttt_apply_fast_weights(&up);
    g_applied_turn = 1;

    if (g_verbose) {
        snprintf(g_verbose_line, sizeof(g_verbose_line),
                 "[ttt] uncertain_mass=%.4f sigmas=%d hash=0x%016llx",
                 (double)uncertain_mass(), n_sig,
                 (unsigned long long)up.prompt_hash);
    }
    return 0;
}

size_t cos_ttt_augment_prompt(char *dst, size_t dst_cap,
                              const char *base_prompt) {
    if (dst == NULL || dst_cap < 8) return 0;
    if (!g_have_active || base_prompt == NULL) {
        int n = snprintf(dst, dst_cap, "%s", base_prompt);
        return n > 0 ? (size_t)n : 0;
    }
    float um = uncertain_mass();
    int   n = snprintf(dst, dst_cap,
                         "%s\n\n"
                         "[TTT fast-weight sketch active — uncertain_mass="
                         "%.4f — bias next tokens away from high-σ spans.]",
                         base_prompt, (double)um);
    return n > 0 ? (size_t)n : 0;
}

int cos_ttt_runtime_self_test(void) {
    int fails = 0;

    if (fabsf(cos_ttt_measure_improvement(0.8f, 0.2f) - 0.6f) > 1e-5f)
        fails++;

    struct cos_ttt_update u;
    memset(&u, 0, sizeof(u));
    float tok[] = { 0.9f, 0.1f, 0.5f };
    cos_ttt_extract_signal("hello", tok, 3, &u);
    if (u.delta_weights[0] < 0.0f) fails++;

    cos_ttt_turn_begin();
    cos_ttt_extract_signal(NULL, NULL, 0, &u);
    if (cos_ttt_apply_fast_weights(&u) != 0) fails++;

    char buf[8192];
    cos_ttt_augment_prompt(buf, sizeof(buf), "test prompt");
    if (strstr(buf, "TTT") == NULL) fails++;

    cos_ttt_revert(&u);
    cos_ttt_augment_prompt(buf, sizeof(buf), "x");
    if (strstr(buf, "TTT") != NULL) fails++;

    cos_ttt_turn_begin();
    /* Revert path without active weights should still succeed. */
    if (cos_ttt_revert(NULL) != 0) fails++;

    cos_ttt_turn_begin();
    cos_ttt_prepare_rethink("in", "fail out", 0.5f);
    if (!cos_ttt_should_revert(0.9f))
        fails++;
    cos_ttt_revert(NULL);
    if (cos_ttt_was_reverted() != 1)
        fails++;

    return fails;
}
