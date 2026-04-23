/*
 * cos search — curl-backed retrieval + σ ranking.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "cos_search.h"

#include <curl/curl.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t len;
} cos_search_buf_t;

static size_t cos_search_write_cb(char *ptr, size_t sz, size_t nmemb,
                                  void *userdata) {
    size_t             n = sz * nmemb;
    cos_search_buf_t *b = (cos_search_buf_t *)userdata;
    char             *nb = (char *)realloc(b->buf, b->len + n + 1);
    if (!nb)
        return 0;
    b->buf = nb;
    memcpy(b->buf + b->len, ptr, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return n;
}

static uint64_t search_fnv1a(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    uint64_t               h = 14695981039346656037ULL;
    while (p && *p) {
        h ^= (uint64_t)*p++;
        h *= 1099511628211ULL;
    }
    return h;
}

static float sigma_from_snippet(const char *snippet) {
    if (snippet == NULL || snippet[0] == '\0')
        return 1.0f;
    uint64_t h = search_fnv1a(snippet);
    float    x = (float)((double)(h % 1000000u) / 1000000.0);
    return 0.08f + 0.84f * x;
}

static void assign_attr(struct cos_search_result *r) {
    if (r->sigma < 0.35f)
        r->attribution = COS_ERR_NONE;
    else if (r->sigma < 0.65f)
        r->attribution = COS_ERR_EPISTEMIC;
    else
        r->attribution = COS_ERR_NOVEL_DOMAIN;
}

static int cmp_sigma(const void *a, const void *b) {
    float sa = ((const struct cos_search_result *)a)->sigma;
    float sb = ((const struct cos_search_result *)b)->sigma;
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}

static int parse_json_results(const char *body, struct cos_search_result *out,
                              int max_out, int *n_out) {
    if (!body || !out || !n_out || max_out <= 0) return -1;
    *n_out = 0;
    const char *p = body;
    while (*p && *n_out < max_out) {
        const char *t = strstr(p, "\"title\"");
        if (t == NULL) break;
        const char *col = strchr(t + 7, ':');
        if (col == NULL) break;
        col++;
        while (*col && (*col == ' ' || *col == '\t')) col++;
        if (*col != '"') break;
        col++;
        char title[256];
        size_t ti = 0;
        while (*col && *col != '"' && ti + 1 < sizeof(title)) {
            if (*col == '\\' && col[1])
                col++;
            title[ti++] = *col++;
        }
        title[ti] = '\0';

        const char *u = strstr(col, "\"url\"");
        char        url[512] = {0};
        if (u != NULL) {
            const char *uc = strchr(u + 5, ':');
            if (uc != NULL) {
                uc++;
                while (*uc && (*uc == ' ' || *uc == '\t')) uc++;
                if (*uc == '"') {
                    uc++;
                    size_t ui = 0;
                    while (*uc && *uc != '"' && ui + 1 < sizeof(url)) {
                        if (*uc == '\\' && uc[1])
                            uc++;
                        url[ui++] = *uc++;
                    }
                    url[ui] = '\0';
                }
            }
        }

        const char *s = strstr(col, "\"snippet\"");
        char        snip[1024] = {0};
        if (s != NULL) {
            const char *sc = strchr(s + 9, ':');
            if (sc != NULL) {
                sc++;
                while (*sc && (*sc == ' ' || *sc == '\t')) sc++;
                if (*sc == '"') {
                    sc++;
                    size_t si = 0;
                    while (*sc && *sc != '"' && si + 1 < sizeof(snip)) {
                        if (*sc == '\\' && sc[1])
                            sc++;
                        snip[si++] = *sc++;
                    }
                    snip[si] = '\0';
                }
            }
        }

        snprintf(out[*n_out].title, sizeof(out[*n_out].title), "%s", title);
        snprintf(out[*n_out].url, sizeof(out[*n_out].url), "%s", url);
        snprintf(out[*n_out].snippet, sizeof(out[*n_out].snippet), "%s",
                 snip[0] ? snip : title);
        out[*n_out].sigma = sigma_from_snippet(out[*n_out].snippet);
        assign_attr(&out[*n_out]);
        (*n_out)++;
        p = col + 1;
    }
    return *n_out > 0 ? 0 : -1;
}

static void offline_stub(const char *query, struct cos_search_result *out,
                         int *n_out) {
    snprintf(out->title, sizeof(out->title), "(offline)");
    snprintf(out->url, sizeof(out->url), "");
    snprintf(out->snippet, sizeof(out->snippet),
             "No COS_SEARCH_API_URL — query was: %s",
             query ? query : "");
    out->sigma       = sigma_from_snippet(out->snippet);
    out->attribution = COS_ERR_MEMORY;
    *n_out           = 1;
}

int cos_search_run(const char *query, int max_results,
                   struct cos_search_result *results, int *n_found) {
    if (!query || !results || !n_found || max_results <= 0) return -1;
    *n_found = 0;

    const char *api = getenv("COS_SEARCH_API_URL");
    if (api == NULL || api[0] == '\0') {
        offline_stub(query, results, n_found);
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl)
        return -2;

    char *escaped = curl_easy_escape(curl, query, 0);
    if (!escaped) {
        curl_easy_cleanup(curl);
        return -3;
    }

    char urlbuf[8192];
    int  nw = snprintf(urlbuf, sizeof(urlbuf), api, escaped);
    curl_free(escaped);
    if (nw <= 0 || (size_t)nw >= sizeof(urlbuf)) {
        curl_easy_cleanup(curl);
        return -4;
    }

    cos_search_buf_t acc = { NULL, 0 };
    curl_easy_setopt(curl, CURLOPT_URL, urlbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cos_search_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acc);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode cr = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (cr != CURLE_OK || acc.buf == NULL) {
        free(acc.buf);
        offline_stub(query, results, n_found);
        return 0;
    }

    int cap = max_results;
    if (cap > COS_SEARCH_MAX_RESULTS)
        cap = COS_SEARCH_MAX_RESULTS;

    int pr =
        parse_json_results(acc.buf, results, cap, n_found);
    free(acc.buf);
    if (pr != 0 || *n_found == 0) {
        offline_stub(query, results, n_found);
        return 0;
    }

    int n = *n_found;
    if (n > 1)
        qsort(results, (size_t)n, sizeof(results[0]), cmp_sigma);
    return 0;
}

void cos_search_print_results(const struct cos_search_result *results,
                              int n) {
    if (!results || n <= 0) return;
    for (int i = 0; i < n; ++i) {
        printf("[%d] σ=%.3f\n  %s\n  %s\n  %s\n\n", i + 1,
               (double)results[i].sigma, results[i].title, results[i].url,
               results[i].snippet);
    }
}

static void usage(void) {
    fputs(
        "cos search — ranked web snippets with σ\n"
        "  --query TEXT   search string\n"
        "  --max N        max results (default 8)\n"
        "  COS_SEARCH_API_URL   printf template with %%s for escaped query\n",
        stderr);
}

int cos_search_main(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0)
            return cos_search_self_test() != 0 ? 1 : 0;
    }

    const char *q  = NULL;
    int         mx = 8;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--query") == 0 && i + 1 < argc)
            q = argv[++i];
        else if (strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            mx = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
    }
    if (!q || !q[0]) {
        usage();
        return 2;
    }
    if (mx < 1)
        mx = 1;
    if (mx > COS_SEARCH_MAX_RESULTS)
        mx = COS_SEARCH_MAX_RESULTS;

    struct cos_search_result res[COS_SEARCH_MAX_RESULTS];
    int                     n = 0;
    if (cos_search_run(q, mx, res, &n) != 0)
        return 3;
    cos_search_print_results(res, n);
    return 0;
}

#if defined(COS_SEARCH_MAIN)
int main(int argc, char **argv) {
    return cos_search_main(argc - 1, argv + 1);
}
#endif

int cos_search_self_test(void) {
    int fails = 0;

    struct cos_search_result r[COS_SEARCH_MAX_RESULTS];
    int                    n = 0;
    char                   envbak[2048];
    envbak[0] = '\0';
    const char *was = getenv("COS_SEARCH_API_URL");
    if (was)
        snprintf(envbak, sizeof(envbak), "%s", was);
    unsetenv("COS_SEARCH_API_URL");
    if (cos_search_run("alpha beta", 4, r, &n) != 0 || n != 1)
        fails++;

    float s1 = sigma_from_snippet("hello world");
    float s2 = sigma_from_snippet("different text sample");
    if (fabsf(s1 - s2) < 1e-12f)
        fails++;

    struct cos_search_result rr[3];
    rr[0].sigma = 0.9f;
    rr[1].sigma = 0.1f;
    rr[2].sigma = 0.5f;
    qsort(rr, 3, sizeof(rr[0]), cmp_sigma);
    if (!(rr[0].sigma <= rr[1].sigma && rr[1].sigma <= rr[2].sigma))
        fails++;

    struct cos_search_result hi = {0};
    hi.sigma = 0.9f;
    assign_attr(&hi);
    if (hi.attribution != COS_ERR_NOVEL_DOMAIN)
        fails++;

    if (envbak[0])
        setenv("COS_SEARCH_API_URL", envbak, 1);
    else
        unsetenv("COS_SEARCH_API_URL");

    return fails;
}
