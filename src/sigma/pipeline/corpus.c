/*
 * σ-Corpus — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "corpus.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void corpus_ncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static const char *corpus_basename(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static int corpus_read_file(const char *path, char **buf_out, size_t *sz_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return COS_CORPUS_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return COS_CORPUS_ERR_IO; }
    long end = ftell(f);
    if (end < 0) { fclose(f); return COS_CORPUS_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return COS_CORPUS_ERR_IO; }
    size_t want = (size_t)end;
    if (want > COS_CORPUS_READ_CAP) want = COS_CORPUS_READ_CAP;
    char *buf = (char *)malloc(want + 1);
    if (!buf) { fclose(f); return COS_CORPUS_ERR_OOM; }
    size_t got = fread(buf, 1, want, f);
    fclose(f);
    buf[got] = '\0';
    *buf_out = buf;
    *sz_out  = got;
    return COS_CORPUS_OK;
}

int cos_corpus_ingest_file(cos_rag_index_t *idx,
                           const char *path,
                           cos_corpus_file_report_t *out) {
    if (!idx || !path || !out) return COS_CORPUS_ERR_ARG;
    memset(out, 0, sizeof *out);
    corpus_ncpy(out->path, sizeof out->path, path);
    char *buf = NULL;
    size_t sz = 0;
    int rc = corpus_read_file(path, &buf, &sz);
    if (rc != COS_CORPUS_OK) { out->ok = 0; out->status = rc; return rc; }

    int before = idx->n_chunks;
    int ct = cos_rag_append_text(idx, buf, corpus_basename(path));
    free(buf);

    if (ct < 0) {
        out->ok = 0;
        out->status = COS_CORPUS_ERR_OOM;
        return COS_CORPUS_ERR_OOM;
    }
    out->ok = 1;
    out->bytes_read = sz;
    out->chunks_added = idx->n_chunks - before;
    out->status = 0;
    return COS_CORPUS_OK;
}

/* Resolve `path`: if starts with '/', keep; otherwise prefix with
 * dirname(manifest_path).  Writes into `out` (cap bytes). */
static void corpus_resolve(char *out, size_t cap,
                           const char *manifest_path, const char *path) {
    if (path[0] == '/') { corpus_ncpy(out, cap, path); return; }
    /* Find dirname(manifest_path) */
    const char *slash = strrchr(manifest_path, '/');
    if (!slash) { corpus_ncpy(out, cap, path); return; }
    size_t dirlen = (size_t)(slash - manifest_path);
    if (dirlen >= cap - 1) dirlen = cap - 2;
    memcpy(out, manifest_path, dirlen);
    out[dirlen] = '/';
    size_t rem = cap - dirlen - 1;
    corpus_ncpy(out + dirlen + 1, rem, path);
}

int cos_corpus_ingest_manifest(cos_rag_index_t *idx,
                               const char *manifest_path,
                               cos_corpus_report_t *rep) {
    if (!idx || !manifest_path || !rep) return COS_CORPUS_ERR_ARG;
    memset(rep, 0, sizeof *rep);

    FILE *f = fopen(manifest_path, "r");
    if (!f) return COS_CORPUS_ERR_IO;

    char line[COS_CORPUS_PATH_MAX];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' ||
                         line[n - 1] == ' '  || line[n - 1] == '\t')) {
            line[--n] = '\0';
        }
        size_t start = 0;
        while (start < n && (line[start] == ' ' || line[start] == '\t')) start++;
        if (start == n) continue;                 /* blank */
        if (line[start] == '#') continue;         /* comment */
        if (rep->n >= COS_CORPUS_MAX_FILES) break;

        char resolved[COS_CORPUS_PATH_MAX];
        corpus_resolve(resolved, sizeof resolved,
                       manifest_path, line + start);

        cos_corpus_file_report_t *r = &rep->files[rep->n++];
        if (cos_corpus_ingest_file(idx, resolved, r) == COS_CORPUS_OK) {
            rep->n_ok++;
            rep->total_bytes  += r->bytes_read;
            rep->total_chunks += r->chunks_added;
        } else {
            rep->n_skipped++;
        }
    }
    fclose(f);
    return COS_CORPUS_OK;
}

/* ==================================================================
 * JSON report
 * ================================================================== */
static int cjson_writef(char *dst, size_t cap, size_t *off,
                        const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int rc = vsnprintf(dst + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (rc < 0 || (size_t)rc >= cap - *off) return -1;
    *off += (size_t)rc;
    return 0;
}

int cos_corpus_report_json(const cos_corpus_report_t *rep,
                           char *dst, size_t cap) {
    if (!rep || !dst || cap == 0) return COS_CORPUS_ERR_ARG;
    size_t off = 0;
    dst[0] = '\0';
    if (cjson_writef(dst, cap, &off,
        "{\"kernel\":\"sigma_corpus\",\"version\":1,"
        "\"total\":%d,\"ok\":%d,\"skipped\":%d,"
        "\"total_bytes\":%zu,\"total_chunks\":%d,\"files\":[",
        rep->n, rep->n_ok, rep->n_skipped,
        rep->total_bytes, rep->total_chunks)) return -1;
    for (int i = 0; i < rep->n; i++) {
        const cos_corpus_file_report_t *r = &rep->files[i];
        const char *base = corpus_basename(r->path);
        if (cjson_writef(dst, cap, &off,
            "%s{\"name\":\"%s\",\"ok\":%s,\"bytes\":%zu,\"chunks\":%d,"
            "\"status\":%d}",
            i ? "," : "",
            base,
            r->ok ? "true" : "false",
            r->bytes_read,
            r->chunks_added,
            r->status)) return -1;
    }
    if (cjson_writef(dst, cap, &off, "]}")) return -1;
    return (int)off;
}

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_corpus_self_test(void) {
    /* Write two tiny files + a manifest into /tmp, ingest, assert. */
    char dir[64]; snprintf(dir, sizeof dir, "/tmp/cos_corpus_%d", (int)getpid()) ;
    mkdir(dir, 0700);

    char p1[128], p2[128], man[128];
    snprintf(p1,  sizeof p1,  "%s/a.md", dir);
    snprintf(p2,  sizeof p2,  "%s/b.md", dir);
    snprintf(man, sizeof man, "%s/m.txt", dir);
    FILE *f;
    f = fopen(p1, "w"); if (!f) return -1;
    fputs("sigma gate measures confidence in creation os", f); fclose(f);
    f = fopen(p2, "w"); if (!f) return -2;
    fputs("rag retrieves local chunks sigma filter", f); fclose(f);
    f = fopen(man, "w"); if (!f) return -3;
    fputs("# comment\n\na.md\nb.md\nnonexistent.md\n", f); fclose(f);

    cos_rag_index_t idx;
    if (cos_rag_index_init(&idx, 6, 0, 0.9f) != COS_RAG_OK) return -4;

    cos_corpus_report_t rep;
    if (cos_corpus_ingest_manifest(&idx, man, &rep) != COS_CORPUS_OK) return -5;

    if (rep.n != 3) return -6;
    if (rep.n_ok != 2) return -7;
    if (rep.n_skipped != 1) return -8;
    if (rep.total_chunks < 1) return -9;

    cos_rag_hit_t hits[4]; int hn = 0;
    cos_rag_search(&idx, "sigma gate", 4, 1, hits, &hn);
    if (hn < 1) return -10;
    if (strcmp(hits[0].chunk->source_file, "a.md") != 0) return -11;

    cos_rag_index_free(&idx);
    remove(p1); remove(p2); remove(man); rmdir(dir);
    return 0;
}
