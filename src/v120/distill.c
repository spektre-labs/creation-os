/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v120 σ-Distill selector implementation.
 */

#include "distill.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Minimal JSONL field extraction — we only need a handful of keys    */
/* and we already know the shape of each line.                         */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

/* Find "key": and return pointer to first non-whitespace char after
 * the colon; NULL if not found. */
static const char *find_key(const char *line, const char *key) {
    size_t klen = strlen(key);
    const char *p = line;
    while ((p = strchr(p, '"')) != NULL) {
        ++p;
        if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
            const char *q = p + klen + 1;
            q = skip_ws(q);
            if (*q != ':') continue;
            ++q;
            return skip_ws(q);
        }
        p = strchr(p, '"');
        if (!p) break;
        ++p;
    }
    return NULL;
}

/* Copy a JSON string value into `dst`, handling \" and \\.  Returns
 * pointer past the closing quote or NULL on error. */
static const char *read_str(const char *p, char *dst, size_t cap) {
    if (*p != '"') return NULL;
    ++p;
    size_t n = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            char e = p[1];
            char out = e;
            if (e == 'n') out = '\n';
            else if (e == 't') out = '\t';
            else if (e == 'r') out = '\r';
            else if (e == '"') out = '"';
            else if (e == '\\') out = '\\';
            else if (e == '/') out = '/';
            else out = e;
            if (n + 1 < cap) dst[n++] = out;
            p += 2;
        } else {
            if (n + 1 < cap) dst[n++] = *p;
            ++p;
        }
    }
    if (*p != '"') return NULL;
    dst[n < cap ? n : cap - 1] = '\0';
    return p + 1;
}

/* Read a JSON number into float; returns pointer past the number or
 * NULL on error. */
static const char *read_num(const char *p, float *out) {
    char *end = NULL;
    *out = (float)strtod(p, &end);
    if (end == p) return NULL;
    return end;
}

/* ------------------------------------------------------------------ */
/* JSON writer helpers                                                */
/* ------------------------------------------------------------------ */

static int jesc(FILE *f, const char *s) {
    if (!s) return fputs("\"\"", f);
    fputc('"', f);
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\n': fputs("\\n", f);  break;
        case '\t': fputs("\\t", f);  break;
        case '\r': fputs("\\r", f);  break;
        default:
            if (c < 0x20) fprintf(f, "\\u%04x", c);
            else fputc(c, f);
        }
    }
    fputc('"', f);
    return 0;
}

/* ------------------------------------------------------------------ */

void cos_v120_config_defaults(cos_v120_config_t *cfg) {
    if (!cfg) return;
    cfg->tau_big_low    = COS_V120_TAU_BIG_LOW_DEFAULT;
    cfg->tau_small_high = COS_V120_TAU_SMALL_HIGH_DEFAULT;
    cfg->max_rows       = 0;
}

int cos_v120_select(const cos_v120_config_t *cfg_in,
                    const char *in_path, const char *out_path,
                    cos_v120_stats_t *stats) {
    if (!in_path || !out_path || !stats) return -1;
    cos_v120_config_t c;
    if (cfg_in) c = *cfg_in; else cos_v120_config_defaults(&c);
    memset(stats, 0, sizeof *stats);

    FILE *fi = fopen(in_path, "r");
    if (!fi) return -1;
    FILE *fo = fopen(out_path, "w");
    if (!fo) { fclose(fi); return -1; }

    static char line[COS_V120_MAX_FIELD * 4];
    static char id[128], prompt[COS_V120_MAX_FIELD];
    static char big[COS_V120_MAX_FIELD], small[COS_V120_MAX_FIELD];

    while (fgets(line, sizeof line, fi)) {
        ++stats->rows_read;
        if (c.max_rows > 0 && stats->rows_kept >= c.max_rows) break;

        id[0] = prompt[0] = big[0] = small[0] = '\0';
        float sigma_big = -1.f, sigma_small = -1.f;

        const char *p;
        if ((p = find_key(line, "id"))              && *p == '"')
            (void)read_str(p, id, sizeof id);
        if ((p = find_key(line, "prompt"))          && *p == '"')
            (void)read_str(p, prompt, sizeof prompt);
        if ((p = find_key(line, "big_response"))    && *p == '"')
            (void)read_str(p, big, sizeof big);
        if ((p = find_key(line, "small_response"))  && *p == '"')
            (void)read_str(p, small, sizeof small);
        if ((p = find_key(line, "sigma_big")))
            (void)read_num(p, &sigma_big);
        if ((p = find_key(line, "sigma_small")))
            (void)read_num(p, &sigma_small);

        if (prompt[0] == '\0' || big[0] == '\0' ||
            sigma_big < 0.f || sigma_small < 0.f) {
            ++stats->rows_malformed;
            continue;
        }

        int big_low   = (sigma_big   <  c.tau_big_low);
        int small_high = (sigma_small >  c.tau_small_high);

        if (big_low && small_high) {
            ++stats->rows_kept;
            stats->sum_sigma_big   += (double)sigma_big;
            stats->sum_sigma_small += (double)sigma_small;

            fputs("{", fo);
            if (id[0]) { fputs("\"id\":", fo); jesc(fo, id); fputc(',', fo); }
            fputs("\"prompt\":",   fo); jesc(fo, prompt); fputc(',', fo);
            fputs("\"response\":", fo); jesc(fo, big);    fputc(',', fo);
            fprintf(fo, "\"sigma_big\":%.4f,\"sigma_small\":%.4f,",
                    (double)sigma_big, (double)sigma_small);
            fputs("\"source\":\"v120-sigma-distill\"}\n", fo);
        } else if (!big_low && !small_high) {
            ++stats->rows_rejected_both;
        } else if (!big_low) {
            ++stats->rows_rejected_sigma_big_too_high;
        } else {
            ++stats->rows_rejected_sigma_small_too_low;
        }
    }

    fclose(fi);
    fclose(fo);
    return 0;
}

int cos_v120_stats_to_json(const cos_v120_config_t *cfg,
                           const cos_v120_stats_t *s,
                           char *out, size_t cap) {
    if (!s || !out || cap == 0) return -1;
    cos_v120_config_t c;
    if (cfg) c = *cfg; else cos_v120_config_defaults(&c);

    double mean_big   = s->rows_kept > 0
        ? s->sum_sigma_big   / (double)s->rows_kept : 0.0;
    double mean_small = s->rows_kept > 0
        ? s->sum_sigma_small / (double)s->rows_kept : 0.0;

    int n = snprintf(out, cap,
        "{\"rows_read\":%d,\"rows_kept\":%d,"
        "\"rows_malformed\":%d,"
        "\"rows_rejected_sigma_big_too_high\":%d,"
        "\"rows_rejected_sigma_small_too_low\":%d,"
        "\"rows_rejected_both\":%d,"
        "\"tau_big_low\":%.4f,\"tau_small_high\":%.4f,"
        "\"mean_sigma_big_kept\":%.4f,"
        "\"mean_sigma_small_kept\":%.4f,"
        "\"source\":\"v120-sigma-distill\"}",
        s->rows_read, s->rows_kept, s->rows_malformed,
        s->rows_rejected_sigma_big_too_high,
        s->rows_rejected_sigma_small_too_low,
        s->rows_rejected_both,
        (double)c.tau_big_low, (double)c.tau_small_high,
        mean_big, mean_small);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v120 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v120_self_test(void) {
    char in_path[]  = "/tmp/cos_v120_in_XXXXXX";
    char out_path[] = "/tmp/cos_v120_out_XXXXXX";
    int fd_in  = mkstemp(in_path);
    int fd_out = mkstemp(out_path);
    _CHECK(fd_in  >= 0, "mkstemp in");
    _CHECK(fd_out >= 0, "mkstemp out");
    close(fd_out);   /* selector will open by name */

    /* 8-row synthetic corpus.  Construction (τ_big_low=0.30,
     * τ_small_high=0.50):
     *
     *   id  σ_big  σ_small  keep?  reason
     *   r1  0.10   0.80     KEEP   teacher confident, student unsure
     *   r2  0.15   0.70     KEEP
     *   r3  0.25   0.60     KEEP
     *   r4  0.15   0.30     drop   student already confident
     *   r5  0.15   0.50     drop   small not strictly > τ
     *   r6  0.60   0.80     drop   teacher too unsure
     *   r7  0.40   0.30     drop   both wrong
     *   r8  malformed               parse failure
     */
    FILE *fi = fdopen(fd_in, "w");
    _CHECK(fi != NULL, "fdopen in");
    fputs("{\"id\":\"r1\",\"prompt\":\"2+2\",\"big_response\":\"4\",\"small_response\":\"4?\",\"sigma_big\":0.10,\"sigma_small\":0.80}\n", fi);
    fputs("{\"id\":\"r2\",\"prompt\":\"capital of France\",\"big_response\":\"Paris\",\"small_response\":\"Lyon\",\"sigma_big\":0.15,\"sigma_small\":0.70}\n", fi);
    fputs("{\"id\":\"r3\",\"prompt\":\"who wrote Hamlet\",\"big_response\":\"Shakespeare\",\"small_response\":\"Dickens\",\"sigma_big\":0.25,\"sigma_small\":0.60}\n", fi);
    fputs("{\"id\":\"r4\",\"prompt\":\"sky color\",\"big_response\":\"blue\",\"small_response\":\"blue\",\"sigma_big\":0.15,\"sigma_small\":0.30}\n", fi);
    fputs("{\"id\":\"r5\",\"prompt\":\"boundary\",\"big_response\":\"a\",\"small_response\":\"b\",\"sigma_big\":0.15,\"sigma_small\":0.50}\n", fi);
    fputs("{\"id\":\"r6\",\"prompt\":\"both sure\",\"big_response\":\"x\",\"small_response\":\"y\",\"sigma_big\":0.60,\"sigma_small\":0.80}\n", fi);
    fputs("{\"id\":\"r7\",\"prompt\":\"both wrong\",\"big_response\":\"p\",\"small_response\":\"q\",\"sigma_big\":0.40,\"sigma_small\":0.30}\n", fi);
    fputs("{\"id\":\"r8\",\"prompt_missing_everything\": true}\n", fi);
    fclose(fi);

    cos_v120_config_t c;
    cos_v120_config_defaults(&c);
    cos_v120_stats_t s;
    int rc = cos_v120_select(&c, in_path, out_path, &s);
    _CHECK(rc == 0, "select rc");
    _CHECK(s.rows_read == 8, "rows_read = 8");
    _CHECK(s.rows_kept == 3, "rows_kept = 3");
    _CHECK(s.rows_malformed >= 1, "malformed row counted");

    /* Verify output JSONL contains Paris (teacher response). */
    FILE *fo = fopen(out_path, "r");
    _CHECK(fo != NULL, "fopen out");
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, fo);
    buf[n] = '\0';
    fclose(fo);
    _CHECK(strstr(buf, "Paris") != NULL, "teacher response in out");
    _CHECK(strstr(buf, "v120-sigma-distill") != NULL, "source tag in out");
    _CHECK(strstr(buf, "Dickens") == NULL, "student response not leaked");

    /* Manifest JSON shape. */
    char js[512];
    int jn = cos_v120_stats_to_json(&c, &s, js, sizeof js);
    _CHECK(jn > 0, "manifest non-empty");
    _CHECK(strstr(js, "\"rows_kept\":3") != NULL, "manifest rows_kept=3");
    _CHECK(strstr(js, "\"tau_big_low\":")    != NULL, "manifest has τ_big");
    _CHECK(strstr(js, "\"tau_small_high\":") != NULL, "manifest has τ_small");

    unlink(in_path);
    unlink(out_path);
    return 0;
}
