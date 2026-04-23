/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "swarm_coordinator.h"

#include "../cli/cos_think.h"

#include <assert.h>
#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SC_MAX COS_SWARM_MAX_PEERS_COORD

static struct cos_swarm_peer g_peers[SC_MAX];
static int                   g_np;
static int                   g_inited;
static sqlite3              *g_db;

typedef struct {
    char  *data;
    size_t len, cap;
} mbuf_t;

static size_t cwcb(void *p, size_t sz, size_t nm, void *ud)
{
    size_t n = sz * nm;
    mbuf_t *m = (mbuf_t *)ud;
    if (m->len + n + 1 > m->cap) {
        size_t nc = m->cap ? m->cap * 2 : 4096;
        while (nc < m->len + n + 1) nc *= 2;
        char *nd = realloc(m->data, nc);
        if (!nd) return 0;
        m->data = nd;
        m->cap  = nc;
    }
    memcpy(m->data + m->len, p, n);
    m->len += n;
    if (m->data) m->data[m->len] = '\0';
    return n;
}

static const char *home_cos(void)
{
    static char b[512];
    const char *h = getenv("HOME");
    if (!h || !h[0]) {
        struct passwd *pw = getpwuid(getuid());
        h = (pw && pw->pw_dir) ? pw->pw_dir : ".";
    }
    snprintf(b, sizeof b, "%s/.cos/swarm", h);
    return b;
}

static int ensure_swarm_dirs(void)
{
    char tmp[768];
    const char *h = getenv("HOME");
    if (!h || !h[0]) return -1;
    snprintf(tmp, sizeof tmp, "%s/.cos", h);
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return -1;
    snprintf(tmp, sizeof tmp, "%s/.cos/swarm", h);
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static const char *js_scan(const char *hay, size_t hl, const char *key)
{
    size_t klen = strlen(key);
    for (size_t i = 0; i + klen + 2 < hl; i++) {
        if (hay[i] != '"') continue;
        if (strncmp(hay + i + 1, key, klen) != 0) continue;
        if (hay[i + 1 + klen] != '"') continue;
        const char *q = hay + i + 1 + klen + 1;
        while (q < hay + hl && (*q == ' ' || *q == '\t')) q++;
        if (q >= hay + hl || *q != ':') continue;
        q++;
        while (q < hay + hl && (*q == ' ' || *q == '\t')) q++;
        return q;
    }
    return NULL;
}

static int js_read_string(const char *p, const char *end, char *out, size_t cap)
{
    if (!p || p >= end || *p != '"') return -1;
    p++;
    size_t w = 0;
    while (p < end && *p != '"' && w + 1 < cap) {
        char c = *p++;
        if (*p == '\\' && p < end)
            ; /* minimal \n */
        out[w++] = c;
    }
    out[w] = '\0';
    return 0;
}

static void normalize_endpoint(const char *ep, char *out, size_t cap)
{
    while (*ep == ' ' || *ep == '\t') ep++;
    if (!strncmp(ep, "http://", 7u) || !strncmp(ep, "https://", 8u))
        snprintf(out, cap, "%s", ep);
    else
        snprintf(out, cap, "http://%s", ep);
}

static int http_swarm_query(const char *endpoint, const char *prompt,
                            char *peer_out, size_t pid_cap,
                            char *rsp_out, size_t rsp_cap,
                            float *sigma_o, enum cos_error_source *attr_o,
                            int64_t *lat_ms)
{
    char url[384], body[8192];
    normalize_endpoint(endpoint, url, sizeof url);
    size_t uw = strlen(url);
    while (uw > 0 && url[uw - 1] == '/') uw--;
    url[uw] = '\0';
    snprintf(url + uw, sizeof url - uw, "/cos/swarm/query");

    snprintf(body, sizeof body, "{\"prompt\":");
    size_t bw = strlen(body);
    /* Minimal JSON encode prompt */
    body[bw++] = '"';
    for (const unsigned char *s = (const unsigned char *)prompt;
         *s && bw + 2 < sizeof body; s++) {
        if (*s == '"' || *s == '\\') body[bw++] = '\\';
        body[bw++] = (char)*s;
    }
    body[bw++] = '"';
    body[bw++] = '}';
    body[bw]   = '\0';

    mbuf_t resp = {0};
    CURL *c = curl_easy_init();
    if (!c) return -1;
    struct curl_slist *hdr =
        curl_slist_append(NULL, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, cwcb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 120L);

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);
    CURLcode cc = curl_easy_perform(c);
    gettimeofday(&tv1, NULL);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);

    int64_t lat = ((int64_t)(tv1.tv_sec - tv0.tv_sec)) * 1000LL
                + ((tv1.tv_usec - tv0.tv_usec) / 1000LL);
    if (lat_ms) *lat_ms = lat;

    int ok = -1;
    rsp_out[0] = peer_out[0] = '\0';
    *sigma_o = 1.0f;
    *attr_o   = COS_ERR_REASONING;

    if (cc == CURLE_OK && resp.data != NULL && resp.len > 2) {
        const char *sp = js_scan(resp.data, resp.len, "sigma");
        if (sp) {
            *sigma_o = (float)strtod(sp, NULL);
            if (*sigma_o < 0.f) *sigma_o = 0.f;
            if (*sigma_o > 1.f) *sigma_o = 1.f;
        }
        sp = js_scan(resp.data, resp.len, "response");
        if (sp && *sp == '"')
            js_read_string(sp, resp.data + resp.len, rsp_out, rsp_cap);
        sp = js_scan(resp.data, resp.len, "peer_id");
        if (!sp)
            sp = js_scan(resp.data, resp.len, "peer");
        if (sp && *sp == '"')
            js_read_string(sp, resp.data + resp.len, peer_out, pid_cap);
        ok = rsp_out[0] ? 0 : -1;
    }
    free(resp.data);
    return ok;
}

static float jaccard_words(const char *a, const char *b)
{
    if (!a || !b) return 0.f;
    char ba[4096], bb[4096];
    snprintf(ba, sizeof ba, "%s", a);
    snprintf(bb, sizeof bb, "%s", b);
    char *wa[256];
    char *wb[256];
    int na = 0, nb = 0;
    for (char *t = strtok(ba, " \t\n\r"); t && na < 256;
         t = strtok(NULL, " \t\n\r"))
        wa[na++] = t;
    for (char *t = strtok(bb, " \t\n\r"); t && nb < 256;
         t = strtok(NULL, " \t\n\r"))
        wb[nb++] = t;
    if (!na || !nb) return 0.f;
    int inter = 0;
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++)
            if (!strcmp(wa[i], wb[j])) {
                inter++;
                break;
            }
    int uni = na + nb - inter;
    return uni > 0 ? (float)inter / (float)uni : 0.f;
}

static float consensus_metric(struct cos_swarm_vote *votes, int n)
{
    if (n < 2) return 1.0f;
    float sum = 0.f;
    int pairs = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            sum += jaccard_words(votes[i].response, votes[j].response);
            pairs++;
        }
    return pairs ? sum / (float)pairs : 0.f;
}

int cos_swarm_vote_lowest_sigma(struct cos_swarm_vote *votes, int n,
                                 struct cos_swarm_result *result)
{
    if (!votes || n <= 0 || !result) return -1;
    memset(result, 0, sizeof *result);
    int bi = 0;
    float best = votes[0].sigma;
    for (int i = 1; i < n; i++)
        if (votes[i].sigma < best) {
            best = votes[i].sigma;
            bi   = i;
        }
    result->n_votes = n;
    for (int i = 0; i < n && i < COS_SWARM_MAX_VOTES; i++)
        result->votes[i] = votes[i];
    snprintf(result->best_response, sizeof result->best_response, "%s",
             votes[bi].response);
    result->sigma_best       = votes[bi].sigma;
    result->sigma_consensus = consensus_metric(votes, n);
    result->consensus =
        (n >= 2 && result->sigma_consensus >= 0.55f) ? 1 : 0;
    snprintf(result->method, sizeof result->method, "%s", "lowest_sigma");
    return 0;
}

static int votes_cluster(const struct cos_swarm_vote *a,
                         const struct cos_swarm_vote *b)
{
    if (!strcmp(a->response, b->response)) return 1;
    return jaccard_words(a->response, b->response) >= 0.55f;
}

int cos_swarm_vote_majority(struct cos_swarm_vote *votes, int n,
                            struct cos_swarm_result *result)
{
    if (!votes || n <= 0 || !result) return -1;
    memset(result, 0, sizeof *result);

    int bi        = 0;
    int best_sup  = -1;
    for (int i = 0; i < n; i++) {
        int sup = 0;
        for (int j = 0; j < n; j++)
            if (votes_cluster(votes + i, votes + j)) sup++;

        if (sup > best_sup ||
            (sup == best_sup && votes[i].sigma < votes[bi].sigma)) {
            best_sup = sup;
            bi       = i;
        }
    }

    result->n_votes = n;
    for (int i = 0; i < n && i < COS_SWARM_MAX_VOTES; i++)
        result->votes[i] = votes[i];
    snprintf(result->best_response, sizeof result->best_response, "%s",
             votes[bi].response);
    result->sigma_best       = votes[bi].sigma;
    result->sigma_consensus = consensus_metric(votes, n);
    result->consensus       = best_sup >= (n + 1) / 2 ? 1 : 0;
    snprintf(result->method, sizeof result->method, "%s", "majority_vote");
    return 0;
}

int cos_swarm_vote_sigma_weighted(struct cos_swarm_vote *votes, int n,
                                  struct cos_swarm_result *result)
{
    if (!votes || n <= 0 || !result) return -1;
    memset(result, 0, sizeof *result);
    double sum = 0.0;
    double wsum = 0.0;
    int all_numeric = 1;
    for (int i = 0; i < n; i++) {
        char *end = NULL;
        double v = strtod(votes[i].response, &end);
        if (!end || end == votes[i].response || *end != '\0')
            all_numeric = 0;
        double w = (double)(1.0f - votes[i].sigma);
        if (w < 1e-9) w = 1e-9;
        sum += v * w;
        wsum += w;
    }
    result->n_votes = n;
    for (int i = 0; i < n && i < COS_SWARM_MAX_VOTES; i++)
        result->votes[i] = votes[i];

    if (all_numeric && wsum > 0.0) {
        snprintf(result->best_response, sizeof result->best_response, "%.12g",
                 sum / wsum);
        snprintf(result->method, sizeof result->method, "%s",
                 "sigma_weighted_numeric");
    } else {
        return cos_swarm_vote_lowest_sigma(votes, n, result);
    }
    /* reuse consensus */
    result->sigma_consensus = consensus_metric(votes, n);
    result->sigma_best      = votes[0].sigma;
    for (int i = 1; i < n; i++)
        if (votes[i].sigma < result->sigma_best)
            result->sigma_best = votes[i].sigma;
    result->consensus =
        (n >= 2 && result->sigma_consensus >= 0.55f) ? 1 : 0;
    return 0;
}

static int peer_idx(const char *id)
{
    for (int i = 0; i < g_np; i++)
        if (!strcmp(g_peers[i].peer_id, id)) return i;
    return -1;
}

int cos_swarm_update_trust(const char *peer_id, float sigma, int was_best)
{
    int i = peer_idx(peer_id);
    if (i < 0) return -1;
    float w = was_best ? 0.15f : 0.05f;
    g_peers[i].trust =
        (1.0f - w) * g_peers[i].trust + w * (1.0f - sigma);
    if (was_best) g_peers[i].best_count++;
    return 0;
}

int cos_swarm_coord_add_peer(const struct cos_swarm_peer *peer)
{
    if (!peer || !peer->endpoint[0]) return -1;
    if (cos_swarm_coord_init() != 0) return -1;
    if (g_np >= SC_MAX) return -1;
    g_peers[g_np++] = *peer;
    return 0;
}

int cos_swarm_coord_remove_peer(const char *peer_id)
{
    int i = peer_idx(peer_id);
    if (i < 0) return -1;
    memmove(g_peers + i, g_peers + i + 1,
            (size_t)(g_np - i - 1) * sizeof g_peers[0]);
    g_np--;
    return 0;
}

int cos_swarm_coord_peers_list(struct cos_swarm_peer *peers, int max, int *n)
{
    if (!peers || max <= 0 || !n) return -1;
    *n = g_np < max ? g_np : max;
    for (int i = 0; i < *n; i++) peers[i] = g_peers[i];
    return 0;
}

static int db_open(void)
{
    if (g_db) return 0;
    char path[768];
    snprintf(path, sizeof path, "%s/coord.sqlite", home_cos());
    return sqlite3_open(path, &g_db) == SQLITE_OK ? 0 : -1;
}

static void db_peers_sync(void)
{
    if (!g_db) return;
    sqlite3_exec(g_db,
                 "CREATE TABLE IF NOT EXISTS peers(pid TEXT PRIMARY KEY,"
                 "endpoint TEXT,trust REAL,sigma_mean REAL,responses INTEGER,"
                 "best_count INTEGER);",
                 NULL, NULL, NULL);
    sqlite3_stmt *del = NULL;
    sqlite3_prepare_v2(g_db, "DELETE FROM peers;", -1, &del, NULL);
    sqlite3_step(del);
    sqlite3_finalize(del);

    sqlite3_stmt *ins = NULL;
    sqlite3_prepare_v2(g_db,
                         "INSERT INTO peers(pid,endpoint,trust,sigma_mean,"
                         "responses,best_count)VALUES(?,?,?,?,?,?);",
                         -1, &ins, NULL);
    for (int i = 0; i < g_np; i++) {
        sqlite3_bind_text(ins, 1, g_peers[i].peer_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, g_peers[i].endpoint, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(ins, 3, (double)g_peers[i].trust);
        sqlite3_bind_double(ins, 4, (double)g_peers[i].sigma_mean);
        sqlite3_bind_int(ins, 5, g_peers[i].responses);
        sqlite3_bind_int(ins, 6, g_peers[i].best_count);
        sqlite3_step(ins);
        sqlite3_reset(ins);
    }
    sqlite3_finalize(ins);
}

static void db_peers_load(void)
{
    if (!g_db) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT pid,endpoint,trust,sigma_mean,"
                                   "responses,best_count FROM peers;",
                           -1, &st, NULL)
        != SQLITE_OK)
        return;
    g_np = 0;
    while (sqlite3_step(st) == SQLITE_ROW && g_np < SC_MAX) {
        snprintf(g_peers[g_np].peer_id, sizeof g_peers[g_np].peer_id, "%s",
                 (const char *)sqlite3_column_text(st, 0));
        snprintf(g_peers[g_np].endpoint, sizeof g_peers[g_np].endpoint, "%s",
                 (const char *)sqlite3_column_text(st, 1));
        g_peers[g_np].trust       = (float)sqlite3_column_double(st, 2);
        g_peers[g_np].sigma_mean  = (float)sqlite3_column_double(st, 3);
        g_peers[g_np].responses   = sqlite3_column_int(st, 4);
        g_peers[g_np].best_count  = sqlite3_column_int(st, 5);
        g_peers[g_np].available   = 1;
        g_peers[g_np].last_seen_ms = 0;
        g_np++;
    }
    sqlite3_finalize(st);
}

int cos_swarm_coord_init(void)
{
    if (g_inited) return 0;
    if (ensure_swarm_dirs() != 0) return -1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    db_open();
    db_peers_load();
    g_inited = 1;
    return 0;
}

void cos_swarm_coord_shutdown(void)
{
    if (!g_inited) return;
    db_peers_sync();
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    curl_global_cleanup();
    g_inited = 0;
}

int cos_swarm_broadcast(const char *prompt, enum cos_swarm_vote_strategy strat,
                        struct cos_swarm_result *result)
{
    if (!prompt || !result) return -1;
    if (cos_swarm_coord_init() != 0) return -2;
    memset(result, 0, sizeof *result);

    struct cos_swarm_vote votes[COS_SWARM_MAX_VOTES];
    int nv = 0;

    int64_t now_ms = (int64_t)time(NULL) * 1000LL;

    for (int i = 0; i < g_np && nv < COS_SWARM_MAX_VOTES; i++) {
        char rsp[4096];
        char pid[64];
        float sg;
        enum cos_error_source at;
        int64_t lat;
        int rc = http_swarm_query(g_peers[i].endpoint, prompt, pid,
                                  sizeof pid, rsp, sizeof rsp, &sg, &at, &lat);
        g_peers[i].last_seen_ms = now_ms;
        g_peers[i].available    = (rc == 0);

        if (rc != 0) continue;

        g_peers[i].responses++;
        g_peers[i].sigma_mean =
            0.95f * g_peers[i].sigma_mean + 0.05f * sg;

        snprintf(votes[nv].peer_id, sizeof votes[nv].peer_id, "%s",
                 pid[0] ? pid : g_peers[i].peer_id);
        snprintf(votes[nv].response, sizeof votes[nv].response, "%s", rsp);
        votes[nv].sigma        = sg;
        votes[nv].attribution  = at;
        votes[nv].latency_ms   = lat;
        nv++;
    }

    if (nv == 0) return -3;

    struct cos_swarm_result tmp;
    int tr = -1;
    if (strat == COS_SWARM_VOTE_MAJORITY)
        tr = cos_swarm_vote_majority(votes, nv, &tmp);
    else if (strat == COS_SWARM_VOTE_SIGMA_WEIGHTED)
        tr = cos_swarm_vote_sigma_weighted(votes, nv, &tmp);
    else
        tr = cos_swarm_vote_lowest_sigma(votes, nv, &tmp);
    if (tr != 0) return -4;
    *result = tmp;

    for (int i = 0; i < nv; i++) {
        int win =
            strcmp(votes[i].response, result->best_response) == 0 ? 1 : 0;
        if (votes[i].peer_id[0])
            (void)cos_swarm_update_trust(votes[i].peer_id, votes[i].sigma,
                                         win);
    }

    db_peers_sync();
    return 0;
}

static int parse_urls_to_peers(const char *comma)
{
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", comma);
    char *save = NULL;
    int idx = 0;
    for (char *tok = strtok_r(buf, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save), idx++) {
        while (*tok == ' ' || *tok == '\t') tok++;
        if (!*tok) continue;
        struct cos_swarm_peer p;
        memset(&p, 0, sizeof p);
        snprintf(p.peer_id, sizeof p.peer_id, "url%d", idx);
        snprintf(p.endpoint, sizeof p.endpoint, "%s", tok);
        p.trust      = 0.5f;
        p.sigma_mean = 0.5f;
        p.available  = 1;
        cos_swarm_coord_add_peer(&p);
    }
    return g_np > 0 ? 0 : -1;
}

int cos_swarm_omega_rescue(const char *goal, struct cos_think_result *tr)
{
    if (!goal || !tr) return -1;
    const char *urls = getenv("COS_SWARM_PEER_URLS");
    if (!urls || !urls[0]) return -1;

    int saved_np = g_np;
    parse_urls_to_peers(urls);

    struct cos_swarm_result sr;
    memset(&sr, 0, sizeof sr);
    enum cos_swarm_vote_strategy st = COS_SWARM_VOTE_LOWEST_SIGMA;
    const char *ss = getenv("COS_SWARM_STRATEGY");
    if (ss && !strcasecmp(ss, "majority")) st = COS_SWARM_VOTE_MAJORITY;
    else if (ss && !strcasecmp(ss, "weighted"))
        st = COS_SWARM_VOTE_SIGMA_WEIGHTED;

    int bc = cos_swarm_broadcast(goal, st, &sr);
    /* Remove ephemeral URL peers */
    while (g_np > saved_np)
        cos_swarm_coord_remove_peer(g_peers[g_np - 1].peer_id);

    if (bc != 0) return -2;
    if (sr.sigma_best >= tr->sigma_mean - 1e-6f) return -3;

    snprintf(tr->goal, sizeof tr->goal, "%s", goal);
    tr->n_subtasks                = 1;
    tr->best_subtask_idx          = 0;
    tr->sigma_mean                = sr.sigma_best;
    tr->sigma_min                 = sr.sigma_best;
    tr->sigma_max                 = sr.sigma_best;
    tr->subtasks[0].sigma_combined = sr.sigma_best;
    snprintf(tr->subtasks[0].answer, sizeof tr->subtasks[0].answer, "%s",
             sr.best_response);
    tr->subtasks[0].final_action = 0; /* COS_SIGMA_ACTION_ACCEPT */
    tr->subtasks[0].attribution  = COS_ERR_NONE;
    return 0;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_SWARM_COORD_TEST)
int cos_swarm_coord_self_test(void)
{
    struct cos_swarm_vote v[3];
    memset(v, 0, sizeof v);
    snprintf(v[0].peer_id, sizeof v[0].peer_id, "a");
    snprintf(v[0].response, sizeof v[0].response, "%s", "hello world");
    v[0].sigma = 0.8f;
    snprintf(v[1].peer_id, sizeof v[1].peer_id, "b");
    snprintf(v[1].response, sizeof v[1].response, "%s", "hello world");
    v[1].sigma = 0.3f;
    snprintf(v[2].peer_id, sizeof v[2].peer_id, "c");
    snprintf(v[2].response, sizeof v[2].response, "%s", "other");
    v[2].sigma = 0.9f;

    struct cos_swarm_result r;
    assert(cos_swarm_vote_lowest_sigma(v, 3, &r) == 0);
    assert(r.sigma_best < 0.35f);

    assert(cos_swarm_vote_majority(v, 3, &r) == 0);

    snprintf(v[0].response, sizeof v[0].response, "%s", "3.0");
    snprintf(v[1].response, sizeof v[1].response, "%s", "5.0");
    snprintf(v[2].response, sizeof v[2].response, "%s", "7.0");
    v[0].sigma = v[1].sigma = v[2].sigma = 0.2f;
    assert(cos_swarm_vote_sigma_weighted(v, 3, &r) == 0);

    assert(cos_swarm_coord_init() == 0);
    struct cos_swarm_peer p = {0};
    snprintf(p.peer_id, sizeof p.peer_id, "unit");
    snprintf(p.endpoint, sizeof p.endpoint, "%s", "127.0.0.1:9");
    p.trust = 0.5f;
    cos_swarm_coord_add_peer(&p);
    assert(cos_swarm_update_trust("unit", 0.4f, 1) == 0);
    cos_swarm_coord_shutdown();
    return 0;
}
#endif
