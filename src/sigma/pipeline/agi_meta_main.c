/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* AGI-5 — domain σ table + trend from ~/.cos/meta_sigma.jsonl (optional). */

#include "meta.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AGI_META_OBS_CAP 256

typedef struct {
    char  domain[COS_META_DOMAIN_CAP];
    int   week;
    float sigma;
} agi_meta_row_t;

static int resolve_meta_path(char *out, size_t cap) {
    const char *e = getenv("CREATION_OS_META_SIGMA_JSONL");
    if (e != NULL && e[0] != '\0')
        return snprintf(out, cap, "%s", e) >= (int)cap ? -1 : 0;
    const char *h = getenv("HOME");
    if (h == NULL || h[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        h = (pw != NULL) ? pw->pw_dir : NULL;
    }
    if (h == NULL) return -1;
    return snprintf(out, cap, "%s/.cos/meta_sigma.jsonl", h) >= (int)cap ? -1 : 0;
}

static const char *skip_to_colon_val(const char *p) {
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Extract "key": "value" string value into out (empty if missing). */
static int json_string_val(const char *line, const char *key, char *out, size_t cap) {
    char tag[48];
    snprintf(tag, sizeof tag, "\"%s\"", key);
    const char *p = strstr(line, tag);
    if (!p) {
        out[0] = '\0';
        return -1;
    }
    p = strchr(p + strlen(tag), '"');
    if (!p) {
        out[0] = '\0';
        return -1;
    }
    p++;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap) out[j++] = *p++;
    out[j] = '\0';
    return (int)j;
}

static int json_float_val(const char *line, const char *key, float *out) {
    char tag[48];
    snprintf(tag, sizeof tag, "\"%s\"", key);
    const char *p = strstr(line, tag);
    if (!p) return -1;
    p = skip_to_colon_val(p);
    if (!p) return -1;
    *out = strtof(p, NULL);
    return 0;
}

static int json_int_val(const char *line, const char *key, int *out) {
    char tag[48];
    snprintf(tag, sizeof tag, "\"%s\"", key);
    const char *p = strstr(line, tag);
    if (!p) return -1;
    p = skip_to_colon_val(p);
    if (!p) return -1;
    *out = (int)strtol(p, NULL, 10);
    return 0;
}

static int load_rows(const char *path, agi_meta_row_t *rows, int cap, int *out_n) {
    *out_n = 0;
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    char *line = NULL;
    size_t lcap = 0;
    ssize_t got;
    while ((got = getline(&line, &lcap, fp)) > 0 && *out_n < cap) {
        char dom[COS_META_DOMAIN_CAP];
        if (json_string_val(line, "domain", dom, sizeof dom) < 0) continue;
        float s = 0.0f;
        if (json_float_val(line, "sigma", &s) != 0) continue;
        int wk = 0;
        (void)json_int_val(line, "week", &wk);
        agi_meta_row_t *r = &rows[*out_n];
        snprintf(r->domain, sizeof r->domain, "%s", dom);
        r->week  = wk;
        r->sigma = s;
        (*out_n)++;
    }
    free(line);
    fclose(fp);
    return 0;
}

static void seed_spec_demo(cos_meta_store_t *store, cos_meta_domain_t *slots, int cap) {
    cos_sigma_meta_init(store, slots, cap);
    cos_sigma_meta_observe(store, "factual_qa",  0.18f);
    cos_sigma_meta_observe(store, "code",        0.35f);
    cos_sigma_meta_observe(store, "math",        0.52f);
    cos_sigma_meta_observe(store, "reasoning",   0.71f);
    cos_sigma_meta_observe(store, "commonsense", 0.74f);
}

static int cmd_domains(const char *path) {
    cos_meta_domain_t slots[16];
    cos_meta_store_t  store;
    int nrows = 0;
    agi_meta_row_t rows[AGI_META_OBS_CAP];

    if (path[0] && access(path, R_OK) == 0) {
        cos_sigma_meta_init(&store, slots, (int)(sizeof slots / sizeof slots[0]));
        load_rows(path, rows, AGI_META_OBS_CAP, &nrows);
        for (int i = 0; i < nrows; ++i)
            cos_sigma_meta_observe(&store, rows[i].domain, rows[i].sigma);
        if (nrows == 0)
            seed_spec_demo(&store, slots, (int)(sizeof slots / sizeof slots[0]));
    } else {
        seed_spec_demo(&store, slots, (int)(sizeof slots / sizeof slots[0]));
    }

    cos_sigma_meta_assess(&store);

    printf("sigma-meta --domains (AGI-5)\n");
    for (int i = 0; i < store.count; ++i) {
        const cos_meta_domain_t *d = &store.slots[i];
        const char *flag = " ";
        if (d->competence == COS_META_STRONG)   flag = "+";
        else if (d->competence == COS_META_MODERATE) flag = "=";
        else if (d->competence == COS_META_WEAK)    flag = "-";
        else if (d->competence == COS_META_LIMITED) flag = "!";
        printf("  %-14s  sigma_mean=%.2f  [%s]  %s  trend=%s\n",
               d->domain, (double)d->sigma_mean, flag,
               cos_sigma_meta_competence_name(d->competence),
               cos_sigma_meta_trend_name(d->trend));
    }
    return 0;
}

static int max_week_idx(const agi_meta_row_t *rows, int n) {
    int m = 0;
    for (int i = 0; i < n; ++i)
        if (rows[i].week > m) m = rows[i].week;
    return m;
}

static void print_week_bucket(const agi_meta_row_t *rows, int n, int wk) {
    int any = 0;
    for (int i = 0; i < n; ++i) {
        if (rows[i].week != wk) continue;
        const char *dom = rows[i].domain;
        int leader = 1;
        for (int k = 0; k < i; ++k) {
            if (rows[k].week == wk && strcmp(rows[k].domain, dom) == 0) {
                leader = 0;
                break;
            }
        }
        if (!leader) continue;
        double sum = 0.0;
        int cnt = 0;
        for (int j = 0; j < n; ++j) {
            if (rows[j].week != wk || strcmp(rows[j].domain, dom) != 0) continue;
            sum += (double)rows[j].sigma;
            cnt++;
        }
        if (!any) {
            printf("  Week %d:\n", wk);
            any = 1;
        }
        printf("    %-12s  sigma_mean=%.2f  (n=%d)\n",
               dom, sum / (double)cnt, cnt);
    }
}

static int cmd_trend(const char *path) {
    agi_meta_row_t rows[AGI_META_OBS_CAP];
    int n = 0;

    printf("sigma-meta --trend (AGI-5)\n");
    if (path[0] == '\0' || access(path, R_OK) != 0) {
        printf("  (no readable %s — demo trajectory)\n", path);
        printf("  Week 1: math  sigma_mean=0.52\n");
        printf("  Week 2: math  sigma_mean=0.45  (self-play + distill)\n");
        printf("  Week 3: math  sigma_mean=0.38\n");
        printf("  Week 4: math  sigma_mean=0.31  (now moderate)\n");
        return 0;
    }
    load_rows(path, rows, AGI_META_OBS_CAP, &n);
    if (n == 0) {
        printf("  (empty %s — demo trajectory)\n", path);
        printf("  Week 1: math  sigma_mean=0.52\n");
        printf("  Week 2: math  sigma_mean=0.45  (self-play + distill)\n");
        printf("  Week 3: math  sigma_mean=0.38\n");
        printf("  Week 4: math  sigma_mean=0.31  (now moderate)\n");
        return 0;
    }

    int hi = max_week_idx(rows, n);
    for (int w = 0; w <= hi; ++w) print_week_bucket(rows, n, w);
    return 0;
}

static int self_test(void) {
    if (cos_sigma_meta_self_test() != 0) return 1;
    char tmpl[] = "/tmp/cosagiXXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return 1;
    FILE *w = fdopen(fd, "w");
    if (!w) {
        close(fd);
        return 1;
    }
    fprintf(w, "{\"domain\":\"factual_qa\",\"sigma\":0.19,\"week\":1}\n");
    fprintf(w, "{\"domain\":\"factual_qa\",\"sigma\":0.17,\"week\":2}\n");
    fclose(w);
    agi_meta_row_t rows[8];
    int n = 0;
    if (load_rows(tmpl, rows, 8, &n) != 0 || n != 2) {
        remove(tmpl);
        return 1;
    }
    remove(tmpl);
    return 0;
}

int main(int argc, char **argv) {
    char path[512];
    path[0] = '\0';
    (void)resolve_meta_path(path, sizeof path);

    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return self_test() != 0;
    if (argc >= 2 && strcmp(argv[1], "--domains") == 0)
        return cmd_domains(path);
    if (argc >= 2 && strcmp(argv[1], "--trend") == 0)
        return cmd_trend(path);

    fprintf(stderr, "usage: creation_os_agi_meta {--domains|--trend|--self-test}\n");
    return 2;
}
