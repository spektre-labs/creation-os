/* cos report — weekly Markdown from ~/.cos/audit + state ledger snapshot.
 *
 * PDF: if `--output` ends in `.pdf`, writes a sibling `.md` then tries
 * `pandoc IN.md -o OUT.pdf` when `pandoc` is on PATH.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_report.h"
#include "../sigma/state_ledger.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int dash_sigma_ok(const char *ln, double *out)
{
    const char *p = strstr(ln, "\"sigma\":");
    if (!p)
        return 0;
    *out = strtod(p + 9, NULL);
    return 1;
}

static void dash_copy_json_str(const char *ln, const char *key, char *out, size_t cap)
{
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = strstr(ln, pat);
    out[0] = '\0';
    if (!p || cap < 2)
        return;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) {
        if (*p == '\\' && p[1]) {
            p += 2;
            continue;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
}

static void date_cutoff_ymd(char *out, size_t cap, int days_back)
{
    time_t    t = time(NULL) - (time_t)days_back * 86400;
    struct tm tmv;
    if (gmtime_r(&t, &tmv) == NULL) {
        if (cap)
            out[0] = '\0';
        return;
    }
    strftime(out, cap, "%Y-%m-%d", &tmv);
}

static int audit_name_ok(const char *base, const char *cut_ymd)
{
    /* base like "2026-04-20.jsonl" */
    char d[16];
    size_t i;
    for (i = 0; i < sizeof d - 1 && base[i] && base[i] != '.'; ++i)
        d[i] = base[i];
    d[i] = '\0';
    return strcmp(d, cut_ymd) >= 0;
}

typedef struct {
    double s;
    char   pv[96];
} top1;

static void top5_push(top1 *t, int *n, double s, const char *pv)
{
    int i, j;
    if (*n < 5) {
        t[*n].s = s;
        snprintf(t[*n].pv, sizeof t[*n].pv, "%s", pv ? pv : "");
        (*n)++;
        return;
    }
    /* replace smallest if s larger */
    int mi = 0;
    for (i = 1; i < 5; ++i)
        if (t[i].s < t[mi].s)
            mi = i;
    if (s <= t[mi].s)
        return;
    t[mi].s = s;
    snprintf(t[mi].pv, sizeof t[mi].pv, "%s", pv ? pv : "");
    /* bubble sort desc */
    for (i = 0; i < 4; ++i)
        for (j = i + 1; j < 5; ++j)
            if (t[j].s > t[i].s) {
                top1 tmp = t[i];
                t[i]     = t[j];
                t[j]     = tmp;
            }
}

static int ensure_dir_recursive(char *dir)
{
    size_t len = strlen(dir);
    if (len == 0)
        return -1;
    for (size_t i = 1; i < len; ++i) {
        if (dir[i] != '/')
            continue;
        dir[i] = '\0';
        if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
            dir[i] = '/';
            return -1;
        }
        dir[i] = '/';
    }
    return (mkdir(dir, 0700) == 0 || errno == EEXIST) ? 0 : -1;
}

static void top5_low_push(top1 *t, int *n, double s, const char *pv)
{
    int i, j;
    if (*n < 5) {
        t[*n].s = s;
        snprintf(t[*n].pv, sizeof t[*n].pv, "%s", pv ? pv : "");
        (*n)++;
        return;
    }
    int mi = 0;
    for (i = 1; i < 5; ++i)
        if (t[i].s > t[mi].s)
            mi = i;
    if (s >= t[mi].s)
        return;
    t[mi].s = s;
    snprintf(t[mi].pv, sizeof t[mi].pv, "%s", pv ? pv : "");
    for (i = 0; i < 4; ++i)
        for (j = i + 1; j < 5; ++j)
            if (t[j].s < t[i].s) {
                top1 tmp = t[i];
                t[i]     = t[j];
                t[j]     = tmp;
            }
}

int cos_report_main(int argc, char **argv)
{
    int         weekly = 0;
    const char *outp   = NULL;
    int         i;
    int         i0 = 1;
    if (argc > 1 && strcmp(argv[1], "report") == 0)
        i0 = 2;
    for (i = i0; i < argc; ++i) {
        if (strcmp(argv[i], "--weekly") == 0)
            weekly = 1;
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            outp = argv[++i];
    }
    if (!weekly) {
        fputs("cos report: use --weekly (see `cos help --all`)\n", stderr);
        return 2;
    }
    if (!outp)
        outp = "reports/weekly_operators.md";

    char cut[16];
    date_cutoff_ymd(cut, sizeof cut, 7);

    char audit_dir[512];
    {
        const char *h = getenv("HOME");
        if (!h || !h[0])
            h = "/tmp";
        snprintf(audit_dir, sizeof audit_dir, "%s/.cos/audit", h);
    }

    int   total_lines = 0, n_abst = 0, n_acc = 0, n_ret = 0;
    top1  hi[5];
    top1  lo[5];
    int   nhi = 0, nlo = 0;
    memset(hi, 0, sizeof hi);
    memset(lo, 0, sizeof lo);

    {
        DIR *d = opendir(audit_dir);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                const char *nm = de->d_name;
                if (strstr(nm, ".jsonl") == NULL)
                    continue;
                if (!audit_name_ok(nm, cut))
                    continue;
                char path[768];
                snprintf(path, sizeof path, "%s/%s", audit_dir, nm);
                FILE *fp = fopen(path, "r");
                if (!fp)
                    continue;
                char buf[16384];
                while (fgets(buf, sizeof buf, fp) != NULL) {
                    double s;
                    if (!dash_sigma_ok(buf, &s) || s < 0.0 || s > 1.0)
                        continue;
                    char pv[128];
                    dash_copy_json_str(buf, "input_preview", pv, sizeof pv);
                    total_lines++;
                    if (strstr(buf, "\"action\":\"ABSTAIN\"") != NULL)
                        n_abst++;
                    else if (strstr(buf, "\"action\":\"ACCEPT\"") != NULL)
                        n_acc++;
                    else if (strstr(buf, "\"action\":\"RETHINK\"") != NULL)
                        n_ret++;
                    top5_push(hi, &nhi, s, pv);
                    top5_low_push(lo, &nlo, s, pv);
                }
                fclose(fp);
            }
            closedir(d);
        }
    }

    struct cos_state_ledger L;
    char                    lp[512];
    cos_state_ledger_init(&L);
    if (cos_state_ledger_default_path(lp, sizeof lp) == 0)
        (void)cos_state_ledger_load(&L, lp);

    char mdpath[768];
    snprintf(mdpath, sizeof mdpath, "%s", outp);
    int want_pdf = 0;
    {
        size_t n = strlen(mdpath);
        if (n > 4 && strcmp(mdpath + n - 4, ".pdf") == 0) {
            want_pdf = 1;
            if (n + 4 >= sizeof mdpath) {
                fputs("cos report: output path too long\n", stderr);
                return 2;
            }
            strcpy(mdpath + n - 4, ".md");
        }
    }

    {
        char dirc[768];
        snprintf(dirc, sizeof dirc, "%s", mdpath);
        char *slash = strrchr(dirc, '/');
        if (slash) {
            *slash = '\0';
            if (dirc[0] != '\0' && ensure_dir_recursive(dirc) != 0) {
                fprintf(stderr, "cos report: cannot mkdir parent of %s\n", mdpath);
                return 1;
            }
        }
    }

    FILE *out = fopen(mdpath, "w");
    if (!out) {
        perror("cos report");
        return 1;
    }

    time_t     nowt = time(NULL);
    struct tm  tml;
    char       tbuf[64];
    if (localtime_r(&nowt, &tml) != NULL)
        strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S (local)", &tml);
    else
        snprintf(tbuf, sizeof tbuf, "%lld", (long long)nowt);
    fprintf(out, "# Creation OS — weekly operator report\n\n");
    fprintf(out, "- Generated: %s\n", tbuf);
    fprintf(out, "- Window: audit files with date ≥ **%s** (UTC date from filename)\n\n",
            cut);
    fprintf(out, "## Volume\n\n");
    fprintf(out, "- JSONL rows scanned: **%d**\n", total_lines);
    fprintf(out, "- ACCEPT: **%d** | RETHINK: **%d** | ABSTAIN: **%d**\n\n", n_acc,
            n_ret, n_abst);
    fprintf(out, "## Ledger snapshot (`~/.cos/state_ledger.json`)\n\n");
    fprintf(out, "- total_queries: **%d** | accepts: **%d** | rethinks: **%d** | "
                 "abstains: **%d**\n",
            L.total_queries, L.accepts, L.rethinks, L.abstains);
    fprintf(out, "- sigma_drift flag: **%d** | sigma_mean_session: **%.4f**\n\n",
            (int)L.drift_detected, (double)L.sigma_mean_session);
    fprintf(out, "## Highest σ (last window, preview text)\n\n");
    for (i = 0; i < nhi; ++i)
        fprintf(out, "%d. σ=%.3f — `%s`\n", i + 1, hi[i].s, hi[i].pv);
    fprintf(out, "\n## Lowest σ (reliable band — descriptive only)\n\n");
    for (i = 0; i < nlo; ++i)
        fprintf(out, "%d. σ=%.3f — `%s`\n", i + 1, lo[i].s, lo[i].pv);
    fprintf(out, "\n## Compliance mapping\n\n");
    fprintf(out, "Run `python3 scripts/compliance/eu_ai_act_report.py` for the EU "
                 "AI Act **mapping** markdown (operator governance; not legal advice).\n");
    fclose(out);

    if (want_pdf) {
        char cmd[1024];
        snprintf(cmd, sizeof cmd, "command -v pandoc >/dev/null 2>&1 && pandoc "
                                    "\"%s\" -o \"%s\" 2>/dev/null",
                 mdpath, outp);
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr,
                    "cos report: PDF requested but pandoc failed or is missing; "
                    "wrote Markdown: %s\n",
                    mdpath);
            return 3;
        }
    }
    fprintf(stderr, "cos report: wrote %s\n", mdpath);
    return 0;
}
