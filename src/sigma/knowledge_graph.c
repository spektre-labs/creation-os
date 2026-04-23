/*
 * σ-knowledge graph — heuristic extraction + SQLite + BSC similarity.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "knowledge_graph.h"

#include "inference_cache.h"

#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static sqlite3 *g_kgdb;
static char     g_kg_open_path[768];

#ifndef COS_KG_MATCH_NORM
#define COS_KG_MATCH_NORM 0.42f
#endif

static int64_t kg_wall_ms(void)
{
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
#endif
    return (int64_t)time(NULL) * 1000LL;
}

static int ensure_cos_home(char *buf, size_t cap)
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

static int kg_db_open(void)
{
    char path[768];
    const char *env = getenv("COS_KG_DB");
    if (env != NULL && env[0] != '\0')
        snprintf(path, sizeof path, "%s", env);
    else {
        char base[512];
        if (ensure_cos_home(base, sizeof base) != 0)
            return -1;
        snprintf(path, sizeof path, "%s/knowledge_graph.db", base);
    }
    if (g_kgdb != NULL && strcmp(path, g_kg_open_path) == 0)
        return 0;
    if (g_kgdb != NULL) {
        sqlite3_close(g_kgdb);
        g_kgdb = NULL;
        g_kg_open_path[0] = '\0';
    }
    if (sqlite3_open_v2(path, &g_kgdb,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        NULL)
        != SQLITE_OK)
        return -1;

    const char *ddl =
        "CREATE TABLE IF NOT EXISTS kg_entity("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT UNIQUE NOT NULL COLLATE NOCASE,"
        "type TEXT NOT NULL,"
        "bsc BLOB NOT NULL,"
        "first_seen INTEGER NOT NULL,"
        "last_seen INTEGER NOT NULL,"
        "mention_count INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS kg_relation("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source INTEGER NOT NULL,"
        "target INTEGER NOT NULL,"
        "predicate TEXT NOT NULL,"
        "sigma REAL NOT NULL,"
        "reliability REAL NOT NULL,"
        "valid_from INTEGER NOT NULL,"
        "valid_until INTEGER NOT NULL DEFAULT 0,"
        "verification_count INTEGER NOT NULL,"
        "contradiction INTEGER NOT NULL DEFAULT 0,"
        "FOREIGN KEY(source) REFERENCES kg_entity(id),"
        "FOREIGN KEY(target) REFERENCES kg_entity(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_kgr_st ON kg_relation(source,target,"
        "valid_until);"
        "CREATE INDEX IF NOT EXISTS idx_kgr_sig ON kg_relation(sigma);";

    char *err = NULL;
    if (sqlite3_exec(g_kgdb, ddl, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(g_kgdb);
        g_kgdb = NULL;
        g_kg_open_path[0] = '\0';
        return -1;
    }
    snprintf(g_kg_open_path, sizeof g_kg_open_path, "%s", path);
    return 0;
}

int cos_kg_init(void)
{
    return kg_db_open();
}

struct sqlite3 *cos_kg_db_ptr(void)
{
    if (kg_db_open() != 0)
        return NULL;
    return g_kgdb;
}

static void kg_encode_name(const char *name, uint64_t out[COS_KG_INF_W])
{
    cos_inference_bsc_encode_prompt(name ? name : "", out);
}

static const char *kg_entity_type(const char *name)
{
    if (!name || !name[0])
        return "CONCEPT";
    if (strcasecmp(name, "Creation") == 0 || strcasecmp(name, "OS") == 0)
        return "DOMAIN";
    size_t L = strlen(name);
    if (L <= 12 && name[0] >= 'A' && name[0] <= 'Z')
        return "PERSON";
    return "CONCEPT";
}

static int kg_entity_upsert(const char *name, uint64_t *out_id)
{
    if (!name || !name[0] || kg_db_open() != 0)
        return -1;

    uint64_t bsc[COS_KG_INF_W];
    kg_encode_name(name, bsc);

    int64_t now = kg_wall_ms();

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,mention_count FROM kg_entity WHERE name=? COLLATE NOCASE "
        "LIMIT 1";
    if (sqlite3_prepare_v2(g_kgdb, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        sqlite3_int64 id = sqlite3_column_int64(st, 0);
        int           mc = sqlite3_column_int(st, 1);
        sqlite3_finalize(st);
        sqlite3_stmt *up = NULL;
        const char *usql =
            "UPDATE kg_entity SET last_seen=?,mention_count=?,bsc=? WHERE id=?";
        if (sqlite3_prepare_v2(g_kgdb, usql, -1, &up, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_int64(up, 1, (sqlite3_int64)now);
        sqlite3_bind_int(up, 2, mc + 1);
        sqlite3_bind_blob(up, 3, bsc, (int)(sizeof(uint64_t) * COS_KG_INF_W),
                          SQLITE_TRANSIENT);
        sqlite3_bind_int64(up, 4, id);
        sqlite3_step(up);
        sqlite3_finalize(up);
        if (out_id)
            *out_id = (uint64_t)id;
        return 0;
    }
    sqlite3_finalize(st);

    sqlite3_stmt *ins = NULL;
    const char *isql =
        "INSERT INTO kg_entity(name,type,bsc,first_seen,last_seen,"
        "mention_count) VALUES(?,?,?,?,?,1)";
    if (sqlite3_prepare_v2(g_kgdb, isql, -1, &ins, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(ins, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 2, kg_entity_type(name), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(ins, 3, bsc, (int)(sizeof(uint64_t) * COS_KG_INF_W),
                      SQLITE_TRANSIENT);
    sqlite3_bind_int64(ins, 4, (sqlite3_int64)now);
    sqlite3_bind_int64(ins, 5, (sqlite3_int64)now);
    rc = sqlite3_step(ins);
    sqlite3_finalize(ins);
    if (rc != SQLITE_DONE)
        return -1;

    if (out_id)
        *out_id = (uint64_t)sqlite3_last_insert_rowid(g_kgdb);
    return 0;
}

int cos_kg_ensure_entity(const char *name, uint64_t *out_id)
{
    return kg_entity_upsert(name, out_id);
}

static int kg_entity_id_by_name(const char *name, uint64_t *out)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_kgdb,
                           "SELECT id FROM kg_entity WHERE name=? COLLATE "
                           "NOCASE LIMIT 1",
                           -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -1;
    }
    if (out)
        *out = (uint64_t)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return 0;
}

static void kg_trim(char *s)
{
    char *a = s;
    while (*a && isspace((unsigned char)*a))
        a++;
    if (a != s)
        memmove(s, a, strlen(a) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        --n;
    }
}

#define KG_MAX_EXTRACT_ENT 48

static int kg_extract_entity_names(const char *text,
                                   char names[][128], int max_names)
{
    if (!text || max_names <= 0)
        return 0;
    int          n = 0;
    const char * p = text;
    while (*p && n < max_names) {
        while (*p && !isalpha((unsigned char)*p))
            p++;
        if (!*p)
            break;
        if (!isupper((unsigned char)*p)) {
            while (*p && isalpha((unsigned char)*p))
                p++;
            continue;
        }
        char   w1[128], w2[128];
        size_t len = 0;
        while (*p && (isalnum((unsigned char)*p) || *p == '-')
               && len + 1 < sizeof w1)
            w1[len++] = *p++;
        w1[len] = '\0';
        while (*p && isspace((unsigned char)*p))
            p++;
        const char *q = p;
        if (isupper((unsigned char)*q)) {
            size_t l2 = 0;
            while (*q && (isalnum((unsigned char)*q) || *q == '-')
                   && l2 + 1 < sizeof w2)
                w2[l2++] = *q++;
            w2[l2] = '\0';
            snprintf(names[n], 128, "%s %s", w1, w2);
            p = q;
        } else {
            snprintf(names[n], 128, "%s", w1);
        }
        n++;
    }
    return n;
}

typedef struct {
    const char *needle;
    const char *predicate;
} kg_pat_t;

static const kg_pat_t kg_patterns[] = {
    { " is ", "is" },
    { " causes ", "causes" },
    { " requires ", "requires" },
    { " is part of ", "part_of" },
};

static const char *kg_substr_ci(const char *hay, const char *needle)
{
    if (!hay || !needle)
        return NULL;
    size_t nl = strlen(needle);
    if (nl == 0)
        return hay;
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, nl) == 0)
            return p;
    }
    return NULL;
}

static int kg_resolve_entity_name(const char *phrase_in,
                                  char names[][128], int n_names, char *out,
                                  size_t cap)
{
    char phrase[512];
    snprintf(phrase, sizeof phrase, "%s", phrase_in ? phrase_in : "");
    kg_trim(phrase);
    if (!phrase[0])
        return -1;
    size_t best = 0;
    size_t best_len = 0;
    for (int i = 0; i < n_names; i++) {
        size_t L = strlen(names[i]);
        if (L >= best_len && strncasecmp(phrase, names[i], L) == 0
            && (phrase[L] == '\0' || isspace((unsigned char)phrase[L]))) {
            best_len = L;
            best     = (size_t)i;
        }
    }
    if (best_len > 0) {
        snprintf(out, cap, "%s", names[best]);
        return 0;
    }
    /* longest name contained in phrase */
    for (int i = 0; i < n_names; i++) {
        if (kg_substr_ci(phrase, names[i]) != NULL) {
            snprintf(out, cap, "%s", names[i]);
            return 0;
        }
    }
    snprintf(out, cap, "%.*s", (int)(cap - 1), phrase);
    return 0;
}

static int kg_store_relation_edge(uint64_t src, uint64_t tgt,
                                  const char *pred, float base_sigma)
{
    int64_t now = kg_wall_ms();

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,predicate,sigma,verification_count FROM kg_relation WHERE "
        "source=? AND target=? AND valid_until=0";
    if (sqlite3_prepare_v2(g_kgdb, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)src);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)tgt);

    float sigma_new = base_sigma;
    int   contradict = 0;

    while (sqlite3_step(st) == SQLITE_ROW) {
        sqlite3_int64 rid      = sqlite3_column_int64(st, 0);
        char          oldp[128];
        snprintf(oldp, sizeof oldp, "%s", sqlite3_column_text(st, 1));
        float old_sig = (float)sqlite3_column_double(st, 2);
        int   vc      = sqlite3_column_int(st, 3);

        if (strcasecmp(oldp, pred) == 0) {
            sigma_new = old_sig * 0.92f;
            if (sigma_new < 0.02f)
                sigma_new = 0.02f;
            sqlite3_finalize(st);

            sqlite3_stmt *up = NULL;
            const char *uq =
                "UPDATE kg_relation SET sigma=?,reliability=reliability*0."
                "98+0.02,verification_count=? WHERE id=?";
            if (sqlite3_prepare_v2(g_kgdb, uq, -1, &up, NULL) != SQLITE_OK)
                return -1;
            sqlite3_bind_double(up, 1, (double)sigma_new);
            sqlite3_bind_int(up, 2, vc + 1);
            sqlite3_bind_int64(up, 3, rid);
            sqlite3_step(up);
            sqlite3_finalize(up);
            return 0;
        }
        /* conflicting active predicate */
        sqlite3_stmt *cl = NULL;
        const char *cq =
            "UPDATE kg_relation SET valid_until=?,contradiction=1 WHERE id=?";
        if (sqlite3_prepare_v2(g_kgdb, cq, -1, &cl, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(cl, 1, (sqlite3_int64)now);
            sqlite3_bind_int64(cl, 2, rid);
            sqlite3_step(cl);
            sqlite3_finalize(cl);
        }
        contradict         = 1;
        sigma_new          = 0.82f;
        (void)old_sig;
    }
    sqlite3_finalize(st);

    sqlite3_stmt *ins = NULL;
    const char *isql =
        "INSERT INTO kg_relation(source,target,predicate,sigma,reliability,"
        "valid_from,valid_until,verification_count,contradiction) "
        "VALUES(?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(g_kgdb, isql, -1, &ins, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(ins, 1, (sqlite3_int64)src);
    sqlite3_bind_int64(ins, 2, (sqlite3_int64)tgt);
    sqlite3_bind_text(ins, 3, pred, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(ins, 4, (double)sigma_new);
    sqlite3_bind_double(ins, 5, contradict ? 0.5 : 0.95);
    sqlite3_bind_int64(ins, 6, (sqlite3_int64)now);
    sqlite3_bind_int64(ins, 7, 0);
    sqlite3_bind_int(ins, 8, 1);
    sqlite3_bind_int(ins, 9, contradict ? 1 : 0);
    int rc = sqlite3_step(ins);
    sqlite3_finalize(ins);
    return rc == SQLITE_DONE ? 0 : -1;
}

int cos_kg_extract_and_store(const char *text)
{
    if (!text || !text[0] || kg_db_open() != 0)
        return -1;

    char names[KG_MAX_EXTRACT_ENT][128];
    int  nn = kg_extract_entity_names(text, names, KG_MAX_EXTRACT_ENT);
    for (int i = 0; i < nn; i++) {
        uint64_t id;
        if (kg_entity_upsert(names[i], &id) != 0)
            return -1;
        (void)id;
    }

    char buf[8192];
    snprintf(buf, sizeof buf, "%s", text);

    for (size_t pi = 0; pi < sizeof(kg_patterns) / sizeof(kg_patterns[0]);
         pi++) {
        const char *p = buf;
        while (*p) {
            const char *hit = kg_substr_ci(p, kg_patterns[pi].needle);
            if (!hit)
                break;
            char left[256], right[256];
            size_t prefix = (size_t)(hit - buf);
            if (prefix >= sizeof left)
                prefix = sizeof left - 1;
            memcpy(left, buf, prefix);
            left[prefix] = '\0';
            const char *rhs = hit + strlen(kg_patterns[pi].needle);
            snprintf(right, sizeof right, "%s", rhs);

            char sl[128], sr[128];
            kg_resolve_entity_name(left, names, nn, sl, sizeof sl);
            kg_resolve_entity_name(right, names, nn, sr, sizeof sr);

            uint64_t ids, idt;
            if (kg_entity_upsert(sl, &ids) != 0
                || kg_entity_upsert(sr, &idt) != 0)
                continue;
            if (kg_store_relation_edge(ids, idt, kg_patterns[pi].predicate,
                                        0.38f)
                != 0)
                continue;

            p = rhs;
        }
    }

    return 0;
}

int cos_kg_add_entity(const struct cos_kg_entity *e)
{
    if (!e || kg_db_open() != 0)
        return -1;
    uint64_t id = 0;
    if (kg_entity_upsert(e->name, &id) != 0)
        return -1;
    (void)id;
    return 0;
}

int cos_kg_add_relation(const struct cos_kg_relation *r)
{
    if (!r || kg_db_open() != 0)
        return -1;
    return kg_store_relation_edge(r->source_id, r->target_id, r->predicate,
                                  r->sigma > 0.f ? r->sigma : 0.40f);
}

static int kg_fill_entity_row(sqlite3_stmt *st, struct cos_kg_entity *e)
{
    e->id = (uint64_t)sqlite3_column_int64(st, 0);
    snprintf(e->name, sizeof e->name, "%s", sqlite3_column_text(st, 1));
    snprintf(e->type, sizeof e->type, "%s", sqlite3_column_text(st, 2));
    int bn = sqlite3_column_bytes(st, 3);
    if (bn == (int)(sizeof(uint64_t) * COS_KG_INF_W)) {
        memcpy(e->bsc_vec, sqlite3_column_blob(st, 3),
               sizeof(uint64_t) * COS_KG_INF_W);
    }
    e->first_seen    = sqlite3_column_int64(st, 4);
    e->last_seen     = sqlite3_column_int64(st, 5);
    e->mention_count = sqlite3_column_int(st, 6);
    return 0;
}

static int kg_fill_relation_row(sqlite3_stmt *st, struct cos_kg_relation *r)
{
    r->source_id          = (uint64_t)sqlite3_column_int64(st, 0);
    r->target_id          = (uint64_t)sqlite3_column_int64(st, 1);
    snprintf(r->predicate, sizeof r->predicate, "%s",
             sqlite3_column_text(st, 2));
    r->sigma               = (float)sqlite3_column_double(st, 3);
    r->reliability         = (float)sqlite3_column_double(st, 4);
    r->valid_from          = sqlite3_column_int64(st, 5);
    r->valid_until         = sqlite3_column_int64(st, 6);
    r->verification_count = sqlite3_column_int(st, 7);
    return 0;
}

int cos_kg_query(const uint64_t *bsc_query, struct cos_kg_query_result *result)
{
    if (!bsc_query || !result || kg_db_open() != 0)
        return -1;
    memset(result, 0, sizeof(*result));

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,name,type,bsc,first_seen,last_seen,mention_count FROM "
        "kg_entity";
    if (sqlite3_prepare_v2(g_kgdb, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;

    typedef struct {
        float    d;
        uint64_t id;
    } rank_t;
    rank_t ranks[128];
    int      nr = 0;

    while (sqlite3_step(st) == SQLITE_ROW && nr < 128) {
        int bn = sqlite3_column_bytes(st, 3);
        if (bn != (int)(sizeof(uint64_t) * COS_KG_INF_W))
            continue;
        const uint64_t *bv = (const uint64_t *)sqlite3_column_blob(st, 3);
        float           dist =
            cos_inference_hamming_norm(bsc_query, bv, COS_KG_INF_W);
        ranks[nr].d  = dist;
        ranks[nr].id = (uint64_t)sqlite3_column_int64(st, 0);
        nr++;
    }
    sqlite3_finalize(st);

    for (int i = 0; i < nr - 1; i++)
        for (int j = i + 1; j < nr; j++)
            if (ranks[j].d < ranks[i].d) {
                rank_t t = ranks[i];
                ranks[i] = ranks[j];
                ranks[j] = t;
            }

    int ne = nr > 16 ? 16 : nr;
    for (int i = 0; i < ne; i++) {
        sqlite3_stmt *one = NULL;
        const char *os =
            "SELECT id,name,type,bsc,first_seen,last_seen,mention_count FROM "
            "kg_entity WHERE id=?";
        if (sqlite3_prepare_v2(g_kgdb, os, -1, &one, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_int64(one, 1, (sqlite3_int64)ranks[i].id);
        if (sqlite3_step(one) == SQLITE_ROW)
            kg_fill_entity_row(one, &result->entities[result->n_entities++]);
        sqlite3_finalize(one);
    }

    double ssum = 0.0;
    int    scnt = 0;

    for (int i = 0; i < result->n_entities && result->n_relations < 32;
         i++) {
        sqlite3_stmt *rs = NULL;
        const char *rsql =
            "SELECT source,target,predicate,sigma,reliability,valid_from,"
            "valid_until,verification_count FROM kg_relation WHERE "
            "(source=? OR target=?) AND valid_until=0 LIMIT 16";
        if (sqlite3_prepare_v2(g_kgdb, rsql, -1, &rs, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_int64(rs, 1,
                           (sqlite3_int64)result->entities[i].id);
        sqlite3_bind_int64(rs, 2,
                           (sqlite3_int64)result->entities[i].id);
        while (sqlite3_step(rs) == SQLITE_ROW
               && result->n_relations < 32) {
            struct cos_kg_relation *R =
                &result->relations[result->n_relations++];
            kg_fill_relation_row(rs, R);
            ssum += (double)R->sigma;
            scnt++;
        }
        sqlite3_finalize(rs);
    }

    result->sigma_mean =
        scnt > 0 ? (float)(ssum / (double)scnt) : 1.0f;
    return 0;
}

int cos_kg_find_contradictions(struct cos_kg_relation *contradictions, int max,
                               int *n_found)
{
    if (!contradictions || max <= 0 || !n_found || kg_db_open() != 0)
        return -1;
    *n_found = 0;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT source,target,predicate,sigma,reliability,valid_from,"
        "valid_until,verification_count FROM kg_relation WHERE "
        "contradiction=1 AND valid_until=0 ORDER BY valid_from DESC LIMIT ?";
    if (sqlite3_prepare_v2(g_kgdb, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, max);
    while (sqlite3_step(st) == SQLITE_ROW && *n_found < max)
        kg_fill_relation_row(st, &contradictions[(*n_found)++]);
    sqlite3_finalize(st);
    return 0;
}

int cos_kg_stats(int *n_entities, int *n_relations, float *avg_sigma)
{
    if (kg_db_open() != 0)
        return -1;
    sqlite3_stmt *st = NULL;
    int           ne  = 0, nr = 0;
    double        as  = 0.0;
    if (sqlite3_prepare_v2(g_kgdb, "SELECT COUNT(*) FROM kg_entity", -1, &st,
                           NULL)
            == SQLITE_OK
        && sqlite3_step(st) == SQLITE_ROW)
        ne = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    if (sqlite3_prepare_v2(g_kgdb,
                           "SELECT COUNT(*),AVG(sigma) FROM kg_relation WHERE "
                           "valid_until=0",
                           -1, &st, NULL)
            == SQLITE_OK
        && sqlite3_step(st) == SQLITE_ROW) {
        nr = sqlite3_column_int(st, 0);
        if (sqlite3_column_type(st, 1) != SQLITE_NULL)
            as = sqlite3_column_double(st, 1);
    }
    sqlite3_finalize(st);
    if (n_entities)
        *n_entities = ne;
    if (n_relations)
        *n_relations = nr;
    if (avg_sigma)
        *avg_sigma = nr > 0 ? (float)as : 0.f;
    return 0;
}

int cos_kg_export_json(FILE *fp)
{
    if (!fp || kg_db_open() != 0)
        return -1;
    fputc('{', fp);
    fprintf(fp, "\"entities\":[");
    sqlite3_stmt *st = NULL;
    int             first = 1;
    if (sqlite3_prepare_v2(g_kgdb,
                           "SELECT id,name,type FROM kg_entity ORDER BY id",
                           -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!first)
            fputc(',', fp);
        first = 0;
        fprintf(fp,
                "{\"id\":%lld,\"name\":\"", (long long)sqlite3_column_int64(st, 0));
        const unsigned char *nm = sqlite3_column_text(st, 1);
        for (const unsigned char *p = nm; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', fp);
            fputc((int)*p, fp);
        }
        fprintf(fp, "\",\"type\":\"%s\"}", sqlite3_column_text(st, 2));
    }
    sqlite3_finalize(st);
    fprintf(fp, "],\"relations\":[");
    first = 1;
    if (sqlite3_prepare_v2(
            g_kgdb,
            "SELECT source,target,predicate,sigma,valid_until FROM "
            "kg_relation ORDER BY id",
            -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (!first)
            fputc(',', fp);
        first = 0;
        fprintf(fp,
                "{\"source\":%lld,\"target\":%lld,\"predicate\":\"%s\","
                "\"sigma\":%.4f,\"valid_until\":%lld}",
                (long long)sqlite3_column_int64(st, 0),
                (long long)sqlite3_column_int64(st, 1),
                sqlite3_column_text(st, 2),
                sqlite3_column_double(st, 3),
                (long long)sqlite3_column_int64(st, 4));
    }
    sqlite3_finalize(st);
    fprintf(fp, "]}\n");
    return 0;
}

int cos_kg_entity_relations(const char *entity_name,
                            struct cos_kg_query_result *out)
{
    if (!entity_name || !out || kg_db_open() != 0)
        return -1;
    memset(out, 0, sizeof(*out));

    uint64_t eid = 0;
    if (kg_entity_id_by_name(entity_name, &eid) != 0)
        return -1;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT id,name,type,bsc,first_seen,last_seen,mention_count FROM "
        "kg_entity WHERE id=?";
    if (sqlite3_prepare_v2(g_kgdb, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)eid);
    if (sqlite3_step(st) == SQLITE_ROW && out->n_entities < 16)
        kg_fill_entity_row(st, &out->entities[out->n_entities++]);
    sqlite3_finalize(st);

    sqlite3_stmt *rs = NULL;
    const char *rsql =
        "SELECT source,target,predicate,sigma,reliability,valid_from,"
        "valid_until,verification_count FROM kg_relation WHERE "
        "(source=? OR target=?) AND valid_until=0";
    if (sqlite3_prepare_v2(g_kgdb, rsql, -1, &rs, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(rs, 1, (sqlite3_int64)eid);
    sqlite3_bind_int64(rs, 2, (sqlite3_int64)eid);
    double ssum = 0.0;
    int    sc   = 0;
    while (sqlite3_step(rs) == SQLITE_ROW && out->n_relations < 32) {
        kg_fill_relation_row(rs, &out->relations[out->n_relations]);
        ssum += (double)out->relations[out->n_relations].sigma;
        out->n_relations++;
        sc++;
    }
    sqlite3_finalize(rs);
    out->sigma_mean = sc > 0 ? (float)(ssum / (double)sc) : 1.f;
    return 0;
}

int cos_kg_self_test(void)
{
    char tmp[256];
    snprintf(tmp, sizeof tmp, "/tmp/cos_kg_%ld.db", (long)getpid());
    remove(tmp);
    setenv("COS_KG_DB", tmp, 1);

    if (cos_kg_init() != 0)
        return 1;

    struct cos_kg_entity e;
    memset(&e, 0, sizeof e);
    snprintf(e.name, sizeof e.name, "AlphaNode");
    snprintf(e.type, sizeof e.type, "CONCEPT");
    kg_encode_name(e.name, e.bsc_vec);
    e.first_seen = e.last_seen = kg_wall_ms();
    e.mention_count            = 1;
    if (cos_kg_add_entity(&e) != 0)
        return 2;

    struct cos_kg_relation r;
    memset(&r, 0, sizeof r);
    r.source_id  = 1;
    r.target_id  = 1;
    snprintf(r.predicate, sizeof r.predicate, "self_loop");
    r.sigma               = 0.2f;
    r.reliability         = 1.f;
    r.valid_from          = kg_wall_ms();
    r.valid_until         = 0;
    r.verification_count  = 1;
    if (cos_kg_add_relation(&r) != 0)
        return 3;

    const char *blob =
        "AlphaNode is TestSubject. Beta causes Gamma. Beta requires Delta.";
    if (cos_kg_extract_and_store(blob) != 0)
        return 4;

    uint64_t qb[COS_KG_INF_W];
    cos_inference_bsc_encode_prompt("What about Beta", qb);
    struct cos_kg_query_result qr;
    memset(&qr, 0, sizeof qr);
    if (cos_kg_query(qb, &qr) != 0)
        return 5;

    int ne = 0, nrel = 0;
    float av = 0.f;
    if (cos_kg_stats(&ne, &nrel, &av) != 0 || ne < 1)
        return 6;

    struct cos_kg_relation cx[8];
    int                     nc = 0;
    if (cos_kg_find_contradictions(cx, 8, &nc) != 0)
        return 7;

    {
        FILE *fp = fopen("/dev/null", "w");
        if (!fp || cos_kg_export_json(fp) != 0) {
            if (fp)
                fclose(fp);
            return 8;
        }
        fclose(fp);
    }

    remove(tmp);
    unsetenv("COS_KG_DB");
    return 0;
}
