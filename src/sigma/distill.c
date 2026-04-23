/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * σ-gated distillation — libcurl cloud calls + SQLite + local σ heuristics.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "distill.h"

#include "../import/bitnet_sigma.h"
#include "../import/bitnet_server.h"

#include <curl/curl.h>
#include <sqlite3.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define COS_DISTILL_MAX_T 16
#define IMPROVE_EPS       0.005f

static sqlite3 *g_db;
static struct cos_distill_teacher g_teachers[COS_DISTILL_MAX_T];
static int                        g_nt;
static int                        g_inited;

/* ------------------------------------------------------------------ */
/* home + db path                                                     */
/* ------------------------------------------------------------------ */

static const char *resolve_home(void) {
    const char *h = getenv("HOME");
    if (h && h[0]) return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw && pw->pw_dir) ? pw->pw_dir : ".";
}

static int ensure_dir(const char *p) {
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int db_path(char *buf, size_t cap) {
    const char *h = resolve_home();
    int n = snprintf(buf, cap, "%s/.cos/distill/examples.db", h);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Minimal JSON + curl buffer (standalone; mirrors escalation.c)       */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *data;
    size_t len, cap;
} dbuf_t;

static size_t curl_wcb(void *ptr, size_t sz, size_t nm, void *ud) {
    size_t n = sz * nm;
    dbuf_t *r = (dbuf_t *)ud;
    if (r->len + n + 1 > r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 8192;
        while (nc < r->len + n + 1) nc *= 2;
        char *nd = realloc(r->data, nc);
        if (!nd) return 0;
        r->data = nd;
        r->cap  = nc;
    }
    memcpy(r->data + r->len, ptr, n);
    r->len += n;
    if (r->data) r->data[r->len] = '\0';
    return n;
}

static size_t je_app(char *dst, size_t cap, size_t w, const char *s, size_t n) {
    if (w + n >= cap) return 0;
    memcpy(dst + w, s, n);
    return w + n;
}

static size_t json_enc(char *dst, size_t cap, const char *s) {
    if (!dst || !s || cap < 3) return 0;
    size_t w = 0;
    dst[w++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char buf[8];
        int n = 0;
        switch (*p) {
            case '"':  buf[0] = '\\'; buf[1] = '"';  n = 2; break;
            case '\\': buf[0] = '\\'; buf[1] = '\\'; n = 2; break;
            case '\n': buf[0] = '\\'; buf[1] = 'n';  n = 2; break;
            case '\r': buf[0] = '\\'; buf[1] = 'r';  n = 2; break;
            case '\t': buf[0] = '\\'; buf[1] = 't';  n = 2; break;
            default:
                buf[0] = (char)*p;
                n      = 1;
        }
        w = je_app(dst, cap, w, buf, (size_t)n);
        if (!w && n) return 0;
    }
    if (w + 1 >= cap) return 0;
    dst[w++] = '"';
    return w;
}

static const char *js_key(const char *hay, size_t hlen, const char *key) {
    size_t klen = strlen(key);
    for (size_t i = 0; i + klen + 2 < hlen; i++) {
        if (hay[i] != '"') continue;
        if (strncmp(hay + i + 1, key, klen) != 0) continue;
        if (hay[i + 1 + klen] != '"') continue;
        const char *q = hay + i + 1 + klen + 1;
        while (q < hay + hlen && (*q == ' ' || *q == '\t')) q++;
        if (q >= hay + hlen || *q != ':') continue;
        q++;
        while (q < hay + hlen && (*q == ' ' || *q == '\t')) q++;
        if (q >= hay + hlen) return NULL;
        return q;
    }
    return NULL;
}

static const char *js_str(const char *p, const char *end, char *out, size_t cap) {
    if (p >= end || *p != '"') return NULL;
    p++;
    size_t w = 0;
    while (p < end && *p != '"') {
        char c = *p;
        if (*p == '\\' && p + 1 < end) {
            char e = p[1];
            p += 2;
            if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else c = e;
        } else {
            p++;
        }
        if (w + 1 < cap) out[w++] = c;
    }
    out[w] = '\0';
    return p < end ? p + 1 : NULL;
}

/* ------------------------------------------------------------------ */
/* Env keys                                                           */
/* ------------------------------------------------------------------ */

static const char *env_claude(void) {
    const char *k = getenv("COS_CLAUDE_API_KEY");
    if (k && k[0]) return k;
    return getenv("CREATION_OS_CLAUDE_API_KEY");
}

static const char *env_openai(void) {
    const char *k = getenv("COS_OPENAI_API_KEY");
    if (k && k[0]) return k;
    k = getenv("COS_GPT_API_KEY");
    if (k && k[0]) return k;
    return getenv("CREATION_OS_OPENAI_API_KEY");
}

static const char *env_gemini(void) {
    const char *k = getenv("COS_GEMINI_API_KEY");
    if (k && k[0]) return k;
    return getenv("GOOGLE_API_KEY");
}

static const char *env_deepseek(void) {
    const char *k = getenv("COS_DEEPSEEK_API_KEY");
    if (k && k[0]) return k;
    return getenv("CREATION_OS_DEEPSEEK_API_KEY");
}

static void builtin_teachers_reset(void) {
    g_nt = 0;
    memset(g_teachers, 0, sizeof g_teachers);

#define ADD(id, url)                                                         \
    do {                                                                     \
        if (g_nt >= COS_DISTILL_MAX_T) break;                                \
        snprintf(g_teachers[g_nt].name, sizeof g_teachers[g_nt].name, "%s",  \
                 (id));                                                      \
        snprintf(g_teachers[g_nt].endpoint, sizeof g_teachers[g_nt].endpoint, \
                 "%s", (url));                                               \
        g_teachers[g_nt].trust_ema    = 0.50f;                               \
        g_teachers[g_nt].available    = 0;                                   \
        g_teachers[g_nt].total_queries = 0;                                  \
        g_teachers[g_nt].best_count    = 0;                                  \
        g_nt++;                                                              \
    } while (0)

    ADD("claude",
        "https://api.anthropic.com/v1/messages");
    ADD("gpt",
        "https://api.openai.com/v1/chat/completions");
    ADD("gemini",
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-1.5-flash:generateContent");
    ADD("deepseek",
        "https://api.deepseek.com/v1/chat/completions");
#undef ADD

    /* availability */
    for (int i = 0; i < g_nt; i++) {
        if (!strcmp(g_teachers[i].name, "claude"))
            g_teachers[i].available = env_claude() ? 1 : 0;
        else if (!strcmp(g_teachers[i].name, "gpt"))
            g_teachers[i].available = env_openai() ? 1 : 0;
        else if (!strcmp(g_teachers[i].name, "gemini"))
            g_teachers[i].available = env_gemini() ? 1 : 0;
        else if (!strcmp(g_teachers[i].name, "deepseek"))
            g_teachers[i].available = env_deepseek() ? 1 : 0;
    }
}

static struct cos_distill_teacher *find_teacher(const char *name) {
    for (int i = 0; i < g_nt; i++)
        if (!strcmp(g_teachers[i].name, name)) return &g_teachers[i];
    return NULL;
}

static int teacher_allowed(const char *name, const char *only_list) {
    if (!only_list || !only_list[0]) return 1;
    char wrap[320];
    snprintf(wrap, sizeof wrap, ",%s,", only_list);
    char needle[80];
    snprintf(needle, sizeof needle, ",%s,", name);
    return strstr(wrap, needle) != NULL;
}

/* ------------------------------------------------------------------ */
/* HTTP providers                                                     */
/* ------------------------------------------------------------------ */

static int call_claude_http(const char *prompt, const char *key,
                            char *out, size_t ocap) {
    static char body[65536];
    const char *model = getenv("COS_DISTILL_CLAUDE_MODEL");
    if (!model || !model[0]) model = "claude-3-5-haiku-20241022";
    size_t bw = 0;
    int n = snprintf(body + bw, sizeof body - bw,
                     "{\"model\":\"%s\",\"max_tokens\":2048,\"messages\":[{\"role\":\"user\",\"content\":",
                     model);
    if (n <= 0) return -1;
    bw += (size_t)n;
    size_t sw = json_enc(body + bw, sizeof body - bw, prompt);
    if (!sw) return -1;
    bw += sw;
    n = snprintf(body + bw, sizeof body - bw, "}]}");
    if (n <= 0) return -1;
    bw += (size_t)n;

    dbuf_t resp = {0};
    CURL *c     = curl_easy_init();
    if (!c) return -1;
    struct curl_slist *hdr = NULL;
    char auth[512];
    snprintf(auth, sizeof auth, "x-api-key: %s", key);
    hdr = curl_slist_append(hdr, auth);
    hdr = curl_slist_append(hdr, "anthropic-version: 2023-06-01");
    hdr = curl_slist_append(hdr, "content-type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bw);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_wcb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 90L);
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    int ok = -1;
    if (rc == CURLE_OK && status >= 200 && status < 300 && resp.data) {
        const char *cp = js_key(resp.data, resp.len, "content");
        if (cp && *cp == '[') {
            const char *tp = js_key(cp, resp.len - (size_t)(cp - resp.data), "text");
            while (tp != NULL && *tp != '"' && *tp != '{') {
                const char *base = tp + 1;
                tp = js_key(base, resp.len - (size_t)(base - resp.data), "text");
            }
            if (tp != NULL && *tp == '"' &&
                js_str(tp, resp.data + resp.len, out, ocap) != NULL)
                ok = 0;
        }
        if (ok != 0) {
            const char *mk = strstr(resp.data, "\"text\":\"");
            if (mk != NULL && js_str(mk + 7, resp.data + resp.len, out, ocap) != NULL)
                ok = 0;
        }
    }
    free(resp.data);
    return ok;
}

static int call_openai_compat(const char *url, const char *prompt,
                              const char *key, const char *model,
                              char *out, size_t ocap, int logprobs) {
    static char body[65536];
    size_t bw = 0;
    int n = snprintf(body + bw, sizeof body - bw,
                     "{\"model\":\"%s\",\"max_tokens\":2048,\"messages\":[{\"role\":\"user\",\"content\":",
                     model);
    if (n <= 0) return -1;
    bw += (size_t)n;
    size_t sw = json_enc(body + bw, sizeof body - bw, prompt);
    if (!sw) return -1;
    bw += sw;
    n = snprintf(body + bw, sizeof body - bw,
                   logprobs ? "}],\"logprobs\":true,\"top_logprobs\":5}"
                            : "}]}");
    if (n <= 0) return -1;
    bw += (size_t)n;

    dbuf_t resp = {0};
    CURL *c     = curl_easy_init();
    if (!c) return -1;
    struct curl_slist *hdr = NULL;
    char auth[512];
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
    hdr = curl_slist_append(hdr, auth);
    hdr = curl_slist_append(hdr, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bw);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_wcb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 90L);
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    int ok = -1;
    if (rc == CURLE_OK && status >= 200 && status < 300 && resp.data) {
        const char *mp = js_key(resp.data, resp.len, "message");
        const char *tp = NULL;
        if (mp) tp = js_key(mp, resp.len - (size_t)(mp - resp.data), "content");
        if (!tp) tp = js_key(resp.data, resp.len, "content");
        if (tp && *tp == '"')
            if (js_str(tp, resp.data + resp.len, out, ocap)) ok = 0;
    }
    free(resp.data);
    return ok;
}

static int call_gemini_http(const char *prompt, const char *key,
                            char *out, size_t ocap) {
    char url[512];
    snprintf(url, sizeof url,
             "https://generativelanguage.googleapis.com/v1beta/models/"
             "gemini-1.5-flash:generateContent?key=%s",
             key);
    static char body[65536];
    size_t bw = 0;
    int n = snprintf(body + bw, sizeof body - bw,
                     "{\"contents\":[{\"parts\":[{\"text\":");
    if (n <= 0) return -1;
    bw += (size_t)n;
    size_t sw = json_enc(body + bw, sizeof body - bw, prompt);
    if (!sw) return -1;
    bw += sw;
    n = snprintf(body + bw, sizeof body - bw, "}]}]}");
    if (n <= 0) return -1;
    bw += (size_t)n;

    dbuf_t resp = {0};
    CURL *c     = curl_easy_init();
    if (!c) return -1;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bw);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_wcb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 90L);
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);
    int ok = -1;
    if (rc == CURLE_OK && status >= 200 && status < 300 && resp.data) {
        const char *tp = js_key(resp.data, resp.len, "text");
        if (tp && *tp == '"')
            js_str(tp, resp.data + resp.len, out, ocap);
        ok = (out[0] != '\0') ? 0 : -1;
    }
    free(resp.data);
    return ok;
}

int cos_distill_teacher_count(void) {
    return g_nt;
}

const struct cos_distill_teacher *cos_distill_teacher_get(int idx) {
    if (idx < 0 || idx >= g_nt) return NULL;
    return &g_teachers[idx];
}

int cos_distill_fetch_teacher_text(const struct cos_distill_teacher *t,
                                   const char *prompt, char *out,
                                   size_t out_cap) {
    if (!t || !prompt || !out || out_cap < 2) return -1;
    out[0] = '\0';
    if (!strcmp(t->name, "claude")) {
        const char *k = env_claude();
        if (!k) return -1;
        return call_claude_http(prompt, k, out, out_cap);
    }
    if (!strcmp(t->name, "gpt")) {
        const char *k = env_openai();
        if (!k) return -1;
        const char *md = getenv("COS_DISTILL_GPT_MODEL");
        if (!md || !md[0]) md = "gpt-4o-mini";
        return call_openai_compat(
            "https://api.openai.com/v1/chat/completions", prompt, k, md,
            out, out_cap, 0);
    }
    if (!strcmp(t->name, "gemini")) {
        const char *k = env_gemini();
        if (!k) return -1;
        return call_gemini_http(prompt, k, out, out_cap);
    }
    if (!strcmp(t->name, "deepseek")) {
        const char *k = env_deepseek();
        if (!k) return -1;
        const char *md = getenv("COS_DISTILL_DEEPSEEK_MODEL");
        if (!md || !md[0]) md = "deepseek-chat";
        return call_openai_compat("https://api.deepseek.com/v1/chat/completions",
                                  prompt, k, md, out, out_cap, 0);
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Local σ                                                            */
/* ------------------------------------------------------------------ */

static float sigma_local_completion(const char *prompt, char *local_buf,
                                    size_t local_cap) {
    cos_bitnet_server_params_t pr;
    memset(&pr, 0, sizeof pr);
    pr.n_predict   = 384;
    pr.temperature = 0.25f;
    cos_bitnet_server_result_t br;
    memset(&br, 0, sizeof br);
    local_buf[0] = '\0';

    if (cos_bitnet_server_is_healthy() &&
        cos_bitnet_server_complete(prompt, &pr, &br) == 0 &&
        br.text != NULL) {
        snprintf(local_buf, local_cap, "%s", br.text);
        return br.sigma;
    }
    /* No live server — shape heuristic on empty tail. */
    (void)local_buf;
    return cos_bitnet_sigma_for_prompt_and_output(prompt, "");
}

static float sigma_teacher_local(const char *prompt, const char *response) {
    return cos_bitnet_sigma_for_prompt_and_output(
        prompt, response ? response : "");
}

/* ------------------------------------------------------------------ */
/* SQLite                                                             */
/* ------------------------------------------------------------------ */

static int db_exec_sql(const char *sql) {
    char *err = NULL;
    if (sqlite3_exec(g_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return -1;
    }
    return 0;
}

static int db_init_schema(void) {
    if (db_exec_sql("CREATE TABLE IF NOT EXISTS examples("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "prompt TEXT NOT NULL,"
                    "best_response TEXT NOT NULL,"
                    "teacher TEXT NOT NULL,"
                    "sigma_local REAL,"
                    "sigma_teacher REAL,"
                    "improvement REAL,"
                    "ts INTEGER"
                    ");") != 0)
        return -1;
    return db_exec_sql("CREATE TABLE IF NOT EXISTS teacher_stats("
                       "name TEXT PRIMARY KEY,"
                       "trust_ema REAL,"
                       "total_queries INTEGER,"
                       "best_count INTEGER"
                       ");");
}

static int db_load_teacher_stats(void) {
    sqlite3_stmt *st = NULL;
    const char *q =
        "SELECT name,trust_ema,total_queries,best_count FROM teacher_stats;";
    if (sqlite3_prepare_v2(g_db, q, -1, &st, NULL) != SQLITE_OK) return 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *nm = (const char *)sqlite3_column_text(st, 0);
        struct cos_distill_teacher *tw = find_teacher(nm);
        if (tw) {
            tw->trust_ema     = (float)sqlite3_column_double(st, 1);
            tw->total_queries = sqlite3_column_int(st, 2);
            tw->best_count    = sqlite3_column_int(st, 3);
        }
    }
    sqlite3_finalize(st);
    return 0;
}

static int db_upsert_teacher(const struct cos_distill_teacher *t) {
    sqlite3_stmt *st = NULL;
    const char *ins  = "INSERT INTO teacher_stats(name,trust_ema,total_queries,best_count)"
        "VALUES(?,?,?,?) ON CONFLICT(name) DO UPDATE SET trust_ema=excluded.trust_ema,"
        "total_queries=excluded.total_queries,best_count=excluded.best_count;";
    if (sqlite3_prepare_v2(g_db, ins, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, t->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 2, (double)t->trust_ema);
    sqlite3_bind_int(st, 3, t->total_queries);
    sqlite3_bind_int(st, 4, t->best_count);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

static int db_insert_example(const struct cos_distill_example *ex) {
    sqlite3_stmt *st = NULL;
    const char *ins =
        "INSERT INTO examples(prompt,best_response,teacher,sigma_local,"
        "sigma_teacher,improvement,ts) VALUES(?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(g_db, ins, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, ex->prompt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, ex->best_response, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, ex->teacher, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 4, (double)ex->sigma_local);
    sqlite3_bind_double(st, 5, (double)ex->sigma_teacher);
    sqlite3_bind_double(st, 6, (double)ex->improvement);
    sqlite3_bind_int64(st, 7, (sqlite3_int64)ex->timestamp);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int cos_distill_init(void) {
    if (g_inited) return 0;
    char cosd[512], dir[512], path[768];
    snprintf(cosd, sizeof cosd, "%s/.cos", resolve_home());
    snprintf(dir, sizeof dir, "%s/.cos/distill", resolve_home());
    if (ensure_dir(cosd) != 0) return -1;
    if (ensure_dir(dir) != 0) return -1;
    if (db_path(path, sizeof path) != 0) return -1;
    if (sqlite3_open(path, &g_db) != SQLITE_OK) return -1;
    if (db_init_schema() != 0) {
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }
    builtin_teachers_reset();
    db_load_teacher_stats();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_inited = 1;
    return 0;
}

void cos_distill_shutdown(void) {
    if (!g_inited) return;
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    curl_global_cleanup();
    g_inited = 0;
}

int cos_distill_add_teacher(const struct cos_distill_teacher *teacher) {
    if (!teacher || !teacher->name[0]) return -1;
    if (cos_distill_init() != 0) return -1;
    if (g_nt >= COS_DISTILL_MAX_T) return -1;
    g_teachers[g_nt] = *teacher;
    g_nt++;
    return 0;
}

int cos_distill_run(const char *prompt, const char *only_teachers,
                    struct cos_distill_example *result) {
    if (!prompt || !result) return -1;
    memset(result, 0, sizeof(*result));
    if (cos_distill_init() != 0) return -1;

    snprintf(result->prompt, sizeof result->prompt, "%s", prompt);

    char local_txt[4096];
    float sloc = sigma_local_completion(prompt, local_txt, sizeof local_txt);

    float best_sig   = 1.0f;
    char  best_txt[4096];
    char  best_nm[64];
    best_txt[0] = '\0';
    best_nm[0]  = '\0';

    /* Teacher loop */
    for (int i = 0; i < g_nt; i++) {
        if (!g_teachers[i].available) continue;
        if (!teacher_allowed(g_teachers[i].name, only_teachers)) continue;

        char rsp[8192];
        rsp[0] = '\0';
        if (cos_distill_fetch_teacher_text(&g_teachers[i], prompt, rsp,
                                           sizeof rsp)
            != 0)
            continue;

        float st_sig = sigma_teacher_local(prompt, rsp);
        g_teachers[i].total_queries++;
        /* Lower σ is better */
        if (st_sig < best_sig) {
            best_sig = st_sig;
            snprintf(best_txt, sizeof best_txt, "%s", rsp);
            snprintf(best_nm, sizeof best_nm, "%s", g_teachers[i].name);
        }
    }

    float sig_teacher_meas = best_nm[0] ? best_sig : sloc;
    snprintf(result->best_response, sizeof result->best_response, "%s",
             best_txt[0] ? best_txt : local_txt);
    snprintf(result->teacher, sizeof result->teacher, "%s",
             best_nm[0] ? best_nm : "local");
    result->sigma_local   = sloc;
    result->sigma_teacher = sig_teacher_meas;
    result->improvement   = sloc - sig_teacher_meas;
    result->timestamp     = (int64_t)time(NULL);

    /* Store when cloud beat local by a margin */
    if (best_nm[0] && result->improvement > IMPROVE_EPS) {
        struct cos_distill_teacher *tw = find_teacher(best_nm);
        if (tw) {
            tw->trust_ema = 0.9f * tw->trust_ema + 0.1f * sig_teacher_meas;
            tw->best_count++;
            db_upsert_teacher(tw);
        }
        db_insert_example(result);
    }

    /* Persist query counts for all touched teachers */
    for (int i = 0; i < g_nt; i++)
        if (g_teachers[i].total_queries > 0) db_upsert_teacher(&g_teachers[i]);

    return 0;
}

int cos_distill_batch(const char *prompts_path, const char *only_teachers,
                      int *n_distilled, int *n_skipped) {
    if (!prompts_path || !n_distilled || !n_skipped) return -1;
    *n_distilled = 0;
    *n_skipped   = 0;
    if (cos_distill_init() != 0) return -1;
    FILE *f = fopen(prompts_path, "r");
    if (!f) return -1;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[n - 1] = '\0';
            n--;
        }
        if (!line[0] || line[0] == '#') continue;
        struct cos_distill_example ex;
        if (cos_distill_run(line, only_teachers, &ex) != 0) continue;
        if (ex.improvement > IMPROVE_EPS && ex.teacher[0] &&
            strcmp(ex.teacher, "local") != 0)
            (*n_distilled)++;
        else
            (*n_skipped)++;
    }
    fclose(f);
    return 0;
}

int cos_distill_export_training_data(const char *output_path, int format) {
    if (!output_path || cos_distill_init() != 0) return -1;
    FILE *fo = fopen(output_path, "w");
    if (!fo) return -1;
    sqlite3_stmt *st = NULL;
    const char *q =
        "SELECT prompt,best_response,teacher FROM examples ORDER BY id;";
    if (sqlite3_prepare_v2(g_db, q, -1, &st, NULL) != SQLITE_OK) {
        fclose(fo);
        return -1;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *p =
            (const char *)sqlite3_column_text(st, 0);
        const char *b =
            (const char *)sqlite3_column_text(st, 1);
        if (!p) p = "";
        if (!b) b = "";
        if (format == (int)COS_DISTILL_EXPORT_JSONL) {
            fputs("{\"instruction\":\"", fo);
            for (const char *s = p; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\",\"output\":\"", fo);
            for (const char *s = b; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\",\"source\":\"cos-distill\"}\n", fo);
        } else if (format == (int)COS_DISTILL_EXPORT_ALPACA) {
            fputs("{\"instruction\":\"", fo);
            for (const char *s = p; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\",\"input\":\"\",\"output\":\"", fo);
            for (const char *s = b; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\"}\n", fo);
        } else {
            fputs("{\"conversations\":[{\"from\":\"human\",\"value\":\"", fo);
            for (const char *s = p; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\"},{\"from\":\"gpt\",\"value\":\"", fo);
            for (const char *s = b; *s; s++) {
                if (*s == '"' || *s == '\\')
                    fputc('\\', fo);
                fputc(*s, fo);
            }
            fputs("\"}]}\n", fo);
        }
    }
    sqlite3_finalize(st);
    fclose(fo);
    return 0;
}

float cos_distill_teacher_ranking(struct cos_distill_teacher *teachers,
                                  int max, int *n_found) {
    if (!teachers || max <= 0 || !n_found) return -1.0f;
    *n_found = (g_nt < max) ? g_nt : max;
    for (int i = 0; i < *n_found; i++) teachers[i] = g_teachers[i];
    /* sort by trust_ema ascending (lower residual σ better) */
    for (int i = 0; i < *n_found; i++)
        for (int j = i + 1; j < *n_found; j++)
            if (teachers[j].trust_ema < teachers[i].trust_ema) {
                struct cos_distill_teacher tmp = teachers[i];
                teachers[i]                    = teachers[j];
                teachers[j]                    = tmp;
            }
    return teachers[0].trust_ema;
}

int cos_distill_fprint_history(FILE *f, int max_rows) {
    if (!f || max_rows <= 0) return -1;
    if (cos_distill_init() != 0) return -1;
    sqlite3_stmt *st = NULL;
    const char *q =
        "SELECT ts,teacher,sigma_local,sigma_teacher,improvement,"
        "substr(prompt,1,120),substr(best_response,1,120) "
        "FROM examples ORDER BY id DESC LIMIT ?;";
    if (sqlite3_prepare_v2(g_db, q, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(st, 1, max_rows);
    while (sqlite3_step(st) == SQLITE_ROW) {
        sqlite3_int64 ts = sqlite3_column_int64(st, 0);
        const char *tn = (const char *)sqlite3_column_text(st, 1);
        double sl = sqlite3_column_double(st, 2);
        double stg = sqlite3_column_double(st, 3);
        double im = sqlite3_column_double(st, 4);
        const char *pr = (const char *)sqlite3_column_text(st, 5);
        const char *br = (const char *)sqlite3_column_text(st, 6);
        fprintf(f,
                "ts=%lld teacher=%s σ_loc=%.4f σ_t=%.4f Δ=%.4f\n"
                "  prompt: %s\n  best: %s\n",
                (long long)ts, tn ? tn : "?", sl, stg, im,
                pr ? pr : "", br ? br : "");
    }
    sqlite3_finalize(st);
    return 0;
}

int cos_distill_stats(long *n_rows, double *avg_improvement,
                      double *avg_sigma_teacher) {
    if (!n_rows || !avg_improvement || !avg_sigma_teacher) return -1;
    if (cos_distill_init() != 0) return -1;
    sqlite3_stmt *st = NULL;
    const char *q =
        "SELECT COUNT(*), AVG(improvement), AVG(sigma_teacher) FROM examples;";
    if (sqlite3_prepare_v2(g_db, q, -1, &st, NULL) != SQLITE_OK) return -1;
    int rc = SQLITE_ROW;
    if (sqlite3_step(st) != SQLITE_ROW)
        rc = -1;
    else {
        *n_rows = (long)sqlite3_column_int64(st, 0);
        *avg_improvement =
            sqlite3_column_type(st, 1) == SQLITE_NULL ? 0.0
                                                      : sqlite3_column_double(st, 1);
        *avg_sigma_teacher =
            sqlite3_column_type(st, 2) == SQLITE_NULL ? 0.0
                                                       : sqlite3_column_double(st, 2);
    }
    sqlite3_finalize(st);
    return rc == SQLITE_ROW ? 0 : -1;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(CREATION_OS_DISTILL_TEST)
int cos_distill_self_test(void) {
    char prev[1024];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    if (setenv("HOME", "/tmp", 1) != 0) return -1;
    /* Avoid slow llama-server integration during unit test */
    (void)setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1);
    (void)setenv("COS_BITNET_SERVER_PORT", "65531", 1);
    cos_distill_shutdown();
    g_db     = NULL;
    g_inited = 0;
    if (cos_distill_init() != 0) {
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -1;
    }
    struct cos_distill_teacher custom = {0};
    snprintf(custom.name, sizeof custom.name, "unit_local");
    snprintf(custom.endpoint, sizeof custom.endpoint, "http://127.0.0.1:9/none");
    custom.available = 0;
    (void)cos_distill_add_teacher(&custom);

    struct cos_distill_example ex;
    memset(&ex, 0, sizeof ex);
    if (cos_distill_run("2+2=?", NULL, &ex) != 0) {
        cos_distill_shutdown();
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -2;
    }
    long nr;
    double ai, ast;
    if (cos_distill_stats(&nr, &ai, &ast) != 0) {
        cos_distill_shutdown();
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -3;
    }
    struct cos_distill_teacher ts[8];
    int nf = 0;
    (void)cos_distill_teacher_ranking(ts, 8, &nf);

    char export_path[128];
    snprintf(export_path, sizeof export_path, "/tmp/cos_distill_export_%ld.jsonl",
             (long)getpid());
    if (cos_distill_export_training_data(
            export_path, (int)COS_DISTILL_EXPORT_JSONL)
        != 0) {
        cos_distill_shutdown();
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -4;
    }
    cos_distill_shutdown();
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
#endif
