/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v115 σ-Memory — persistent cross-session state with σ-weighted
 * recall.
 *
 * Storage model: a single SQLite file at `~/.creation-os/memory.sqlite`
 * with three tables:
 *
 *   episodic_memory (
 *     id             INTEGER PRIMARY KEY,
 *     content        TEXT    NOT NULL,
 *     embedding      BLOB    NOT NULL,      -- 384 × float32 LE
 *     sigma_product  REAL    NOT NULL,      -- σ at write time
 *     session_id     TEXT,
 *     tags           TEXT,                    -- comma-joined tags
 *     created_at     INTEGER NOT NULL         -- unix seconds
 *   );
 *
 *   knowledge_base  (
 *     id             INTEGER PRIMARY KEY,
 *     source_file    TEXT,
 *     chunk          TEXT    NOT NULL,
 *     embedding      BLOB    NOT NULL,
 *     doc_type       TEXT,
 *     metadata_json  TEXT
 *   );
 *
 *   chat_history   (
 *     id             INTEGER PRIMARY KEY,
 *     session_id     TEXT    NOT NULL,
 *     role           TEXT    NOT NULL,        -- "user"|"assistant"|"system"
 *     content        TEXT    NOT NULL,
 *     sigma_profile  TEXT,                    -- v101 8-channel JSON
 *     created_at     INTEGER NOT NULL
 *   );
 *
 * σ innovation: every `episodic_memory` row carries the σ_product
 * observed on the generation that produced it.  Recall ranks by
 *
 *     score(q,m) =  cosine(q, m) / (1 + λ · σ_product(m))
 *
 * i.e. uncertain memories are down-weighted, not silently mixed into
 * context.  No other RAG framework does this today.
 *
 * Embeddings: v115.0 ships a pure-C deterministic hash-shingled
 * 384-dimensional embedding ("sigma-hash-embed") — zero deps,
 * zero download.  It is a shape-correct placeholder that makes the
 * SQL / σ-weighting / API contracts testable.  Real MiniLM-L6-v2
 * (ONNX) drop-in is scheduled for v115.1; the `embed_fn` is an
 * indirect-called function pointer, so the rest of the stack is
 * frozen against a future upgrade.
 *
 * This kernel is optional: if libsqlite3 is absent at compile time
 * (−DCOS_V115_NO_SQLITE=1), every API returns -ENOSYS and the CLI /
 * HTTP endpoints advertise "memory unavailable on this build".
 */
#ifndef COS_V115_MEMORY_H
#define COS_V115_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed embedding dimension.  Chosen to match MiniLM-L6-v2 so the
 * v115.1 ONNX swap-in is field-compatible. */
#define COS_V115_EMBED_DIM        384

#define COS_V115_SESSION_MAX      64
#define COS_V115_TAGS_MAX         256
#define COS_V115_CONTENT_MAX      8192
#define COS_V115_TOPK_MAX         32

/* Opaque handle — wraps an sqlite3* + prepared statements. */
typedef struct cos_v115_store cos_v115_store_t;

typedef struct cos_v115_config {
    char   db_path[1024];     /* defaults to $HOME/.creation-os/memory.sqlite */
    /* Recall weighting: score = cos / (1 + lambda * sigma_product).
     * 0 disables σ-weighting (pure cosine).  Default 1.0. */
    float  recall_lambda;
    /* Optional cap on memory rows to retain (LRU prune on write);
     * 0 = unlimited. */
    int    max_rows_episodic;
    int    max_rows_knowledge;
} cos_v115_config_t;

typedef struct cos_v115_memory_row {
    int64_t id;
    char    content[COS_V115_CONTENT_MAX];
    float   sigma_product;
    int64_t created_at;
    char    tags[COS_V115_TAGS_MAX];
    char    session_id[COS_V115_SESSION_MAX];
    /* Only populated on search results, not raw fetches. */
    float   score;            /* σ-weighted cosine; higher is better */
    float   cosine;           /* raw cosine for diagnostics */
} cos_v115_memory_row_t;

/* Population knobs & errors. */
void cos_v115_config_defaults(cos_v115_config_t *cfg);
int  cos_v115_default_db_path(char *out, size_t cap);

/* Open (create on first use).  Returns 0 on success, -1 on errors,
 * +1 when the build has no sqlite support (out_store = NULL in that
 * case — callers should treat memory operations as "disabled"). */
int  cos_v115_open(const cos_v115_config_t *cfg, cos_v115_store_t **out);
void cos_v115_close(cos_v115_store_t *s);

/* Compute a 384-d embedding for `text` into `out[384]`.
 * Implementation is the deterministic hash-shingled embedder;
 * v115.1 will overload this with a function pointer set at open-time
 * so real MiniLM weights can take over without breaking the API. */
void cos_v115_embed(const char *text, float *out);

/* cosine(a, b) on COS_V115_EMBED_DIM-dimensional vectors, assumed
 * already L2-normalised.  Safe for zero-norm inputs (returns 0.f). */
float cos_v115_cosine(const float *a, const float *b);

/* Write an episodic memory row.  Returns the new row id on success
 * (> 0), or a negative error code.  The caller provides the σ
 * observed at generation time; v115 stores it verbatim. */
int64_t cos_v115_write_episodic(cos_v115_store_t *s,
                                 const char *content,
                                 float       sigma_product,
                                 const char *session_id,
                                 const char *tags_csv);

/* Write a knowledge chunk (from a document ingestion pipeline). */
int64_t cos_v115_write_knowledge(cos_v115_store_t *s,
                                  const char *source_file,
                                  const char *chunk,
                                  const char *doc_type,
                                  const char *metadata_json);

/* Append a chat_history row. */
int64_t cos_v115_write_chat(cos_v115_store_t *s,
                             const char *session_id,
                             const char *role,
                             const char *content,
                             const char *sigma_profile_json);

/* Search top-k by σ-weighted cosine similarity over the episodic
 * store.  Writes at most `top_k` rows to `out`; returns the count
 * written (>= 0), or a negative error. */
int  cos_v115_search_episodic(cos_v115_store_t *s,
                              const char *query,
                              int         top_k,
                              cos_v115_memory_row_t *out,
                              int         out_cap);

/* Search the knowledge_base table similarly.  Content column is
 * populated from `chunk` on each row. */
int  cos_v115_search_knowledge(cos_v115_store_t *s,
                               const char *query,
                               int         top_k,
                               cos_v115_memory_row_t *out,
                               int         out_cap);

/* Row-count getters (for health endpoint + smoke tests). */
int  cos_v115_count_episodic (cos_v115_store_t *s);
int  cos_v115_count_knowledge(cos_v115_store_t *s);
int  cos_v115_count_chat     (cos_v115_store_t *s);

/* Serialize a search result list to an OpenAI-adjacent JSON shape:
 *   {"results":[{"id":…, "content":…, "sigma_product":…,
 *                "score":…, "cosine":…, "tags":…, "created_at":…}, …],
 *    "lambda":…, "top_k":…}
 * Returns bytes written (excluding NUL), -1 on overflow. */
int  cos_v115_search_results_to_json(const cos_v115_memory_row_t *rows,
                                     int n,
                                     float lambda, int top_k,
                                     char *out, size_t cap);

/* Pure-C self-test: runs the embedder + in-memory SQLite round-trip
 * (episodic write/search, σ-weighted ranking, zero-match guard).
 * Returns 0 on all-pass.  Skipped cleanly when COS_V115_NO_SQLITE. */
int  cos_v115_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V115_MEMORY_H */
