/* cos report — human-readable audit summary to stdout (pilot MVP).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_report.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int sigma_ok(const char *ln, double *out)
{
    const char *p = strstr(ln, "\"sigma\":");
    if (!p)
        return 0;
    *out = strtod(p + 9, NULL);
    return 1;
}

int cos_report_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    char dir[512];
    snprintf(dir, sizeof dir, "%s/.cos/audit", h);

    int    nlines = 0, n_abst = 0, n_acc = 0, n_ret = 0, n_other = 0;
    double sum_sigma = 0.0;
    int    n_sigma   = 0;

    DIR *d = opendir(dir);
    if (!d) {
        printf("cos report: no audit directory (%s)\n", dir);
        return 0;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strstr(de->d_name, ".jsonl") == NULL)
            continue;
        char path[768];
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;
        char buf[16384];
        while (fgets(buf, sizeof buf, fp) != NULL) {
            nlines++;
            if (strstr(buf, "\"action\":\"ABSTAIN\"") != NULL)
                n_abst++;
            else if (strstr(buf, "\"action\":\"ACCEPT\"") != NULL)
                n_acc++;
            else if (strstr(buf, "\"action\":\"RETHINK\"") != NULL)
                n_ret++;
            else
                n_other++;
            double s;
            if (sigma_ok(buf, &s) && s >= 0.0 && s <= 1.0) {
                sum_sigma += s;
                n_sigma++;
            }
        }
        fclose(fp);
    }
    closedir(d);

    printf("Creation OS — audit summary\n");
    printf("Directory: %s\n", dir);
    printf("JSONL lines (all files): %d\n", nlines);
    printf("Actions — ACCEPT: %d  RETHINK: %d  ABSTAIN: %d  (other/unparsed: %d)\n",
           n_acc, n_ret, n_abst, n_other);
    if (n_sigma > 0)
        printf("Mean sigma (parsed): %.4f\n", sum_sigma / (double)n_sigma);
    else
        printf("Mean sigma: n/a\n");
    printf("\nRaw rows: ~/.cos/audit/*.jsonl\n");
    printf("HTTP API: ./cos serve --port 3001  then  curl -s localhost:3001/v1/health\n");
    return 0;
}
