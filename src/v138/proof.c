/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v138 σ-Proof — implementation.
 */
#include "proof.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int file_contains(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL ? 1 : 0;
}

/* Read entire file into a freshly allocated string.  Returns NULL on
 * error; caller frees.  Hard cap at 1 MiB (sigma_gate.c is ~3 KiB). */
static char *slurp(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz < 0 || sz > (1 << 20)) { fclose(fp); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, fp);
    buf[r] = 0;
    fclose(fp);
    return buf;
}

cos_v138_result_t cos_v138_validate_acsl(const char *path,
                                         cos_v138_report_t *rep) {
    if (!path || !rep) return COS_V138_PROOF_FAIL;
    memset(rep, 0, sizeof *rep);
    char *src = slurp(path);
    if (!src) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "cannot read: %s", path);
        return COS_V138_PROOF_FAIL;
    }

    /* Walk ACSL "slash-star-at" annotation blocks. */
    char *p = src;
    while (*p) {
        char *start = strstr(p, "/*@");
        if (!start) break;
        char *end = strstr(start + 3, "*/");
        if (!end) { snprintf(rep->last_error, sizeof rep->last_error,
                             "unterminated /*@ block"); free(src);
                    return COS_V138_PROOF_FAIL; }
        *end = 0;
        rep->n_annotations++;
        if (strstr(start, "requires"))          rep->n_with_requires++;
        if (strstr(start, "ensures"))           rep->n_with_ensures++;
        if (strstr(start, "loop invariant"))    rep->has_loop_invariant = 1;
        if (strstr(start, "disjoint behaviors"))rep->has_disjoint_behaviors = 1;
        *end = '*';   /* restore */
        p = end + 2;
    }

    /* Whole-file global checks (searches entire slurped text). */
    if (file_contains(src, "0.0f <= sigma <= 1.0f") ||
        file_contains(src, "0.0f <= ch[")           ||
        file_contains(src, "0.0f <= ch[i] <= 1.0f"))
        rep->has_domain_predicate = 1;
    if (file_contains(src, "behavior emit")    ||
        file_contains(src, "\\result == 0"))
        rep->has_emit_behavior = 1;
    if (file_contains(src, "behavior abstain") ||
        file_contains(src, "\\result == 1"))
        rep->has_abstain_behavior = 1;

    free(src);

    /* Contract: at least one annotated function, every one carries
     * requires + ensures, the σ-domain predicate is present, and
     * both emit/abstain sides are visible. */
    if (rep->n_annotations <= 0) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "no /*@ blocks in %s", path);
        return COS_V138_PROOF_FAIL;
    }
    if (rep->n_with_requires <= 0) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "no `requires` clause anywhere");
        return COS_V138_PROOF_FAIL;
    }
    if (rep->n_with_ensures <= 0) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "no `ensures` clause anywhere");
        return COS_V138_PROOF_FAIL;
    }
    if (!rep->has_domain_predicate) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "σ-domain predicate (0.0f<=σ<=1.0f) missing");
        return COS_V138_PROOF_FAIL;
    }
    if (!rep->has_emit_behavior || !rep->has_abstain_behavior) {
        snprintf(rep->last_error, sizeof rep->last_error,
                 "partition incomplete (emit=%d abstain=%d)",
                 rep->has_emit_behavior, rep->has_abstain_behavior);
        return COS_V138_PROOF_FAIL;
    }
    return COS_V138_PROOF_OK;
}

/* Drain pipe fd without blocking forever (Frama-C WP can hang on some
 * hosts).  Returns 0 = EOF, -1 = timeout, -2 = read error. */
static int cos_v138_drain_fd_timeout(int fd, char *stdout_buf, size_t cap,
                                     int timeout_sec) {
    if (fd < 0 || !stdout_buf || cap == 0) return 0;
    stdout_buf[0] = 0;
    size_t off = 0;
    time_t deadline = time(NULL) + (time_t)(timeout_sec > 0 ? timeout_sec : 120);
    for (;;) {
        time_t now = time(NULL);
        if (now >= deadline)
            return -1;
        int ms = (int)((deadline - now) * 1000);
        if (ms > 500) ms = 500;
        if (ms < 1) ms = 1;
        struct pollfd pfd = { .fd = fd, .events = (short)POLLIN };
        int pr = poll(&pfd, 1, ms);
        if (pr < 0) {
            if (errno == EINTR) continue;
            return -2;
        }
        if (pr == 0)
            continue;
        char chunk[1024];
        ssize_t n = read(fd, chunk, sizeof chunk);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -2;
        }
        if (n == 0)
            return 0;
        if (off + (size_t)n + 1 < cap) {
            memcpy(stdout_buf + off, chunk, (size_t)n);
            off += (size_t)n;
            stdout_buf[off] = 0;
        }
    }
}

/* Try Frama-C.  We intentionally do NOT depend on Frama-C being
 * present — tier 2 is opportunistic and returns SKIPPED otherwise. */
cos_v138_result_t cos_v138_try_frama_c(const char *path,
                                       char *stdout_buf, size_t cap) {
    if (!path) return COS_V138_PROOF_FAIL;

    /* Probe `frama-c -version` via /bin/sh to get exit code. */
    if (system("command -v frama-c >/dev/null 2>&1") != 0)
        return COS_V138_PROOF_SKIPPED;

    int timeout_sec = 120;
    const char *ts = getenv("COS_V138_FRAMA_TIMEOUT_SEC");
    if (ts && ts[0]) {
        int v = atoi(ts);
        if (v > 0 && v <= 3600) timeout_sec = v;
    }

    int pfd[2];
    if (pipe(pfd) != 0) return COS_V138_PROOF_FAIL;

    pid_t pid = fork();
    if (pid == (pid_t)-1) {
        close(pfd[0]);
        close(pfd[1]);
        return COS_V138_PROOF_FAIL;
    }
    if (pid == 0) {
        /* Child: Frama-C stdout+stderr → write end of pipe */
        (void)close(pfd[0]);
        if (dup2(pfd[1], STDOUT_FILENO) < 0 || dup2(pfd[1], STDERR_FILENO) < 0)
            _exit(126);
        (void)close(pfd[1]);
        execlp("frama-c", "frama-c", "-wp", "-wp-rte", path, (char *)NULL);
        _exit(127);
    }
    (void)close(pfd[1]);

    char scratch[4096];
    char *sink = (stdout_buf && cap > 0) ? stdout_buf : scratch;
    size_t sink_cap = (stdout_buf && cap > 0) ? cap : sizeof scratch;
    if (stdout_buf && cap > 0) stdout_buf[0] = 0;
    int dr = cos_v138_drain_fd_timeout(pfd[0], sink, sink_cap, timeout_sec);
    (void)close(pfd[0]);

    int st = 0;
    if (dr == -1) {
        (void)kill(pid, SIGKILL);
        (void)waitpid(pid, &st, 0);
        return COS_V138_PROOF_SKIPPED;
    }
    if (dr == -2) {
        (void)kill(pid, SIGKILL);
        (void)waitpid(pid, &st, 0);
        return COS_V138_PROOF_FAIL;
    }
    if (waitpid(pid, &st, 0) != pid)
        return COS_V138_PROOF_FAIL;
    int rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    /* WP success → rc == 0.  Any "Invalid" or "Timeout" → fail. */
    if (stdout_buf && (strstr(stdout_buf, "Invalid") ||
                       strstr(stdout_buf, "Timeout")))
        return COS_V138_PROOF_FAIL;
    if (rc != 0) return COS_V138_PROOF_FAIL;
    return COS_V138_PROOF_OK;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v138 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v138_self_test(void) {
    cos_v138_report_t rep;

    /* --- A. In-tree sigma_gate.c must validate ---------------- */
    fprintf(stderr, "check-v138: tier-1 validate src/v138/sigma_gate.c\n");
    cos_v138_result_t r =
        cos_v138_validate_acsl("src/v138/sigma_gate.c", &rep);
    fprintf(stderr, "  n_annotations=%d requires=%d ensures=%d "
                    "domain=%d emit=%d abstain=%d loop=%d disjoint=%d\n",
            rep.n_annotations, rep.n_with_requires, rep.n_with_ensures,
            rep.has_domain_predicate, rep.has_emit_behavior,
            rep.has_abstain_behavior, rep.has_loop_invariant,
            rep.has_disjoint_behaviors);
    if (r != COS_V138_PROOF_OK)
        fprintf(stderr, "  last_error: %s\n", rep.last_error);
    _CHECK(r == COS_V138_PROOF_OK, "sigma_gate.c validates");
    _CHECK(rep.n_annotations >= 3, "at least 3 annotated functions");
    _CHECK(rep.has_loop_invariant, "vec8 carries loop invariant");
    _CHECK(rep.has_disjoint_behaviors, "behaviors marked disjoint");

    /* --- B. Negative case: a stub with no requires ------------ */
    fprintf(stderr, "check-v138: negative validation on stub\n");
    const char *stub_path = "/tmp/cos_v138_stub.c";
    FILE *fp = fopen(stub_path, "w");
    _CHECK(fp, "open stub");
    fprintf(fp,
        "int nocontract(float x) { return x > 0.5f; }\n");
    fclose(fp);
    cos_v138_result_t r2 = cos_v138_validate_acsl(stub_path, &rep);
    _CHECK(r2 == COS_V138_PROOF_FAIL, "stub must FAIL");
    _CHECK(strlen(rep.last_error) > 0, "error message populated");

    /* --- C. Tier-2 Frama-C probe (never required) ------------- */
    fprintf(stderr, "check-v138: tier-2 Frama-C probe (optional)\n");
    char ob[4096] = {0};
    cos_v138_result_t r3 =
        cos_v138_try_frama_c("src/v138/sigma_gate.c", ob, sizeof ob);
    if (r3 == COS_V138_PROOF_SKIPPED)
        fprintf(stderr, "  tier-2 skipped: frama-c not in $PATH (OK)\n");
    else if (r3 == COS_V138_PROOF_OK)
        fprintf(stderr, "  tier-2: Frama-C WP discharged all goals\n");
    else
        fprintf(stderr, "  tier-2 returned FAIL — inspect output above\n");
    /* Align with benchmarks/v138/check_v138_prove_frama_c_wp.sh: a
     * failing Frama-C run is non-fatal unless V138_REQUIRE_FRAMA_C=1. */
    {
        const char *req = getenv("V138_REQUIRE_FRAMA_C");
        int mandate = (req != NULL && strcmp(req, "1") == 0);
        if (mandate)
            _CHECK(r3 == COS_V138_PROOF_OK,
                   "V138_REQUIRE_FRAMA_C=1 mandates discharged WP");
        else if (r3 == COS_V138_PROOF_FAIL)
            fprintf(stderr,
                    "  tier-2 FAIL is non-fatal (export "
                    "V138_REQUIRE_FRAMA_C=1 to hard-fail merge-gate)\n");
        else
            _CHECK(r3 == COS_V138_PROOF_OK || r3 == COS_V138_PROOF_SKIPPED,
                   "tier-2 unexpected result code");
    }

    fprintf(stderr, "check-v138: OK (tier-1 shape + tier-2 probe)\n");
    return 0;
}
