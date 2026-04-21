/* AGI-1 — ICL composition from engram exemplars. */

#include "inplace_ttt.h"

#include <stdio.h>
#include <string.h>

static int append(char *buf, size_t cap, size_t *off, const char *s) {
    if (buf == NULL || off == NULL || s == NULL) return -1;
    size_t o = *off;
    int n = snprintf(buf + o, cap > o ? cap - o : 0, "%s", s);
    if (n < 0) return -1;
    if ((size_t)n >= cap - o) return -1;
    *off += (size_t)n;
    return 0;
}

int cos_inplace_ttt_compose_from_engram(
    cos_engram_persist_t *persist,
    uint64_t exclude_prompt_hash,
    float exemplar_max_sigma,
    int k_shots,
    const char *user_prompt,
    char *out, size_t out_cap) {
    if (out == NULL || out_cap < 16) return -1;
    if (user_prompt == NULL) {
        out[0] = '\0';
        return 0;
    }
    if (persist == NULL || k_shots <= 0) {
        if (snprintf(out, out_cap, "%s", user_prompt) >= (int)out_cap)
            return -1;
        return 0;
    }

    cos_engram_icl_exemplar_t ex[8];
    int want = k_shots > 8 ? 8 : k_shots;
    int n = cos_engram_persist_fetch_icl_exemplars(
        persist, exclude_prompt_hash, exemplar_max_sigma, want, ex, 8);
    if (n <= 0) {
        if (snprintf(out, out_cap, "%s", user_prompt) >= (int)out_cap)
            return -1;
        return 0;
    }

    size_t off = 0;
    if (append(out, out_cap, &off,
                "Prior local exchanges (low-σ cache). Use as hints; "
                "answer only the final user line.\n\n") != 0)
        return -1;

    for (int i = 0; i < n; ++i) {
        char line[1024];
        int w = snprintf(line, sizeof line,
                         "User: %s\nAssistant: %s\n\n",
                         ex[i].prompt, ex[i].response);
        if (w < 0 || (size_t)w >= sizeof line) return -1;
        if (append(out, out_cap, &off, line) != 0) return -1;
    }
    if (append(out, out_cap, &off, "---\nUser: ") != 0) return -1;
    if (append(out, out_cap, &off, user_prompt) != 0) return -1;
    return 0;
}

int cos_inplace_ttt_self_test(void) {
    char buf[4096];
    if (cos_inplace_ttt_compose_from_engram(NULL, 0u, 0.40f, 3,
                                            "hello", buf, sizeof buf) != 0)
        return -1;
    if (strcmp(buf, "hello") != 0) return -1;
    if (cos_inplace_ttt_compose_from_engram(NULL, 0u, 0.40f, 3,
                                            "x", buf, 1) != -1)
        return -1;
    return 0;
}
