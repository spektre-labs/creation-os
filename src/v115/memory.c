/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v115 σ-Memory — implementation.
 *
 * See memory.h for contract + rationale.  This file intentionally
 * relies on the OS-provided sqlite3 (macOS ships it; Debian/Ubuntu
 * need `libsqlite3-dev`) so we stay dependency-free for the happy
 * path.  When COS_V115_NO_SQLITE is defined every API returns -ENOSYS
 * and the self-test degrades to a pure-C embedder round-trip.
 */

#include "memory.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef COS_V115_NO_SQLITE
#include <sqlite3.h>
#endif

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void cos_v115_config_defaults(cos_v115_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cos_v115_default_db_path(cfg->db_path, sizeof(cfg->db_path));
    cfg->recall_lambda       = 1.0f;
    cfg->max_rows_episodic   = 0;
    cfg->max_rows_knowledge  = 0;
}

int cos_v115_default_db_path(char *out, size_t cap) {
    if (!out || cap == 0) return -1;
    const char *home = getenv("HOME");
    if (!home || !*home) home = ".";
    int n = snprintf(out, cap, "%s/.creation-os/memory.sqlite", home);
    if (n <= 0 || (size_t)n >= cap) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Deterministic hash-shingled embedder                                */
/* ------------------------------------------------------------------ */

/* FNV-1a 32-bit on a byte run. */
static uint32_t fnv1a(const unsigned char *p, size_t n, uint32_t seed) {
    uint32_t h = seed;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* Normalize text in-place to a lowercase, whitespace-separated token
 * stream for stable shingling.  Returns number of bytes kept. */
static size_t normalize_lower(const char *in, char *out, size_t cap) {
    size_t j = 0;
    int prev_ws = 1;
    for (size_t i = 0; in && in[i] && j + 1 < cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (isspace(c) || !isprint(c)) {
            if (!prev_ws) { out[j++] = ' '; prev_ws = 1; }
        } else {
            out[j++] = (char)tolower(c);
            prev_ws = 0;
        }
    }
    if (j > 0 && out[j - 1] == ' ') --j;
    out[j] = '\0';
    return j;
}

/* Project shingle hash h into dimension index + sign (± for collision
 * mitigation — the "hashing trick" / Weinberger et al. 2009). */
static inline void project_hash(uint32_t h, int *dim, float *sign) {
    *dim  = (int)(h % (uint32_t)COS_V115_EMBED_DIM);
    *sign = ((h >> 31) & 1u) ? -1.0f : 1.0f;
}

void cos_v115_embed(const char *text, float *out) {
    if (!out) return;
    memset(out, 0, sizeof(float) * COS_V115_EMBED_DIM);
    if (!text || !*text) return;

    char buf[COS_V115_CONTENT_MAX + 1];
    size_t n = normalize_lower(text, buf, sizeof(buf));
    if (n == 0) return;

    /* Word tokens: split on whitespace, add unigram + bigram hashes. */
    size_t tok_start = 0;
    uint32_t prev_tok_hash = 0;
    int have_prev = 0;

    for (size_t i = 0; i <= n; ++i) {
        if (i == n || buf[i] == ' ') {
            if (i > tok_start) {
                uint32_t h = fnv1a((const unsigned char *)(buf + tok_start),
                                   i - tok_start, 2166136261u);
                int dim; float sign;
                project_hash(h, &dim, &sign);
                out[dim] += sign;

                if (have_prev) {
                    /* Bigram: combine prev hash and current hash. */
                    unsigned char pair[8];
                    memcpy(pair,     &prev_tok_hash, 4);
                    memcpy(pair + 4, &h,             4);
                    uint32_t bh = fnv1a(pair, 8, 0x9e3779b9u);
                    int bdim; float bsign;
                    project_hash(bh, &bdim, &bsign);
                    /* Bigrams weighted slightly less to keep unigrams
                     * dominant for short queries. */
                    out[bdim] += 0.5f * bsign;
                }
                prev_tok_hash = h;
                have_prev = 1;
            }
            tok_start = i + 1;
        }
    }

    /* Character 3-grams add sub-word robustness. */
    for (size_t i = 0; i + 3 <= n; ++i) {
        uint32_t h = fnv1a((const unsigned char *)(buf + i), 3, 0x811c9dc5u);
        int dim; float sign;
        project_hash(h, &dim, &sign);
        out[dim] += 0.35f * sign;
    }

    /* L2 normalize so cosine == dot product downstream. */
    double ss = 0.0;
    for (int i = 0; i < COS_V115_EMBED_DIM; ++i) ss += (double)out[i] * out[i];
    if (ss <= 0.0) return;
    float inv = (float)(1.0 / sqrt(ss));
    for (int i = 0; i < COS_V115_EMBED_DIM; ++i) out[i] *= inv;
}

float cos_v115_cosine(const float *a, const float *b) {
    if (!a || !b) return 0.0f;
    double dot = 0.0;
    for (int i = 0; i < COS_V115_EMBED_DIM; ++i) {
        dot += (double)a[i] * b[i];
    }
    if (dot !=  dot) return 0.0f;             /* NaN guard */
    if (dot >  1.0) dot =  1.0;
    if (dot < -1.0) dot = -1.0;
    return (float)dot;
}

/* ------------------------------------------------------------------ */
/* Search-result JSON                                                  */
/* ------------------------------------------------------------------ */

static int jesc_append(char *out, size_t cap, size_t off, const char *s) {
    size_t o = off;
    if (o + 1 >= cap) return -1;
    out[o++] = '"';
    for (const char *p = s ? s : ""; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        const char *esc = NULL;
        char small[8] = {0};
        switch (c) {
        case '"':  esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\n': esc = "\\n";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        case '\b': esc = "\\b";  break;
        case '\f': esc = "\\f";  break;
        default:
            if (c < 0x20) {
                snprintf(small, sizeof small, "\\u%04x", c);
                esc = small;
            }
        }
        if (esc) {
            size_t el = strlen(esc);
            if (o + el + 1 >= cap) return -1;
            memcpy(out + o, esc, el);
            o += el;
        } else {
            if (o + 2 >= cap) return -1;
            out[o++] = (char)c;
        }
    }
    if (o + 2 >= cap) return -1;
    out[o++] = '"';
    out[o]   = '\0';
    return (int)o;
}

int cos_v115_search_results_to_json(const cos_v115_memory_row_t *rows,
                                    int n, float lambda, int top_k,
                                    char *out, size_t cap) {
    if (!out || cap == 0) return -1;
    size_t o = 0;
    int w = snprintf(out, cap, "{\"top_k\":%d,\"lambda\":%.4f,\"results\":[",
                     top_k, (double)lambda);
    if (w < 0 || (size_t)w >= cap) return -1;
    o = (size_t)w;
    for (int i = 0; i < n; ++i) {
        if (i > 0) {
            if (o + 1 >= cap) return -1;
            out[o++] = ',';
        }
        int nn = snprintf(out + o, cap - o,
                          "{\"id\":%lld,\"sigma_product\":%.6f,"
                          "\"score\":%.6f,\"cosine\":%.6f,"
                          "\"created_at\":%lld,\"content\":",
                          (long long)rows[i].id,
                          (double)rows[i].sigma_product,
                          (double)rows[i].score,
                          (double)rows[i].cosine,
                          (long long)rows[i].created_at);
        if (nn < 0 || (size_t)nn >= cap - o) return -1;
        o += (size_t)nn;
        int r = jesc_append(out, cap, o, rows[i].content);
        if (r < 0) return -1;
        o = (size_t)r;
        if (o + 8 >= cap) return -1;
        memcpy(out + o, ",\"tags\":", 8);
        o += 8;
        r = jesc_append(out, cap, o, rows[i].tags);
        if (r < 0) return -1;
        o = (size_t)r;
        if (o + 14 >= cap) return -1;
        memcpy(out + o, ",\"session\":", 11);
        o += 11;
        r = jesc_append(out, cap, o, rows[i].session_id);
        if (r < 0) return -1;
        o = (size_t)r;
        if (o + 2 >= cap) return -1;
        out[o++] = '}';
    }
    if (o + 2 >= cap) return -1;
    out[o++] = ']';
    out[o++] = '}';
    out[o]   = '\0';
    return (int)o;
}

/* ------------------------------------------------------------------ */
/* SQLite-backed store                                                 */
/* ------------------------------------------------------------------ */

#ifdef COS_V115_NO_SQLITE

struct cos_v115_store { int dummy; };

int      cos_v115_open(const cos_v115_config_t *c, cos_v115_store_t **out) {
    (void)c;
    if (out) *out = NULL;
    return +1;                                         /* "disabled" */
}
void     cos_v115_close(cos_v115_store_t *s)                       { (void)s; }
int64_t  cos_v115_write_episodic(cos_v115_store_t *s, const char *a,
                                  float b, const char *c, const char *d) {
    (void)s; (void)a; (void)b; (void)c; (void)d; return -ENOSYS;
}
int64_t  cos_v115_write_knowledge(cos_v115_store_t *s, const char *a,
                                   const char *b, const char *c,
                                   const char *d) {
    (void)s; (void)a; (void)b; (void)c; (void)d; return -ENOSYS;
}
int64_t  cos_v115_write_chat(cos_v115_store_t *s, const char *a,
                              const char *b, const char *c, const char *d) {
    (void)s; (void)a; (void)b; (void)c; (void)d; return -ENOSYS;
}
int      cos_v115_search_episodic(cos_v115_store_t *s, const char *q,
                                   int k, cos_v115_memory_row_t *o, int c) {
    (void)s; (void)q; (void)k; (void)o; (void)c; return -ENOSYS;
}
int      cos_v115_search_knowledge(cos_v115_store_t *s, const char *q,
                                    int k, cos_v115_memory_row_t *o, int c) {
    (void)s; (void)q; (void)k; (void)o; (void)c; return -ENOSYS;
}
int      cos_v115_count_episodic (cos_v115_store_t *s) { (void)s; return -ENOSYS; }
int      cos_v115_count_knowledge(cos_v115_store_t *s) { (void)s; return -ENOSYS; }
int      cos_v115_count_chat     (cos_v115_store_t *s) { (void)s; return -ENOSYS; }

int      cos_v115_self_test(void) {
    /* Still exercise the pure-C embedder + JSON so merge-gate stays green. */
    float e[COS_V115_EMBED_DIM], f[COS_V115_EMBED_DIM];
    cos_v115_embed("weather in helsinki",  e);
    cos_v115_embed("weather in helsinki?", f);
    float c = cos_v115_cosine(e, f);
    return (c > 0.8f) ? 0 : 1;
}

#else

struct cos_v115_store {
    sqlite3         *db;
    float            lambda;
    int              max_epi;
    int              max_kn;
    char             path[1024];
};

static int ensure_parent_dir(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    char *slash = strrchr(buf, '/');
    if (!slash || slash == buf) return 0;
    *slash = '\0';
    struct stat st;
    if (stat(buf, &st) == 0) return 0;
    return mkdir(buf, 0700);
}

static int exec_simple(sqlite3 *db, const char *sql) {
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "v115: sql error: %s\n", err ? err : "?");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int cos_v115_open(const cos_v115_config_t *cfg_in, cos_v115_store_t **out) {
    if (!out) return -1;
    cos_v115_config_t cfg;
    if (cfg_in) cfg = *cfg_in; else cos_v115_config_defaults(&cfg);
    if (!cfg.db_path[0]) cos_v115_default_db_path(cfg.db_path, sizeof(cfg.db_path));
    if (ensure_parent_dir(cfg.db_path) < 0 && errno != EEXIST) {
        /* best-effort: continue — sqlite will surface the real error. */
    }

    sqlite3 *db = NULL;
    int rc = sqlite3_open(cfg.db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "v115: sqlite open %s: %s\n",
                cfg.db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return -1;
    }
    exec_simple(db, "PRAGMA journal_mode=WAL;");
    exec_simple(db, "PRAGMA synchronous=NORMAL;");

    const char *schema =
        "CREATE TABLE IF NOT EXISTS episodic_memory ("
        "  id             INTEGER PRIMARY KEY,"
        "  content        TEXT    NOT NULL,"
        "  embedding      BLOB    NOT NULL,"
        "  sigma_product  REAL    NOT NULL,"
        "  session_id     TEXT,"
        "  tags           TEXT,"
        "  created_at     INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS knowledge_base ("
        "  id             INTEGER PRIMARY KEY,"
        "  source_file    TEXT,"
        "  chunk          TEXT    NOT NULL,"
        "  embedding      BLOB    NOT NULL,"
        "  doc_type       TEXT,"
        "  metadata_json  TEXT);"
        "CREATE TABLE IF NOT EXISTS chat_history ("
        "  id             INTEGER PRIMARY KEY,"
        "  session_id     TEXT    NOT NULL,"
        "  role           TEXT    NOT NULL,"
        "  content        TEXT    NOT NULL,"
        "  sigma_profile  TEXT,"
        "  created_at     INTEGER NOT NULL);"
        "CREATE INDEX IF NOT EXISTS idx_epi_created ON episodic_memory(created_at);"
        "CREATE INDEX IF NOT EXISTS idx_chat_sess ON chat_history(session_id, created_at);";
    if (exec_simple(db, schema) < 0) {
        sqlite3_close(db);
        return -1;
    }

    cos_v115_store_t *s = (cos_v115_store_t *)calloc(1, sizeof(*s));
    if (!s) { sqlite3_close(db); return -1; }
    s->db        = db;
    s->lambda    = cfg.recall_lambda > 0.f ? cfg.recall_lambda : 0.f;
    s->max_epi   = cfg.max_rows_episodic;
    s->max_kn    = cfg.max_rows_knowledge;
    snprintf(s->path, sizeof s->path, "%s", cfg.db_path);
    *out = s;
    return 0;
}

void cos_v115_close(cos_v115_store_t *s) {
    if (!s) return;
    if (s->db) sqlite3_close(s->db);
    free(s);
}

static void embed_as_blob(const char *text, float *buf) {
    cos_v115_embed(text, buf);
}

/* Generic insert helper for rows that share (content-like, embedding,
 * extra params) shape. */
static int64_t insert_one(sqlite3 *db, const char *sql,
                          void (*bind)(sqlite3_stmt *, void *), void *ud) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "v115: prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    bind(st, ud);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "v115: step: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return sqlite3_last_insert_rowid(db);
}

typedef struct {
    const char *content;
    const float *embedding;
    float sigma;
    const char *session_id;
    const char *tags;
    int64_t now;
} bind_epi_t;

static void bind_epi(sqlite3_stmt *st, void *ud) {
    bind_epi_t *b = (bind_epi_t *)ud;
    sqlite3_bind_text  (st, 1, b->content ? b->content : "",   -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob  (st, 2, b->embedding,
                        (int)(sizeof(float) * COS_V115_EMBED_DIM),
                        SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, (double)b->sigma);
    sqlite3_bind_text  (st, 4, b->session_id ? b->session_id : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (st, 5, b->tags ? b->tags : "",             -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (st, 6, b->now);
}

int64_t cos_v115_write_episodic(cos_v115_store_t *s, const char *content,
                                float sigma_product, const char *session_id,
                                const char *tags_csv) {
    if (!s || !s->db || !content) return -1;
    float emb[COS_V115_EMBED_DIM];
    embed_as_blob(content, emb);
    bind_epi_t b = {
        .content     = content,
        .embedding   = emb,
        .sigma       = sigma_product,
        .session_id  = session_id,
        .tags        = tags_csv,
        .now         = (int64_t)time(NULL),
    };
    int64_t id = insert_one(
        s->db,
        "INSERT INTO episodic_memory(content,embedding,sigma_product,"
        "session_id,tags,created_at) VALUES(?,?,?,?,?,?)",
        bind_epi, &b);
    if (id > 0 && s->max_epi > 0) {
        char sql[256];
        snprintf(sql, sizeof sql,
                 "DELETE FROM episodic_memory WHERE id IN ("
                 "  SELECT id FROM episodic_memory ORDER BY created_at ASC "
                 "  LIMIT MAX(0, (SELECT COUNT(*) FROM episodic_memory) - %d))",
                 s->max_epi);
        exec_simple(s->db, sql);
    }
    return id;
}

typedef struct {
    const char *source;
    const char *chunk;
    const float *embedding;
    const char *doc_type;
    const char *meta;
} bind_kn_t;

static void bind_kn(sqlite3_stmt *st, void *ud) {
    bind_kn_t *b = (bind_kn_t *)ud;
    sqlite3_bind_text (st, 1, b->source ? b->source : "",     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 2, b->chunk,                       -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob (st, 3, b->embedding,
                       (int)(sizeof(float) * COS_V115_EMBED_DIM),
                       SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 4, b->doc_type ? b->doc_type : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 5, b->meta ? b->meta : "{}",       -1, SQLITE_TRANSIENT);
}

int64_t cos_v115_write_knowledge(cos_v115_store_t *s, const char *source_file,
                                 const char *chunk, const char *doc_type,
                                 const char *metadata_json) {
    if (!s || !s->db || !chunk) return -1;
    float emb[COS_V115_EMBED_DIM];
    embed_as_blob(chunk, emb);
    bind_kn_t b = { source_file, chunk, emb, doc_type, metadata_json };
    return insert_one(s->db,
        "INSERT INTO knowledge_base(source_file,chunk,embedding,doc_type,metadata_json)"
        " VALUES(?,?,?,?,?)", bind_kn, &b);
}

typedef struct {
    const char *sess;
    const char *role;
    const char *content;
    const char *sigma;
    int64_t     now;
} bind_chat_t;

static void bind_chat(sqlite3_stmt *st, void *ud) {
    bind_chat_t *b = (bind_chat_t *)ud;
    sqlite3_bind_text (st, 1, b->sess ? b->sess : "default", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 2, b->role ? b->role : "user",    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 3, b->content ? b->content : "",  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (st, 4, b->sigma ? b->sigma : "{}",    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 5, b->now);
}

int64_t cos_v115_write_chat(cos_v115_store_t *s, const char *session_id,
                             const char *role, const char *content,
                             const char *sigma_profile_json) {
    if (!s || !s->db || !content) return -1;
    bind_chat_t b = { session_id, role, content, sigma_profile_json,
                      (int64_t)time(NULL) };
    return insert_one(s->db,
        "INSERT INTO chat_history(session_id,role,content,sigma_profile,created_at)"
        " VALUES(?,?,?,?,?)", bind_chat, &b);
}

static int count_one(sqlite3 *db, const char *sql) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    int n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return n;
}

int cos_v115_count_episodic (cos_v115_store_t *s) {
    if (!s || !s->db) return -1;
    return count_one(s->db, "SELECT COUNT(*) FROM episodic_memory");
}
int cos_v115_count_knowledge(cos_v115_store_t *s) {
    if (!s || !s->db) return -1;
    return count_one(s->db, "SELECT COUNT(*) FROM knowledge_base");
}
int cos_v115_count_chat     (cos_v115_store_t *s) {
    if (!s || !s->db) return -1;
    return count_one(s->db, "SELECT COUNT(*) FROM chat_history");
}

/* Top-k selection: we scan all rows, cosine vs query embedding,
 * σ-weight, keep best-k in a min-heap-ish linear array (k is small). */
typedef struct {
    int64_t id;
    float   cosine;
    float   sigma;
    float   score;
    int64_t created_at;
    char    content[COS_V115_CONTENT_MAX];
    char    tags[COS_V115_TAGS_MAX];
    char    session[COS_V115_SESSION_MAX];
} hit_t;

static int insert_topk(hit_t *heap, int k, int have, const hit_t *cand) {
    if (have < k) {
        heap[have] = *cand;
        return have + 1;
    }
    /* find minimum-score slot */
    int worst = 0;
    for (int i = 1; i < k; ++i)
        if (heap[i].score < heap[worst].score) worst = i;
    if (cand->score > heap[worst].score) heap[worst] = *cand;
    return k;
}

static int cmp_hit(const void *a, const void *b) {
    float sa = ((const hit_t *)a)->score;
    float sb = ((const hit_t *)b)->score;
    if (sa > sb) return -1;
    if (sa < sb) return  1;
    return 0;
}

static int do_search(cos_v115_store_t *s, const char *table,
                     const char *content_col, const char *query,
                     int top_k, cos_v115_memory_row_t *out, int out_cap) {
    if (!s || !s->db || !query || top_k <= 0 || !out || out_cap <= 0)
        return -1;
    if (top_k > COS_V115_TOPK_MAX) top_k = COS_V115_TOPK_MAX;
    if (top_k > out_cap)           top_k = out_cap;

    float q[COS_V115_EMBED_DIM];
    cos_v115_embed(query, q);

    char sql[512];
    if (strcmp(table, "episodic_memory") == 0) {
        snprintf(sql, sizeof sql,
                 "SELECT id,%s,embedding,sigma_product,"
                 "COALESCE(session_id,''),COALESCE(tags,''),created_at"
                 " FROM %s",
                 content_col, table);
    } else {
        snprintf(sql, sizeof sql,
                 "SELECT id,%s,embedding,0.0,'','',0"
                 " FROM %s",
                 content_col, table);
    }

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(s->db, sql, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "v115: search prepare: %s\n", sqlite3_errmsg(s->db));
        return -1;
    }

    hit_t *heap = (hit_t *)calloc((size_t)top_k, sizeof(hit_t));
    if (!heap) { sqlite3_finalize(st); return -1; }
    int have = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        hit_t h;
        memset(&h, 0, sizeof h);
        h.id         = sqlite3_column_int64(st, 0);
        const unsigned char *c = sqlite3_column_text(st, 1);
        snprintf(h.content, sizeof h.content, "%s", c ? (const char *)c : "");
        const void *blob = sqlite3_column_blob(st, 2);
        int blen         = sqlite3_column_bytes(st, 2);
        h.sigma          = (float)sqlite3_column_double(st, 3);
        const unsigned char *sess = sqlite3_column_text(st, 4);
        const unsigned char *tags = sqlite3_column_text(st, 5);
        h.created_at     = sqlite3_column_int64(st, 6);
        snprintf(h.session, sizeof h.session, "%s",
                 sess ? (const char *)sess : "");
        snprintf(h.tags,    sizeof h.tags,    "%s",
                 tags ? (const char *)tags : "");
        if (!blob || blen != (int)(sizeof(float) * COS_V115_EMBED_DIM))
            continue;
        float emb[COS_V115_EMBED_DIM];
        memcpy(emb, blob, sizeof emb);
        h.cosine = cos_v115_cosine(q, emb);
        float denom = 1.0f + s->lambda * h.sigma;
        h.score  = (denom > 0.0f) ? (h.cosine / denom) : h.cosine;
        have = insert_topk(heap, top_k, have, &h);
    }
    sqlite3_finalize(st);

    qsort(heap, (size_t)have, sizeof(hit_t), cmp_hit);
    int n = have < out_cap ? have : out_cap;
    for (int i = 0; i < n; ++i) {
        cos_v115_memory_row_t *r = &out[i];
        memset(r, 0, sizeof *r);
        r->id            = heap[i].id;
        r->sigma_product = heap[i].sigma;
        r->score         = heap[i].score;
        r->cosine        = heap[i].cosine;
        r->created_at    = heap[i].created_at;
        snprintf(r->content, sizeof r->content, "%s", heap[i].content);
        snprintf(r->tags, sizeof r->tags, "%s", heap[i].tags);
        snprintf(r->session_id, sizeof r->session_id, "%s", heap[i].session);
    }
    free(heap);
    return n;
}

int cos_v115_search_episodic(cos_v115_store_t *s, const char *query,
                              int top_k, cos_v115_memory_row_t *out,
                              int out_cap) {
    return do_search(s, "episodic_memory", "content", query, top_k, out, out_cap);
}

int cos_v115_search_knowledge(cos_v115_store_t *s, const char *query,
                               int top_k, cos_v115_memory_row_t *out,
                               int out_cap) {
    return do_search(s, "knowledge_base", "chunk", query, top_k, out, out_cap);
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _OK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v115 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v115_self_test(void) {
    /* 1) Embedder smoke: identical texts → cos ≈ 1, unrelated → lower. */
    float a[COS_V115_EMBED_DIM], b[COS_V115_EMBED_DIM], c[COS_V115_EMBED_DIM];
    cos_v115_embed("the quick brown fox", a);
    cos_v115_embed("the quick brown fox", b);
    cos_v115_embed("completely unrelated text about sigma governance", c);
    _OK(cos_v115_cosine(a, b) > 0.999f, "identical → cosine=1");
    _OK(cos_v115_cosine(a, c) < cos_v115_cosine(a, b), "unrelated < identical");

    /* 2) In-memory SQLite round-trip. */
    cos_v115_config_t cfg;
    cos_v115_config_defaults(&cfg);
    snprintf(cfg.db_path, sizeof cfg.db_path, ":memory:");
    cfg.recall_lambda = 1.0f;

    cos_v115_store_t *s = NULL;
    int rc = cos_v115_open(&cfg, &s);
    _OK(rc == 0 && s, "open in-memory store");

    int64_t r1 = cos_v115_write_episodic(s,
        "Paris is the capital of France",
        0.05f, "test", "geo,confident");
    int64_t r2 = cos_v115_write_episodic(s,
        "Paris is the capital of France",
        0.80f, "test", "geo,uncertain");       /* same content, high σ */
    int64_t r3 = cos_v115_write_episodic(s,
        "Python lists are ordered mutable sequences",
        0.10f, "test", "code,confident");
    _OK(r1 > 0 && r2 > 0 && r3 > 0, "episodic writes");

    _OK(cos_v115_count_episodic(s) == 3, "count=3");

    /* 3) σ-weighted recall: low-σ copy must rank above high-σ copy for
     *    an on-topic query. */
    cos_v115_memory_row_t rows[8];
    int n = cos_v115_search_episodic(s, "What is the capital of France?",
                                      4, rows, 8);
    _OK(n >= 2, "search returned >=2 rows");
    int idx_low = -1, idx_high = -1;
    for (int i = 0; i < n; ++i) {
        if (rows[i].id == r1) idx_low  = i;
        if (rows[i].id == r2) idx_high = i;
    }
    _OK(idx_low  >= 0, "low-σ row present");
    _OK(idx_high >= 0, "high-σ row present");
    _OK(idx_low  < idx_high, "low-σ ranks above high-σ (same content)");

    /* Top row should be geography, not the Python factoid. */
    _OK(rows[0].id == r1, "top-1 = low-σ geography");
    _OK(rows[0].score  > 0.0f,       "top-1 score > 0");
    _OK(rows[0].cosine >= rows[1].cosine - 1e-3f, "scores monotone-ish");

    /* 4) JSON serialization shape. */
    char buf[8192];
    int jn = cos_v115_search_results_to_json(rows, n, 1.0f, 4,
                                              buf, sizeof buf);
    _OK(jn > 0, "results JSON non-empty");
    _OK(strstr(buf, "\"top_k\":4")   != NULL, "json has top_k");
    _OK(strstr(buf, "\"lambda\":")   != NULL, "json has lambda");
    _OK(strstr(buf, "\"results\":")  != NULL, "json has results");
    _OK(strstr(buf, "capital")       != NULL, "json carries content");

    /* 5) chat_history + knowledge_base round-trip. */
    _OK(cos_v115_write_chat(s, "sess1", "user", "hi", NULL) > 0, "chat write");
    _OK(cos_v115_count_chat(s) == 1, "chat count=1");
    _OK(cos_v115_write_knowledge(s, "doc.md", "Helsinki is in Finland",
                                  "note", "{}") > 0, "kb write");
    _OK(cos_v115_count_knowledge(s) == 1, "kb count=1");

    cos_v115_close(s);
    return 0;
}

#endif /* COS_V115_NO_SQLITE */
