/*
 * σ-measured skill distillation — SQLite + BSC trigger similarity.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "skill_distill.h"

#include "cos_think.h"

#include "inference_cache.h"

#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static sqlite3 *g_db;

static uint64_t g_lookup_hash;

static int64_t wall_ms(void)
{
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
#endif
    return (int64_t)time(NULL) * 1000LL;
}

static int ensure_cos_dir(char *buf, size_t cap)
{
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    if (snprintf(buf, cap, "%s/.cos", home) >= (int)cap)
        return -1;
    if (mkdir(buf, 0700) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int db_open(void)
{
    if (g_db)
        return 0;
    char path[768];
    const char *env = getenv("COS_SKILLS_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(path, sizeof path, "%s", env);
    } else {
        char base[512];
        if (ensure_cos_dir(base, sizeof base) != 0)
            return -1;
        snprintf(path, sizeof path, "%s/skills.db", base);
    }
    if (sqlite3_open_v2(path, &g_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL)
        != SQLITE_OK)
        return -1;

    const char *ddl =
        "CREATE TABLE IF NOT EXISTS skills("
        "skill_hash INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "trigger_pattern TEXT NOT NULL,"
        "trigger_bsc BLOB NOT NULL,"
        "n_steps INTEGER NOT NULL,"
        "sigma_mean REAL NOT NULL,"
        "reliability REAL NOT NULL,"
        "times_used INTEGER NOT NULL,"
        "times_succeeded INTEGER NOT NULL,"
        "created_ms INTEGER NOT NULL,"
        "last_used_ms INTEGER NOT NULL,"
        "archived INTEGER NOT NULL DEFAULT 0,"
        "review_flag INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS skill_steps("
        "skill_hash INTEGER NOT NULL,"
        "step_ord INTEGER NOT NULL,"
        "step_text TEXT NOT NULL,"
        "PRIMARY KEY(skill_hash, step_ord)"
        ");";

    char *err = NULL;
    if (sqlite3_exec(g_db, ddl, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }
    return 0;
}

void cos_skill_begin_session(void)
{
    g_lookup_hash = 0ULL;
}

uint64_t cos_skill_last_lookup_hash(void)
{
    return g_lookup_hash;
}

void cos_skill_clear_lookup_hash(void)
{
    g_lookup_hash = 0ULL;
}

static void name_from_goal(const char *goal, char *name, size_t cap)
{
    char words[8][64];
    int    nw = 0;
    const char *p = goal ? goal : "";
    while (*p && nw < 8) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        size_t w = 0;
        while (*p && !isspace((unsigned char)*p) && w + 1 < 64)
            words[nw][w++] = *p++;
        words[nw][w] = '\0';
        if (w > 0)
            nw++;
    }
    size_t j = 0;
    for (int i = 0; i < nw && j + 2 < cap; i++) {
        if (i > 0 && j + 1 < cap)
            name[j++] = '_';
        for (size_t k = 0; words[i][k] && j + 1 < cap; k++) {
            unsigned char c = (unsigned char)words[i][k];
            if (isalnum(c))
                name[j++] = (char)tolower((int)c);
        }
    }
    if (j == 0 && cap > 5) {
        snprintf(name, cap, "skill");
    } else if (j < cap)
        name[j] = '\0';
}

int cos_skill_distill(const struct cos_think_result *result,
                      struct cos_skill *skill_out)
{
    if (!result || !skill_out || result->n_subtasks <= 0)
        return -1;
    memset(skill_out, 0, sizeof(*skill_out));

    int ns = result->n_subtasks;
    if (ns > COS_SKILL_MAX_STEPS)
        ns = COS_SKILL_MAX_STEPS;

    uint64_t h = cos_inference_prompt_fnv(result->goal);
    if (h == 0)
        h = 1ULL;
    skill_out->skill_hash = h ^ ((uint64_t)ns << 40);
    snprintf(skill_out->trigger_pattern, sizeof skill_out->trigger_pattern,
             "%.*s", 255, result->goal);
    name_from_goal(result->goal, skill_out->name, sizeof skill_out->name);

    for (int i = 0; i < ns; i++) {
        snprintf(skill_out->steps[i], sizeof skill_out->steps[i], "%s",
                 result->subtasks[i].prompt);
    }
    skill_out->n_steps          = ns;
    skill_out->sigma_mean       = result->sigma_mean;
    skill_out->reliability      = 1.0f;
    skill_out->times_used       = 1;
    skill_out->times_succeeded  = 1;
    skill_out->created_ms       = wall_ms();
    skill_out->last_used_ms     = skill_out->created_ms;
    return 0;
}

int cos_skill_save(const struct cos_skill *s)
{
    if (!s || s->n_steps <= 0 || s->n_steps > COS_SKILL_MAX_STEPS)
        return -1;
    if (db_open() != 0)
        return -1;

    uint64_t bsc[COS_INF_W];
    cos_inference_bsc_encode_prompt(s->trigger_pattern, bsc);

    sqlite3_exec(g_db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    sqlite3_stmt *st = NULL;
    const char *isq =
        "INSERT OR REPLACE INTO skills("
        "skill_hash,name,trigger_pattern,trigger_bsc,n_steps,sigma_mean,"
        "reliability,times_used,times_succeeded,created_ms,last_used_ms,"
        "archived,review_flag) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(g_db, isq, -1, &st, NULL) != SQLITE_OK)
        goto fail;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)s->skill_hash);
    sqlite3_bind_text(st, 2, s->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s->trigger_pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(st, 4, bsc, (int)(sizeof(uint64_t) * COS_INF_W),
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, s->n_steps);
    sqlite3_bind_double(st, 6, (double)s->sigma_mean);
    sqlite3_bind_double(st, 7, (double)s->reliability);
    sqlite3_bind_int(st, 8, s->times_used);
    sqlite3_bind_int(st, 9, s->times_succeeded);
    sqlite3_bind_int64(st, 10, (sqlite3_int64)s->created_ms);
    sqlite3_bind_int64(st, 11, (sqlite3_int64)s->last_used_ms);
    sqlite3_bind_int(st, 12, 0);
    sqlite3_bind_int(st, 13, 0);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        goto fail;
    }
    sqlite3_finalize(st);

    sqlite3_stmt *del = NULL;
    if (sqlite3_prepare_v2(g_db,
                           "DELETE FROM skill_steps WHERE skill_hash=?",
                           -1, &del, NULL)
        != SQLITE_OK)
        goto fail;
    sqlite3_bind_int64(del, 1, (sqlite3_int64)s->skill_hash);
    sqlite3_step(del);
    sqlite3_finalize(del);

    for (int i = 0; i < s->n_steps; i++) {
        sqlite3_stmt *ins = NULL;
        if (sqlite3_prepare_v2(g_db,
                               "INSERT INTO skill_steps(skill_hash,step_ord,"
                               "step_text) VALUES(?,?,?)",
                               -1, &ins, NULL)
            != SQLITE_OK)
            goto fail;
        sqlite3_bind_int64(ins, 1, (sqlite3_int64)s->skill_hash);
        sqlite3_bind_int(ins, 2, i);
        sqlite3_bind_text(ins, 3, s->steps[i], -1, SQLITE_TRANSIENT);
        if (sqlite3_step(ins) != SQLITE_DONE) {
            sqlite3_finalize(ins);
            goto fail;
        }
        sqlite3_finalize(ins);
    }

    sqlite3_exec(g_db, "COMMIT", NULL, NULL, NULL);
    return 0;
fail:
    sqlite3_exec(g_db, "ROLLBACK", NULL, NULL, NULL);
    return -1;
}

struct cos_skill *cos_skill_lookup(const uint64_t *bsc_goal,
                                   float threshold_norm)
{
    if (!bsc_goal || db_open() != 0)
        return NULL;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT skill_hash,name,trigger_pattern,trigger_bsc,n_steps,"
        "sigma_mean,reliability,times_used,times_succeeded,created_ms,"
        "last_used_ms FROM skills WHERE archived=0";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;

    float         best_d  = 1.0f;
    sqlite3_int64 best_h = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        int blob_n = sqlite3_column_bytes(st, 3);
        if (blob_n != (int)(sizeof(uint64_t) * COS_INF_W))
            continue;
        const void *blob = sqlite3_column_blob(st, 3);
        if (!blob)
            continue;
        float d = cos_inference_hamming_norm(
            bsc_goal, (const uint64_t *)blob, COS_INF_W);
        if (d < best_d && d <= threshold_norm) {
            best_d = d;
            best_h = sqlite3_column_int64(st, 0);
        }
    }
    sqlite3_finalize(st);

    if (best_h == 0)
        return NULL;

    struct cos_skill *out =
        (struct cos_skill *)calloc(1, sizeof(struct cos_skill));
    if (!out)
        return NULL;

    if (cos_skill_fetch((uint64_t)best_h, out) != 0) {
        free(out);
        return NULL;
    }

    int64_t now = wall_ms();
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(g_db,
                           "UPDATE skills SET last_used_ms=? WHERE skill_hash=?",
                           -1, &up, NULL)
        == SQLITE_OK) {
        sqlite3_bind_int64(up, 1, (sqlite3_int64)now);
        sqlite3_bind_int64(up, 2, best_h);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }

    g_lookup_hash = (uint64_t)best_h;
    return out;
}

void cos_skill_free(struct cos_skill *p)
{
    free(p);
}

int cos_skill_fetch(uint64_t skill_hash, struct cos_skill *out)
{
    if (!out || db_open() != 0)
        return -1;
    memset(out, 0, sizeof(*out));

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT name,trigger_pattern,n_steps,sigma_mean,reliability,"
        "times_used,times_succeeded,created_ms,last_used_ms FROM skills "
        "WHERE skill_hash=? AND archived=0";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)skill_hash);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -1;
    }
    out->skill_hash = skill_hash;
    snprintf(out->name, sizeof out->name, "%s",
             sqlite3_column_text(st, 0));
    snprintf(out->trigger_pattern, sizeof out->trigger_pattern, "%s",
             sqlite3_column_text(st, 1));
    out->n_steps         = sqlite3_column_int(st, 2);
    out->sigma_mean      = (float)sqlite3_column_double(st, 3);
    out->reliability     = (float)sqlite3_column_double(st, 4);
    out->times_used      = sqlite3_column_int(st, 5);
    out->times_succeeded = sqlite3_column_int(st, 6);
    out->created_ms      = sqlite3_column_int64(st, 7);
    out->last_used_ms    = sqlite3_column_int64(st, 8);
    sqlite3_finalize(st);

    if (out->n_steps > COS_SKILL_MAX_STEPS)
        out->n_steps = COS_SKILL_MAX_STEPS;

    sqlite3_stmt *ss = NULL;
    if (sqlite3_prepare_v2(g_db,
                           "SELECT step_text FROM skill_steps WHERE "
                           "skill_hash=? ORDER BY step_ord ASC",
                           -1, &ss, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(ss, 1, (sqlite3_int64)skill_hash);
    int row = 0;
    while (sqlite3_step(ss) == SQLITE_ROW && row < out->n_steps) {
        snprintf(out->steps[row], sizeof(out->steps[row]), "%s",
                 sqlite3_column_text(ss, 0));
        row++;
    }
    sqlite3_finalize(ss);
    return 0;
}

int cos_skill_update_reliability(uint64_t skill_hash, int succeeded)
{
    if (db_open() != 0)
        return -1;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT times_used,times_succeeded FROM skills WHERE skill_hash=?";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)skill_hash);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -1;
    }
    int tu = sqlite3_column_int(st, 0);
    int ts = sqlite3_column_int(st, 1);
    sqlite3_finalize(st);

    tu++;
    if (succeeded)
        ts++;
    float rel = (tu > 0) ? (float)ts / (float)tu : 0.f;

    sqlite3_stmt *up = NULL;
    const char *usql =
        "UPDATE skills SET times_used=?,times_succeeded=?,reliability=?,"
        "last_used_ms=? WHERE skill_hash=?";
    if (sqlite3_prepare_v2(g_db, usql, -1, &up, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(up, 1, tu);
    sqlite3_bind_int(up, 2, ts);
    sqlite3_bind_double(up, 3, (double)rel);
    sqlite3_bind_int64(up, 4, (sqlite3_int64)wall_ms());
    sqlite3_bind_int64(up, 5, (sqlite3_int64)skill_hash);
    int rc = sqlite3_step(up);
    sqlite3_finalize(up);
    return rc == SQLITE_DONE ? 0 : -1;
}

int cos_skill_retire(uint64_t skill_hash)
{
    if (db_open() != 0)
        return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                           "UPDATE skills SET archived=1 WHERE skill_hash=?",
                           -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)skill_hash);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

int cos_skill_list(struct cos_skill *skills, int max_skills, int *n_found)
{
    if (!skills || max_skills <= 0 || !n_found || db_open() != 0)
        return -1;
    *n_found = 0;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT skill_hash FROM skills WHERE archived=0 ORDER BY last_used_ms DESC";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;

    while (sqlite3_step(st) == SQLITE_ROW && *n_found < max_skills) {
        sqlite3_int64 h = sqlite3_column_int64(st, 0);
        if (cos_skill_fetch((uint64_t)h, &skills[*n_found]) == 0)
            (*n_found)++;
    }
    sqlite3_finalize(st);
    return 0;
}

int cos_skill_retire_low_reliability(void)
{
    if (db_open() != 0)
        return -1;
    char *err = NULL;
    const char *sql =
        "UPDATE skills SET archived=1 WHERE archived=0 AND times_used>=5 "
        "AND CAST(times_succeeded AS REAL)/times_used < 0.5";
    if (sqlite3_exec(g_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int cos_skill_archive_stale(int64_t now_ms, int64_t max_age_ms)
{
    if (db_open() != 0)
        return -1;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "UPDATE skills SET archived=1 WHERE archived=0 AND (? - last_used_ms) > ?";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)now_ms);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)max_age_ms);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return 0;
}

int cos_skill_flag_sigma_drift(float tau_ref)
{
    if (db_open() != 0)
        return -1;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "UPDATE skills SET review_flag=1 WHERE archived=0 AND sigma_mean > ?";
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_double(st, 1, (double)tau_ref);
    sqlite3_step(st);
    sqlite3_finalize(st);
    return 0;
}

int cos_skill_maintain(float tau_ref, int64_t now_ms)
{
    (void)cos_skill_flag_sigma_drift(tau_ref);
    int64_t month = (int64_t)30 * 86400LL * 1000LL;
    (void)cos_skill_archive_stale(now_ms, month);
    (void)cos_skill_retire_low_reliability();
    return 0;
}

int cos_skill_self_test(void)
{
    char tmp[256];
    snprintf(tmp, sizeof tmp, "/tmp/cos_skills_test_%ld.db", (long)getpid());
    remove(tmp);
    setenv("COS_SKILLS_DB", tmp, 1);

    cos_skill_begin_session();
    if (db_open() != 0)
        return 1;

    struct cos_think_result tr;
    memset(&tr, 0, sizeof(tr));
    snprintf(tr.goal, sizeof tr.goal, "unit test goal alpha beta gamma");
    tr.n_subtasks      = 2;
    tr.sigma_mean      = 0.12f;
    snprintf(tr.subtasks[0].prompt, sizeof tr.subtasks[0].prompt,
             "step one question");
    snprintf(tr.subtasks[1].prompt, sizeof tr.subtasks[1].prompt,
             "step two question");
    tr.subtasks[0].final_action = 0;
    tr.subtasks[1].final_action = 0;

    struct cos_skill sk;
    if (cos_skill_distill(&tr, &sk) != 0)
        return 2;
    if (cos_skill_save(&sk) != 0)
        return 3;

    uint64_t bsc[COS_INF_W];
    cos_inference_bsc_encode_prompt(tr.goal, bsc);
    struct cos_skill *lk = cos_skill_lookup(bsc, 0.40f);
    if (!lk || lk->n_steps != 2)
        return 4;
    cos_skill_free(lk);

    if (cos_skill_update_reliability(sk.skill_hash, 1) != 0)
        return 5;

    struct cos_skill buf[4];
    int nf = 0;
    if (cos_skill_list(buf, 4, &nf) != 0 || nf < 1)
        return 6;

    if (cos_skill_retire(sk.skill_hash) != 0)
        return 7;

    cos_skill_begin_session();
    struct cos_skill *lk2 = cos_skill_lookup(bsc, 0.40f);
    if (lk2 != NULL) {
        cos_skill_free(lk2);
        return 8;
    }

    remove(tmp);
    unsetenv("COS_SKILLS_DB");
    return 0;
}
