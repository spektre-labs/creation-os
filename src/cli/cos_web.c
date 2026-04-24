/*
 * cos web — wraps COS_SEARCH_API_URL + optional page fetch for verify.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "cos_web.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char  *buf;
    size_t len;
} web_buf_t;

static size_t web_write_cb(char *ptr, size_t sz, size_t nmemb, void *ud) {
    size_t       n = sz * nmemb;
    web_buf_t *b = (web_buf_t *)ud;
    char       *nb = (char *)realloc(b->buf, b->len + n + 1);
    if (nb == NULL)
        return 0;
    b->buf = nb;
    memcpy(b->buf + b->len, ptr, n);
    b->len += n;
    b->buf[b->len] = '\0';
    return n;
}

static void strip_html_inplace(char *s) {
    int in = 0, r = 0, w = 0;
    if (s == NULL)
        return;
    for (; s[r] != '\0'; ++r) {
        if (s[r] == '<')
            in = 1;
        else if (s[r] == '>')
            in = 0;
        else if (!in) {
            unsigned char c = (unsigned char)s[r];
            s[w++] = (char)(isspace(c) ? ' ' : c);
        }
    }
    s[w] = '\0';
}

int cos_web_search(const char *query, struct cos_search_result *results,
                   int max, int *n) {
    if (query == NULL || results == NULL || n == NULL || max <= 0)
        return -1;
    return cos_search_run(query, max > 3 ? 3 : max, results, n);
}

int cos_web_fetch_page(const char *url, char *text, int max_len) {
    CURL       *c;
    web_buf_t acc = { NULL, 0 };
    CURLcode    cr;

    if (url == NULL || text == NULL || max_len <= 8)
        return -1;
    text[0] = '\0';
    c       = curl_easy_init();
    if (c == NULL)
        return -2;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, web_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &acc);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 25L);
    cr = curl_easy_perform(c);
    curl_easy_cleanup(c);
    if (cr != CURLE_OK || acc.buf == NULL) {
        free(acc.buf);
        return -3;
    }
    strip_html_inplace(acc.buf);
    snprintf(text, (size_t)max_len, "%s", acc.buf);
    free(acc.buf);
    return 0;
}

int cos_web_verify_claim(const char *claim, int *verified) {
    struct cos_search_result r1[COS_SEARCH_MAX_RESULTS],
        r2[COS_SEARCH_MAX_RESULTS];
    int n1 = 0, n2 = 0, i, j, hit;

    if (claim == NULL || verified == NULL)
        return -1;
    *verified = 0;
    if (cos_search_run(claim, 3, r1, &n1) != 0 || n1 < 1)
        return -2;
    {
        char q2[384];
        snprintf(q2, sizeof q2, "%.200s corroborate", claim);
        if (cos_search_run(q2, 3, r2, &n2) != 0 || n2 < 1)
            return -3;
    }
    hit = 0;
    for (i = 0; i < n1; ++i) {
        for (j = 0; j < n2; ++j) {
            if (r1[i].url[0] != '\0' && strcasecmp(r1[i].url, r2[j].url) == 0)
                hit = 1;
        }
    }
    if (!hit) {
        const char *a = r1[0].snippet[0] ? r1[0].snippet : r1[0].title;
        const char *b = r2[0].snippet[0] ? r2[0].snippet : r2[0].title;
        size_t      k, same = 0, lim = strlen(a) < strlen(b) ? strlen(a)
                                                             : strlen(b);
        for (k = 0; k < lim && k < 64; ++k) {
            if (tolower((unsigned char)a[k]) == tolower((unsigned char)b[k]))
                same++;
        }
        if (lim > 0 && (double)same / (double)lim > 0.55)
            hit = 1;
    }
    *verified = hit;
    return 0;
}

static void web_usage(FILE *fp) {
    fputs(
        "cos web — σ-ranked snippets (COS_SEARCH_API_URL printf %%s)\n"
        "  cos web \"your query\"\n"
        "  cos web --verify \"claim to cross-check\"\n"
        "  COS_SEARCH_ENDPOINT — alias; if set, used as COS_SEARCH_API_URL\n",
        fp);
}

int cos_web_main(int argc, char **argv) {
    int         verify = 0, i, mx = 3;
    const char *q = NULL;

    const char *ep = getenv("COS_SEARCH_ENDPOINT");
    if (ep != NULL && ep[0] != '\0' && getenv("COS_SEARCH_API_URL") == NULL)
        setenv("COS_SEARCH_API_URL", ep, 1);

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--verify") == 0)
            verify = 1;
        else if (strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            mx = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            web_usage(stdout);
            return 0;
        } else if (argv[i][0] != '-' && q == NULL)
            q = argv[i];
    }
    if (q == NULL || q[0] == '\0') {
        web_usage(stderr);
        return 2;
    }
    if (verify) {
        int ok = 0;
        if (cos_web_verify_claim(q, &ok) != 0) {
            fputs("cos web: verify search failed\n", stderr);
            return 3;
        }
        printf("verify: %s (sources %saligned)\n", q, ok ? "" : "not ");
        return 0;
    }
    {
        struct cos_search_result res[COS_SEARCH_MAX_RESULTS];
        int                      n = 0, k;
        if (mx < 1)
            mx = 1;
        if (mx > COS_SEARCH_MAX_RESULTS)
            mx = COS_SEARCH_MAX_RESULTS;
        if (cos_web_search(q, res, mx, &n) != 0)
            return 4;
        for (k = 0; k < n; ++k) {
            printf("[%d] σ=%.3f\n%s\n%s\n\n", k + 1, (double)res[k].sigma,
                   res[k].title, res[k].snippet);
        }
    }
    return 0;
}

int cos_web_self_test(void) {
    struct cos_search_result r[4];
    int                      n = 0;
    char                     page[256];
    int                      v = 0;

    if (cos_web_search("cos web self-test", r, 3, &n) != 0)
        return 1;
    (void)cos_web_fetch_page("https://example.invalid/404", page,
                            (int)sizeof page);
    (void)cos_web_verify_claim("test claim", &v);
    return 0;
}
