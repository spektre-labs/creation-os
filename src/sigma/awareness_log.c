/*
 * Persistent awareness log (~/.cos/awareness.db).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "awareness_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static sqlite3 *g_db;
static int      g_open;

static int awareness_db_path(char *buf, size_t cap)
{
    const char *env = getenv("COS_AWARENESS_DB");
    const char *home;

    if (env && env[0]) {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    home = getenv("HOME");
    if (!home || !home[0])
        home = "/tmp";
    snprintf(buf, cap, "%s/.cos/awareness.db", home);
    return 0;
}

static int awareness_mkparent(const char *path)
{
    char b[768];
    char *slash;
    snprintf(b, sizeof b, "%s", path);
    slash = strrchr(b, '/');
    if (!slash || slash == b)
        return 0;
    *slash = '\0';
    return mkdir(b, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

static int awareness_schema(sqlite3 *db)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS awareness("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ts INTEGER NOT NULL,"
        "consciousness_index REAL NOT NULL,"
        "level INTEGER NOT NULL,"
        "phi_proxy REAL NOT NULL);";

    char *err = NULL;
    int    rc;

    rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err)
            sqlite3_free(err);
        return -1;
    }
    return 0;
}

int cos_awareness_open(void)
{
    char path[768];
    int  rc;

    if (g_open && g_db)
        return 0;

    awareness_db_path(path, sizeof path);
    if (awareness_mkparent(path) != 0 && errno != EEXIST)
        return -1;

    rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK || !g_db)
        return -2;

    if (awareness_schema(g_db) != 0) {
        sqlite3_close(g_db);
        g_db = NULL;
        return -3;
    }

    g_open = 1;
    return 0;
}

void cos_awareness_close(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    g_open = 0;
}

int cos_awareness_log(const struct cos_consciousness_state *cs)
{
    sqlite3_stmt *st = NULL;
    char          sql[256];
    int           rc;

    if (!cs)
        return -1;

    if (!g_open || !g_db) {
        if (cos_awareness_open() != 0)
            return -2;
    }

    snprintf(sql, sizeof sql,
             "INSERT INTO awareness(ts,consciousness_index,level,phi_proxy) "
             "VALUES(?,?,?,?);");

    rc = sqlite3_prepare_v2(g_db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK)
        return -3;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)cs->timestamp_ms);
    sqlite3_bind_double(st, 2, (double)cs->consciousness_index);
    sqlite3_bind_int(st, 3, cs->level);
    sqlite3_bind_double(st, 4, (double)cs->phi_proxy);

    rc = sqlite3_step(st);
    sqlite3_finalize(st);

    return rc == SQLITE_DONE ? 0 : -4;
}

int cos_awareness_trend(float *trend, int *n_entries)
{
    sqlite3_stmt *st = NULL;
    const char   *sql =
        "SELECT COUNT(*) FROM awareness;";
    sqlite3_int64 cnt = 0;
    float         first_ci, last_ci;
    double        delta;

    if (!trend || !n_entries)
        return -1;

    if (!g_open || !g_db) {
        if (cos_awareness_open() != 0) {
            *trend      = 0.f;
            *n_entries  = 0;
            return 0;
        }
    }

    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -2;
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -3;
    }
    cnt = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);

    *n_entries = (int)(cnt > 2147483647LL ? 2147483647 : cnt);

    if (cnt < 2) {
        *trend = 0.f;
        return 0;
    }

    sql =
        "SELECT consciousness_index FROM awareness ORDER BY ts ASC LIMIT 1;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -4;
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -5;
    }
    first_ci = (float)sqlite3_column_double(st, 0);
    sqlite3_finalize(st);

    sql =
        "SELECT consciousness_index FROM awareness ORDER BY ts DESC LIMIT 1;";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -6;
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -7;
    }
    last_ci = (float)sqlite3_column_double(st, 0);
    sqlite3_finalize(st);

    delta    = (double)last_ci - (double)first_ci;
    *trend = (float)delta;
    return 0;
}

int cos_awareness_report(char *report, int max_len)
{
    float trend;
    int   n;

    if (!report || max_len <= 0)
        return -1;

    if (cos_awareness_trend(&trend, &n) != 0)
        return -2;

    snprintf(report, (size_t)max_len,
             "awareness_log: n_samples=%d  delta_consciousness_index=%.6f\n",
             n, (double)trend);
    return 0;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int aw_fail(const char *m)
{
    fprintf(stderr, "awareness_log self-test: %s\n", m);
    return 1;
}
#endif

int cos_awareness_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_consciousness_state cs;
    float                          ch[4] = {0.3f, 0.3f, 0.3f, 0.3f};
    float                          meta[4];
    char                           buf[512];
    char                           path[256];
    float                          tr;
    int                            ne;

    memset(meta, 0, sizeof meta);
    meta[0] = 0.31f;
    meta[1] = 0.29f;
    meta[2] = 0.30f;
    meta[3] = 0.28f;

    if (cos_consciousness_init(&cs) != 0)
        return aw_fail("meter_init");

    if (cos_consciousness_measure(&cs, 0.31f, ch, meta, 0.35f) != 0)
        return aw_fail("measure");

    snprintf(path, sizeof path, "/tmp/cos_aware_selftest_%llu.db",
             (unsigned long long)getpid());
    unlink(path);
    if (setenv("COS_AWARENESS_DB", path, 1) != 0)
        return aw_fail("setenv");

    if (cos_awareness_open() != 0)
        return aw_fail("open");

    if (cos_awareness_log(&cs) != 0)
        return aw_fail("log");

    if (cos_awareness_trend(&tr, &ne) != 0)
        return aw_fail("trend");

    if (cos_awareness_report(buf, sizeof buf) != 0)
        return aw_fail("report");

    cos_awareness_close();
    unlink(path);

    fprintf(stderr, "awareness_log self-test: OK\n");
#endif
    return 0;
}
