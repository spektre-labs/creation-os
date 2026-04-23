/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "reputation.h"

#include <errno.h>
#include <math.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_REPUTATION_TEST)
#include <assert.h>
#endif

static sqlite3 *g_db;
static int      g_inited;

static void wipe_all_rows(void)
{
    if (!g_db) return;
    sqlite3_exec(g_db, "DELETE FROM reputation;", NULL, NULL, NULL);
}

static const char *home_mp(void)
{
    static char b[512];
    const char *h = getenv("HOME");
    if (!h || !h[0]) {
        struct passwd *pw = getpwuid(getuid());
        h = (pw && pw->pw_dir) ? pw->pw_dir : ".";
    }
    snprintf(b, sizeof b, "%s/.cos/marketplace", h);
    return b;
}

static int ensure_dirs(void)
{
    char p[768];
    const char *h = getenv("HOME");
    if (!h || !h[0]) return -1;
    snprintf(p, sizeof p, "%s/.cos", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    snprintf(p, sizeof p, "%s/.cos/marketplace", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static float compute_trust(float sigma_mean, float reliability, float vratio,
                           int total_served)
{
    float logterm = logf((float)(total_served + 1)) / 10.f;
    if (logterm > 1.f)
        logterm = 1.f;
    if (logterm < 0.f)
        logterm = 0.f;
    float s = sigma_mean;
    if (s < 0.f) s = 0.f;
    if (s > 1.f) s = 1.f;
    float t =
        0.4f * (1.f - s) + 0.3f * reliability + 0.2f * vratio + 0.1f * logterm;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    return t;
}

static void fill_row(sqlite3_stmt *st, struct cos_reputation *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->node_id, sizeof out->node_id, "%s",
             (const char *)sqlite3_column_text(st, 0));
    double sum_sigma = sqlite3_column_double(st, 1);
    int    cnt_sigma = sqlite3_column_int(st, 2);
    int    tot_serv   = sqlite3_column_int(st, 3);
    int    tot_ok     = sqlite3_column_int(st, 4);
    int    tot_v      = sqlite3_column_int(st, 5);
    int    tot_r      = sqlite3_column_int(st, 6);

    out->total_served    = tot_serv;
    out->total_verified  = tot_v;
    out->total_rejected  = tot_r;
    out->sigma_mean_served =
        cnt_sigma > 0 ? (float)(sum_sigma / (double)cnt_sigma) : 1.0f;
    out->reliability =
        tot_serv > 0 ? (float)tot_ok / (float)tot_serv : 0.f;
    float vratio = 0.f;
    if (tot_v + tot_r > 0)
        vratio = (float)tot_v / (float)(tot_v + tot_r);
    out->trust_composite =
        compute_trust(out->sigma_mean_served, out->reliability, vratio,
                      tot_serv);
}

int cos_reputation_init(void)
{
    if (g_inited) return 0;
    if (ensure_dirs() != 0) return -1;
    char path[768];
    snprintf(path, sizeof path, "%s/reputation.sqlite", home_mp());
    if (sqlite3_open(path, &g_db) != SQLITE_OK) return -1;
    sqlite3_exec(g_db,
                 "CREATE TABLE IF NOT EXISTS reputation("
                 "node_id TEXT PRIMARY KEY,"
                 "sum_sigma REAL NOT NULL DEFAULT 0,"
                 "count_sigma INTEGER NOT NULL DEFAULT 0,"
                 "total_served INTEGER NOT NULL DEFAULT 0,"
                 "total_ok INTEGER NOT NULL DEFAULT 0,"
                 "total_verified INTEGER NOT NULL DEFAULT 0,"
                 "total_rejected INTEGER NOT NULL DEFAULT 0"
                 ");",
                 NULL, NULL, NULL);
    g_inited = 1;
    return 0;
}

void cos_reputation_shutdown(void)
{
    if (!g_inited) return;
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    g_inited = 0;
}

int cos_reputation_update(const char *node_id, float sigma, int verified,
                          int succeeded)
{
    if (!node_id || !node_id[0]) return -1;
    if (cos_reputation_init() != 0) return -1;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT sum_sigma,count_sigma,total_served,"
                             "total_ok,total_verified,total_rejected FROM "
                             "reputation WHERE node_id=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, node_id, -1, SQLITE_TRANSIENT);

    double sum_sigma = 0.;
    int    cnt_sigma = 0, tot_serv = 0, tot_ok = 0, tot_v = 0, tot_r = 0;
    int    found = 0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        found      = 1;
        sum_sigma  = sqlite3_column_double(st, 0);
        cnt_sigma  = sqlite3_column_int(st, 1);
        tot_serv   = sqlite3_column_int(st, 2);
        tot_ok     = sqlite3_column_int(st, 3);
        tot_v      = sqlite3_column_int(st, 4);
        tot_r      = sqlite3_column_int(st, 5);
    }
    sqlite3_finalize(st);

    tot_serv++;
    if (succeeded) {
        sum_sigma += (double)sigma;
        cnt_sigma++;
        tot_ok++;
    }
    if (verified)
        tot_v++;
    else
        tot_r++;

    st = NULL;
    if (!found) {
        if (sqlite3_prepare_v2(g_db,
                                 "INSERT INTO reputation(node_id,sum_sigma,"
                                 "count_sigma,total_served,total_ok,"
                                 "total_verified,total_rejected)VALUES(?,?,?,?,?,?,"
                                 "?);",
                                 -1, &st, NULL)
            != SQLITE_OK)
            return -1;
        sqlite3_bind_text(st, 1, node_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, sum_sigma);
        sqlite3_bind_int(st, 3, cnt_sigma);
        sqlite3_bind_int(st, 4, tot_serv);
        sqlite3_bind_int(st, 5, tot_ok);
        sqlite3_bind_int(st, 6, tot_v);
        sqlite3_bind_int(st, 7, tot_r);
    } else {
        if (sqlite3_prepare_v2(g_db,
                                 "UPDATE reputation SET sum_sigma=?,"
                                 "count_sigma=?,total_served=?,total_ok=?,"
                                 "total_verified=?,total_rejected=? WHERE "
                                 "node_id=?;",
                                 -1, &st, NULL)
            != SQLITE_OK)
            return -1;
        sqlite3_bind_double(st, 1, sum_sigma);
        sqlite3_bind_int(st, 2, cnt_sigma);
        sqlite3_bind_int(st, 3, tot_serv);
        sqlite3_bind_int(st, 4, tot_ok);
        sqlite3_bind_int(st, 5, tot_v);
        sqlite3_bind_int(st, 6, tot_r);
        sqlite3_bind_text(st, 7, node_id, -1, SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

struct cos_reputation cos_reputation_get(const char *node_id)
{
    struct cos_reputation z;
    memset(&z, 0, sizeof z);
    snprintf(z.node_id, sizeof z.node_id, "%s", node_id ? node_id : "");
    if (!node_id || !node_id[0] || cos_reputation_init() != 0) return z;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT node_id,sum_sigma,count_sigma,total_served,"
                             "total_ok,total_verified,total_rejected FROM "
                             "reputation WHERE node_id=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return z;
    sqlite3_bind_text(st, 1, node_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        snprintf(z.node_id, sizeof z.node_id, "%s", node_id);
        z.sigma_mean_served = 1.0f;
        z.trust_composite     = compute_trust(1.f, 0.f, 0.f, 0);
        return z;
    }
    fill_row(st, &z);
    sqlite3_finalize(st);
    return z;
}

int cos_reputation_ranking(struct cos_reputation *nodes, int max, int *n)
{
    if (!nodes || max <= 0 || !n) return -1;
    if (cos_reputation_init() != 0) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT node_id,sum_sigma,count_sigma,total_served,"
                             "total_ok,total_verified,total_rejected FROM "
                             "reputation;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;

    struct cos_reputation tmp[128];
    int nt = 0;
    while (sqlite3_step(st) == SQLITE_ROW && nt < 128)
        fill_row(st, &tmp[nt++]);
    sqlite3_finalize(st);

    *n = nt < max ? nt : max;
    /* Sort by trust descending (bubble — small n) */
    for (int i = 0; i < nt; i++)
        for (int j = i + 1; j < nt; j++)
            if (tmp[j].trust_composite > tmp[i].trust_composite) {
                struct cos_reputation sw = tmp[i];
                tmp[i]           = tmp[j];
                tmp[j]           = sw;
            }
    for (int i = 0; i < *n; i++) nodes[i] = tmp[i];
    return 0;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_REPUTATION_TEST)
int cos_reputation_self_test(void)
{
    char prev[256];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh)
        snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);
    cos_reputation_shutdown();

    assert(cos_reputation_init() == 0);
    wipe_all_rows();
    assert(cos_reputation_update("nodeA", 0.2f, 1, 1) == 0);
    assert(cos_reputation_update("nodeA", 0.3f, 1, 1) == 0);
    struct cos_reputation g = cos_reputation_get("nodeA");
    assert(g.total_served == 2);
    assert(g.sigma_mean_served < 0.35f);

    struct cos_reputation nr[8];
    int nn = 0;
    assert(cos_reputation_ranking(nr, 8, &nn) == 0);
    assert(nn >= 1);

    cos_reputation_shutdown();
    if (prev[0])
        (void)setenv("HOME", prev, 1);
    return 0;
}
#endif
