/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos memory — episodic / semantic SQLite inspection (AGI stack). */
#include "engram_episodic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

static void episodes_path(char *buf, size_t cap) {
    const char *env = getenv("COS_ENGRAM_EPISODES_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0')
        snprintf(buf, cap, "%s/.cos/engram_episodes.db", home);
    else
        buf[0] = '\0';
}

static void semantic_path(char *buf, size_t cap) {
    const char *env = getenv("COS_ENGRAM_SEMANTIC_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0')
        snprintf(buf, cap, "%s/.cos/engram_semantic.db", home);
    else
        buf[0] = '\0';
}

static int open_db(const char *path, sqlite3 **out) {
    if (path == NULL || path[0] == '\0') return -1;
    return sqlite3_open_v2(path, out,
                             SQLITE_OPEN_READONLY, NULL);
}

int main(int argc, char **argv) {
    int want_ep = 0;
    int want_con = 0;
    int want_forget = 0;
    int want_tau = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--episodes")) want_ep = 1;
        else if (!strcmp(argv[i], "--consolidate")) want_con = 1;
        else if (!strcmp(argv[i], "--forget")) want_forget = 1;
        else if (!strcmp(argv[i], "--tau")) want_tau = 1;
    }

    if (want_con) {
        int n = 5000;
        const char *e = getenv("COS_ENGRAM_CONSOLIDATE_N");
        if (e != NULL && e[0] != '\0') n = atoi(e);
        if (cos_engram_consolidate(n) != 0) {
            fprintf(stderr, "cos memory: consolidate failed\n");
            return 1;
        }
        printf("consolidated (n_recent=%d)\n", n);
        return 0;
    }

    if (want_forget) {
        int64_t older = (int64_t)time(NULL) * 1000LL - 86400000LL * 30LL;
        float rel = 0.2f;
        const char *oe = getenv("COS_ENGRAM_FORGET_OLDER_MS");
        if (oe != NULL && oe[0] != '\0') older = (int64_t)strtoll(oe, NULL, 10);
        const char *re = getenv("COS_ENGRAM_FORGET_REL");
        if (re != NULL && re[0] != '\0') rel = (float)atof(re);
        if (cos_engram_forget(older, rel) != 0) {
            fprintf(stderr, "cos memory: forget failed\n");
            return 1;
        }
        printf("forget policy applied (older_than_ms=%lld rel<%g)\n",
               (long long)older, (double)rel);
        return 0;
    }

    char ep_path[512];
    char sm_path[512];
    episodes_path(ep_path, sizeof(ep_path));
    semantic_path(sm_path, sizeof(sm_path));

    sqlite3 *db = NULL;
    if (want_ep) {
        if (open_db(ep_path, &db) != SQLITE_OK) {
            fprintf(stderr, "cos memory: cannot open %s\n", ep_path);
            return 1;
        }
        sqlite3_stmt *st = NULL;
        const char *sql =
            "SELECT id,ts,prompt_hash,sigma,action,was_correct,attribution "
            "FROM episode ORDER BY id DESC LIMIT 32";
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "cos memory: episodes query failed\n");
            return 1;
        }
        printf("recent episodes (newest first, max 32):\n");
        while (sqlite3_step(st) == SQLITE_ROW) {
            printf("  id=%lld ts=%lld hash=%016llx σ=%.3f act=%d ok=%d attr=%d\n",
                   (long long)sqlite3_column_int64(st, 0),
                   (long long)sqlite3_column_int64(st, 1),
                   (unsigned long long)sqlite3_column_int64(st, 2),
                   sqlite3_column_double(st, 3),
                   sqlite3_column_int(st, 4),
                   sqlite3_column_int(st, 5),
                   sqlite3_column_int(st, 6));
        }
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 0;
    }

    if (want_tau || argc <= 1) {
        if (open_db(sm_path, &db) != SQLITE_OK) {
            fprintf(stderr, "cos memory: cannot open %s\n", sm_path);
            return 1;
        }
        sqlite3_stmt *st = NULL;
        const char *sql =
            "SELECT COUNT(*), AVG(reliability), AVG(tau_local) FROM semantic";
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "cos memory: semantic summary query failed\n");
            return 1;
        }
        double cnt = 0.0, avgr = 0.0, avgt = 0.0;
        if (sqlite3_step(st) == SQLITE_ROW) {
            cnt = sqlite3_column_double(st, 0);
            avgr = sqlite3_column_double(st, 1);
            avgt = sqlite3_column_double(st, 2);
        }
        sqlite3_finalize(st);
        printf("semantic memory (%s):\n", sm_path);
        printf("  domains (rows): %.0f  avg reliability: %.4f  avg τ_local: %.4f\n",
               cnt, avgr, avgt);

        if (want_tau) {
            const char *sq2 =
                "SELECT pattern_hash,sigma_mean,encounter_count,reliability,"
                "tau_local FROM semantic ORDER BY encounter_count DESC LIMIT 64";
            if (sqlite3_prepare_v2(db, sq2, -1, &st, NULL) != SQLITE_OK) {
                sqlite3_close(db);
                return 1;
            }
            printf("  τ per domain (top 64 by encounters):\n");
            while (sqlite3_step(st) == SQLITE_ROW) {
                printf("    hash=%016llx n=%d rel=%.3f τ_loc=%.3f σ̄=%.3f\n",
                       (unsigned long long)sqlite3_column_int64(st, 0),
                       sqlite3_column_int(st, 2),
                       sqlite3_column_double(st, 3),
                       sqlite3_column_double(st, 4),
                       sqlite3_column_double(st, 1));
            }
            sqlite3_finalize(st);
        }
        sqlite3_close(db);
        return 0;
    }

    printf("usage: cos memory [--episodes|--consolidate|--forget|--tau]\n");
    return 0;
}
