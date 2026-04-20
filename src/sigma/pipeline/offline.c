/*
 * σ-Offline — verified airplane-mode readiness (implementation).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "offline.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ==================================================================
 * Helpers
 * ================================================================== */
static void off_ncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static uint64_t off_hash_file(const char *path, size_t max_bytes) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint64_t h = 1469598103934665603ull;
    unsigned char buf[4096];
    size_t read = 0;
    while (read < max_bytes) {
        size_t want = sizeof buf;
        if (read + want > max_bytes) want = max_bytes - read;
        size_t got = fread(buf, 1, want, f);
        if (got == 0) break;
        for (size_t i = 0; i < got; i++) {
            h ^= buf[i];
            h *= 1099511628211ull;
        }
        read += got;
    }
    fclose(f);
    return h;
}

/* ==================================================================
 * Plan helpers
 * ================================================================== */
void cos_offline_plan_init(cos_offline_plan_t *plan) {
    if (!plan) return;
    memset(plan, 0, sizeof *plan);
}

int cos_offline_plan_add(cos_offline_plan_t *plan,
                         const char *name,
                         const char *path,
                         int required,
                         size_t min_bytes) {
    if (!plan || !name || !path) return COS_OFFLINE_ERR_ARG;
    if (plan->n >= COS_OFFLINE_MAX_PROBES) return COS_OFFLINE_ERR_FULL;
    cos_offline_probe_t *p = &plan->probes[plan->n];
    off_ncpy(p->name, sizeof p->name, name);
    off_ncpy(p->path, sizeof p->path, path);
    p->required  = required ? 1 : 0;
    p->min_bytes = min_bytes;
    plan->n++;
    return COS_OFFLINE_OK;
}

static void off_join(char *dst, size_t cap,
                     const char *base, const char *tail) {
    if (!base || !*base) { off_ncpy(dst, cap, tail); return; }
    size_t bl = strlen(base);
    int need_sep = (bl > 0 && base[bl - 1] != '/');
    snprintf(dst, cap, "%s%s%s", base, need_sep ? "/" : "", tail);
}

int cos_offline_plan_default(cos_offline_plan_t *plan, const char *base) {
    if (!plan) return COS_OFFLINE_ERR_ARG;
    cos_offline_plan_init(plan);
    char p[COS_OFFLINE_PATH_MAX];

    off_join(p, sizeof p, base, "model.bin");
    cos_offline_plan_add(plan, "model", p, 1, 1);

    off_join(p, sizeof p, base, "engram.sqlite");
    cos_offline_plan_add(plan, "engram", p, 1, 1);

    off_join(p, sizeof p, base, "rag_index.bin");
    cos_offline_plan_add(plan, "rag", p, 1, 1);

    off_join(p, sizeof p, base, "codex.txt");
    cos_offline_plan_add(plan, "codex", p, 1, 1);

    off_join(p, sizeof p, base, "persona.json");
    cos_offline_plan_add(plan, "persona", p, 0, 1);

    return COS_OFFLINE_OK;
}

/* ==================================================================
 * Verify
 * ================================================================== */
int cos_offline_verify(const cos_offline_plan_t *plan,
                       cos_offline_report_t     *out) {
    if (!plan || !out) return COS_OFFLINE_ERR_ARG;
    memset(out, 0, sizeof *out);
    out->n = plan->n;
    for (int i = 0; i < plan->n; i++) {
        const cos_offline_probe_t *pr = &plan->probes[i];
        cos_offline_probe_result_t *r = &out->results[i];
        off_ncpy(r->name, sizeof r->name, pr->name);
        off_ncpy(r->path, sizeof r->path, pr->path);
        r->required = pr->required;

        if (pr->required) out->n_required++;

        struct stat st;
        if (stat(pr->path, &st) != 0) {
            r->present = 0;
            r->status  = COS_OFFLINE_MISSING;
            continue;
        }
        r->present    = 1;
        r->size_bytes = (size_t)st.st_size;

        if (r->size_bytes < pr->min_bytes) {
            r->status = COS_OFFLINE_EMPTY;
            continue;
        }

        /* Content hash: cap at 1 MiB so giant model shards don't
         * stall the verifier. */
        r->content_hash = off_hash_file(pr->path, 1u << 20);
        if (r->content_hash == 0) {
            r->status = COS_OFFLINE_UNREADABLE;
            continue;
        }
        r->status = COS_OFFLINE_OK;
        out->n_ok++;
        if (pr->required) out->n_required_ok++;
    }
    out->verdict = (out->n_required_ok == out->n_required) ? 1 : 0;
    return COS_OFFLINE_OK;
}

/* ==================================================================
 * JSON report
 * ================================================================== */
static int off_writef(char *dst, size_t cap, size_t *off,
                      const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rc = vsnprintf(dst + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (rc < 0 || (size_t)rc >= cap - *off) return -1;
    *off += (size_t)rc;
    return 0;
}

int cos_offline_report_json(const cos_offline_report_t *rep,
                            char *dst, size_t cap) {
    if (!rep || !dst || cap == 0) return COS_OFFLINE_ERR_ARG;
    size_t off = 0;
    dst[0] = '\0';
    if (off_writef(dst, cap, &off,
        "{\"kernel\":\"sigma_offline\",\"version\":1,"
        "\"verdict\":%s,\"required\":%d,\"required_ok\":%d,"
        "\"total\":%d,\"ok\":%d,\"probes\":[",
        rep->verdict ? "\"ready\"" : "\"not_ready\"",
        rep->n_required, rep->n_required_ok,
        rep->n, rep->n_ok)) return COS_OFFLINE_ERR_ARG;

    for (int i = 0; i < rep->n; i++) {
        const cos_offline_probe_result_t *r = &rep->results[i];
        if (off_writef(dst, cap, &off,
            "%s{\"name\":\"%s\",\"required\":%s,\"present\":%s,"
            "\"status\":\"%s\",\"size\":%zu,\"hash\":\"%016llx\"}",
            i ? "," : "",
            r->name,
            r->required ? "true" : "false",
            r->present  ? "true" : "false",
            cos_offline_status_str(r->status),
            r->size_bytes,
            (unsigned long long)r->content_hash)) return COS_OFFLINE_ERR_ARG;
    }
    if (off_writef(dst, cap, &off, "]}")) return COS_OFFLINE_ERR_ARG;
    return (int)off;
}

const char *cos_offline_status_str(int status) {
    switch (status) {
        case COS_OFFLINE_OK:         return "ok";
        case COS_OFFLINE_MISSING:    return "missing";
        case COS_OFFLINE_EMPTY:      return "empty";
        case COS_OFFLINE_UNREADABLE: return "unreadable";
        default:                     return "error";
    }
}

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_offline_self_test(void) {
    /* Build a pretend ~/.cos directory in /tmp and verify it. */
    char dir[256];
    snprintf(dir, sizeof dir, "/tmp/cos_offline_self_%d", (int)getpid());
    mkdir(dir, 0700);

    char path[512];
    /* model */
    snprintf(path, sizeof path, "%s/model.bin", dir);
    { FILE *f = fopen(path, "wb"); if (!f) return -1;
      fprintf(f, "BITNET-2B-SHARD"); fclose(f); }
    /* engram */
    snprintf(path, sizeof path, "%s/engram.sqlite", dir);
    { FILE *f = fopen(path, "wb"); if (!f) return -2;
      fprintf(f, "SQLITE"); fclose(f); }
    /* rag */
    snprintf(path, sizeof path, "%s/rag_index.bin", dir);
    { FILE *f = fopen(path, "wb"); if (!f) return -3;
      fprintf(f, "RAGINDEX"); fclose(f); }
    /* codex */
    snprintf(path, sizeof path, "%s/codex.txt", dir);
    { FILE *f = fopen(path, "wb"); if (!f) return -4;
      fprintf(f, "1=1 assert(declared == realized)"); fclose(f); }
    /* persona (optional) */
    snprintf(path, sizeof path, "%s/persona.json", dir);
    { FILE *f = fopen(path, "wb"); if (!f) return -5;
      fprintf(f, "{\"name\":\"alpha\"}"); fclose(f); }

    cos_offline_plan_t plan;
    cos_offline_plan_default(&plan, dir);
    cos_offline_report_t rep;
    if (cos_offline_verify(&plan, &rep) != COS_OFFLINE_OK) return -6;
    if (rep.verdict != 1) return -7;
    if (rep.n_required != 4) return -8;
    if (rep.n_required_ok != 4) return -9;

    /* Knock out engram → not ready */
    char engram[512];
    snprintf(engram, sizeof engram, "%s/engram.sqlite", dir);
    remove(engram);
    cos_offline_verify(&plan, &rep);
    if (rep.verdict != 0) return -10;

    /* Cleanup */
    snprintf(path, sizeof path, "%s/model.bin",    dir); remove(path);
    snprintf(path, sizeof path, "%s/rag_index.bin", dir); remove(path);
    snprintf(path, sizeof path, "%s/codex.txt",    dir); remove(path);
    snprintf(path, sizeof path, "%s/persona.json", dir); remove(path);
    rmdir(dir);
    return 0;
}
