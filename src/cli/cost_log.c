/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *
 *  src/cli/cost_log.c — see cost_log.h.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "cost_log.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static const char *ccl_home(void) {
    const char *h = getenv("HOME");
    if (h != NULL && h[0] != '\0') return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : ".";
}

static void ccl_path(char *out, size_t cap) {
    const char *ov = getenv("CREATION_OS_COST_LOG");
    if (ov != NULL && ov[0] != '\0') {
        snprintf(out, cap, "%s", ov);
        return;
    }
    snprintf(out, cap, "%s/.cos/cost_log.jsonl", ccl_home());
}

static void ccl_mkdir_parent(const char *path) {
    char dup[1024];
    snprintf(dup, sizeof dup, "%s", path);
    char *slash = strrchr(dup, '/');
    if (slash == NULL) return;
    *slash = '\0';
    (void)mkdir(dup, 0700);
}

void cos_cost_log_append(const char *provider,
                         const char *route,
                         int         tokens_in,
                         int         tokens_out,
                         float       sigma,
                         double      cost_eur) {
    char path[1024];
    ccl_path(path, sizeof path);
    ccl_mkdir_parent(path);

    FILE *f = fopen(path, "a");
    if (f == NULL) return;  /* silent */

    long long ts = (long long)time(NULL);
    fprintf(f,
            "{\"ts\":%lld,\"tokens_in\":%d,\"tokens_out\":%d,"
            "\"route\":\"%s\",\"provider\":\"%s\","
            "\"cost_eur\":%.6f,\"sigma\":%.4f}\n",
            ts, tokens_in, tokens_out,
            route     ? route     : "?",
            provider  ? provider  : "?",
            cost_eur, (double)sigma);
    fclose(f);
}

/* Minimal scalar extractor: find "key": and parse the trailing
 * integer / float.  Shape is controlled by our own writer so we
 * skip quoted-string scanning. */
static int ccl_parse_long(const char *line, const char *key, long long *out) {
    const char *p = strstr(line, key);
    if (p == NULL) return -1;
    p = strchr(p, ':');
    if (p == NULL) return -1;
    *out = strtoll(p + 1, NULL, 10);
    return 0;
}

static int ccl_parse_double(const char *line, const char *key, double *out) {
    const char *p = strstr(line, key);
    if (p == NULL) return -1;
    p = strchr(p, ':');
    if (p == NULL) return -1;
    *out = strtod(p + 1, NULL);
    return 0;
}

static int ccl_parse_route_is_local(const char *line) {
    const char *p = strstr(line, "\"route\":\"");
    if (p == NULL) return 0;
    p += strlen("\"route\":\"");
    return strncmp(p, "LOCAL", 5) == 0;
}

int cos_cost_log_aggregate(cos_cost_log_agg_t *out) {
    if (out == NULL) return -1;
    memset(out, 0, sizeof *out);

    char path[1024];
    ccl_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (f == NULL) return -1;

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
    time_t midnight = mktime(&lt);
    time_t t_week   = midnight - (time_t)(6 * 86400);  /* 7-day rolling */
    time_t t_month  = midnight - (time_t)(29 * 86400); /* 30-day rolling */

    char line[65536];
    while (fgets(line, sizeof line, f) != NULL) {
        long long ts = 0;
        if (ccl_parse_long(line, "\"ts\"", &ts) != 0) continue;
        double eur = 0.0;
        ccl_parse_double(line, "\"cost_eur\"", &eur);
        long long tin = 0, tout = 0;
        ccl_parse_long(line, "\"tokens_in\"",  &tin);
        ccl_parse_long(line, "\"tokens_out\"", &tout);
        int is_local = ccl_parse_route_is_local(line);

        time_t t = (time_t)ts;
        if (t >= midnight) {
            out->queries_today++;
            out->eur_today += eur;
            if (is_local) out->local_today++;
        }
        if (t >= t_week) {
            out->queries_week++;
            out->eur_week += eur;
            if (is_local) out->local_week++;
        }
        if (t >= t_month) {
            out->queries_month++;
            out->eur_month += eur;
            if (is_local) out->local_month++;
            out->tokens_in_month  += (int)tin;
            out->tokens_out_month += (int)tout;
        }
    }
    fclose(f);
    return 0;
}
