/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "stigmergy.h"

#include <errno.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static sqlite3 *g_db;
static int      g_inited;

static struct cos_stigmergy_trail g_last_trail;
static int                        g_have_trail;

static const char *home_swarm(void)
{
    static char b[512];
    const char *h = getenv("HOME");
    if (!h || !h[0]) {
        struct passwd *pw = getpwuid(getuid());
        h = (pw && pw->pw_dir) ? pw->pw_dir : ".";
    }
    snprintf(b, sizeof b, "%s/.cos/swarm", h);
    return b;
}

static int ensure_dirs(void)
{
    char p[768];
    const char *h = getenv("HOME");
    if (!h || !h[0]) return -1;
    snprintf(p, sizeof p, "%s/.cos", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    snprintf(p, sizeof p, "%s/.cos/swarm", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

int cos_stigmergy_init(void)
{
    if (g_inited) return 0;
    if (ensure_dirs() != 0) return -1;
    char path[768];
    snprintf(path, sizeof path, "%s/trails.db", home_swarm());
    if (sqlite3_open(path, &g_db) != SQLITE_OK) return -1;
    sqlite3_exec(g_db,
                 "CREATE TABLE IF NOT EXISTS trails("
                 "prompt_hash INTEGER PRIMARY KEY,"
                 "response TEXT NOT NULL,"
                 "sigma REAL NOT NULL,"
                 "peer_id TEXT NOT NULL,"
                 "ts INTEGER NOT NULL,"
                 "decay REAL NOT NULL"
                 ");",
                 NULL, NULL, NULL);
    g_inited = 1;
    return 0;
}

void cos_stigmergy_shutdown(void)
{
    if (!g_inited) return;
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    g_inited = 0;
}

int cos_stigmergy_leave_trail(const struct cos_stigmergy_trail *trail)
{
    if (!trail || cos_stigmergy_init() != 0) return -1;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT sigma FROM trails WHERE prompt_hash=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)trail->prompt_hash);
    int sk = sqlite3_step(st);
    float old_sigma = 1.0f;
    if (sk == SQLITE_ROW)
        old_sigma = (float)sqlite3_column_double(st, 0);
    sqlite3_finalize(st);

    if (sk == SQLITE_ROW && old_sigma <= trail->sigma) return 0;

    st = NULL;
    const char *iq =
        "INSERT OR REPLACE INTO trails(prompt_hash,response,sigma,peer_id,"
        "ts,decay)VALUES(?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(g_db, iq, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)trail->prompt_hash);
    sqlite3_bind_text(st, 2, trail->response, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, (double)trail->sigma);
    sqlite3_bind_text(st, 4, trail->peer_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, (sqlite3_int64)trail->timestamp);
    sqlite3_bind_double(st, 6, (double)trail->decay);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

const struct cos_stigmergy_trail *cos_stigmergy_check(uint64_t prompt_hash)
{
    g_have_trail = 0;
    if (cos_stigmergy_init() != 0) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT prompt_hash,response,sigma,peer_id,ts,"
                             "decay FROM trails WHERE prompt_hash=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return NULL;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)prompt_hash);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return NULL;
    }
    memset(&g_last_trail, 0, sizeof g_last_trail);
    g_last_trail.prompt_hash =
        (uint64_t)sqlite3_column_int64(st, 0);
    snprintf(g_last_trail.response, sizeof g_last_trail.response, "%s",
             (const char *)sqlite3_column_text(st, 1));
    g_last_trail.sigma = (float)sqlite3_column_double(st, 2);
    snprintf(g_last_trail.peer_id, sizeof g_last_trail.peer_id, "%s",
             (const char *)sqlite3_column_text(st, 3));
    g_last_trail.timestamp = sqlite3_column_int64(st, 4);
    g_last_trail.decay     = (float)sqlite3_column_double(st, 5);
    sqlite3_finalize(st);
    g_have_trail = 1;
    return &g_last_trail;
}

int cos_stigmergy_decay(float decay_rate)
{
    if (decay_rate <= 0.f || decay_rate > 1.f || cos_stigmergy_init() != 0)
        return -1;
    char q[160];
    snprintf(q, sizeof q,
             "UPDATE trails SET decay = decay * %.8f WHERE decay > 1e-9;",
             (double)decay_rate);
    return sqlite3_exec(g_db, q, NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int cos_stigmergy_reinforce(uint64_t prompt_hash, float boost)
{
    if (cos_stigmergy_init() != 0) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "UPDATE trails SET decay = MIN(1.0, decay + ?) "
                             "WHERE prompt_hash=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_double(st, 1, (double)boost);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)prompt_hash);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

void cos_stigmergy_fprint_active(FILE *f, int max_rows)
{
    if (!f || max_rows <= 0 || cos_stigmergy_init() != 0) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT prompt_hash,sigma,peer_id,ts,decay,"
                             "substr(response,1,120) FROM trails "
                             "ORDER BY ts DESC LIMIT ?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return;
    sqlite3_bind_int(st, 1, max_rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        fprintf(f,
                "hash=%llu σ=%.4f peer=%s ts=%lld decay=%.4f text=%s\n",
                (unsigned long long)sqlite3_column_int64(st, 0),
                sqlite3_column_double(st, 1),
                sqlite3_column_text(st, 2),
                (long long)sqlite3_column_int64(st, 3),
                sqlite3_column_double(st, 4),
                sqlite3_column_text(st, 5));
    }
    sqlite3_finalize(st);
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_STIGMERGY_TEST)
#include <assert.h>
int cos_stigmergy_self_test(void)
{
    char prev[256];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);
    cos_stigmergy_shutdown();

    assert(cos_stigmergy_init() == 0);
    struct cos_stigmergy_trail t = {0};
    t.prompt_hash = 12345u;
    snprintf(t.response, sizeof t.response, "%s", "answer");
    t.sigma     = 0.2f;
    snprintf(t.peer_id, sizeof t.peer_id, "%s", "peerA");
    t.timestamp = (int64_t)time(NULL);
    t.decay     = 1.0f;
    assert(cos_stigmergy_leave_trail(&t) == 0);

    const struct cos_stigmergy_trail *g = cos_stigmergy_check(12345u);
    assert(g != NULL && g->sigma < 0.3f);

    assert(cos_stigmergy_reinforce(12345u, 0.1f) == 0);
    assert(cos_stigmergy_decay(0.99f) == 0);

    cos_stigmergy_shutdown();
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
#endif
