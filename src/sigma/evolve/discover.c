/*
 * σ-Discover — implementation.  OMEGA-5.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "discover.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

int cos_discover_run_one(const cos_discover_experiment_t *ex,
                         cos_discover_result_t *out) {
    if (!ex || !out) return -1;
    memset(out, 0, sizeof *out);
    snprintf(out->id, sizeof out->id, "%s", ex->id);
    out->verdict = COS_DISCOVER_INCONCLUSIVE;

    /* Run with both streams merged so the verdict can see stderr too. */
    char shell[sizeof ex->cmd + 32];
    snprintf(shell, sizeof shell, "%s 2>&1", ex->cmd);
    double t0 = now_ms();
    FILE *fp = popen(shell, "r");
    if (!fp) return -1;

    enum { CAP = 64 * 1024 };
    char *buf = malloc(CAP);
    if (!buf) { pclose(fp); return -1; }
    size_t n = 0;
    int c;
    while (n + 1 < CAP && (c = fgetc(fp)) != EOF) buf[n++] = (char)c;
    buf[n] = '\0';
    int status = pclose(fp);
    out->elapsed_ms = now_ms() - t0;

    int exit_code = -1;
    if (status == -1) exit_code = -1;
#ifdef WIFEXITED
    else if (WIFEXITED(status))   exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) exit_code = 128 + WTERMSIG(status);
    else                          exit_code = status;
#else
    else exit_code = status;
#endif
    out->exit_code = exit_code;

    /* stdout snippet — last 500 bytes, so we see the tail. */
    size_t snip_cap = sizeof out->stdout_snippet;
    if (n + 1 <= snip_cap) {
        memcpy(out->stdout_snippet, buf, n);
        out->stdout_snippet[n] = '\0';
    } else {
        size_t tail = snip_cap - 1 - 4;   /* "...\0" + 1 */
        memcpy(out->stdout_snippet, "...\n", 4);
        memcpy(out->stdout_snippet + 4, buf + n - tail, tail);
        out->stdout_snippet[4 + tail] = '\0';
    }

    switch (ex->expect_kind) {
    case COS_DISCOVER_EXIT_0:
        out->verdict = (exit_code == 0)
            ? COS_DISCOVER_CONFIRMED : COS_DISCOVER_REJECTED;
        break;
    case COS_DISCOVER_EXIT_NZ:
        out->verdict = (exit_code != 0)
            ? COS_DISCOVER_CONFIRMED : COS_DISCOVER_REJECTED;
        break;
    case COS_DISCOVER_CONTAINS:
        out->verdict = strstr(buf, ex->expect)
            ? COS_DISCOVER_CONFIRMED : COS_DISCOVER_REJECTED;
        break;
    case COS_DISCOVER_NOT_CONTAINS:
        out->verdict = !strstr(buf, ex->expect)
            ? COS_DISCOVER_CONFIRMED : COS_DISCOVER_REJECTED;
        break;
    }
    free(buf);
    return 0;
}

static const char *verdict_str(cos_discover_verdict_t v) {
    switch (v) {
    case COS_DISCOVER_CONFIRMED:    return "confirmed";
    case COS_DISCOVER_REJECTED:     return "rejected";
    case COS_DISCOVER_INCONCLUSIVE: return "inconclusive";
    }
    return "?";
}

static void json_escape_into(const char *in, char *out, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 6 < cap; i++) {
        unsigned char c = (unsigned char)in[i];
        switch (c) {
        case '"':  out[j++] = '\\'; out[j++] = '"';  break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
        case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
        case '\t': out[j++] = '\\'; out[j++] = 't';  break;
        default:
            if (c < 0x20) {
                snprintf(out + j, cap - j, "\\u%04x", c);
                j += 6;
            } else out[j++] = (char)c;
        }
    }
    if (j < cap) out[j] = '\0';
    else         out[cap - 1] = '\0';
}

int cos_discover_log_append_jsonl(const char *path,
                                  const cos_discover_experiment_t *ex,
                                  const cos_discover_result_t *r) {
    if (!path || !ex || !r) return -1;
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    char hyp[512], snip[1024];
    json_escape_into(ex->hypothesis, hyp,  sizeof hyp);
    json_escape_into(r->stdout_snippet, snip, sizeof snip);
    fprintf(f,
        "{\"ts\":%lld,\"id\":\"%s\",\"hypothesis\":\"%s\","
        "\"verdict\":\"%s\",\"exit\":%d,\"elapsed_ms\":%.3f,"
        "\"tail\":\"%s\"}\n",
        (long long)time(NULL),
        ex->id, hyp, verdict_str(r->verdict),
        r->exit_code, r->elapsed_ms, snip);
    fclose(f);
    return 0;
}

/* ------------------ self-test ------------------ */

int cos_discover_self_test(void) {
    cos_discover_experiment_t ex_ok = {
        .id = "sx_exit_0",
        .hypothesis = "trivially-true: /bin/true exits 0",
        .cmd = "/usr/bin/env true",
        .expect_kind = COS_DISCOVER_EXIT_0,
    };
    cos_discover_result_t r;
    if (cos_discover_run_one(&ex_ok, &r) != 0) return 1;
    if (r.verdict != COS_DISCOVER_CONFIRMED) return 2;

    cos_discover_experiment_t ex_nz = {
        .id = "sx_exit_nz",
        .hypothesis = "trivially-true: /bin/false exits non-zero",
        .cmd = "/usr/bin/env false",
        .expect_kind = COS_DISCOVER_EXIT_NZ,
    };
    if (cos_discover_run_one(&ex_nz, &r) != 0) return 3;
    if (r.verdict != COS_DISCOVER_CONFIRMED) return 4;

    cos_discover_experiment_t ex_cn = {
        .id = "sx_contains",
        .hypothesis = "echo emits its argument",
        .cmd = "echo the-needle-in-the-haystack",
        .expect = "needle",
        .expect_kind = COS_DISCOVER_CONTAINS,
    };
    if (cos_discover_run_one(&ex_cn, &r) != 0) return 5;
    if (r.verdict != COS_DISCOVER_CONFIRMED) return 6;

    cos_discover_experiment_t ex_rj = {
        .id = "sx_contains_miss",
        .hypothesis = "string not present",
        .cmd = "echo abc",
        .expect = "zzz",
        .expect_kind = COS_DISCOVER_CONTAINS,
    };
    if (cos_discover_run_one(&ex_rj, &r) != 0) return 7;
    if (r.verdict != COS_DISCOVER_REJECTED) return 8;

    return 0;
}
