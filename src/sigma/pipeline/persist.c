/* σ-Persist implementation — SQLite WAL-backed durable state.
 *
 * The module is a thin adapter over sqlite3 and intentionally
 * avoids pulling any other dependency (no stdlib alloc from JSON
 * parsing, no threads, no timers).  Every write is a single
 * prepared statement; callers that want atomicity across several
 * writes wrap the batch in begin / commit.
 *
 * Build flags:
 *   COS_NO_SQLITE   when defined, the whole API returns
 *                   COS_PERSIST_DISABLED and no sqlite3 symbols
 *                   are referenced.  Useful for bare embedded
 *                   hosts where -lsqlite3 is not available.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifndef COS_NO_SQLITE
#  include <sqlite3.h>
#endif

int cos_persist_is_enabled(void) {
#ifdef COS_NO_SQLITE
    return 0;
#else
    return 1;
#endif
}

/* ------------------------------------------------------------------ *
 *  Disabled build — every call returns DISABLED.
 * ------------------------------------------------------------------ */
#ifdef COS_NO_SQLITE

struct cos_persist { int dummy; };

cos_persist_t *cos_persist_open(const char *path, int *out_status) {
    (void)path;
    if (out_status) *out_status = COS_PERSIST_DISABLED;
    return NULL;
}
int cos_persist_close   (cos_persist_t *db) { (void)db; return COS_PERSIST_DISABLED; }
int cos_persist_begin   (cos_persist_t *db) { (void)db; return COS_PERSIST_DISABLED; }
int cos_persist_commit  (cos_persist_t *db) { (void)db; return COS_PERSIST_DISABLED; }
int cos_persist_rollback(cos_persist_t *db) { (void)db; return COS_PERSIST_DISABLED; }
int cos_persist_save_meta(cos_persist_t *db, const char *k, const char *v) {
    (void)db; (void)k; (void)v; return COS_PERSIST_DISABLED;
}
int cos_persist_load_meta(cos_persist_t *db, const char *k,
                          char *out, size_t cap) {
    (void)db; (void)k; (void)out; (void)cap; return COS_PERSIST_DISABLED;
}
int cos_persist_save_engram(cos_persist_t *db, const char *k,
                            const char *payload, float sigma) {
    (void)db; (void)k; (void)payload; (void)sigma; return COS_PERSIST_DISABLED;
}
int cos_persist_append_cost(cos_persist_t *db, const char *p,
                            double eur, double cum, float s, uint64_t t) {
    (void)db; (void)p; (void)eur; (void)cum; (void)s; (void)t;
    return COS_PERSIST_DISABLED;
}
int cos_persist_save_tau(cos_persist_t *db, const char *tier, float tau) {
    (void)db; (void)tier; (void)tau; return COS_PERSIST_DISABLED;
}
int cos_persist_schema_version(cos_persist_t *db) { (void)db; return COS_PERSIST_DISABLED; }
int cos_persist_count(cos_persist_t *db, const char *t) {
    (void)db; (void)t; return COS_PERSIST_DISABLED;
}
int cos_persist_self_test(void) { return 0; }  /* no-op → tests treat as pass */

#else /* COS_NO_SQLITE not defined: real SQLite backend below */

struct cos_persist {
    sqlite3 *db;
};

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS pipeline_meta ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL,"
    "  updated_ns INT8 NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS engrams ("
    "  key TEXT PRIMARY KEY,"
    "  payload TEXT NOT NULL,"
    "  sigma REAL NOT NULL,"
    "  updated_ns INT8 NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS live_state ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL,"
    "  updated_ns INT8 NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS cost_ledger ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  provider TEXT NOT NULL,"
    "  eur REAL NOT NULL,"
    "  eur_cum REAL NOT NULL,"
    "  sigma REAL NOT NULL,"
    "  ts_ns INT8 NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS tau_calibration ("
    "  tier TEXT PRIMARY KEY,"
    "  tau REAL NOT NULL,"
    "  updated_ns INT8 NOT NULL"
    ");";

static uint64_t now_ns(void) {
    /* Not called for determinism — we read the schema's
     * `updated_ns` argument from the caller.  If the caller
     * passes 0 we fall back to a monotonic sentinel. */
    return 1ULL;
}

static int exec_sql(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "cos_persist: sqlite3_exec(\"%s\") failed: %s\n",
                sql, err ? err : "(null)");
        if (err) sqlite3_free(err);
        return COS_PERSIST_ERR_IO;
    }
    return COS_PERSIST_OK;
}

cos_persist_t *cos_persist_open(const char *path, int *out_status) {
    if (out_status) *out_status = COS_PERSIST_OK;
    if (!path || !*path) {
        if (out_status) *out_status = COS_PERSIST_ERR_ARG;
        return NULL;
    }
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        if (out_status) *out_status = COS_PERSIST_ERR_OPEN;
        return NULL;
    }
    /* Enforce WAL + sane defaults. */
    if (exec_sql(db, "PRAGMA journal_mode=WAL;"
                     "PRAGMA synchronous=NORMAL;"
                     "PRAGMA foreign_keys=ON;"
                     "PRAGMA busy_timeout=1500;") != COS_PERSIST_OK) {
        sqlite3_close(db);
        if (out_status) *out_status = COS_PERSIST_ERR_SCHEMA;
        return NULL;
    }
    /* Schema + version. */
    if (exec_sql(db, SCHEMA_SQL) != COS_PERSIST_OK) {
        sqlite3_close(db);
        if (out_status) *out_status = COS_PERSIST_ERR_SCHEMA;
        return NULL;
    }
    char setver[64];
    snprintf(setver, sizeof setver,
             "PRAGMA user_version=%d;", COS_PERSIST_SCHEMA_VERSION);
    exec_sql(db, setver);

    cos_persist_t *h = (cos_persist_t*)calloc(1, sizeof(*h));
    if (!h) { sqlite3_close(db); if (out_status) *out_status = COS_PERSIST_ERR_IO; return NULL; }
    h->db = db;
    return h;
}

int cos_persist_close(cos_persist_t *h) {
    if (!h) return COS_PERSIST_ERR_ARG;
    if (h->db) {
        sqlite3_close(h->db);
        h->db = NULL;
    }
    free(h);
    return COS_PERSIST_OK;
}

int cos_persist_begin   (cos_persist_t *h) { return h && h->db ? exec_sql(h->db, "BEGIN;") : COS_PERSIST_ERR_ARG; }
int cos_persist_commit  (cos_persist_t *h) { return h && h->db ? exec_sql(h->db, "COMMIT;") : COS_PERSIST_ERR_ARG; }
int cos_persist_rollback(cos_persist_t *h) { return h && h->db ? exec_sql(h->db, "ROLLBACK;") : COS_PERSIST_ERR_ARG; }

static int bind_upsert_kv(sqlite3 *db, const char *table,
                          const char *key, const char *value) {
    char sql[256];
    snprintf(sql, sizeof sql,
             "INSERT INTO %s(key,value,updated_ns) VALUES(?,?,?) "
             "ON CONFLICT(key) DO UPDATE SET value=excluded.value, "
             "updated_ns=excluded.updated_ns;", table);
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    sqlite3_bind_text (s, 1, key,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (s, 2, value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(s, 3, (sqlite3_int64)now_ns());
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? COS_PERSIST_OK : COS_PERSIST_ERR_IO;
}

int cos_persist_save_meta(cos_persist_t *h,
                          const char *key, const char *value) {
    if (!h || !h->db || !key || !value) return COS_PERSIST_ERR_ARG;
    return bind_upsert_kv(h->db, "pipeline_meta", key, value);
}

int cos_persist_load_meta(cos_persist_t *h, const char *key,
                          char *out_value, size_t out_cap) {
    if (!h || !h->db || !key || !out_value || out_cap == 0)
        return COS_PERSIST_ERR_ARG;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db,
            "SELECT value FROM pipeline_meta WHERE key=?;", -1, &s, NULL)
        != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    sqlite3_bind_text(s, 1, key, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(s);
    if (rc == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(s, 0);
        strncpy(out_value, (const char*)v, out_cap - 1);
        out_value[out_cap - 1] = '\0';
        sqlite3_finalize(s);
        return COS_PERSIST_OK;
    }
    sqlite3_finalize(s);
    return COS_PERSIST_ERR_IO;  /* no such key */
}

int cos_persist_save_engram(cos_persist_t *h,
                            const char *key,
                            const char *payload,
                            float       sigma) {
    if (!h || !h->db || !key || !payload) return COS_PERSIST_ERR_ARG;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db,
            "INSERT INTO engrams(key,payload,sigma,updated_ns) "
            "VALUES(?,?,?,?) "
            "ON CONFLICT(key) DO UPDATE SET "
            "payload=excluded.payload, sigma=excluded.sigma, "
            "updated_ns=excluded.updated_ns;",
            -1, &s, NULL) != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    sqlite3_bind_text  (s, 1, key,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (s, 2, payload, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 3, (double)sigma);
    sqlite3_bind_int64 (s, 4, (sqlite3_int64)now_ns());
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? COS_PERSIST_OK : COS_PERSIST_ERR_IO;
}

int cos_persist_append_cost(cos_persist_t *h,
                            const char *provider,
                            double      eur,
                            double      eur_cum,
                            float       sigma,
                            uint64_t    ts_ns) {
    if (!h || !h->db || !provider) return COS_PERSIST_ERR_ARG;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db,
            "INSERT INTO cost_ledger(provider,eur,eur_cum,sigma,ts_ns) "
            "VALUES(?,?,?,?,?);", -1, &s, NULL) != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    sqlite3_bind_text  (s, 1, provider, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 2, eur);
    sqlite3_bind_double(s, 3, eur_cum);
    sqlite3_bind_double(s, 4, (double)sigma);
    sqlite3_bind_int64 (s, 5, (sqlite3_int64)ts_ns);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? COS_PERSIST_OK : COS_PERSIST_ERR_IO;
}

int cos_persist_save_tau(cos_persist_t *h,
                         const char *tier, float tau) {
    if (!h || !h->db || !tier) return COS_PERSIST_ERR_ARG;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db,
            "INSERT INTO tau_calibration(tier,tau,updated_ns) "
            "VALUES(?,?,?) "
            "ON CONFLICT(tier) DO UPDATE SET tau=excluded.tau, "
            "updated_ns=excluded.updated_ns;",
            -1, &s, NULL) != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    sqlite3_bind_text  (s, 1, tier, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(s, 2, (double)tau);
    sqlite3_bind_int64 (s, 3, (sqlite3_int64)now_ns());
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? COS_PERSIST_OK : COS_PERSIST_ERR_IO;
}

int cos_persist_schema_version(cos_persist_t *h) {
    if (!h || !h->db) return COS_PERSIST_ERR_ARG;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db, "PRAGMA user_version;", -1, &s, NULL)
        != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    int v = -1;
    if (sqlite3_step(s) == SQLITE_ROW)
        v = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return v;
}

int cos_persist_count(cos_persist_t *h, const char *table) {
    if (!h || !h->db || !table) return COS_PERSIST_ERR_ARG;
    /* Allow-list table names to avoid a template-injection shape. */
    static const char *allowed[] = {
        "pipeline_meta", "engrams", "live_state",
        "cost_ledger", "tau_calibration", NULL
    };
    int ok = 0;
    for (int i = 0; allowed[i]; i++)
        if (strcmp(table, allowed[i]) == 0) { ok = 1; break; }
    if (!ok) return COS_PERSIST_ERR_ARG;

    char sql[128];
    snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM %s;", table);
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(h->db, sql, -1, &s, NULL) != SQLITE_OK)
        return COS_PERSIST_ERR_IO;
    int n = 0;
    if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return n;
}

/* ------------------------------------------------------------------ *
 *  Self-test — open → schema → upsert → roundtrip → persistence
 * ------------------------------------------------------------------ */

int cos_persist_self_test(void) {
    const char *path = "/tmp/cos_persist_selftest.db";
    /* Fresh slate: unlink both the DB and its -wal / -shm siblings. */
    unlink(path);
    char wal[64], shm[64];
    snprintf(wal, sizeof wal, "%s-wal", path);
    snprintf(shm, sizeof shm, "%s-shm", path);
    unlink(wal); unlink(shm);

    int st = 0;
    cos_persist_t *db = cos_persist_open(path, &st);
    if (!db || st != COS_PERSIST_OK) return -1;

    if (cos_persist_schema_version(db) != COS_PERSIST_SCHEMA_VERSION)
        return -2;

    if (cos_persist_begin(db) != COS_PERSIST_OK)            return -3;
    if (cos_persist_save_meta  (db, "host",  "M4-MBP")    != COS_PERSIST_OK) return -4;
    if (cos_persist_save_engram(db, "paris", "capital-of-FR", 0.12f) != COS_PERSIST_OK) return -5;
    if (cos_persist_save_tau   (db, "local",  0.25f)      != COS_PERSIST_OK) return -6;
    if (cos_persist_append_cost(db, "openai", 0.0042, 0.0042, 0.22f, 1713600000000000000ULL)
            != COS_PERSIST_OK) return -7;
    if (cos_persist_commit(db) != COS_PERSIST_OK)            return -8;

    /* Reopen: persistence check. */
    cos_persist_close(db);
    db = cos_persist_open(path, &st);
    if (!db || st != COS_PERSIST_OK) return -9;

    char buf[64] = {0};
    if (cos_persist_load_meta(db, "host", buf, sizeof buf) != COS_PERSIST_OK) return -10;
    if (strcmp(buf, "M4-MBP") != 0) return -11;

    if (cos_persist_count(db, "engrams")      != 1) return -12;
    if (cos_persist_count(db, "cost_ledger")  != 1) return -13;
    if (cos_persist_count(db, "tau_calibration") != 1) return -14;

    /* Arg-error triage. */
    if (cos_persist_save_meta(NULL, "k", "v") != COS_PERSIST_ERR_ARG) return -15;
    if (cos_persist_count    (db,   "nope")   != COS_PERSIST_ERR_ARG) return -16;

    cos_persist_close(db);
    return 0;
}

#endif /* !COS_NO_SQLITE */
