/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v113 σ-Sandbox — implementation.
 */
#include "sandbox.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ---------------------------------------------------------------- */
/* JSON parsing helpers (small + local; v112 has its own copy).     */
/* ---------------------------------------------------------------- */

static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

static int copy_json_string(const char *src, char *out, size_t cap) {
    if (*src != '"') return -1;
    size_t w = 0;
    const char *p = src + 1;
    while (*p && *p != '"') {
        if (w + 1 >= cap) return -1;
        if (*p == '\\' && p[1]) {
            char c = p[1];
            switch (c) {
                case 'n': out[w++] = '\n'; break;
                case 't': out[w++] = '\t'; break;
                case 'r': out[w++] = '\r'; break;
                case '"': out[w++] = '"'; break;
                case '\\': out[w++] = '\\'; break;
                case '/': out[w++] = '/'; break;
                default:  out[w++] = c; break;
            }
            p += 2;
        } else {
            out[w++] = *p++;
        }
    }
    if (*p != '"') return -1;
    out[w] = '\0';
    return (int)w;
}

/* ---------------------------------------------------------------- */

void cos_v113_request_defaults(cos_v113_request_t *req)
{
    if (!req) return;
    req->code             = NULL;
    req->code_len         = 0;
    req->language         = COS_V113_LANG_PYTHON;
    req->timeout_sec      = 30;
    req->sigma_product_in = -1.f;
    req->tau_code         = 0.60f;
}

cos_v113_language_t cos_v113_lang_from_string(const char *s)
{
    if (!s || !*s) return COS_V113_LANG_UNKNOWN;
    if (strcasecmp(s, "python") == 0 || strcasecmp(s, "py") == 0 ||
        strcasecmp(s, "python3") == 0) return COS_V113_LANG_PYTHON;
    if (strcasecmp(s, "bash") == 0)    return COS_V113_LANG_BASH;
    if (strcasecmp(s, "sh") == 0)      return COS_V113_LANG_SH;
    if (strcasecmp(s, "text") == 0)    return COS_V113_LANG_TEXT;
    return COS_V113_LANG_UNKNOWN;
}

const char *cos_v113_lang_name(cos_v113_language_t lang)
{
    switch (lang) {
        case COS_V113_LANG_PYTHON: return "python";
        case COS_V113_LANG_BASH:   return "bash";
        case COS_V113_LANG_SH:     return "sh";
        case COS_V113_LANG_TEXT:   return "text";
        default: return "unknown";
    }
}

/* ---------------------------------------------------------------- */
/* Child runner                                                       */
/* ---------------------------------------------------------------- */

static void apply_rlimits(int cpu_secs)
{
    struct rlimit rl;
    rl.rlim_cur = (rlim_t)cpu_secs + 1;
    rl.rlim_max = (rlim_t)cpu_secs + 2;
    setrlimit(RLIMIT_CPU, &rl);

    /* 512 MiB address space cap — enough for most scripts, tight
     * enough to catch runaway malloc loops. */
    rl.rlim_cur = (rlim_t)512 * 1024 * 1024;
    rl.rlim_max = (rlim_t)512 * 1024 * 1024;
#ifdef RLIMIT_AS
    setrlimit(RLIMIT_AS, &rl);
#endif

    /* 64 MiB file-write cap: no surprise 20 GB tempfiles. */
    rl.rlim_cur = (rlim_t)64 * 1024 * 1024;
    rl.rlim_max = (rlim_t)64 * 1024 * 1024;
#ifdef RLIMIT_FSIZE
    setrlimit(RLIMIT_FSIZE, &rl);
#endif

    /* Disable core dumps — they're noise in a sandbox. */
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
#ifdef RLIMIT_CORE
    setrlimit(RLIMIT_CORE, &rl);
#endif
}

static void prepare_argv(cos_v113_language_t lang, const char *code_path,
                         const char **argv_out)
{
    switch (lang) {
        case COS_V113_LANG_PYTHON:
            argv_out[0] = "python3";
            argv_out[1] = code_path;
            argv_out[2] = NULL;
            break;
        case COS_V113_LANG_BASH:
            argv_out[0] = "bash";
            argv_out[1] = code_path;
            argv_out[2] = NULL;
            break;
        case COS_V113_LANG_SH:
        default:
            argv_out[0] = "sh";
            argv_out[1] = code_path;
            argv_out[2] = NULL;
            break;
    }
}

/* Read as much as fits into `buf` from `fd`.  Truncates cleanly at
 * cap-1 and always null-terminates.  Returns bytes read (<= cap-1). */
static size_t drain_fd(int fd, char *buf, size_t cap)
{
    if (cap == 0) return 0;
    size_t total = 0;
    while (total + 1 < cap) {
        ssize_t n = read(fd, buf + total, cap - 1 - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

int cos_v113_sandbox_run(const cos_v113_request_t *req,
                         cos_v113_result_t *r)
{
    if (!req || !r) return -1;
    memset(r, 0, sizeof *r);
    r->tau_code      = req->tau_code;
    r->sigma_product = (req->sigma_product_in >= 0.f)
                           ? req->sigma_product_in : -1.f;

    if (!req->code || req->code_len == 0 ||
        req->code_len >= COS_V113_CODE_MAX) {
        snprintf(r->gate_reason, sizeof r->gate_reason, "empty_or_oversized_code");
        return 0;
    }

    /* 1. σ-gate check. */
    if (req->sigma_product_in >= 0.f &&
        req->tau_code > 0.f &&
        req->sigma_product_in > req->tau_code) {
        r->gated_out = 1;
        snprintf(r->gate_reason, sizeof r->gate_reason,
                 "low_confidence_code (σ=%.3f>τ=%.2f)",
                 req->sigma_product_in, req->tau_code);
        return 0;
    }

    /* 2. Write code to a tempfile. */
    char tmpl[] = "/tmp/cos_v113_XXXXXX";
    int  tfd = mkstemp(tmpl);
    if (tfd < 0) {
        snprintf(r->gate_reason, sizeof r->gate_reason,
                 "mkstemp failed: %s", strerror(errno));
        return 0;
    }
    ssize_t wr = write(tfd, req->code, req->code_len);
    close(tfd);
    if (wr != (ssize_t)req->code_len) {
        unlink(tmpl);
        snprintf(r->gate_reason, sizeof r->gate_reason, "tempfile write short");
        return 0;
    }

    /* 3. Set up pipes for stdout + stderr. */
    int out_p[2], err_p[2];
    if (pipe(out_p) != 0) { unlink(tmpl); return -1; }
    if (pipe(err_p) != 0) {
        close(out_p[0]); close(out_p[1]); unlink(tmpl); return -1;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pid_t pid = fork();
    if (pid < 0) {
        close(out_p[0]); close(out_p[1]);
        close(err_p[0]); close(err_p[1]);
        unlink(tmpl);
        return -1;
    }

    if (pid == 0) {
        /* ----- child ----- */
        /* New process group so kill(-pgid, SIGKILL) from the parent
         * takes down any grandchildren (sh → sleep, etc.). */
        setpgid(0, 0);

        close(out_p[0]);
        close(err_p[0]);
        dup2(out_p[1], STDOUT_FILENO);
        dup2(err_p[1], STDERR_FILENO);
        close(out_p[1]);
        close(err_p[1]);

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }

        apply_rlimits(req->timeout_sec);

        /* Minimal env.  Callers that need more should proxy through
         * the tool function rather than the sandbox. */
        setenv("PATH", "/usr/bin:/bin:/usr/local/bin", 1);
        setenv("HOME", "/tmp", 1);
        unsetenv("PYTHONPATH");
        unsetenv("LD_LIBRARY_PATH");

        const char *argv[3] = {0};
        prepare_argv(req->language, tmpl, argv);

        execvp(argv[0], (char *const *)argv);
        /* execvp returned → failure. */
        fprintf(stderr, "exec of %s failed: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    /* ----- parent ----- */
    close(out_p[1]);
    close(err_p[1]);

    /* Enforce deadline with a blocking wait + alarm-style loop: we
     * poll waitpid(WNOHANG) every 100 ms, kill on deadline. */
    int status = 0;
    pid_t wp = 0;
    double deadline = req->timeout_sec;
    if (deadline <= 0.0) deadline = 30.0;
    for (;;) {
        wp = waitpid(pid, &status, WNOHANG);
        if (wp == pid) break;
        if (wp < 0) {
            if (errno == EINTR) continue;
            break;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - t0.tv_sec)
                       + (double)(now.tv_nsec - t0.tv_nsec) / 1e9;
        if (elapsed > deadline) {
            /* Kill the whole process group so `sh → sleep` style
             * grandchildren go down together. */
            kill(-pid, SIGKILL);
            kill(pid,  SIGKILL);
            r->timed_out = 1;
            waitpid(pid, &status, 0);
            break;
        }
        struct timespec ts = {0, 100 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }

    r->stdout_len = drain_fd(out_p[0], r->stdout_buf, sizeof r->stdout_buf);
    r->stderr_len = drain_fd(err_p[0], r->stderr_buf, sizeof r->stderr_buf);
    close(out_p[0]);
    close(err_p[0]);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    r->wall_seconds = (double)(t1.tv_sec - t0.tv_sec)
                    + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

    if (r->timed_out) {
        r->exit_code = -1;
        snprintf(r->gate_reason, sizeof r->gate_reason,
                 "deadline_exceeded (%.1fs > %ds)",
                 r->wall_seconds, req->timeout_sec);
    } else if (WIFEXITED(status)) {
        r->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        r->exit_code = 128 + WTERMSIG(status);
    } else {
        r->exit_code = -1;
    }
    r->executed = 1;
    unlink(tmpl);
    return 0;
}

/* ---------------------------------------------------------------- */
/* JSON output                                                        */
/* ---------------------------------------------------------------- */

static int json_escape(const char *src, char *dst, size_t cap)
{
    size_t w = 0;
    for (const char *p = src; *p && w + 7 < cap; ++p) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  dst[w++] = '\\'; dst[w++] = '"';  break;
            case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
            case '\n': dst[w++] = '\\'; dst[w++] = 'n';  break;
            case '\r': dst[w++] = '\\'; dst[w++] = 'r';  break;
            case '\t': dst[w++] = '\\'; dst[w++] = 't';  break;
            default:
                if (c < 0x20) {
                    int n = snprintf(dst + w, cap - w, "\\u%04x", c);
                    if (n < 0 || (size_t)n >= cap - w) break;
                    w += (size_t)n;
                } else {
                    dst[w++] = (char)c;
                }
        }
    }
    dst[w] = '\0';
    return (int)w;
}

int cos_v113_result_to_json(const cos_v113_result_t *r,
                            char *out, size_t cap)
{
    if (!r || !out) return -1;
    static char stdout_esc[COS_V113_STREAM_MAX * 2];
    static char stderr_esc[COS_V113_STREAM_MAX * 2];
    static char reason_esc[512];
    json_escape(r->stdout_buf, stdout_esc, sizeof stdout_esc);
    json_escape(r->stderr_buf, stderr_esc, sizeof stderr_esc);
    json_escape(r->gate_reason, reason_esc, sizeof reason_esc);
    int n = snprintf(out, cap,
        "{"
        "\"executed\":%s,"
        "\"gated_out\":%s,"
        "\"timed_out\":%s,"
        "\"exit_code\":%d,"
        "\"wall_seconds\":%.4f,"
        "\"stdout\":\"%s\","
        "\"stderr\":\"%s\","
        "\"sigma_product\":%.4f,"
        "\"tau_code\":%.4f,"
        "\"gate_reason\":\"%s\""
        "}",
        r->executed  ? "true" : "false",
        r->gated_out ? "true" : "false",
        r->timed_out ? "true" : "false",
        r->exit_code, r->wall_seconds,
        stdout_esc, stderr_esc,
        (double)r->sigma_product, (double)r->tau_code,
        reason_esc);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/* ---------------------------------------------------------------- */
/* Parse the POST body.                                               */
/* ---------------------------------------------------------------- */

static int extract_string(const char *body, const char *key,
                          char *out, size_t cap)
{
    char pat[64];
    int n = snprintf(pat, sizeof pat, "\"%s\"", key);
    if (n < 0) return -1;
    const char *hit = strstr(body, pat);
    if (!hit) return -1;
    const char *colon = strchr(hit, ':');
    if (!colon) return -1;
    ++colon;
    colon = skip_ws(colon);
    if (*colon != '"') return -1;
    return copy_json_string(colon, out, cap);
}

static long extract_int(const char *body, const char *key, long fb)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *hit = strstr(body, pat);
    if (!hit) return fb;
    const char *colon = strchr(hit, ':');
    if (!colon) return fb;
    ++colon;
    return strtol(skip_ws(colon), NULL, 10);
}

static double extract_double(const char *body, const char *key, double fb)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *hit = strstr(body, pat);
    if (!hit) return fb;
    const char *colon = strchr(hit, ':');
    if (!colon) return fb;
    ++colon;
    return strtod(skip_ws(colon), NULL);
}

int cos_v113_parse_request(const char *body, cos_v113_request_t *out)
{
    if (!body || !out) return -1;
    cos_v113_request_defaults(out);
    static char code_buf[COS_V113_CODE_MAX];
    int cn = extract_string(body, "code", code_buf, sizeof code_buf);
    if (cn < 0) return -1;
    out->code     = code_buf;
    out->code_len = (size_t)cn;

    char lang[COS_V113_LANG_MAX];
    if (extract_string(body, "language", lang, sizeof lang) > 0) {
        out->language = cos_v113_lang_from_string(lang);
        if (out->language == COS_V113_LANG_UNKNOWN)
            out->language = COS_V113_LANG_PYTHON;
    }

    long tmo = extract_int(body, "timeout", 30);
    if (tmo < 1)  tmo = 1;
    if (tmo > 120) tmo = 120;
    out->timeout_sec = (int)tmo;

    double sp = extract_double(body, "sigma_product", -1.0);
    out->sigma_product_in = (sp >= 0.0 && sp <= 1.0) ? (float)sp : -1.f;
    double tc = extract_double(body, "tau_code", 0.60);
    if (tc < 0.0) tc = 0.0; if (tc > 1.0) tc = 1.0;
    out->tau_code = (float)tc;
    return 0;
}

/* ---------------------------------------------------------------- */
/* Self-test                                                          */
/* ---------------------------------------------------------------- */

#define _CHECK(cond, msg) do {                            \
    if (!(cond)) { fprintf(stderr, "FAIL %s\n", (msg)); ++_fail; } \
    else ++_pass;                                         \
} while (0)

int cos_v113_self_test(void)
{
    int _pass = 0, _fail = 0;

    /* lang parse */
    _CHECK(cos_v113_lang_from_string("python") == COS_V113_LANG_PYTHON, "lang python");
    _CHECK(cos_v113_lang_from_string("PY")     == COS_V113_LANG_PYTHON, "lang PY");
    _CHECK(cos_v113_lang_from_string("bash")   == COS_V113_LANG_BASH,   "lang bash");
    _CHECK(cos_v113_lang_from_string("sh")     == COS_V113_LANG_SH,     "lang sh");
    _CHECK(cos_v113_lang_from_string("nope")   == COS_V113_LANG_UNKNOWN,"lang nope");

    /* parse body */
    cos_v113_request_t req;
    const char *b1 = "{\"code\":\"print('hi')\",\"language\":\"python\",\"timeout\":5}";
    _CHECK(cos_v113_parse_request(b1, &req) == 0, "parse request ok");
    _CHECK(req.language == COS_V113_LANG_PYTHON, "parsed lang");
    _CHECK(req.timeout_sec == 5, "parsed timeout");
    _CHECK(strstr(req.code, "print") != NULL, "parsed code");

    /* σ-gate: code is rejected */
    cos_v113_request_defaults(&req);
    req.code             = "echo gated";
    req.code_len         = strlen(req.code);
    req.language         = COS_V113_LANG_BASH;
    req.timeout_sec      = 5;
    req.sigma_product_in = 0.90f;
    req.tau_code         = 0.60f;
    cos_v113_result_t res;
    int rc = cos_v113_sandbox_run(&req, &res);
    _CHECK(rc == 0, "gated run rc=0");
    _CHECK(res.gated_out == 1, "gated_out set");
    _CHECK(res.executed == 0,  "not executed");
    _CHECK(strstr(res.gate_reason, "low_confidence_code") != NULL,
           "gate reason message");

    /* Live run: `echo hello` via sh.  Gate disabled (σ=-1). */
    cos_v113_request_defaults(&req);
    const char *code = "echo sandbox_hello";
    req.code             = code;
    req.code_len         = strlen(code);
    req.language         = COS_V113_LANG_SH;
    req.timeout_sec      = 5;
    req.sigma_product_in = -1.f;
    rc = cos_v113_sandbox_run(&req, &res);
    _CHECK(rc == 0, "live run rc=0");
    _CHECK(res.executed == 1, "executed");
    _CHECK(res.gated_out == 0, "not gated");
    _CHECK(res.timed_out == 0, "not timed out");
    _CHECK(res.exit_code == 0, "exit 0");
    _CHECK(strstr(res.stdout_buf, "sandbox_hello") != NULL,
           "stdout captured");

    /* JSON receipt. */
    char js[64 * 1024];
    int  jl = cos_v113_result_to_json(&res, js, sizeof js);
    _CHECK(jl > 0, "json ok");
    _CHECK(strstr(js, "\"executed\":true") != NULL, "json executed");
    _CHECK(strstr(js, "sandbox_hello") != NULL,     "json contents");

    /* Deadline trip on a sleep-6 with 1s limit. */
    cos_v113_request_defaults(&req);
    const char *slow = "sleep 6";
    req.code        = slow;
    req.code_len    = strlen(slow);
    req.language    = COS_V113_LANG_SH;
    req.timeout_sec = 1;
    req.sigma_product_in = -1.f;
    rc = cos_v113_sandbox_run(&req, &res);
    _CHECK(rc == 0, "deadline run rc=0");
    _CHECK(res.timed_out == 1, "deadline fired");
    _CHECK(res.wall_seconds < 5.0, "deadline bounded");

    if (_fail > 0) {
        fprintf(stderr, "v113 self-test: %d PASS / %d FAIL\n", _pass, _fail);
        return 1;
    }
    printf("v113 self-test: %d PASS / 0 FAIL\n", _pass);
    return 0;
}
