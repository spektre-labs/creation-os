/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 project-context loader — minimal creation.md parser.
 */
#include "project_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int v53_line_starts_with(const char *p, size_t n, const char *pfx)
{
    size_t m = strlen(pfx);
    if (n < m) {
        return 0;
    }
    return memcmp(p, pfx, m) == 0;
}

static int v53_is_numbered_bullet(const char *p, size_t n)
{
    /* matches "1. " through "9. " at line start */
    if (n < 3) return 0;
    if (p[0] < '1' || p[0] > '9') return 0;
    if (p[1] != '.') return 0;
    if (p[2] != ' ') return 0;
    return 1;
}

static int v53_is_dash_bullet(const char *p, size_t n)
{
    return (n >= 2 && p[0] == '-' && p[1] == ' ') ? 1 : 0;
}

enum { V53_SEC_NONE, V53_SEC_INV, V53_SEC_CONV, V53_SEC_PROFILE };

static void v53_trim_token(char *dst, size_t cap,
                           const char *src, size_t n)
{
    if (!dst || cap == 0) return;
    size_t i = 0;
    while (i < n && (src[i] == ' ' || src[i] == '\t' || src[i] == '|')) i++;
    size_t j = n;
    while (j > i && (src[j-1] == ' ' || src[j-1] == '\t' || src[j-1] == '|')) j--;
    size_t k = j - i;
    if (k >= cap) k = cap - 1;
    memcpy(dst, src + i, k);
    dst[k] = '\0';
}

static int v53_parse_profile_row(const char *line, size_t n,
                                 v53_sigma_profile_row_t *row)
{
    /* Look for "| task | 0.NN | …" style rows. */
    if (n < 3 || line[0] != '|') return 0;
    /* Find first and second '|' after position 0. */
    size_t i = 1;
    while (i < n && line[i] != '|') i++;
    if (i >= n) return 0;
    size_t j = i + 1;
    while (j < n && line[j] != '|') j++;
    if (j >= n) return 0;

    char task[V53_TASK_NAME_MAX];
    v53_trim_token(task, sizeof(task), line + 1, i - 1);
    if (!task[0]) return 0;
    /* Skip header row "| Task type | σ ceiling (total) | Action… |" */
    if (strstr(task, "ask type")) return 0;
    if (task[0] == '-' || task[0] == ':') return 0;

    char num[16];
    v53_trim_token(num, sizeof(num), line + i + 1, j - i - 1);
    char *endp = NULL;
    float v = strtof(num, &endp);
    if (endp == num) return 0;
    if (!(v >= 0.0f && v <= 1.0f)) return 0;

    memset(row, 0, sizeof(*row));
    size_t tlen = strlen(task);
    if (tlen >= V53_TASK_NAME_MAX) tlen = V53_TASK_NAME_MAX - 1;
    memcpy(row->task, task, tlen);
    row->task[tlen] = '\0';
    row->sigma_ceiling = v;
    return 1;
}

void v53_parse_project_context(const char *buf, size_t len,
                               v53_project_context_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!buf || len == 0) return;
    out->ok = 1;

    int section = V53_SEC_NONE;
    size_t i = 0;
    while (i < len) {
        /* find line end */
        size_t j = i;
        while (j < len && buf[j] != '\n') j++;
        size_t nline = j - i;
        const char *p = buf + i;

        if (v53_line_starts_with(p, nline, "## Invariants")) {
            section = V53_SEC_INV;
        } else if (v53_line_starts_with(p, nline, "## Conventions")) {
            section = V53_SEC_CONV;
        } else if (v53_line_starts_with(p, nline, "## σ-profile")) {
            section = V53_SEC_PROFILE;
        } else if (v53_line_starts_with(p, nline, "## ")) {
            section = V53_SEC_NONE;
        } else {
            if (section == V53_SEC_INV && v53_is_numbered_bullet(p, nline)) {
                out->invariants_count++;
            } else if (section == V53_SEC_CONV && v53_is_dash_bullet(p, nline)) {
                out->conventions_count++;
            } else if (section == V53_SEC_PROFILE) {
                if (out->sigma_profile_rows <
                    (int)(sizeof(out->sigma_profile) /
                          sizeof(out->sigma_profile[0]))) {
                    v53_sigma_profile_row_t r;
                    if (v53_parse_profile_row(p, nline, &r)) {
                        out->sigma_profile[out->sigma_profile_rows++] = r;
                    }
                }
            }
        }
        i = (j < len) ? j + 1 : j;
    }
}

void v53_load_project_context(const char *path, v53_project_context_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!path) return;

    FILE *f = fopen(path, "rb");
    if (!f) return;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return; }
    if (sz > (long)(1 << 20)) sz = (1 << 20); /* 1 MiB cap */
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';

    v53_parse_project_context(buf, got, out);
    free(buf);
}
