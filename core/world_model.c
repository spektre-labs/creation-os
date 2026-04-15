#include "world_model.h"

#include "facts_store.h"
#include "hdc.h"

#include <math.h>
#include <string.h>

typedef struct {
    char  state[WM_KEY_MAX];
    char  action[WM_KEY_MAX];
    char  next[WM_KEY_MAX];
    float sigma_edge;
    HV    state_hv;
} wm_edge_t;

static wm_edge_t g_wm[WM_MAX_TRANS];
static int       g_wm_n;

static void wm_norm_key(char *dst, size_t dstsz, const char *src) {
    facts_norm_line(dst, src, dstsz);
}

static int wm_find_exact(const char *s, const char *a) {
    for (int i = 0; i < g_wm_n; i++) {
        if (strcmp(g_wm[i].state, s) == 0 && strcmp(g_wm[i].action, a) == 0)
            return i;
    }
    return -1;
}

void world_model_reset(void) {
    memset(g_wm, 0, sizeof g_wm);
    g_wm_n = 0;
}

void world_model_learn(const char *state, const char *action, const char *next_state, float sigma) {
    if (!state || !action || !next_state)
        return;
    char s[WM_KEY_MAX], a[WM_KEY_MAX], n[WM_KEY_MAX];
    wm_norm_key(s, sizeof s, state);
    wm_norm_key(a, sizeof a, action);
    wm_norm_key(n, sizeof n, next_state);
    if (!s[0] || !a[0])
        return;

    int i = wm_find_exact(s, a);
    if (i < 0) {
        if (g_wm_n >= WM_MAX_TRANS)
            return;
        i = g_wm_n++;
    }
    strncpy(g_wm[i].state, s, sizeof g_wm[i].state - 1);
    g_wm[i].state[sizeof g_wm[i].state - 1] = 0;
    strncpy(g_wm[i].action, a, sizeof g_wm[i].action - 1);
    g_wm[i].action[sizeof g_wm[i].action - 1] = 0;
    strncpy(g_wm[i].next, n, sizeof g_wm[i].next - 1);
    g_wm[i].next[sizeof g_wm[i].next - 1] = 0;
    g_wm[i].sigma_edge = sigma;
    g_wm[i].state_hv = hv_encode(g_wm[i].state);
}

void world_model_predict(const char *state_raw, const char *action_raw, wm_prediction_t *out) {
    if (!out)
        return;
    memset(out, 0, sizeof *out);
    strncpy(out->next_state, "UNKNOWN", sizeof out->next_state - 1);
    out->sigma = 1.f;
    out->confidence = 0.f;

    if (!state_raw || !action_raw)
        return;

    char s[WM_KEY_MAX], a[WM_KEY_MAX];
    wm_norm_key(s, sizeof s, state_raw);
    wm_norm_key(a, sizeof a, action_raw);
    if (!s[0] || !a[0])
        return;

    int ex = wm_find_exact(s, a);
    if (ex >= 0) {
        strncpy(out->next_state, g_wm[ex].next, sizeof out->next_state - 1);
        out->next_state[sizeof out->next_state - 1] = 0;
        out->sigma = g_wm[ex].sigma_edge;
        out->confidence = fmaxf(0.f, fminf(1.f, 1.f - out->sigma));
        return;
    }

    HV    q = hv_encode(s);
    int   best_i = -1;
    float best_dot = -2.f;
    for (int i = 0; i < g_wm_n; i++) {
        if (strcmp(g_wm[i].action, a) != 0)
            continue;
        float d = hdc_similarity(q, g_wm[i].state_hv);
        if (d > best_dot) {
            best_dot = d;
            best_i = i;
        }
    }
    if (best_i >= 0 && best_dot >= WM_GEN_HIT_DOT) {
        strncpy(out->next_state, g_wm[best_i].next, sizeof out->next_state - 1);
        out->next_state[sizeof out->next_state - 1] = 0;
        out->sigma = fmaxf(0.f, 1.f - best_dot);
        out->confidence = fmaxf(0.f, fminf(1.f, best_dot));
        return;
    }
}
