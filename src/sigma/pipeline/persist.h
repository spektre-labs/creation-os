/*
 * σ-Persist — durable pipeline state via SQLite WAL.
 *
 * The σ-pipeline (codex + engram + reinforce + live + sovereign
 * cost + τ-calibration) is entirely in-memory at runtime.  For a
 * long-lived agent that is a problem: a crash / restart throws
 * away the engram store (expensive to rebuild), the τ-calibration
 * (user adapted it by hand), and the running cost ledger (GDPR
 * audit trail).  σ-Persist is the module that lands those four
 * facets into a single SQLite file in WAL mode so:
 *
 *   1. concurrent readers do not block writers (WAL-mode guarantee),
 *   2. an uncommitted transaction at crash time leaves the file
 *      in a consistent state (SQLite B-tree + -wal rollback),
 *   3. `cos network migrate` is `cp state.db remote:` — one file
 *      carries every stateful bit of the agent.
 *
 * Schema (v1):
 *
 *   pipeline_meta  (key TEXT PK, value TEXT, updated_ns INT8)
 *   engrams        (key TEXT PK, payload TEXT, sigma REAL, updated_ns INT8)
 *   live_state     (key TEXT PK, value TEXT, updated_ns INT8)
 *   cost_ledger    (id INT8 PK AUTOINCREMENT, provider TEXT,
 *                   eur REAL, eur_cum REAL, sigma REAL, ts_ns INT8)
 *   tau_calibration(tier TEXT PK, tau REAL, updated_ns INT8)
 *
 * All strings are UTF-8; payload fields are opaque JSON the caller
 * serialises.  Version is tracked via PRAGMA user_version and
 * upgraded on open; downgrades are refused.
 *
 * Contracts (v0):
 *   1. `open` creates the file if missing, applies schema, sets
 *      WAL + synchronous=NORMAL + foreign_keys=ON.
 *   2. `save_*` is idempotent by key: a replay of the same call
 *      overwrites the row.
 *   3. `commit` wraps a batch of saves in a single transaction so
 *      callers who want atomic snapshots can opt in.
 *   4. `close` flushes WAL and releases the db handle.  After
 *      close, the handle is NULL-poisoned.
 *   5. When the build-time flag COS_NO_SQLITE is set, the API
 *      is still present but every call returns COS_PERSIST_DISABLED
 *      and callers that want best-effort persistence can still
 *      compile and link.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PERSIST_H
#define COS_SIGMA_PERSIST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_persist_status {
    COS_PERSIST_OK         =  0,
    COS_PERSIST_ERR_ARG    = -1,
    COS_PERSIST_ERR_OPEN   = -2,
    COS_PERSIST_ERR_SCHEMA = -3,
    COS_PERSIST_ERR_IO     = -4,
    COS_PERSIST_DISABLED   = -9,   /* compiled without SQLite         */
};

#define COS_PERSIST_SCHEMA_VERSION 1

typedef struct cos_persist cos_persist_t;   /* opaque                */

/* -------- Lifecycle ------------------------------------------------ */

cos_persist_t *cos_persist_open(const char *path, int *out_status);
int            cos_persist_close(cos_persist_t *db);

/* -------- Transactions -------------------------------------------- */

int cos_persist_begin (cos_persist_t *db);
int cos_persist_commit(cos_persist_t *db);
int cos_persist_rollback(cos_persist_t *db);

/* -------- Save / load helpers ------------------------------------- */

/* Pipeline meta: arbitrary key/value.  `value` is UTF-8 (JSON
 * blob, env, feature flag — the module does not parse it). */
int cos_persist_save_meta(cos_persist_t *db,
                          const char *key, const char *value);
int cos_persist_load_meta(cos_persist_t *db,
                          const char *key,
                          char *out_value, size_t out_cap);

/* Engram store: one row per fact, σ attached. */
int cos_persist_save_engram(cos_persist_t *db,
                            const char *key,
                            const char *payload,
                            float       sigma);

/* Cost ledger: append-only tape of (provider, eur, eur_cum, σ). */
int cos_persist_append_cost(cos_persist_t *db,
                            const char *provider,
                            double      eur,
                            double      eur_cum,
                            float       sigma,
                            uint64_t    ts_ns);

/* τ calibration snapshot: one row per tier (engram, local, api). */
int cos_persist_save_tau(cos_persist_t *db,
                         const char *tier, float tau);

/* -------- Introspection ------------------------------------------- */

/* Returns the current schema version of the DB, or negative on
 * error / disabled. */
int cos_persist_schema_version(cos_persist_t *db);

/* Count rows per table — handy for health checks. */
int cos_persist_count(cos_persist_t *db, const char *table);

/* Returns 1 iff the build honours SQLite, 0 iff stubbed. */
int cos_persist_is_enabled(void);

/* -------- Self-test ----------------------------------------------- */

int cos_persist_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PERSIST_H */
