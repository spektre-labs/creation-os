/*
 * σ-gated autonomous learning (web + engram). SQLite log ~/.cos/learning_log.db
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "learn_engine.h"

#include "../cli/cos_search.h"
#include "engram_episodic.h"

#include <ctype.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static sqlite3 *g_learn_db;

static const char *learn_home(void) {
    const char *h = getenv("HOME");
    if (h != NULL && h[0] != '\0')
        return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : ".";
}

static int learn_db_path(char *buf, size_t cap) {
    const char *o = getenv("COS_LEARNING_LOG_DB");
    if (o != NULL && o[0] != '\0')
        return snprintf(buf, cap, "%s", o);
    return snprintf(buf, cap, "%s/.cos/learning_log.db", learn_home());
}

static int learn_exec(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int   rc  = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err != NULL)
        sqlite3_free(err);
    return rc == SQLITE_OK ? 0 : -1;
}

static uint64_t learn_fnv1a(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    uint64_t               h = 14695981039346656037ULL;
    while (p != NULL && *p != '\0') {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static float learn_sigma_from_text(const char *t) {
    if (t == NULL || t[0] == '\0')
        return 1.0f;
    uint64_t h = learn_fnv1a(t);
    float    x = (float)((double)(h % 1000000u) / 1000000.0);
    return 0.08f + 0.84f * x;
}

static int word_tokens(const char *s, char words[32][32], int maxw) {
    char       buf[512];
    int        w = 0;
    size_t     bi = 0;
    const char *p;

    if (s == NULL || maxw <= 0)
        return 0;
    for (p = s; *p != '\0' && bi + 1 < sizeof buf; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) && bi < sizeof buf - 1)
            buf[bi++] = (char)tolower((int)c);
        else if (bi > 0) {
            buf[bi] = '\0';
            if (strlen(buf) >= 4u && w < maxw) {
                snprintf(words[w], sizeof words[w], "%s", buf);
                w++;
            }
            bi = 0;
        }
    }
    if (bi > 0) {
        buf[bi] = '\0';
        if (strlen(buf) >= 4u && w < maxw) {
            snprintf(words[w], sizeof words[w], "%s", buf);
            w++;
        }
    }
    return w;
}

static float learn_overlap_ratio(const char *a, const char *b) {
    char wa[32][32], wb[32][32];
    int  na = word_tokens(a, wa, 32);
    int  nb = word_tokens(b, wb, 32);
    int  hit, i, j;

    if (na <= 0 || nb <= 0)
        return 0.f;
    hit = 0;
    for (i = 0; i < na; ++i) {
        for (j = 0; j < nb; ++j) {
            if (strcasecmp(wa[i], wb[j]) == 0) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / (float)na;
}

int cos_learn_init(void) {
    char        path[768];
    const char *sql =
        "CREATE TABLE IF NOT EXISTS learn_row(\n"
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "  ts INTEGER NOT NULL,\n"
        "  domain_hash INTEGER NOT NULL,\n"
        "  source_url TEXT,\n"
        "  summary TEXT,\n"
        "  sigma_before REAL NOT NULL,\n"
        "  sigma_after REAL NOT NULL,\n"
        "  verified INTEGER NOT NULL);\n";

    if (g_learn_db != NULL)
        return 0;
    if (learn_db_path(path, sizeof path) >= (int)sizeof path)
        return -1;
    {
        char dirc[768];
        snprintf(dirc, sizeof dirc, "%s", path);
        char *slash = strrchr(dirc, '/');
        if (slash != NULL) {
            *slash = '\0';
            (void)mkdir(dirc, 0700);
        }
    }
    if (sqlite3_open_v2(path, &g_learn_db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL)
        != SQLITE_OK)
        return -1;
    return learn_exec(g_learn_db, sql);
}

void cos_learn_shutdown(void) {
    if (g_learn_db != NULL) {
        sqlite3_close(g_learn_db);
        g_learn_db = NULL;
    }
}

int cos_learn_identify_gaps(struct cos_learn_task *tasks, int max, int *n) {
    uint64_t hashes[8];
    float    sigs[8];
    int      nw = 0;
    int      take, i;

    if (tasks == NULL || max <= 0 || n == NULL)
        return -1;
    *n = 0;
    if (cos_engram_semantic_weakest(hashes, sigs,
                                    max > 8 ? 8 : max, &nw)
        != 0)
        return -1;
    take = nw;
    if (take > max)
        take = max;
    if (take > 3)
        take = 3;
    for (i = 0; i < take; ++i) {
        struct cos_learn_task *t = &tasks[*n];
        memset(t, 0, sizeof(*t));
        t->domain_hash    = hashes[i];
        t->current_sigma  = sigs[i];
        t->target_sigma   = 0.35f;
        t->priority       = 100 - i * 10;
        snprintf(t->topic, sizeof t->topic,
                 "verification summary domain:%016llx uncertain_knowledge",
                 (unsigned long long)hashes[i]);
        (*n)++;
    }
    return 0;
}

int cos_learn_verify(const struct cos_learn_result *result) {
    struct cos_search_result sr[COS_SEARCH_MAX_RESULTS];
    int                      n = 0;
    char                     qbuf[320];

    if (result == NULL)
        return 0;
    snprintf(qbuf, sizeof qbuf, "%.300s", result->summary);
    if (cos_search_run(qbuf, 8, sr, &n) != 0 || n < 2)
        return 0;
    {
        float oa = learn_overlap_ratio(result->summary, sr[0].snippet);
        float ob = learn_overlap_ratio(result->summary, sr[1].snippet);
        float ab = learn_overlap_ratio(sr[0].snippet, sr[1].snippet);
        if (oa > 0.12f && ob > 0.12f && ab > 0.08f)
            return 1;
    }
    return 0;
}

int cos_learn_research(const struct cos_learn_task *task,
                       struct cos_learn_result *result) {
    struct cos_search_result sr[COS_SEARCH_MAX_RESULTS];
    int                      n = 0;

    if (task == NULL || result == NULL)
        return -1;
    memset(result, 0, sizeof(*result));
    result->domain_hash   = task->domain_hash;
    result->sigma_before  = task->current_sigma;
    result->timestamp =
        (int64_t)time(NULL) * 1000LL; /* coarse wall ms surrogate */

    if (cos_search_run(task->topic, 8, sr, &n) != 0 || n < 1)
        return -1;

    snprintf(result->source_url, sizeof result->source_url, "%s",
             sr[0].url[0] ? sr[0].url : "(no-url)");
    snprintf(result->summary, sizeof result->summary, "%s",
             sr[0].snippet[0] ? sr[0].snippet : sr[0].title);
    result->sigma_after =
        learn_sigma_from_text(result->summary);
    if (result->sigma_after + 1e-4f < result->sigma_before)
        result->verified = cos_learn_verify(result);
    else
        result->verified = 0;
    return 0;
}

int cos_learn_store(const struct cos_learn_result *result) {
    sqlite3_stmt *st = NULL;
    const char *ins =
        "INSERT INTO learn_row(ts,domain_hash,source_url,summary,"
        "sigma_before,sigma_after,verified) VALUES(?,?,?,?,?,?,?)";

    if (result == NULL || g_learn_db == NULL)
        return -1;
    if (sqlite3_prepare_v2(g_learn_db, ins, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)result->timestamp);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)result->domain_hash);
    sqlite3_bind_text(st, 3, result->source_url, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, result->summary, -1, SQLITE_STATIC);
    sqlite3_bind_double(st, 5, (double)result->sigma_before);
    sqlite3_bind_double(st, 6, (double)result->sigma_after);
    sqlite3_bind_int(st, 7, result->verified ? 1 : 0);
    if (sqlite3_step(st) != SQLITE_DONE) {
        sqlite3_finalize(st);
        return -1;
    }
    sqlite3_finalize(st);

    if (result->verified
        || result->sigma_after + 0.02f < result->sigma_before) {
        (void)cos_engram_semantic_merge_learn(result->domain_hash,
                                              result->sigma_after,
                                              result->verified);
    }
    return 0;
}

void cos_learn_report(void) {
    sqlite3_stmt *st = NULL;
    if (g_learn_db == NULL && cos_learn_init() != 0)
        return;
    if (sqlite3_prepare_v2(
            g_learn_db,
            "SELECT ts,domain_hash,verified,sigma_before,sigma_after,"
            "substr(summary,1,120) FROM learn_row ORDER BY id DESC LIMIT 24",
            -1, &st, NULL)
        != SQLITE_OK)
        return;
    fputs("learning_log (last 24)\n", stdout);
    while (sqlite3_step(st) == SQLITE_ROW) {
        printf(
            "  ts=%lld domain=0x%016llx ver=%d σ:%.3f→%.3f | %s\n",
            (long long)sqlite3_column_int64(st, 0),
            (unsigned long long)sqlite3_column_int64(st, 1),
            sqlite3_column_int(st, 2),
            sqlite3_column_double(st, 3), sqlite3_column_double(st, 4),
            (const char *)sqlite3_column_text(st, 5));
    }
    sqlite3_finalize(st);
}

static void learn_usage(FILE *fp) {
    fputs(
        "cos learn — autonomous gap fill (engram σ + COS_SEARCH_API_URL)\n"
        "  --once       run one identify→research→store cycle\n"
        "  --report     print ~/.cos/learning_log.db tail\n"
        "  COS_LEARNING_LOG_DB   override SQLite path\n",
        fp);
}

int cos_learn_main(int argc, char **argv) {
    int once = 0, report = 0;
    int i;

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--once") == 0)
            once = 1;
        else if (strcmp(argv[i], "--report") == 0)
            report = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            learn_usage(stdout);
            return 0;
        }
    }
    if (report) {
        cos_learn_report();
        return 0;
    }
    if (!once) {
        learn_usage(stderr);
        return 2;
    }
    if (cos_learn_init() != 0) {
        fputs("cos learn: SQLite init failed\n", stderr);
        return 3;
    }
    {
        struct cos_learn_task tasks[4];
        int                   nt = 0, k;
        if (cos_learn_identify_gaps(tasks, 3, &nt) != 0) {
            fputs("cos learn: identify_gaps failed\n", stderr);
            return 4;
        }
        if (nt <= 0) {
            fputs("cos learn: no high-σ semantic domains (run Ω / chat first)\n",
                  stderr);
            return 0;
        }
        for (k = 0; k < nt; ++k) {
            struct cos_learn_result res;
            if (cos_learn_research(&tasks[k], &res) != 0) {
                fprintf(stderr, "cos learn: research failed for task %d\n", k);
                continue;
            }
            if (cos_learn_store(&res) != 0)
                fprintf(stderr, "cos learn: store failed for task %d\n", k);
            else
                printf("[learn] domain=0x%016llx σ %.3f→%.3f verified=%d\n",
                       (unsigned long long)res.domain_hash,
                       (double)res.sigma_before, (double)res.sigma_after,
                       res.verified);
        }
    }
    return 0;
}

int cos_learn_engine_self_test(void) {
    char path[512];
    snprintf(path, sizeof path, "/tmp/cos_learn_self_%ld.db",
             (long)getpid());
    remove(path);
    setenv("COS_LEARNING_LOG_DB", path, 1);
    cos_learn_shutdown();
    if (cos_learn_init() != 0)
        return 1;
    if (cos_learn_identify_gaps(NULL, 1, NULL) != -1)
        return 2;
    {
        struct cos_learn_task  t[2];
        struct cos_learn_result r;
        int                    n = 0;
        memset(t, 0, sizeof t);
        t[0].domain_hash   = 0xdeadbeefULL;
        t[0].current_sigma = 0.9f;
        t[0].target_sigma  = 0.3f;
        snprintf(t[0].topic, sizeof t[0].topic, "self-test learn engine");
        (void)n;
        if (cos_learn_research(&t[0], &r) != 0)
            return 3;
        if (cos_learn_store(&r) != 0)
            return 4;
    }
    cos_learn_shutdown();
    remove(path);
    unsetenv("COS_LEARNING_LOG_DB");
    return 0;
}
