/*
 * Curiosity ranking from engram σ + SQLite attempt history.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "curiosity.h"

#include "engram_episodic.h"

#include <math.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static sqlite3 *g_cur_db;

static const char *cur_home(void) {
    const char *h = getenv("HOME");
    if (h != NULL && h[0] != '\0')
        return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : ".";
}

static int cur_db_path(char *buf, size_t cap) {
    const char *o = getenv("COS_CURIOSITY_DB");
    if (o != NULL && o[0] != '\0')
        return snprintf(buf, cap, "%s", o);
    return snprintf(buf, cap, "%s/.cos/curiosity.db", cur_home());
}

static int cur_exec(const char *sql) {
    char *err = NULL;
    int   rc;
    if (g_cur_db == NULL)
        return -1;
    rc = sqlite3_exec(g_cur_db, sql, NULL, NULL, &err);
    if (err != NULL)
        sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

int cos_curiosity_init(void) {
    char path[768];
    if (g_cur_db != NULL)
        return 0;
    if (cur_db_path(path, sizeof path) >= (int)sizeof path)
        return -1;
    if (sqlite3_open_v2(path, &g_cur_db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)
        != SQLITE_OK)
        return -1;
    return cur_exec(
        "CREATE TABLE IF NOT EXISTS curiosity(\n"
        "  domain_hash INTEGER PRIMARY KEY,\n"
        "  attempts INTEGER NOT NULL DEFAULT 0,\n"
        "  improvements INTEGER NOT NULL DEFAULT 0,\n"
        "  last_sigma REAL NOT NULL DEFAULT 1,\n"
        "  prev_sigma REAL NOT NULL DEFAULT 1,\n"
        "  updated_ms INTEGER NOT NULL DEFAULT 0);\n");
}

void cos_curiosity_shutdown(void) {
    if (g_cur_db != NULL) {
        sqlite3_close(g_cur_db);
        g_cur_db = NULL;
    }
}

float cos_curiosity_score(float sigma_mean, int encounters, int attempts) {
    float novelty = 1.f / (float)(encounters + 1);
    float attw    = 1.f / (float)(attempts + 1);
    return sigma_mean * novelty * attw;
}

static void cur_fetch_stats(uint64_t domain_hash, int *attempts,
                            int *improvements, float *last_sigma,
                            float *prev_sigma) {
    sqlite3_stmt *st = NULL;
    *attempts     = 0;
    *improvements = 0;
    *last_sigma   = 1.f;
    *prev_sigma   = 1.f;
    if (g_cur_db == NULL)
        return;
    if (sqlite3_prepare_v2(
            g_cur_db,
            "SELECT attempts,improvements,last_sigma,prev_sigma FROM curiosity "
            "WHERE domain_hash=?",
            -1, &st, NULL)
        != SQLITE_OK)
        return;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)domain_hash);
    if (sqlite3_step(st) == SQLITE_ROW) {
        *attempts     = sqlite3_column_int(st, 0);
        *improvements = sqlite3_column_int(st, 1);
        *last_sigma   = (float)sqlite3_column_double(st, 2);
        *prev_sigma   = (float)sqlite3_column_double(st, 3);
    }
    sqlite3_finalize(st);
}

int cos_curiosity_update(uint64_t domain_hash, float sigma_before,
                         float sigma_after) {
    int            att, imp, add_imp;
    float          ls, ps;
    sqlite3_stmt *st = NULL;
    int64_t        tms = (int64_t)time(NULL) * 1000LL;

    if (g_cur_db == NULL && cos_curiosity_init() != 0)
        return -1;
    cur_fetch_stats(domain_hash, &att, &imp, &ls, &ps);
    add_imp   = (sigma_after + 0.015f < sigma_before) ? 1 : 0;
    {
        int old_att = att;
        att += 1;
        imp += add_imp;
        if (old_att == 0) {
            ps = sigma_before;
            ls = sigma_after;
        } else {
            ps = ls;
            ls = sigma_after;
        }
    }

    if (sqlite3_prepare_v2(
            g_cur_db,
            "INSERT INTO curiosity(domain_hash,attempts,improvements,"
            "last_sigma,prev_sigma,updated_ms) VALUES(?,?,?,?,?,?)\n"
            "ON CONFLICT(domain_hash) DO UPDATE SET\n"
            " attempts=excluded.attempts,\n"
            " improvements=excluded.improvements,\n"
            " prev_sigma=excluded.prev_sigma,\n"
            " last_sigma=excluded.last_sigma,\n"
            " updated_ms=excluded.updated_ms",
            -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)domain_hash);
    sqlite3_bind_int(st, 2, att);
    sqlite3_bind_int(st, 3, imp);
    sqlite3_bind_double(st, 4, (double)ls);
    sqlite3_bind_double(st, 5, (double)ps);
    sqlite3_bind_int64(st, 6, (sqlite3_int64)tms);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return -1;
    }
    sqlite3_finalize(st);
    return 0;
}

static int cmp_curiosity_desc(const void *a, const void *b) {
    float sa = ((const struct cos_curiosity_target *)a)->curiosity_score;
    float sb = ((const struct cos_curiosity_target *)b)->curiosity_score;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

int cos_curiosity_rank(struct cos_curiosity_target *targets, int max, int *n) {
    uint64_t hashes[24];
    float    sigs[24];
    int      nw = 0, i, w;
    struct cos_curiosity_target tmp[24];

    if (targets == NULL || max <= 0 || n == NULL)
        return -1;
    *n = 0;
    if (cos_curiosity_init() != 0)
        return -1;
    if (cos_engram_semantic_weakest(hashes, sigs, 24, &nw) != 0)
        return -1;
    w = 0;
    for (i = 0; i < nw && w < 24; ++i) {
        struct cos_engram_semantic *sem;
        int                         att, imp;
        float                       ls, ps, lr;
        sem = cos_engram_query_semantic(hashes[i]);
        if (sem == NULL)
            continue;
        cur_fetch_stats(hashes[i], &att, &imp, &ls, &ps);
        if (att > 24 && imp == 0)
            continue;
        memset(&tmp[w], 0, sizeof tmp[w]);
        tmp[w].domain_hash = hashes[i];
        tmp[w].sigma_mean  = sigs[i];
        snprintf(tmp[w].topic, sizeof tmp[w].topic,
                 "domain:%016llx verification",
                 (unsigned long long)hashes[i]);
        tmp[w].attempts     = att;
        tmp[w].improvements = imp;
        lr                  = ps - ls;
        tmp[w].learning_rate = lr;
        tmp[w].curiosity_score =
            cos_curiosity_score(sigs[i], sem->encounter_count, att);
        free(sem);
        w++;
    }
    if (w <= 0)
        return 0;
    qsort(tmp, (size_t)w, sizeof tmp[0], cmp_curiosity_desc);
    for (i = 0; i < w && i < max; ++i)
        targets[i] = tmp[i];
    *n = (w < max) ? w : max;
    return 0;
}

int cos_curiosity_explore(const struct cos_curiosity_target *target) {
    if (target == NULL)
        return -1;
    /* Exploration is orchestrated by Ω-loop / cos_think; this hook is a
     * no-op placeholder so callers can attach telemetry later. */
    return 0;
}

static void bar_for_sigma(FILE *fp, float sigma) {
    int n = (int)(sigma * 10.f + 0.5f);
    int j;
    if (n < 0) n = 0;
    if (n > 10) n = 10;
    for (j = 0; j < n; ++j)
        fputc('#', fp);
    for (; j < 10; ++j)
        fputc('.', fp);
}

void cos_curiosity_fprint_queue(FILE *fp, int max_rows) {
    struct cos_curiosity_target row[8];
    int                         n = 0, i;
    if (fp == NULL)
        return;
    if (max_rows > 8)
        max_rows = 8;
    if (cos_curiosity_rank(row, max_rows, &n) != 0 || n <= 0) {
        fputs("  (no ranked curiosity targets — engram semantic empty)\n", fp);
        return;
    }
    for (i = 0; i < n; ++i) {
        fprintf(fp, "  %d. %s  σ=%.2f score=%.4f ", i + 1, row[i].topic,
                (double)row[i].sigma_mean, (double)row[i].curiosity_score);
        bar_for_sigma(fp, row[i].sigma_mean);
        fputc('\n', fp);
    }
}

int cos_curiosity_self_test(void) {
    char path[512];
    snprintf(path, sizeof path, "/tmp/cos_cur_%ld.db", (long)getpid());
    remove(path);
    setenv("COS_CURIOSITY_DB", path, 1);
    cos_curiosity_shutdown();
    if (cos_curiosity_init() != 0)
        return 1;
    if (fabsf(cos_curiosity_score(0.8f, 3, 2) - 0.8f * (1.f / 4.f) * (1.f / 3.f))
        > 1e-3f)
        return 2;
    if (cos_curiosity_update(42ULL, 0.9f, 0.4f) != 0)
        return 3;
    cos_curiosity_shutdown();
    remove(path);
    unsetenv("COS_CURIOSITY_DB");
    return 0;
}
