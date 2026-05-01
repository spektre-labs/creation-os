/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Engram persistence (DEV-3) — SQLite write-through layer.
 *
 * The in-memory engram (engram.h) is allocation-free and O(1), but it
 * evaporates at process exit.  DEV-3 pairs it with a tiny SQLite
 * replica at ~/.cos/engram.db so that
 *
 *   cos chat → "What is 2+2?" → FRESH  (pipeline generates, stores)
 *   cos chat → "What is 2+2?" → CACHE  (engram HIT, σ-at-store < τ)
 *   <restart>
 *   cos chat → "What is 2+2?" → CACHE  (loaded from disk, still HIT)
 *
 * Schema (single table, versioned via `user_version` pragma):
 *
 *   CREATE TABLE IF NOT EXISTS engram (
 *     prompt_hash INTEGER PRIMARY KEY,   -- FNV-1a-64 of the prompt
 *     prompt      TEXT NOT NULL,         -- full text (for debug/export)
 *     response    TEXT NOT NULL,         -- verbatim model output
 *     sigma       REAL NOT NULL,         -- σ-at-store
 *     ts          INTEGER NOT NULL       -- UNIX seconds
 *   );
 *
 * Non-goals:
 *   - This is NOT an embedding store.  It caches exact-match prompts
 *     only; nearest-neighbour retrieval is a separate primitive
 *     (see src/sigma/pipeline/rag.c).
 *   - This is NOT the source of truth at runtime.  The in-memory
 *     engram is.  This layer only persists new writes and rehydrates
 *     on startup.
 *   - No network.  ~/.cos/engram.db is local-only; exporting is a
 *     manual `sqlite3 engram.db .dump` away.
 *
 * Thread-safety: single-threaded.  The CLI is single-threaded today;
 * if we add a streaming writer in DEV-4 we'll need to revisit, but
 * SQLite's own thread-safe mode would suffice (libsqlite3 is compiled
 * with SQLITE_THREADSAFE=1 on every platform we ship).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ENGRAM_PERSIST_H
#define COS_SIGMA_ENGRAM_PERSIST_H

#include <stdint.h>
#include <stddef.h>

#include "engram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle.  Internally wraps an sqlite3* plus the resolved
 * filesystem path and a small number of prepared statements.  Always
 * pair open / close. */
typedef struct cos_engram_persist_s cos_engram_persist_t;

/* Open (creating if absent) the persistence layer at `path`.  When
 * `path` is NULL or empty, defaults to ~/.cos/engram.db (creating the
 * ~/.cos directory if necessary).  The env var COS_ENGRAM_DB
 * overrides both.  Returns 0 on success and writes the handle into
 * `*out`; negative on error.  The handle is NULL on failure.
 *
 * Side effects: runs a one-shot schema migration (IF NOT EXISTS), sets
 * user_version=1, enables journal_mode=WAL for crash-durability. */
int cos_engram_persist_open(const char *path,
                            cos_engram_persist_t **out);

/* Close the handle and release all resources.  Safe on NULL. */
void cos_engram_persist_close(cos_engram_persist_t *p);

/* Write-through: UPSERT one (prompt, hash, response, sigma, ts=now)
 * row.  Matches the shape of cos_pipeline_config_t::on_engram_store
 * so it can be passed directly as that callback.  No return value —
 * errors are logged to stderr with a `bitnet_server`-style prefix. */
void cos_engram_persist_store(const char *prompt,
                              uint64_t    prompt_hash,
                              const char *response,
                              float       sigma,
                              void       *ctx);

/* One-shot rehydrate: iterate every row in the DB and put it into the
 * supplied in-memory engram.  On eviction (engram is full and
 * evict-on-insert fires), the evicted value pointer is NOT freed —
 * `load` intentionally never leaks ownership back into itself.
 * Instead, the engram's own aging policy handles oldest-first
 * eviction from then on.
 *
 * `max_rows` caps how many rows we pull in (0 = unlimited).  Useful
 * when the DB has grown large but the in-memory table is small.
 *
 * Ordering: newest-first (ts DESC) so the most recent wisdom lands
 * first and is least likely to be evicted by the engram's
 * eviction-on-full policy.
 *
 * Returns the number of rows successfully rehydrated (>= 0), or
 * negative on error. */
int cos_engram_persist_load(cos_engram_persist_t *p,
                            cos_sigma_engram_t   *engram,
                            uint32_t              max_rows);

/* Diagnostics: how many rows are stored on disk right now.  Returns
 * -1 on error. */
int64_t cos_engram_persist_row_count(cos_engram_persist_t *p);

/* Diagnostics: resolved filesystem path.  Lifetime is the handle's. */
const char *cos_engram_persist_path(const cos_engram_persist_t *p);

/* ------------------------------------------------------------------ *
 * AGI-1: few-shot exemplars for in-context "test-time training".
 *
 * Pull up to `k` prior rows with σ < max_sigma, excluding the row
 * whose prompt_hash equals `exclude_hash` (the current query).  Rows
 * are ordered by ascending σ (most confident first), then newest
 * `ts` as tie-break.  Each prompt/response is truncated to fit the
 * fixed-width fields — this is a hot-path guard, not a semantic
 * guarantee.
 *
 * Returns the number of exemplars written (0..k), or -1 on error.
 * ------------------------------------------------------------------ */
#define COS_ENGRAM_ICL_PROMPT_MAX    256
#define COS_ENGRAM_ICL_RESPONSE_MAX 512

typedef struct {
    char  prompt[COS_ENGRAM_ICL_PROMPT_MAX];
    char  response[COS_ENGRAM_ICL_RESPONSE_MAX];
    float sigma;
} cos_engram_icl_exemplar_t;

int cos_engram_persist_fetch_icl_exemplars(
    cos_engram_persist_t *p,
    uint64_t exclude_hash,
    float max_sigma,
    int k,
    cos_engram_icl_exemplar_t *out,
    int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_ENGRAM_PERSIST_H */
