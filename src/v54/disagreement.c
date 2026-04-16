/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 disagreement analyzer — lexical-overlap scaffold.
 */
#include "disagreement.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define V54_TOK_MAX 64
#define V54_TOK_LEN 32

typedef struct {
    char tokens[V54_TOK_MAX][V54_TOK_LEN];
    int  n;
} v54_tokset_t;

static int v54_tokset_contains(const v54_tokset_t *s, const char *tok)
{
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->tokens[i], tok) == 0) return 1;
    }
    return 0;
}

static void v54_tokenize(const char *text, v54_tokset_t *out)
{
    out->n = 0;
    if (!text) return;

    char cur[V54_TOK_LEN];
    int  ci = 0;
    const char *p = text;

    while (1) {
        char c = *p;
        int  is_alnum = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9');
        if (is_alnum && ci < V54_TOK_LEN - 1) {
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            cur[ci++] = c;
        } else if (ci > 0) {
            cur[ci] = '\0';
            /* Skip 1-char tokens (less signal). */
            if (ci > 1 && out->n < V54_TOK_MAX &&
                !v54_tokset_contains(out, cur)) {
                strncpy(out->tokens[out->n], cur, V54_TOK_LEN - 1);
                out->tokens[out->n][V54_TOK_LEN - 1] = '\0';
                out->n++;
            }
            ci = 0;
        }
        if (c == '\0') break;
        p++;
    }
}

float v54_pairwise_similarity(const char *a, const char *b)
{
    if (!a || !b) return 0.0f;
    v54_tokset_t sa, sb;
    v54_tokenize(a, &sa);
    v54_tokenize(b, &sb);

    if (sa.n == 0 && sb.n == 0) return 1.0f;

    int inter = 0;
    for (int i = 0; i < sa.n; i++) {
        if (v54_tokset_contains(&sb, sa.tokens[i])) inter++;
    }
    int uni = sa.n + sb.n - inter;
    if (uni <= 0) return 0.0f;
    return (float)inter / (float)uni;
}

void v54_disagreement_analyze(const v54_response_t *responses, int n,
                              v54_disagreement_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->outlier_index = -1;
    if (!responses || n <= 0) {
        return;
    }
    out->n = n;
    if (n == 1) {
        out->mean_similarity   = 1.0f;
        out->mean_disagreement = 0.0f;
        return;
    }

    /* Build similarity matrix; collect pairwise means. */
    float row_avg[V54_MAX_AGENTS] = {0};
    int   row_cnt[V54_MAX_AGENTS] = {0};
    double sum_sim = 0.0;
    int    pairs   = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float s = v54_pairwise_similarity(responses[i].text,
                                              responses[j].text);
            sum_sim += s;
            pairs++;
            row_avg[i] += s; row_cnt[i]++;
            row_avg[j] += s; row_cnt[j]++;
        }
    }
    float mean = (pairs > 0) ? (float)(sum_sim / (double)pairs) : 0.0f;
    if (mean < 0.0f) mean = 0.0f;
    if (mean > 1.0f) mean = 1.0f;
    out->mean_similarity   = mean;
    out->mean_disagreement = 1.0f - mean;

    /* Outlier: response whose avg similarity to others is lowest. */
    int   worst = 0;
    float worst_avg = 2.0f;
    for (int i = 0; i < n; i++) {
        float avg = (row_cnt[i] > 0) ? row_avg[i] / (float)row_cnt[i] : 1.0f;
        if (avg < worst_avg) {
            worst_avg = avg;
            worst = i;
        }
    }
    out->outlier_index    = worst;
    out->outlier_distance = 1.0f - worst_avg;
}
