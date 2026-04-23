/*
 * σ-federation — pack/unpack updates, τ merge, KG/skill ingest, forget propagation.
 */

#include "federation.h"

#include "a2a.h"
#include "error_attribution.h"
#include "engram_episodic.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#define FED_MAGIC  0x55444546u /* 'FEDU' little-endian marker */
#define FED_VER    1u

static struct cos_federation_config g_fc;
static cos_a2a_network_t            g_net;
static int                          g_ready;
static uint64_t                     g_self_node;
static uint64_t                     g_share_out;
static uint64_t                     g_recv_in;
static uint64_t                     g_quarantine;

static uint64_t fed_fnv1a_id(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    uint64_t             h = 14695981039346656037ULL;
    while (p && *p) {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static void fed_self_id_string(char *buf, size_t cap)
{
    const char *e = getenv("COS_FEDERATION_SELF_ID");
    if (e != NULL && e[0] != '\0')
        snprintf(buf, cap, "%s", e);
    else
        snprintf(buf, cap, "creation-os-local");
}

static uint64_t fed_now_ms(void)
{
    return (uint64_t)time(NULL) * 1000ULL;
}

int cos_federation_default_config(struct cos_federation_config *cfg)
{
    if (!cfg)
        return -1;
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_peers             = COS_A2A_MAX_PEERS;
    cfg->min_trust_to_accept    = 0.7f;
    cfg->min_sigma_to_share     = 0.35f;
    cfg->auto_share             = 0;
    cfg->accept_forget          = 1;
    return 0;
}

static int semantic_db_path(char *buf, size_t cap)
{
    const char *env = getenv("COS_ENGRAM_SEMANTIC_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(buf, cap, "%s/.cos/engram_semantic.db", home);
    return 0;
}

static int semantic_upsert(uint64_t domain_hash, float sigma_mean,
                           int encounter_count, float reliability,
                           float tau_local)
{
    char path[768];
    if (semantic_db_path(path, sizeof(path)) != 0)
        return -1;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                          NULL) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return -2;
    }
    const char *ddl =
        "CREATE TABLE IF NOT EXISTS semantic (\n"
        "  pattern_hash INTEGER PRIMARY KEY,\n"
        "  sigma_mean REAL NOT NULL,\n"
        "  encounter_count INTEGER NOT NULL,\n"
        "  reliability REAL NOT NULL,\n"
        "  tau_local REAL NOT NULL);\n";
    char *err = NULL;
    if (sqlite3_exec(db, ddl, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return -3;
    }
    sqlite3_stmt *st = NULL;
    const char *ins =
        "INSERT OR REPLACE INTO semantic(pattern_hash,sigma_mean,"
        "encounter_count,reliability,tau_local) VALUES(?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, ins, -1, &st, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return -4;
    }
    sqlite3_bind_int64(st, 1, (sqlite3_int64)domain_hash);
    sqlite3_bind_double(st, 2, (double)sigma_mean);
    sqlite3_bind_int(st, 3, encounter_count);
    sqlite3_bind_double(st, 4, (double)reliability);
    sqlite3_bind_double(st, 5, (double)tau_local);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    sqlite3_close(db);
    return rc == SQLITE_DONE ? 0 : -5;
}

static int semantic_fetch(uint64_t domain_hash, float *tau_local,
                          int *encounter_count, float *sigma_mean,
                          float *reliability, int *found)
{
    if (found)
        *found = 0;
    char path[768];
    if (semantic_db_path(path, sizeof(path)) != 0)
        return -1;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return -2;
    }
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT tau_local,encounter_count,sigma_mean,reliability FROM "
        "semantic WHERE pattern_hash=? LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return -3;
    }
    sqlite3_bind_int64(st, 1, (sqlite3_int64)domain_hash);
    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        if (tau_local != NULL)
            *tau_local = (float)sqlite3_column_double(st, 0);
        if (encounter_count != NULL)
            *encounter_count = sqlite3_column_int(st, 1);
        if (sigma_mean != NULL)
            *sigma_mean = (float)sqlite3_column_double(st, 2);
        if (reliability != NULL)
            *reliability = (float)sqlite3_column_double(st, 3);
        if (found)
            *found = 1;
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return 0;
}

static int semantic_delete_hash(uint64_t pattern_hash)
{
    char path[768];
    if (semantic_db_path(path, sizeof(path)) != 0)
        return -1;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return -2;
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM semantic WHERE pattern_hash=?",
                           -1, &st, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return -3;
    }
    sqlite3_bind_int64(st, 1, (sqlite3_int64)pattern_hash);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    sqlite3_close(db);
    return rc == SQLITE_DONE ? 0 : -4;
}

static uint64_t fed_hash_update(const struct cos_federation_update *u)
{
    uint64_t h = 14695981039346656037ULL;
#define FH(v)                                                              \
    do {                                                                   \
        uint64_t x = (uint64_t)(v);                                      \
        for (size_t i = 0; i < sizeof(x); ++i) {                           \
            h ^= (x >> (i * 8)) & 0xffu;                                   \
            h *= 1099511628211ULL;                                         \
        }                                                                  \
    } while (0)
    FH(u->node_id);
    FH((uint64_t)(uint32_t)u->update_type);
    FH(u->domain_hash);
    FH(u->forget_hash);
    return h;
}

int cos_federation_merge_tau(uint64_t domain_hash, float remote_tau,
                             int remote_samples, float trust)
{
    if (remote_samples < 0)
        return -1;
    float local_tau = 0.65f;
    int   n_local   = 1;
    float sm        = 0.5f;
    float rel       = 0.5f;
    int   has_row   = 0;
    if (semantic_fetch(domain_hash, &local_tau, &n_local, &sm, &rel,
                       &has_row) != 0)
        return -8;
    if (!has_row)
        n_local = 1;
    else if (n_local <= 0)
        n_local = 1;
    float num = local_tau * (float)n_local
              + remote_tau * (float)remote_samples * trust;
    float den =
        (float)n_local + (float)remote_samples * trust;
    if (den < 1e-12f)
        return -2;
    float tau_m = num / den;
    int   ntot  = n_local + (int)((float)remote_samples * trust + 0.5f);
    if (ntot < 1)
        ntot = 1;
    return semantic_upsert(domain_hash, sm, ntot, rel, tau_m);
}

int cos_federation_init(const struct cos_federation_config *cfg)
{
    cos_federation_default_config(&g_fc);
    if (cfg != NULL)
        g_fc = *cfg;
    char sid[128];
    fed_self_id_string(sid, sizeof(sid));
    cos_a2a_init(&g_net, sid);
    g_self_node = fed_fnv1a_id(sid);
    g_ready     = 1;
    (void)cos_kg_init();
    return 0;
}

static int fed_store_episode_audit(const char *tag)
{
    struct cos_engram_episode ep;
    memset(&ep, 0, sizeof(ep));
    ep.timestamp_ms   = (int64_t)time(NULL) * 1000LL;
    ep.prompt_hash    = cos_engram_prompt_hash(tag ? tag : "fed");
    ep.sigma_combined = 0.2f;
    ep.action         = 0;
    ep.was_correct    = -1;
    ep.attribution    = COS_ERR_NONE;
    ep.ttt_applied    = 0;
    return cos_engram_episode_store(&ep);
}

int cos_federation_share_update(const struct cos_federation_update *update)
{
    if (!update || !g_ready)
        return -1;
    struct cos_federation_update u = *update;
    if (u.node_id == 0)
        u.node_id = g_self_node;
    if (u.timestamp_ms == 0)
        u.timestamp_ms = (int64_t)fed_now_ms();
    u.update_hash = fed_hash_update(&u);

    unsigned char buf[16384];
    size_t        sz = 0;
    if (cos_federation_pack(&u, buf, sizeof(buf), &sz) != 0)
        return -2;

#if defined(__unix__) || defined(__APPLE__)
    char dir[512];
    const char *home = getenv("HOME");
    if (!home)
        return -3;
    snprintf(dir, sizeof(dir), "%s/.cos/federation/outbox", home);
    if (mkdir(dir, 0755) != 0 && errno != EEXIST)
        return -4;
    char fn[640];
    snprintf(fn, sizeof(fn), "%s/%llx_%lld.bin", dir,
             (unsigned long long)u.update_hash, (long long)u.timestamp_ms);
    FILE *fp = fopen(fn, "wb");
    if (!fp)
        return -5;
    if (fwrite(buf, 1, sz, fp) != sz) {
        fclose(fp);
        return -6;
    }
    fclose(fp);
#endif

    g_share_out++;
    (void)fed_store_episode_audit("federation:share_update");
    return 0;
}

static int fed_accept_sigma(float sigma_mean)
{
    /* Confident knowledge: low σ_mean */
    return (sigma_mean <= g_fc.min_sigma_to_share) ? 1 : 0;
}

int cos_federation_receive_update(const struct cos_federation_update *u)
{
    if (!u || !g_ready)
        return -1;
    /* A2A: lower sigma_trust = more trusted.  Reject when peer is untrusted. */
    if (u->trust_score > g_fc.min_trust_to_accept) {
        g_quarantine++;
        return -2;
    }

    switch (u->update_type) {
    case COS_FED_UPDATE_TAU:
        if (!fed_accept_sigma(u->sigma_mean))
            return -3;
        if (cos_federation_merge_tau(u->domain_hash, u->tau_local,
                                   u->sample_count > 0 ? u->sample_count : 1,
                                   1.0f - u->trust_score)
            != 0)
            return -4;
        break;
    case COS_FED_UPDATE_SKILL: {
        struct cos_skill sk = u->skill;
        sk.reliability *= 0.5f;
        if (sk.reliability > 1.f)
            sk.reliability = 1.f;
        if (cos_skill_save(&sk) != 0)
            return -5;
        break;
    }
    case COS_FED_UPDATE_KG_RELATION: {
        struct cos_kg_relation r = u->relation;
        float inflate          = (1.0f - u->trust_score);
        if (inflate < 0.f)
            inflate = 0.f;
        r.sigma += inflate;
        if (r.sigma > 1.f)
            r.sigma = 1.f;
        if (cos_kg_add_relation(&r) != 0)
            return -6;
        break;
    }
    case COS_FED_UPDATE_FORGET:
        if (!g_fc.accept_forget)
            return -7;
        if (semantic_delete_hash(u->forget_hash) != 0)
            return -8;
        break;
    default:
        return -9;
    }

    g_recv_in++;
    (void)fed_store_episode_audit("federation:receive_update");
    return 0;
}

int cos_federation_broadcast_forget(uint64_t forget_hash,
                                      const char *reason)
{
    if (g_fc.accept_forget)
        (void)semantic_delete_hash(forget_hash);
    struct cos_federation_update u;
    memset(&u, 0, sizeof(u));
    u.node_id      = g_self_node;
    u.update_type  = COS_FED_UPDATE_FORGET;
    u.forget_hash  = forget_hash;
    u.timestamp_ms = (int64_t)fed_now_ms();
    u.trust_score  = COS_A2A_TRUST_INIT;
    if (reason != NULL)
        snprintf(u.forget_reason, sizeof(u.forget_reason), "%s", reason);
    return cos_federation_share_update(&u);
}

int cos_federation_peers(uint64_t *node_ids, float *trust_scores,
                         int max_peers, int *n_found)
{
    if (!n_found || max_peers <= 0)
        return -1;
    *n_found = 0;
    if (!g_ready)
        return -2;
    for (int i = 0; i < g_net.n_peers && *n_found < max_peers; ++i) {
        if (node_ids != NULL)
            node_ids[*n_found] =
                fed_fnv1a_id(g_net.peers[i].agent_id);
        if (trust_scores != NULL)
            trust_scores[*n_found] = g_net.peers[i].sigma_trust;
        (*n_found)++;
    }
    return 0;
}

void cos_federation_print_status(FILE *fp)
{
    if (!fp)
        return;
    fprintf(fp,
            "federation: ready=%d self_node=%016llx shares_out=%llu recv_in=%llu "
            "quarantine=%llu\n",
            g_ready, (unsigned long long)g_self_node,
            (unsigned long long)g_share_out,
            (unsigned long long)g_recv_in,
            (unsigned long long)g_quarantine);
    fprintf(fp,
            "  min_trust<=%.3f min_sigma<=%.3f auto_share=%d accept_forget=%d\n",
            (double)g_fc.min_trust_to_accept,
            (double)g_fc.min_sigma_to_share, g_fc.auto_share,
            g_fc.accept_forget);
}

int cos_federation_pack(const struct cos_federation_update *u,
                        unsigned char *buf, size_t buf_cap, size_t *out_len)
{
    if (!u || !buf || !out_len)
        return -1;
    size_t need = sizeof(uint32_t) * 2 + sizeof(uint64_t) * 3 + sizeof(int32_t)
                + sizeof(int64_t) + sizeof(float) + sizeof(struct cos_skill)
                + sizeof(struct cos_kg_relation) + 128 + 16;
    if (buf_cap < need)
        return -2;
    unsigned char *p = buf;
#define W32(x)                                                               \
    do {                                                                     \
        uint32_t v = (x);                                                    \
        memcpy(p, &v, 4);                                                    \
        p += 4;                                                              \
    } while (0)
#define W64(x)                                                               \
    do {                                                                     \
        uint64_t v = (x);                                                    \
        memcpy(p, &v, 8);                                                    \
        p += 8;                                                              \
    } while (0)
#define WF(x)                                                                \
    do {                                                                     \
        float v = (x);                                                       \
        memcpy(p, &v, 4);                                                  \
        p += 4;                                                              \
    } while (0)
    W32(FED_MAGIC);
    W32(FED_VER);
    W64(u->node_id);
    W64(u->update_hash);
    W32((uint32_t)u->update_type);
    W64((uint64_t)u->timestamp_ms);
    WF(u->trust_score);

    switch (u->update_type) {
    case COS_FED_UPDATE_TAU:
        W64(u->domain_hash);
        WF(u->tau_local);
        WF(u->sigma_mean);
        W32((uint32_t)u->sample_count);
        break;
    case COS_FED_UPDATE_SKILL:
        memcpy(p, &u->skill, sizeof(u->skill));
        p += sizeof(u->skill);
        break;
    case COS_FED_UPDATE_KG_RELATION:
        memcpy(p, &u->relation, sizeof(u->relation));
        p += sizeof(u->relation);
        break;
    case COS_FED_UPDATE_FORGET:
        W64(u->forget_hash);
        memcpy(p, u->forget_reason, sizeof(u->forget_reason));
        p += sizeof(u->forget_reason);
        break;
    default:
        return -3;
    }
#undef W32
#undef W64
#undef WF
    *out_len = (size_t)(p - buf);
    return 0;
}

int cos_federation_unpack(const unsigned char *buf, size_t len,
                          struct cos_federation_update *out)
{
    if (!buf || len < 32 || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    const unsigned char *p = buf;
#define R32(v)                                                               \
    do {                                                                     \
        if ((size_t)(p - buf) + 4 > len)                                     \
            return -2;                                                       \
        memcpy(&(v), p, 4);                                                  \
        p += 4;                                                              \
    } while (0)
#define R64(v)                                                               \
    do {                                                                     \
        if ((size_t)(p - buf) + 8 > len)                                     \
            return -2;                                                       \
        memcpy(&(v), p, 8);                                                  \
        p += 8;                                                              \
    } while (0)
#define RF(v)                                                                \
    do {                                                                     \
        if ((size_t)(p - buf) + 4 > len)                                     \
            return -2;                                                       \
        memcpy(&(v), p, 4);                                                  \
        p += 4;                                                              \
    } while (0)
    uint32_t magic, ver;
    R32(magic);
    R32(ver);
    if (magic != FED_MAGIC || ver != FED_VER)
        return -3;
    R64(out->node_id);
    R64(out->update_hash);
    uint32_t ut;
    R32(ut);
    out->update_type = (int)ut;
    uint64_t ts;
    R64(ts);
    out->timestamp_ms = (int64_t)ts;
    RF(out->trust_score);

    switch (out->update_type) {
    case COS_FED_UPDATE_TAU: {
        R64(out->domain_hash);
        RF(out->tau_local);
        RF(out->sigma_mean);
        uint32_t sc;
        R32(sc);
        out->sample_count = (int)sc;
    } break;
    case COS_FED_UPDATE_SKILL:
        if ((size_t)(p - buf) + sizeof(out->skill) > len)
            return -4;
        memcpy(&out->skill, p, sizeof(out->skill));
        p += sizeof(out->skill);
        break;
    case COS_FED_UPDATE_KG_RELATION:
        if ((size_t)(p - buf) + sizeof(out->relation) > len)
            return -4;
        memcpy(&out->relation, p, sizeof(out->relation));
        p += sizeof(out->relation);
        break;
    case COS_FED_UPDATE_FORGET:
        R64(out->forget_hash);
        if ((size_t)(p - buf) + sizeof(out->forget_reason) > len)
            return -4;
        memcpy(out->forget_reason, p, sizeof(out->forget_reason));
        p += sizeof(out->forget_reason);
        break;
    default:
        return -5;
    }
#undef R32
#undef R64
#undef RF
    return 0;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS)

int cos_federation_self_test(void)
{
    struct cos_federation_config fc;
    cos_federation_default_config(&fc);
    fc.min_trust_to_accept = 0.85f;
    if (cos_federation_init(&fc) != 0)
        return -1;

    char tmp_sem[512];
    snprintf(tmp_sem, sizeof(tmp_sem), "/tmp/cos_fed_sem_%ld.db",
             (long)getpid());
    remove(tmp_sem);
    cos_engram_sqlite_shutdown();
    setenv("COS_ENGRAM_SEMANTIC_DB", tmp_sem, 1);

    if (semantic_upsert(42u, 0.2f, 3, 0.9f, 0.55f) != 0)
        return -2;
    if (cos_federation_merge_tau(42u, 0.65f, 4, 0.8f) != 0)
        return -3;

    struct cos_federation_update u;
    memset(&u, 0, sizeof(u));
    u.node_id       = 99;
    u.update_type   = COS_FED_UPDATE_TAU;
    u.domain_hash   = 7;
    u.tau_local     = 0.5f;
    u.sigma_mean    = 0.4f;
    u.sample_count  = 2;
    u.timestamp_ms  = 1;
    u.trust_score   = 0.9f;
    unsigned char buf[16384];
    size_t        sz = 0;
    if (cos_federation_pack(&u, buf, sizeof(buf), &sz) != 0)
        return -4;
    struct cos_federation_update v;
    if (cos_federation_unpack(buf, sz, &v) != 0)
        return -5;
    if (v.domain_hash != u.domain_hash || v.tau_local != u.tau_local)
        return -6;

    struct cos_federation_update bad_trust;
    memset(&bad_trust, 0, sizeof(bad_trust));
    bad_trust.update_type  = COS_FED_UPDATE_TAU;
    bad_trust.trust_score  = 0.95f;
    bad_trust.sigma_mean   = 0.1f;
    bad_trust.tau_local    = 0.5f;
    bad_trust.sample_count = 1;
    bad_trust.domain_hash  = 100;
    uint64_t q0             = g_quarantine;
    if (cos_federation_receive_update(&bad_trust) != -2)
        return -7;
    if (g_quarantine != q0 + 1)
        return -8;

    struct cos_federation_update ok;
    memset(&ok, 0, sizeof(ok));
    ok.update_type   = COS_FED_UPDATE_TAU;
    ok.trust_score   = 0.5f;
    ok.sigma_mean    = 0.2f;
    ok.tau_local     = 0.6f;
    ok.sample_count  = 2;
    ok.domain_hash   = 200;
    if (cos_federation_receive_update(&ok) != 0)
        return -9;

    const char *caps[] = { "sigma.federation" };
    cos_a2a_peer_register(&g_net, "peer-a", "http://x/.well-known/agent.json",
                          caps, 1, 1, 1);
    uint64_t ids[4];
    float    tr[4];
    int      nf = 0;
    if (cos_federation_peers(ids, tr, 4, &nf) != 0 || nf != 1)
        return -10;

    cos_engram_sqlite_shutdown();
    remove(tmp_sem);
    return 0;
}

#else

int cos_federation_self_test(void)
{
    return 0;
}

#endif
