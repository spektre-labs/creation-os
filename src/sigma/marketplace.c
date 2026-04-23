/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "marketplace.h"

#include "../cli/cos_think.h"
#include "error_attribution.h"
#include "proof_receipt.h"
#include "reputation.h"
#include "reinforce.h"

#include <curl/curl.h>
#include <errno.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_MARKETPLACE_TEST)
#include <assert.h>
#endif

static sqlite3 *g_db;
static int      g_inited;

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

static const char *home_mp(void)
{
    static char b[512];
    const char *h = getenv("HOME");
    if (!h || !h[0]) {
        struct passwd *pw = getpwuid(getuid());
        h = (pw && pw->pw_dir) ? pw->pw_dir : ".";
    }
    snprintf(b, sizeof b, "%s/.cos/marketplace", h);
    return b;
}

static int ensure_dirs(void)
{
    char p[768];
    const char *h = getenv("HOME");
    if (!h || !h[0]) return -1;
    snprintf(p, sizeof p, "%s/.cos", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
    snprintf(p, sizeof p, "%s/.cos/marketplace", h);
    if (mkdir(p, 0700) != 0 && errno != EEXIST) return -1;
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
        if (*p == '\\' && p + 1 < end) {
            p++;
            if (*p == 'n')
                out[w++] = '\n';
            else if (*p == 't')
                out[w++] = '\t';
            else if (*p == 'r')
                out[w++] = '\r';
            else
                out[w++] = *p;
            p++;
            continue;
        }
        out[w++] = *p++;
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

static int hex_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_decode_struct(const char *hex, unsigned char *out, size_t need)
{
    size_t n = strlen(hex);
    if (n < need * 2) return -1;
    for (size_t i = 0; i < need; i++) {
        int hi = hex_val(hex[i * 2]);
        int lo = hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

static const char *hex_field_begin(const char *hay, size_t hl)
{
    const char *t = hay;
    while (t < hay + hl - 14) {
        if (strncmp(t, "\"receipt_hex\"", 13) == 0) {
            const char *v = strchr(t + 13, ':');
            if (!v) return NULL;
            v++;
            while (v < hay + hl && (*v == ' ' || *v == '\t')) v++;
            if (v < hay + hl && *v == '"') return v + 1;
        }
        t++;
    }
    return NULL;
}

static int parse_http_service_response(const char *json, size_t jl,
                                       struct cos_service_response *resp)
{
    const char *sp = js_scan(json, jl, "sigma");
    if (sp)
        resp->sigma = (float)strtod(sp, NULL);
    sp = js_scan(json, jl, "attribution");
    if (sp)
        resp->attribution = (cos_error_source_t)atoi(sp);
    sp = js_scan(json, jl, "cost_eur");
    if (sp)
        resp->cost_eur = (float)strtod(sp, NULL);
    sp = js_scan(json, jl, "response");
    if (sp && *sp == '"')
        js_read_string(sp, json + jl, resp->response, sizeof resp->response);

    const char *rh = hex_field_begin(json, jl);
    if (rh) {
        const char *endq = (const char *)memchr(rh, '"', json + jl - rh);
        if (endq && (size_t)(endq - rh) >= sizeof(resp->receipt) * 2) {
            char tmp[sizeof(resp->receipt) * 2 + 4];
            size_t tl = (size_t)(endq - rh);
            if (tl >= sizeof tmp) tl = sizeof tmp - 1;
            memcpy(tmp, rh, tl);
            tmp[tl] = '\0';
            if (hex_decode_struct(tmp, (unsigned char *)&resp->receipt,
                                   sizeof(resp->receipt))
                == 0)
                return 0;
        }
    }
    return resp->response[0] ? 0 : -1;
}

static int json_escape_append(char *buf, size_t cap, size_t *pos,
                              const char *s)
{
    if (*pos >= cap) return -1;
    for (; *s && *pos + 4 < cap; s++) {
        if (*s == '"' || *s == '\\') buf[(*pos)++] = '\\';
        buf[(*pos)++] = *s;
    }
    buf[*pos] = '\0';
    return 0;
}

static void upsert_stat(const char *k, double delta)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "INSERT INTO stats(k,v)VALUES(?,?)ON CONFLICT(k)DO "
                             "UPDATE SET v=v+excluded.v;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return;
    sqlite3_bind_text(st, 1, k, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 2, delta);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

static double get_stat(const char *k)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db, "SELECT v FROM stats WHERE k=?;", -1, &st,
                             NULL)
        != SQLITE_OK)
        return 0.;
    sqlite3_bind_text(st, 1, k, -1, SQLITE_TRANSIENT);
    double v = 0.;
    if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_double(st, 0);
    sqlite3_finalize(st);
    return v;
}

int cos_marketplace_init(void)
{
    if (g_inited) return 0;
    if (ensure_dirs() != 0) return -1;
    char path[768];
    snprintf(path, sizeof path, "%s/catalog.sqlite", home_mp());
    if (sqlite3_open(path, &g_db) != SQLITE_OK) return -1;
    sqlite3_exec(g_db,
                 "CREATE TABLE IF NOT EXISTS services("
                 "service_id TEXT PRIMARY KEY,"
                 "name TEXT,"
                 "description TEXT,"
                 "provider_id TEXT,"
                 "sigma_mean REAL,"
                 "reliability REAL,"
                 "times_served INTEGER,"
                 "cost_eur REAL,"
                 "registered INTEGER,"
                 "endpoint TEXT"
                 ");",
                 NULL, NULL, NULL);
    sqlite3_exec(g_db,
                 "CREATE TABLE IF NOT EXISTS stats(k TEXT PRIMARY KEY,v REAL "
                 "NOT NULL);",
                 NULL, NULL, NULL);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_inited = 1;
    return 0;
}

void cos_marketplace_shutdown(void)
{
    if (!g_inited) return;
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    curl_global_cleanup();
    g_inited = 0;
}

int cos_marketplace_register_service(const struct cos_service *service)
{
    if (!service || !service->service_id[0]) return -1;
    if (cos_marketplace_init() != 0) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "INSERT OR REPLACE INTO services(service_id,name,"
                             "description,provider_id,sigma_mean,reliability,"
                             "times_served,cost_eur,registered,endpoint)VALUES("
                             "?,?,?,?,?,?,?,?,?,?);",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, service->service_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, service->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, service->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, service->provider_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 5, (double)service->sigma_mean);
    sqlite3_bind_double(st, 6, (double)service->reliability);
    sqlite3_bind_int(st, 7, service->times_served);
    sqlite3_bind_double(st, 8, (double)service->cost_eur);
    sqlite3_bind_int64(st, 9, service->registered);
    sqlite3_bind_text(st, 10, service->endpoint, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

static void row_to_service(sqlite3_stmt *st, struct cos_service *s)
{
    memset(s, 0, sizeof *s);
    snprintf(s->service_id, sizeof s->service_id, "%s",
             (const char *)sqlite3_column_text(st, 0));
    snprintf(s->name, sizeof s->name, "%s",
             sqlite3_column_text(st, 1)
                 ? (const char *)sqlite3_column_text(st, 1)
                 : "");
    snprintf(s->description, sizeof s->description, "%s",
             sqlite3_column_text(st, 2)
                 ? (const char *)sqlite3_column_text(st, 2)
                 : "");
    snprintf(s->provider_id, sizeof s->provider_id, "%s",
             sqlite3_column_text(st, 3)
                 ? (const char *)sqlite3_column_text(st, 3)
                 : "");
    s->sigma_mean     = (float)sqlite3_column_double(st, 4);
    s->reliability    = (float)sqlite3_column_double(st, 5);
    s->times_served   = sqlite3_column_int(st, 6);
    s->cost_eur       = (float)sqlite3_column_double(st, 7);
    s->registered     = sqlite3_column_int64(st, 8);
    snprintf(s->endpoint, sizeof s->endpoint, "%s",
             sqlite3_column_text(st, 9)
                 ? (const char *)sqlite3_column_text(st, 9)
                 : "");
}

static int fetch_remote_catalog(const char *base_url, struct cos_service *out,
                                int max, int *n_off)
{
    char url[384];
    normalize_endpoint(base_url, url, sizeof url);
    size_t uw = strlen(url);
    while (uw > 0 && url[uw - 1] == '/') uw--;
    url[uw] = '\0';
    snprintf(url + uw, sizeof url - uw, "/cos/marketplace/list");

    mbuf_t resp = {0};
    CURL *c = curl_easy_init();
    if (!c) return -1;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, cwcb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    CURLcode cc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (cc != CURLE_OK || !resp.data || resp.len < 4) {
        free(resp.data);
        return -1;
    }

    const char *blob = resp.data;
    size_t bl        = resp.len;
    const char *p    = blob;
    while (*n_off < max && p < blob + bl) {
        const char *sid = strstr(p, "\"service_id\"");
        if (!sid)
            break;
        struct cos_service tmp;
        memset(&tmp, 0, sizeof tmp);
        const char *vq = strchr(sid, ':');
        if (!vq)
            break;
        vq++;
        while (*vq == ' ' || *vq == '\t') vq++;
        if (*vq != '"')
            break;
        js_read_string(vq, blob + bl, tmp.service_id, sizeof tmp.service_id);

        tmp.sigma_mean = 0.5f;
        tmp.reliability = 0.5f;
        char epbuf[256] = {0};
        const char *sig = strstr(sid, "\"sigma_mean\"");
        if (sig && sig < blob + bl - 12) {
            const char *col = strchr(sig, ':');
            if (col)
                tmp.sigma_mean = (float)strtod(col + 1, NULL);
        }
        const char *rel = strstr(sid, "\"reliability\"");
        if (!rel || rel > blob + bl)
            rel = strstr(vq, "\"reliability\"");
        if (rel && rel < blob + bl) {
            const char *col = strchr(rel, ':');
            if (col)
                tmp.reliability = (float)strtod(col + 1, NULL);
        }
        const char *ep = strstr(sid, "\"endpoint\"");
        if (ep && ep < blob + bl) {
            const char *col = strchr(ep, ':');
            if (col && col + 1 < blob + bl && col[1] == '"')
                js_read_string(col + 1, blob + bl, epbuf, sizeof epbuf);
        }
        snprintf(tmp.endpoint, sizeof tmp.endpoint, "%s",
                 epbuf[0] ? epbuf : url);
        snprintf(tmp.name, sizeof tmp.name, "remote");
        snprintf(tmp.description, sizeof tmp.description, "%s",
                 "remote listing");
        snprintf(tmp.provider_id, sizeof tmp.provider_id, "%s", "remote");
        tmp.cost_eur    = 0.f;
        tmp.registered = (int64_t)time(NULL) * 1000LL;
        out[*n_off] = tmp;
        (*n_off)++;
        p = strstr(sid + 12, "\"service_id\"");
        if (!p || p >= blob + bl)
            break;
    }
    free(resp.data);
    return 0;
}

int cos_marketplace_browse(struct cos_service *services, int max, int *n)
{
    if (!services || max <= 0 || !n) return -1;
    if (cos_marketplace_init() != 0) return -1;
    *n = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT service_id,name,description,provider_id,"
                             "sigma_mean,reliability,times_served,cost_eur,"
                             "registered,endpoint FROM services ORDER BY "
                             "sigma_mean ASC;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -1;
    while (sqlite3_step(st) == SQLITE_ROW && *n < max)
        row_to_service(st, &services[(*n)++]);
    sqlite3_finalize(st);

    const char *peers = getenv("COS_MARKETPLACE_PEERS");
    if (peers && peers[0]) {
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", peers);
        char *save = NULL;
        for (char *tok = strtok_r(buf, ",", &save); tok && *n < max;
             tok      = strtok_r(NULL, ",", &save)) {
            while (*tok == ' ' || *tok == '\t') tok++;
            if (!*tok) continue;
            fetch_remote_catalog(tok, services, max, n);
        }
    }
    return 0;
}

static int http_invoke(const char *endpoint, const struct cos_service_request *req,
                       struct cos_service_response *resp)
{
    char url[384];
    normalize_endpoint(endpoint, url, sizeof url);
    size_t uw = strlen(url);
    while (uw > 0 && url[uw - 1] == '/') uw--;
    url[uw] = '\0';
    snprintf(url + uw, sizeof url - uw, "/cos/marketplace/invoke");

    char body[6144];
    size_t bw = 0;
    bw += (size_t)snprintf(body + bw, sizeof body - bw,
                           "{\"service_id\":\"%s\",\"requester_id\":\"%s\","
                           "\"max_sigma\":%.8g,\"max_cost_eur\":%.8g,"
                           "\"prompt\":\"",
                           req->service_id, req->requester_id,
                           (double)req->max_sigma, (double)req->max_cost_eur);
    json_escape_append(body, sizeof body, &bw, req->prompt);
    if (bw + 8 < sizeof body)
        snprintf(body + bw, sizeof body - bw, "\"}");

    mbuf_t res = {0};
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
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &res);
    curl_easy_setopt(c, CURLOPT_TIMEOUT,
                     req->timeout_ms > 0 ? (long)(req->timeout_ms / 1000) : 120L);

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);
    CURLcode cc = curl_easy_perform(c);
    gettimeofday(&tv1, NULL);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    resp->latency_ms =
        ((int64_t)(tv1.tv_sec - tv0.tv_sec)) * 1000LL
        + ((tv1.tv_usec - tv0.tv_usec) / 1000LL);

    int ok = -1;
    if (cc == CURLE_OK && res.data && res.len > 2)
        ok = parse_http_service_response(res.data, res.len, resp);
    free(res.data);
    return ok;
}

int cos_marketplace_request(const struct cos_service_request *req,
                            struct cos_service_response *resp)
{
    if (!req || !resp) return -1;
    memset(resp, 0, sizeof *resp);
    if (cos_marketplace_init() != 0) return -1;

    struct cos_service catalog[64];
    int nc = 0;
    if (cos_marketplace_browse(catalog, 64, &nc) != 0) return -2;

    int best = -1;
    for (int i = 0; i < nc; i++) {
        if (req->service_id[0]
            && strcmp(catalog[i].service_id, req->service_id) != 0)
            continue;
        if (catalog[i].sigma_mean >= req->max_sigma - 1e-9f) continue;
        if (catalog[i].cost_eur > req->max_cost_eur + 1e-9f) continue;
        if (!catalog[i].endpoint[0]) continue;
        if (best < 0 || catalog[i].sigma_mean < catalog[best].sigma_mean
            || (catalog[i].sigma_mean == catalog[best].sigma_mean
                && catalog[i].reliability > catalog[best].reliability))
            best = i;
    }
    if (best < 0) return -3;

    int hr = http_invoke(catalog[best].endpoint, req, resp);
    if (hr != 0) return -4;

    int vr = cos_marketplace_verify_receipt(resp);
    if (vr != 0) {
        (void)cos_reputation_update(catalog[best].provider_id, resp->sigma, 0,
                                    0);
        return -5;
    }

    if (resp->sigma < req->max_sigma - 1e-9f) {
        upsert_stat("spent_eur", (double)resp->cost_eur);
        upsert_stat("invokes_out", 1.);
        (void)cos_reputation_update(catalog[best].provider_id, resp->sigma, 1,
                                    1);
    }
    return 0;
}

int cos_marketplace_verify_receipt(const struct cos_service_response *resp)
{
    if (!resp) return -1;
    return cos_proof_receipt_verify(&resp->receipt);
}

int cos_marketplace_serve(const struct cos_service_request *req,
                          struct cos_service_response *resp)
{
    if (!req || !resp) return -1;
    memset(resp, 0, sizeof *resp);
    if (cos_marketplace_init() != 0) return -1;

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "SELECT service_id,name,description,provider_id,"
                             "sigma_mean,reliability,times_served,cost_eur,"
                             "registered,endpoint FROM services WHERE "
                             "service_id=?;",
                             -1, &st, NULL)
        != SQLITE_OK)
        return -2;
    sqlite3_bind_text(st, 1, req->service_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) != SQLITE_ROW) {
        sqlite3_finalize(st);
        return -3;
    }
    struct cos_service svc;
    row_to_service(st, &svc);
    sqlite3_finalize(st);

    struct cos_think_result tr;
    memset(&tr, 0, sizeof tr);
    if (cos_think_run(req->prompt, 1, &tr) != 0)
        return -4;

    int bi = tr.best_subtask_idx >= 0 ? tr.best_subtask_idx : 0;
    snprintf(resp->response, sizeof resp->response, "%s",
             tr.subtasks[bi].answer);
    resp->sigma        = tr.subtasks[bi].sigma_combined;
    resp->attribution  = tr.subtasks[bi].attribution;
    resp->cost_eur     = svc.cost_eur;

    struct cos_error_attribution ea =
        cos_error_attribute(resp->sigma, resp->sigma, resp->sigma, 0.f);
    float sch[4] = {resp->sigma, resp->sigma, resp->sigma, 0.f};
    float mch[4] = {1.f, 1.f, 1.f, 1.f};
    int gd =
        (int)cos_sigma_reinforce(resp->sigma, 0.35f, 0.55f);
    if (cos_proof_receipt_generate(resp->response, resp->sigma, sch, mch, gd,
                                   &ea, 0xffULL, &resp->receipt)
        != 0)
        return -5;

    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(g_db,
                             "UPDATE services SET times_served=times_served+1 "
                             "WHERE service_id=?;",
                             -1, &up, NULL)
        == SQLITE_OK) {
        sqlite3_bind_text(up, 1, svc.service_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    upsert_stat("earnings_eur", (double)svc.cost_eur);
    upsert_stat("invokes_in", 1.);
    (void)cos_reputation_update(svc.provider_id, resp->sigma, 1, 1);
    return 0;
}

int cos_marketplace_stats_fprint(FILE *fp)
{
    if (!fp || cos_marketplace_init() != 0) return -1;
    fprintf(fp,
            "earnings_eur=%.4f spent_eur=%.4f invokes_in=%.0f invokes_out=%.0f\n",
            get_stat("earnings_eur"), get_stat("spent_eur"),
            get_stat("invokes_in"), get_stat("invokes_out"));
    return 0;
}

int cos_marketplace_omega_rescue(const char *goal, struct cos_think_result *tr)
{
    if (!goal || !tr) return -1;
    const char *svc = getenv("COS_MARKETPLACE_SERVICE_ID");
    if (!svc || !svc[0]) svc = "general";

    struct cos_service_request rq;
    memset(&rq, 0, sizeof rq);
    snprintf(rq.service_id, sizeof rq.service_id, "%s", svc);
    snprintf(rq.prompt, sizeof rq.prompt, "%s", goal);
    const char *rid = getenv("COS_NODE_ID");
    snprintf(rq.requester_id, sizeof rq.requester_id, "%s",
             rid && rid[0] ? rid : "omega");
    rq.max_sigma    = tr->sigma_mean;
    rq.max_cost_eur = 1e9f;
    rq.timeout_ms   = 120000LL;

    struct cos_service_response resp;
    memset(&resp, 0, sizeof resp);
    int rc = cos_marketplace_request(&rq, &resp);
    if (rc != 0) return -2;
    if (resp.sigma >= tr->sigma_mean - 1e-9f) return -3;

    snprintf(tr->goal, sizeof tr->goal, "%s", goal);
    tr->n_subtasks                = 1;
    tr->best_subtask_idx          = 0;
    tr->sigma_mean                = resp.sigma;
    tr->sigma_min                 = resp.sigma;
    tr->sigma_max                 = resp.sigma;
    tr->subtasks[0].sigma_combined = resp.sigma;
    snprintf(tr->subtasks[0].answer, sizeof tr->subtasks[0].answer, "%s",
             resp.response);
    tr->subtasks[0].final_action = 0;
    tr->subtasks[0].attribution  = resp.attribution;
    return 0;
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_MARKETPLACE_TEST)
int cos_marketplace_self_test(void)
{
    char prev[256];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    assert(setenv("HOME", "/tmp", 1) == 0);
    cos_marketplace_shutdown();
    cos_reputation_shutdown();

    assert(cos_marketplace_init() == 0);
    assert(cos_reputation_init() == 0);
    struct cos_service s;
    memset(&s, 0, sizeof s);
    snprintf(s.service_id, sizeof s.service_id, "%s", "demo.svc");
    snprintf(s.name, sizeof s.name, "%s", "Demo");
    snprintf(s.description, sizeof s.description, "%s", "test");
    snprintf(s.provider_id, sizeof s.provider_id, "%s", "local");
    s.sigma_mean     = 0.15f;
    s.reliability    = 0.9f;
    s.times_served   = 0;
    s.cost_eur       = 0.f;
    s.registered     = (int64_t)time(NULL) * 1000LL;
    snprintf(s.endpoint, sizeof s.endpoint, "%s", "http://127.0.0.1:9");
    assert(cos_marketplace_register_service(&s) == 0);

    struct cos_service sv[8];
    int nn = 0;
    assert(cos_marketplace_browse(sv, 8, &nn) == 0);
    assert(nn >= 1);

    struct cos_service_response r;
    memset(&r, 0, sizeof r);
    struct cos_error_attribution ea;
    memset(&ea, 0, sizeof ea);
    float sch[4] = {0.2f, 0.2f, 0.2f, 0.f};
    float mch[4] = {1.f, 1.f, 1.f, 1.f};
    assert(cos_proof_receipt_generate("hello", 0.2f, sch, mch,
                                      COS_SIGMA_ACTION_ACCEPT, &ea, 0xffULL,
                                      &r.receipt)
           == 0);
    snprintf(r.response, sizeof r.response, "%s", "hello");
    r.sigma = 0.2f;
    assert(cos_marketplace_verify_receipt(&r) == 0);

    assert(cos_reputation_init() == 0);
    assert(cos_reputation_update("local", 0.2f, 1, 1) == 0);

    cos_marketplace_shutdown();
    cos_reputation_shutdown();
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
#endif

/* See marketplace_think_stub.c: check binaries link weak think; cos links
 * strong cos_think_run. */
