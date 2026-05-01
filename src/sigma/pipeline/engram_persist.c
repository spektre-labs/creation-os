/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Engram persistence — SQLite write-through.  See engram_persist.h
 * for the design narrative.
 *
 * Implementation notes:
 *
 *   - Hand-rolled path resolution (expand_home) avoids pulling in
 *     wordexp(3) or shell-escape semantics.  We only support the
 *     leading `~/` form, which covers every realistic COS_ENGRAM_DB
 *     override we've seen.
 *
 *   - `mkdir_p(".cos")` is implemented by hand (two segments deep)
 *     rather than via system("mkdir -p …") to keep the hot path
 *     allocation-small and side-effect-free from a portability
 *     standpoint.
 *
 *   - Prepared statements: we compile INSERT-UPSERT and the `SELECT`
 *     for rehydration once at open().  That keeps per-turn cost at a
 *     handful of bound-parameter calls + one `sqlite3_step`.
 *
 *   - prompt_hash collision safety: FNV-1a-64 has a ≈1.8e-10 double-
 *     collision probability at 10^4 entries (which is the default
 *     engram capacity ceiling).  For the same hash we do UPSERT on
 *     conflict (newer wisdom wins) — the worst case is a stale cache
 *     hit, which the σ-gate then handles correctly via hit.sigma_at_
 *     store ≥ τ_accept → regenerate.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#  define _DARWIN_C_SOURCE
#endif

#include "engram_persist.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

/* ===================================================================== */
/* handle                                                                */
/* ===================================================================== */

struct cos_engram_persist_s {
    sqlite3       *db;
    sqlite3_stmt  *stmt_upsert;   /* INSERT ... ON CONFLICT ... UPDATE   */
    sqlite3_stmt  *stmt_select;   /* SELECT ... ORDER BY ts DESC LIMIT ? */
    char           path[1024];
};

/* ===================================================================== */
/* helpers                                                               */
/* ===================================================================== */

static void warn_sqlite(const char *where, sqlite3 *db) {
    fprintf(stderr, "engram_persist: %s: %s\n",
            where, db ? sqlite3_errmsg(db) : "(no db)");
}

static int resolve_home(char *buf, size_t n) {
    const char *h = getenv("HOME");
    if (h == NULL || h[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        h = (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : NULL;
    }
    if (h == NULL) return -1;
    size_t len = strlen(h);
    if (len + 1 > n) return -1;
    memcpy(buf, h, len);
    buf[len] = '\0';
    return 0;
}

static int ensure_dir(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (errno == ENOENT) {
        if (mkdir(dir, 0700) == 0) return 0;
        return -1;
    }
    return -1;
}

/* Expand leading `~/` and create the parent dir as needed.  Result
 * written into `out` (cap `n`).  Returns 0 on success, -1 on error. */
static int expand_and_prepare(const char *in, char *out, size_t n) {
    char home[512];
    const char *p = in;
    size_t used = 0;

    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
        if (resolve_home(home, sizeof home) != 0) return -1;
        size_t hlen = strlen(home);
        if (hlen >= n) return -1;
        memcpy(out, home, hlen);
        used = hlen;
        p = in + 1; /* skip '~' */
    }
    size_t rem = strlen(p);
    if (used + rem + 1 > n) return -1;
    memcpy(out + used, p, rem);
    out[used + rem] = '\0';

    /* Ensure parent directory exists. */
    char parent[1024];
    if (strlen(out) + 1 > sizeof parent) return -1;
    snprintf(parent, sizeof parent, "%s", out);
    char *slash = strrchr(parent, '/');
    if (slash != NULL) {
        *slash = '\0';
        if (parent[0] != '\0') {
            /* walk ancestors, creating missing segments */
            char tmp[1024];
            size_t pl = strlen(parent);
            if (pl >= sizeof tmp) return -1;
            memcpy(tmp, parent, pl);
            tmp[pl] = '\0';
            for (size_t i = 1; i < pl; i++) {
                if (tmp[i] == '/') {
                    tmp[i] = '\0';
                    (void)ensure_dir(tmp);
                    tmp[i] = '/';
                }
            }
            if (ensure_dir(tmp) != 0) return -1;
        }
    }
    return 0;
}

static int resolve_path(const char *requested, char *out, size_t n) {
    const char *env = getenv("COS_ENGRAM_DB");
    if (env != NULL && env[0] != '\0') {
        return expand_and_prepare(env, out, n);
    }
    if (requested != NULL && requested[0] != '\0') {
        return expand_and_prepare(requested, out, n);
    }
    return expand_and_prepare("~/.cos/engram.db", out, n);
}

/* ===================================================================== */
/* schema                                                                */
/* ===================================================================== */

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS engram ("
    "  prompt_hash INTEGER PRIMARY KEY,"
    "  prompt      TEXT NOT NULL,"
    "  response    TEXT NOT NULL,"
    "  sigma       REAL NOT NULL,"
    "  ts          INTEGER NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS engram_ts_idx ON engram(ts);"
    "PRAGMA user_version = 1;"
    "PRAGMA journal_mode = WAL;"
    "PRAGMA synchronous  = NORMAL;";

static int apply_schema(sqlite3 *db) {
    char *err = NULL;
    if (sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "engram_persist: schema: %s\n",
                err ? err : "(null)");
        if (err) sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* ===================================================================== */
/* public API                                                            */
/* ===================================================================== */

int cos_engram_persist_open(const char *path,
                            cos_engram_persist_t **out) {
    if (out == NULL) return -1;
    *out = NULL;

    cos_engram_persist_t *p = calloc(1, sizeof(*p));
    if (p == NULL) return -1;

    if (resolve_path(path, p->path, sizeof p->path) != 0) {
        free(p);
        return -1;
    }

    if (sqlite3_open(p->path, &p->db) != SQLITE_OK) {
        warn_sqlite("open", p->db);
        if (p->db) sqlite3_close(p->db);
        free(p);
        return -1;
    }

    if (apply_schema(p->db) != 0) {
        sqlite3_close(p->db);
        free(p);
        return -1;
    }

    /* Prepare UPSERT. */
    const char *UPSERT_SQL =
        "INSERT INTO engram (prompt_hash, prompt, response, sigma, ts) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(prompt_hash) DO UPDATE SET "
        "  prompt   = excluded.prompt,"
        "  response = excluded.response,"
        "  sigma    = excluded.sigma,"
        "  ts       = excluded.ts";
    if (sqlite3_prepare_v2(p->db, UPSERT_SQL, -1,
                           &p->stmt_upsert, NULL) != SQLITE_OK) {
        warn_sqlite("prepare UPSERT", p->db);
        sqlite3_close(p->db);
        free(p);
        return -1;
    }

    /* Prepare SELECT for rehydration (NEWEST-FIRST). */
    const char *SELECT_SQL =
        "SELECT prompt_hash, prompt, response, sigma "
        "FROM engram ORDER BY ts DESC LIMIT ?";
    if (sqlite3_prepare_v2(p->db, SELECT_SQL, -1,
                           &p->stmt_select, NULL) != SQLITE_OK) {
        warn_sqlite("prepare SELECT", p->db);
        sqlite3_finalize(p->stmt_upsert);
        sqlite3_close(p->db);
        free(p);
        return -1;
    }

    *out = p;
    return 0;
}

void cos_engram_persist_close(cos_engram_persist_t *p) {
    if (p == NULL) return;
    if (p->stmt_upsert) sqlite3_finalize(p->stmt_upsert);
    if (p->stmt_select) sqlite3_finalize(p->stmt_select);
    if (p->db)          sqlite3_close(p->db);
    free(p);
}

void cos_engram_persist_store(const char *prompt,
                              uint64_t    prompt_hash,
                              const char *response,
                              float       sigma,
                              void       *ctx) {
    cos_engram_persist_t *p = (cos_engram_persist_t *)ctx;
    if (p == NULL || p->db == NULL || p->stmt_upsert == NULL) return;
    if (prompt == NULL || response == NULL) return;

    sqlite3_stmt *s = p->stmt_upsert;
    sqlite3_reset(s);
    /* SQLite INTEGER column takes int64; FNV-1a-64 is u64.  We rely
     * on two's-complement round-trip: the column stores the signed
     * view, the PK equality still holds because the INSERT and
     * SELECT both round-trip through int64 identically. */
    sqlite3_bind_int64(s, 1, (sqlite3_int64)prompt_hash);
    sqlite3_bind_text (s, 2, prompt,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 3, response, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 4, (double)sigma);
    sqlite3_bind_int64(s, 5, (sqlite3_int64)time(NULL));

    int rc = sqlite3_step(s);
    if (rc != SQLITE_DONE) {
        warn_sqlite("step UPSERT", p->db);
    }
    sqlite3_reset(s);
}

int cos_engram_persist_load(cos_engram_persist_t *p,
                            cos_sigma_engram_t   *engram,
                            uint32_t              max_rows) {
    if (p == NULL || engram == NULL || p->stmt_select == NULL) return -1;

    sqlite3_stmt *s = p->stmt_select;
    sqlite3_reset(s);
    /* 0 or negative → load everything (SQLite's LIMIT -1 means "no
     * limit"). */
    sqlite3_bind_int64(s, 1, (max_rows == 0)
                             ? (sqlite3_int64)-1
                             : (sqlite3_int64)max_rows);

    int n_loaded = 0;
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        uint64_t    h   = (uint64_t)sqlite3_column_int64(s, 0);
        /* s[1] = prompt (unused by in-memory engram; kept for debug). */
        const unsigned char *resp = sqlite3_column_text(s, 2);
        double               sig  = sqlite3_column_double(s, 3);
        if (resp == NULL) continue;

        char *copy = strdup((const char *)resp);
        if (copy == NULL) continue;

        uint64_t evicted = 0;
        int pr = cos_sigma_engram_put(engram, h, (float)sig, copy, &evicted);
        if (pr < 0) {
            free(copy);
            break;
        }
        /* If pr == 1 (eviction fired) we intentionally leak the
         * evicted slot's value pointer — we don't own it here; the
         * CLI drains the whole table via
         * cos_sigma_pipeline_free_engram_values() at shutdown. */
        n_loaded++;
    }
    if (rc != SQLITE_DONE && rc != SQLITE_OK && rc != SQLITE_ROW) {
        warn_sqlite("step SELECT", p->db);
    }
    sqlite3_reset(s);
    return n_loaded;
}

int64_t cos_engram_persist_row_count(cos_engram_persist_t *p) {
    if (p == NULL || p->db == NULL) return -1;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(p->db, "SELECT COUNT(*) FROM engram", -1,
                           &s, NULL) != SQLITE_OK) {
        warn_sqlite("prepare COUNT", p->db);
        return -1;
    }
    int64_t out = -1;
    if (sqlite3_step(s) == SQLITE_ROW) {
        out = (int64_t)sqlite3_column_int64(s, 0);
    }
    sqlite3_finalize(s);
    return out;
}

const char *cos_engram_persist_path(const cos_engram_persist_t *p) {
    return (p != NULL) ? p->path : NULL;
}

/* ------------------------------------------------------------------ *
 * AGI-1 — ICL exemplar fetch.
 * ------------------------------------------------------------------ */
static void copy_trunc(char *dst, size_t dst_cap, const char *src) {
    if (dst == NULL || dst_cap == 0) return;
    dst[0] = '\0';
    if (src == NULL) return;
    size_t n = strlen(src);
    if (n >= dst_cap) n = dst_cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int cos_engram_persist_fetch_icl_exemplars(
    cos_engram_persist_t *p,
    uint64_t exclude_hash,
    float max_sigma,
    int k,
    cos_engram_icl_exemplar_t *out,
    int out_cap) {
    if (p == NULL || p->db == NULL || out == NULL || out_cap <= 0)
        return -1;
    if (k <= 0) return 0;
    if (k > out_cap) k = out_cap;

    const char *sql =
        "SELECT prompt, response, sigma FROM engram "
        "WHERE prompt_hash != ?1 AND sigma < ?2 "
        "ORDER BY sigma ASC, ts DESC LIMIT ?3";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(p->db, sql, -1, &st, NULL) != SQLITE_OK) {
        warn_sqlite("prepare ICL SELECT", p->db);
        return -1;
    }
    sqlite3_bind_int64(st, 1, (sqlite3_int64)exclude_hash);
    sqlite3_bind_double(st, 2, (double)max_sigma);
    sqlite3_bind_int(st, 3, k);

    int n = 0;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW && n < k) {
        const unsigned char *pr = sqlite3_column_text(st, 0);
        const unsigned char *rs = sqlite3_column_text(st, 1);
        double sig = sqlite3_column_double(st, 2);
        copy_trunc(out[n].prompt, sizeof out[n].prompt,
                   pr ? (const char *)pr : "");
        copy_trunc(out[n].response, sizeof out[n].response,
                   rs ? (const char *)rs : "");
        out[n].sigma = (float)sig;
        n++;
    }
    if (rc != SQLITE_DONE && rc != SQLITE_ROW && rc != SQLITE_OK) {
        warn_sqlite("step ICL SELECT", p->db);
    }
    sqlite3_finalize(st);
    return n;
}
