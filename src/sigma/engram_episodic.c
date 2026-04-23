/* Persistent episodic→semantic SQLite memory. */
#include "engram_episodic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#if defined(CLOCK_REALTIME)
#include <time.h>
static int64_t wall_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
}
#elif defined(__APPLE__)
static int64_t wall_ms(void) {
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    uint64_t nano = t * (uint64_t)tb.numer / (uint64_t)tb.denom;
    return (int64_t)(nano / 1000000ULL);
}
#else
#include <time.h>
static int64_t wall_ms(void) {
    return (int64_t)time(NULL) * 1000LL;
}
#endif

static sqlite3 *db_ep = NULL;
static sqlite3 *db_sem = NULL;

uint64_t cos_engram_prompt_hash(const char *prompt_utf8) {
    const unsigned char *s = (const unsigned char *)prompt_utf8;
    uint64_t h = 14695981039346656037ULL;
    while (s && *s) {
        h ^= (uint64_t)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static int episodes_path(char *buf, size_t cap) {
    const char *env = getenv("COS_ENGRAM_EPISODES_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0]) return -1;
    snprintf(buf, cap, "%s/.cos/engram_episodes.db", home);
    return 0;
}

static int semantic_path(char *buf, size_t cap) {
    const char *env = getenv("COS_ENGRAM_SEMANTIC_DB");
    if (env != NULL && env[0] != '\0') {
        snprintf(buf, cap, "%s", env);
        return 0;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0]) return -1;
    snprintf(buf, cap, "%s/.cos/engram_semantic.db", home);
    return 0;
}

static int exec(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return -1;
    }
    return 0;
}

static int ensure_open(void) {
    if (db_ep && db_sem) return 0;
    char path[768];
    if (!db_ep) {
        if (episodes_path(path, sizeof(path)) != 0) return -1;
        if (sqlite3_open_v2(path, &db_ep,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                            NULL) != SQLITE_OK)
            return -1;
        const char *ddl_ep =
            "CREATE TABLE IF NOT EXISTS episode (\n"
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "  ts INTEGER NOT NULL,\n"
            "  prompt_hash INTEGER NOT NULL,\n"
            "  sigma REAL NOT NULL,\n"
            "  action INTEGER NOT NULL,\n"
            "  was_correct INTEGER NOT NULL,\n"
            "  attribution INTEGER NOT NULL);\n"
            "CREATE INDEX IF NOT EXISTS ix_ep_ts ON episode(ts);\n";
        if (exec(db_ep, ddl_ep) != 0) return -1;
    }
    if (!db_sem) {
        if (semantic_path(path, sizeof(path)) != 0) return -1;
        if (sqlite3_open_v2(path, &db_sem,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                              NULL) != SQLITE_OK)
            return -1;
        const char *ddl_sm =
            "CREATE TABLE IF NOT EXISTS semantic (\n"
            "  pattern_hash INTEGER PRIMARY KEY,\n"
            "  sigma_mean REAL NOT NULL,\n"
            "  encounter_count INTEGER NOT NULL,\n"
            "  reliability REAL NOT NULL,\n"
            "  tau_local REAL NOT NULL);\n";
        if (exec(db_sem, ddl_sm) != 0) return -1;
    }
    return 0;
}

int cos_engram_episode_store(const struct cos_engram_episode *ep) {
    if (!ep || ensure_open() != 0) return -1;
    sqlite3_stmt *st = NULL;
    const char *ins =
        "INSERT INTO episode(ts,prompt_hash,sigma,action,was_correct,"
        "attribution) VALUES(?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_ep, ins, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)ep->timestamp_ms);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)ep->prompt_hash);
    sqlite3_bind_double(st, 3, (double)ep->sigma_combined);
    sqlite3_bind_int(st, 4, ep->action);
    sqlite3_bind_int(st, 5, ep->was_correct);
    sqlite3_bind_int(st, 6, (int)ep->attribution);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

static float best_tau_local(const float *sigma, const int *correct_flag,
                           int n) {
    float best_tau = 0.65f;
    float best_acc = -1.f;
    for (float tau = 0.05f; tau <= 0.95f; tau += 0.05f) {
        int tot = 0;
        int ok = 0;
        for (int i = 0; i < n; ++i) {
            if (sigma[i] >= tau) continue;
            if (correct_flag[i] < 0) continue;
            tot++;
            if (correct_flag[i] == 1) ok++;
        }
        if (tot == 0) continue;
        float acc = (float)ok / (float)tot;
        if (acc > best_acc) {
            best_acc = acc;
            best_tau = tau;
        }
    }
    if (best_acc < 0.f) {
        double sm = 0.0;
        for (int i = 0; i < n; ++i) sm += (double)sigma[i];
        return (float)(sm / (double)n);
    }
    return best_tau;
}

int cos_engram_consolidate(int n_recent) {
    if (n_recent <= 0) return 0;
    if (ensure_open() != 0) return -1;
    sqlite3_stmt *sel = NULL;
    const char *sql =
        "SELECT ts,prompt_hash,sigma,action,was_correct,attribution "
        "FROM episode ORDER BY id DESC LIMIT ?";
    if (sqlite3_prepare_v2(db_ep, sql, -1, &sel, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(sel, 1, n_recent);

    typedef struct {
        uint64_t ph;
        float sig[4096];
        int cor[4096];
        int n;
    } grp_t;
    grp_t groups[128];
    int ng = 0;

    while (sqlite3_step(sel) == SQLITE_ROW) {
        uint64_t ph = (uint64_t)sqlite3_column_int64(sel, 1);
        float sg = (float)sqlite3_column_double(sel, 2);
        int wc = sqlite3_column_int(sel, 4);
        int found = -1;
        for (int g = 0; g < ng; ++g) {
            if (groups[g].ph == ph) {
                found = g;
                break;
            }
        }
        if (found < 0) {
            if (ng >= 128) break;
            found = ng++;
            groups[found].ph = ph;
            groups[found].n = 0;
        }
        grp_t *G = &groups[found];
        if (G->n < 4096) {
            G->sig[G->n] = sg;
            G->cor[G->n] = wc;
            G->n += 1;
        }
    }
    sqlite3_finalize(sel);

    for (int g = 0; g < ng; ++g) {
        grp_t *G = &groups[g];
        if (G->n == 0) continue;
        double sum = 0.0;
        int known = 0;
        int good = 0;
        for (int i = 0; i < G->n; ++i) {
            sum += (double)G->sig[i];
            if (G->cor[i] >= 0) {
                known++;
                if (G->cor[i] == 1) good++;
            }
        }
        float sm = (float)(sum / (double)G->n);
        float rel = (known > 0) ? ((float)good / (float)known) : 0.5f;
        float tloc = best_tau_local(G->sig, G->cor, G->n);

        sqlite3_stmt *up = NULL;
        const char *isql =
            "INSERT OR REPLACE INTO semantic(pattern_hash,sigma_mean,"
            "encounter_count,reliability,tau_local) VALUES(?,?,?,?,?)";
        if (sqlite3_prepare_v2(db_sem, isql, -1, &up, NULL) != SQLITE_OK)
            return -1;
        sqlite3_bind_int64(up, 1, (sqlite3_int64)G->ph);
        sqlite3_bind_double(up, 2, (double)sm);
        sqlite3_bind_int(up, 3, G->n);
        sqlite3_bind_double(up, 4, (double)rel);
        sqlite3_bind_double(up, 5, (double)tloc);
        int rc = sqlite3_step(up);
        sqlite3_finalize(up);
        if (rc != SQLITE_DONE) return -1;
    }
    return 0;
}

struct cos_engram_semantic *cos_engram_query_semantic(uint64_t prompt_hash) {
    if (ensure_open() != 0) return NULL;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT pattern_hash,sigma_mean,encounter_count,reliability,"
        "tau_local FROM semantic WHERE pattern_hash=? LIMIT 1";
    if (sqlite3_prepare_v2(db_sem, sql, -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)prompt_hash);
    struct cos_engram_semantic *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        out = (struct cos_engram_semantic *)calloc(1, sizeof(*out));
        if (out) {
            out->pattern_hash =
                (uint64_t)sqlite3_column_int64(st, 0);
            out->sigma_mean = (float)sqlite3_column_double(st, 1);
            out->encounter_count = sqlite3_column_int(st, 2);
            out->reliability = (float)sqlite3_column_double(st, 3);
            out->tau_local = (float)sqlite3_column_double(st, 4);
        }
    }
    sqlite3_finalize(st);
    return out;
}

float cos_engram_get_local_tau(uint64_t domain_hash) {
    struct cos_engram_semantic *s = cos_engram_query_semantic(domain_hash);
    if (!s) return -1.f;
    float t = s->tau_local;
    free(s);
    if (t > 0.f && t < 1.f) return t;
    return -1.f;
}

int cos_engram_forget(int64_t older_than_ms, float reliability_below) {
    if (ensure_open() != 0) return -1;
    sqlite3_stmt *st = NULL;
    const char *sqle = "DELETE FROM episode WHERE ts < ?";
    if (sqlite3_prepare_v2(db_ep, sqle, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, (sqlite3_int64)older_than_ms);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return -1;

    if (sqlite3_prepare_v2(db_sem,
                           "DELETE FROM semantic WHERE reliability < ?", -1,
                           &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_double(st, 1, (double)reliability_below);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

void cos_engram_sqlite_shutdown(void) {
    if (db_ep) {
        sqlite3_close(db_ep);
        db_ep = NULL;
    }
    if (db_sem) {
        sqlite3_close(db_sem);
        db_sem = NULL;
    }
}

int cos_engram_episodic_self_test(void) {
    char tmp_ep[512];
    char tmp_sm[512];
    snprintf(tmp_ep, sizeof(tmp_ep), "/tmp/cos_ep_%ld_test.db",
             (long)getpid());
    snprintf(tmp_sm, sizeof(tmp_sm), "/tmp/cos_sem_%ld_test.db",
             (long)getpid());

    cos_engram_sqlite_shutdown();
    remove(tmp_ep);
    remove(tmp_sm);

    setenv("COS_ENGRAM_EPISODES_DB", tmp_ep, 1);
    setenv("COS_ENGRAM_SEMANTIC_DB", tmp_sm, 1);

    if (cos_engram_prompt_hash("abc") != cos_engram_prompt_hash("abc"))
        return 1;
    if (cos_engram_prompt_hash("x") == cos_engram_prompt_hash("y"))
        return 2;

    struct cos_engram_episode ep = {
        .timestamp_ms = wall_ms(),
        .prompt_hash = 12345ULL,
        .sigma_combined = 0.42f,
        .action = 0,
        .was_correct = 1,
        .attribution = COS_ERR_NONE,
    };
    if (cos_engram_episode_store(&ep) != 0) {
        cos_engram_sqlite_shutdown();
        return 3;
    }

    ep.sigma_combined = 0.55f;
    ep.was_correct = -1;
    if (cos_engram_episode_store(&ep) != 0) {
        cos_engram_sqlite_shutdown();
        return 4;
    }

    if (cos_engram_consolidate(10) != 0) {
        cos_engram_sqlite_shutdown();
        return 5;
    }

    struct cos_engram_semantic *q = cos_engram_query_semantic(12345ULL);
    if (!q || q->encounter_count < 1) {
        free(q);
        cos_engram_sqlite_shutdown();
        return 6;
    }
    free(q);

    float tl = cos_engram_get_local_tau(12345ULL);
    if (!(tl > 0.f && tl <= 1.f)) {
        cos_engram_sqlite_shutdown();
        return 7;
    }

    if (cos_engram_forget(wall_ms() + 1000000, 0.99f) != 0) {
        cos_engram_sqlite_shutdown();
        return 8;
    }

    if (cos_engram_consolidate(0) != 0) {
        cos_engram_sqlite_shutdown();
        return 9;
    }

    cos_engram_sqlite_shutdown();
    remove(tmp_ep);
    remove(tmp_sm);

    unsetenv("COS_ENGRAM_EPISODES_DB");
    unsetenv("COS_ENGRAM_SEMANTIC_DB");

    /* Epistemic attribution enum round-trip */
    if ((int)COS_ERR_EPISTEMIC != 1) return 10;

    /* episode NULL guard */
    if (cos_engram_episode_store(NULL) == 0) return 11;

    /* query missing row */
    setenv("COS_ENGRAM_EPISODES_DB", tmp_ep, 1);
    setenv("COS_ENGRAM_SEMANTIC_DB", tmp_sm, 1);
    cos_engram_sqlite_shutdown();
    remove(tmp_ep);
    remove(tmp_sm);
    if (cos_engram_query_semantic(999999ULL) != NULL) return 12;

    cos_engram_sqlite_shutdown();
    return 0;
}
