/*
 * σ-Evolve optimisation memory — sqlite3 implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "opt_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sqlite3.h>

struct cos_opt_memory_s {
    sqlite3 *db;
};

static const char SCHEMA[] =
    "CREATE TABLE IF NOT EXISTS opt_mutations ("
    "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  ts           INTEGER NOT NULL,"
    "  kernel       TEXT    NOT NULL,"
    "  mutation     TEXT    NOT NULL,"
    "  brier_before REAL    NOT NULL,"
    "  brier_after  REAL    NOT NULL,"
    "  delta        REAL    NOT NULL,"
    "  accepted     INTEGER NOT NULL,"
    "  code_diff    TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_kernel_delta "
    "  ON opt_mutations(kernel, accepted, delta);";

int cos_opt_memory_open(const char *path, cos_opt_memory_t **out) {
    if (!path || !out) return -1;
    cos_opt_memory_t *m = calloc(1, sizeof *m);
    if (!m) return -1;
    if (sqlite3_open(path, &m->db) != SQLITE_OK) {
        sqlite3_close(m->db);
        free(m);
        return -1;
    }
    char *err = NULL;
    if (sqlite3_exec(m->db, SCHEMA, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(m->db);
        free(m);
        return -1;
    }
    *out = m;
    return 0;
}

void cos_opt_memory_close(cos_opt_memory_t *m) {
    if (!m) return;
    if (m->db) sqlite3_close(m->db);
    free(m);
}

int cos_opt_memory_record(cos_opt_memory_t *m,
                          const char *kernel,
                          const char *mutation,
                          double brier_before,
                          double brier_after,
                          int    accepted,
                          const char *code_diff) {
    if (!m || !m->db || !kernel || !mutation) return -1;
    const char *sql =
        "INSERT INTO opt_mutations "
        "(ts, kernel, mutation, brier_before, brier_after, delta, "
        " accepted, code_diff) "
        "VALUES (?,?,?,?,?,?,?,?);";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(m->db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64 (st, 1, (sqlite3_int64)time(NULL));
    sqlite3_bind_text  (st, 2, kernel,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (st, 3, mutation, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 4, brier_before);
    sqlite3_bind_double(st, 5, brier_after);
    sqlite3_bind_double(st, 6, brier_after - brier_before);
    sqlite3_bind_int   (st, 7, accepted ? 1 : 0);
    if (code_diff) sqlite3_bind_text(st, 8, code_diff, -1, SQLITE_TRANSIENT);
    else           sqlite3_bind_null(st, 8);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int cos_opt_memory_top_accepted(cos_opt_memory_t *m,
                                const char *kernel,
                                cos_opt_memory_row_t *rows, int cap) {
    if (!m || !m->db || !rows || cap <= 0) return -1;
    const char *sql_all =
        "SELECT id, ts, kernel, mutation, brier_before, brier_after, "
        "       delta, accepted "
        "FROM opt_mutations WHERE accepted=1 "
        "ORDER BY delta ASC LIMIT ?;";
    const char *sql_k =
        "SELECT id, ts, kernel, mutation, brier_before, brier_after, "
        "       delta, accepted "
        "FROM opt_mutations WHERE accepted=1 AND kernel=? "
        "ORDER BY delta ASC LIMIT ?;";
    sqlite3_stmt *st = NULL;
    int rc;
    if (kernel) {
        if (sqlite3_prepare_v2(m->db, sql_k, -1, &st, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_text(st, 1, kernel, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int (st, 2, cap);
    } else {
        if (sqlite3_prepare_v2(m->db, sql_all, -1, &st, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_int(st, 1, cap);
    }
    int n = 0;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW && n < cap) {
        cos_opt_memory_row_t *r = &rows[n++];
        memset(r, 0, sizeof *r);
        r->id           = sqlite3_column_int64 (st, 0);
        r->ts           = sqlite3_column_int64 (st, 1);
        const unsigned char *k = sqlite3_column_text(st, 2);
        const unsigned char *mu = sqlite3_column_text(st, 3);
        if (k)  snprintf(r->kernel,   sizeof r->kernel,   "%s", (const char *)k);
        if (mu) snprintf(r->mutation, sizeof r->mutation, "%s", (const char *)mu);
        r->brier_before = sqlite3_column_double(st, 4);
        r->brier_after  = sqlite3_column_double(st, 5);
        r->delta        = sqlite3_column_double(st, 6);
        r->accepted     = sqlite3_column_int   (st, 7);
    }
    sqlite3_finalize(st);
    return n;
}

int cos_opt_memory_stats(cos_opt_memory_t *m,
                         const char *kernel,
                         cos_opt_memory_stats_t *out) {
    if (!m || !m->db || !out) return -1;
    memset(out, 0, sizeof *out);
    const char *sql_all =
        "SELECT COUNT(*), SUM(accepted), MIN(delta), "
        "       AVG(CASE accepted WHEN 1 THEN delta ELSE NULL END) "
        "FROM opt_mutations;";
    const char *sql_k =
        "SELECT COUNT(*), SUM(accepted), MIN(delta), "
        "       AVG(CASE accepted WHEN 1 THEN delta ELSE NULL END) "
        "FROM opt_mutations WHERE kernel=?;";
    sqlite3_stmt *st = NULL;
    if (kernel) {
        if (sqlite3_prepare_v2(m->db, sql_k, -1, &st, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_text(st, 1, kernel, -1, SQLITE_TRANSIENT);
    } else {
        if (sqlite3_prepare_v2(m->db, sql_all, -1, &st, NULL) != SQLITE_OK)
            return -1;
    }
    if (sqlite3_step(st) == SQLITE_ROW) {
        out->n_total             = sqlite3_column_int   (st, 0);
        out->n_accepted          = sqlite3_column_int   (st, 1);
        out->best_delta          = sqlite3_column_double(st, 2);
        out->mean_delta_accepted = sqlite3_column_double(st, 3);
    }
    sqlite3_finalize(st);
    return 0;
}

/* ------------------ self-test ------------------ */

int cos_sigma_opt_memory_self_test(void) {
    char path[128];
    snprintf(path, sizeof path, "/tmp/cos_opt_memory_selftest_%d.db",
             (int)getpid());
    unlink(path);
    cos_opt_memory_t *m = NULL;
    if (cos_opt_memory_open(path, &m) != 0) return 1;
    if (cos_opt_memory_record(m, "sigma_router", "w_lp +0.03",
                              0.40, 0.35, 1, "diff...") != 0) return 2;
    if (cos_opt_memory_record(m, "sigma_router", "w_ent +0.10",
                              0.40, 0.42, 0, NULL) != 0) return 3;
    if (cos_opt_memory_record(m, "sigma_router", "w_lp +0.05",
                              0.40, 0.32, 1, NULL) != 0) return 4;
    if (cos_opt_memory_record(m, "engram",       "cap +128",
                              0.40, 0.38, 1, NULL) != 0) return 5;
    cos_opt_memory_row_t rows[8];
    int n = cos_opt_memory_top_accepted(m, "sigma_router", rows, 8);
    if (n != 2) return 6;
    /* sort: most-negative delta first */
    if (!(rows[0].delta <= rows[1].delta)) return 7;
    cos_opt_memory_stats_t st;
    if (cos_opt_memory_stats(m, NULL, &st) != 0) return 8;
    if (st.n_total != 4) return 9;
    if (st.n_accepted != 3) return 10;
    cos_opt_memory_close(m);
    unlink(path);
    return 0;
}
