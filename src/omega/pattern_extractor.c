/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "pattern_extractor.h"
#include "pattern_keywords.h"

#include "../sigma/engram_episodic.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define COS_PE_MAX_ROWS 512
#define COS_PE_MAX_DOM  32

typedef struct {
    char     domain[64];   /* CSV category (audit) */
    char     kwdom[64];    /* keyword bucket for adaptive τ */
    uint64_t phash;
} graded_row_t;

typedef struct {
    char  domain[64];
    double sum_sigma;
    int   n_sigma;
    int   n_correct;
    int   n_total;
} agg_t;

static int pe_default_csv(char *buf, size_t cap)
{
    const char *r = getenv("CREATION_OS_ROOT");
    if (r != NULL && r[0] != '\0') {
        if (snprintf(buf, cap, "%s/benchmarks/graded/graded_prompts.csv", r)
            < (int)cap)
            return 0;
    }
    if (snprintf(buf, cap, "benchmarks/graded/graded_prompts.csv") < (int)cap)
        return 0;
    return -1;
}

static int pe_load_graded(graded_row_t *rows, int *n_out)
{
    char  path[1024];
    FILE *f;
    char  line[2048];
    int   n = 0;

    *n_out = 0;
    if (pe_default_csv(path, sizeof path) != 0)
        return -1;
    f = fopen(path, "r");
    if (f == NULL)
        return -1;
    if (fgets(line, sizeof line, f) == NULL) {
        fclose(f);
        return -1;
    }
    while (n < COS_PE_MAX_ROWS && fgets(line, sizeof line, f) != NULL) {
        char cat[80];
        char pr[1800];
        int  rc = sscanf(line, " %79[^,],%1799[^,]", cat, pr);
        if (rc < 2)
            continue;
        snprintf(rows[n].domain, sizeof rows[n].domain, "%s", cat);
        cos_pattern_keyword_domain(pr, rows[n].kwdom, sizeof rows[n].kwdom);
        rows[n].phash = cos_engram_prompt_hash(pr);
        n++;
    }
    fclose(f);
    *n_out = n;
    return 0;
}

static agg_t *pe_find_agg(agg_t *ag, int *na, const char *dom)
{
    int i;
    for (i = 0; i < *na; i++) {
        if (strcmp(ag[i].domain, dom) == 0)
            return &ag[i];
    }
    if (*na >= COS_PE_MAX_DOM)
        return NULL;
    snprintf(ag[*na].domain, sizeof ag[*na].domain, "%s", dom);
    ag[*na].sum_sigma  = 0.0;
    ag[*na].n_sigma    = 0;
    ag[*na].n_correct  = 0;
    ag[*na].n_total    = 0;
    (*na)++;
    return &ag[*na - 1];
}

int cos_pattern_extract_write(const char *path_out)
{
    graded_row_t rows[COS_PE_MAX_ROWS];
    int          nr = 0;
    struct cos_engram_episode eps[200];
    int          ne = 0, i, j;
    agg_t        ag[COS_PE_MAX_DOM];
    int          na = 0;
    FILE        *fp;
    char         defpath[512];
    const char  *h = getenv("HOME");

    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(defpath, sizeof defpath, "%s/.cos/patterns.json", h)
        >= (int)sizeof defpath)
        return -1;
    if (path_out == NULL || path_out[0] == '\0')
        path_out = defpath;
    {
        char dirc[512];
        if (snprintf(dirc, sizeof dirc, "%s/.cos", h) < (int)sizeof dirc)
            (void)mkdir(dirc, 0700);
    }

    if (pe_load_graded(rows, &nr) != 0)
        nr = 0;

    if (cos_engram_episode_fetch_recent(eps, 200, &ne) != 0)
        ne = 0;

    for (i = 0; i < ne; i++) {
        const char *dom = "unknown";
        for (j = 0; j < nr; j++) {
            if (rows[j].phash == eps[i].prompt_hash) {
                dom = (rows[j].kwdom[0] != '\0') ? rows[j].kwdom : rows[j].domain;
                break;
            }
        }
        {
            agg_t *a = pe_find_agg(ag, &na, dom);
            if (a == NULL)
                continue;
            a->n_total++;
            a->sum_sigma += (double)eps[i].sigma_combined;
            a->n_sigma++;
            if (eps[i].was_correct)
                a->n_correct++;
        }
    }

    fp = fopen(path_out, "w");
    if (fp == NULL)
        return -1;
    fprintf(fp, "{\"schema\":\"cos.patterns.v1\",\"domains\":[");
    for (i = 0; i < na; i++) {
        float mean_s =
            (ag[i].n_sigma > 0)
                ? (float)(ag[i].sum_sigma / (double)ag[i].n_sigma)
                : 0.5f;
        float acc = (ag[i].n_total > 0)
                        ? ((float)ag[i].n_correct / (float)ag[i].n_total)
                        : 0.f;
        float opt_tau = 0.35f + (1.0f - acc) * 0.15f;
        if (opt_tau > 0.55f)
            opt_tau = 0.55f;
        if (opt_tau < 0.20f)
            opt_tau = 0.20f;
        fprintf(fp, "%s{\"domain\":\"%s\",\"mean_sigma\":%.4f,\"n_queries\":%d,"
                    "\"accuracy\":%.4f,\"optimal_tau\":%.4f}",
                (i > 0) ? "," : "", ag[i].domain, (double)mean_s, ag[i].n_total,
                (double)acc, (double)opt_tau);
    }
    fprintf(fp, "]}\n");
    fclose(fp);
    return 0;
}
