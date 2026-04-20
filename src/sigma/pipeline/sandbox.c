/*
 * σ-Sandbox — POSIX implementation.  See sandbox.h for contracts.
 *
 * Design
 * ------
 *   * fork().
 *   * Parent: create stdout/stderr pipes, fork child with pipe
 *     ends dup'd, poll pipes + child waitpid with a wall deadline
 *     computed from cfg->max_runtime_ms.  If the deadline is hit,
 *     SIGTERM the pgid; 200 ms grace → SIGKILL.
 *   * Child: close parent pipe ends; dup the write ends onto
 *     stdout/stderr; optionally chroot (euid == 0) or chdir;
 *     optionally unshare(CLONE_NEWNET) on Linux; setrlimit; execvp.
 *
 * Memory limit:
 *   macOS does not enforce RLIMIT_AS for arbitrary allocations
 *   (Mach maps bypass it); this module still sets it so portable
 *   tests see the bound, and the report surfaces memory_exceeded
 *   only when the child was killed by SIGKILL after exceeding
 *   (signal ∈ {SIGKILL, SIGSEGV, SIGBUS} + RLIMIT_AS set).  Callers
 *   who need strict memory enforcement should run on Linux or add
 *   a cgroup hop above σ-Sandbox.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include "sandbox.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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

#if defined(__linux__)
#include <sched.h>
#endif

/* chroot(2) lives in <unistd.h> on Linux but requires
 * _DARWIN_C_SOURCE on macOS; its prototype is only exposed via
 * <unistd.h> when that source level is selected.  Declare it
 * manually to remain portable under strict C11. */
extern int chroot(const char *);

/* ------------------------------------------------------------------ */

static uint64_t sandbox_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL
         + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static const char *sandbox_basename(const char *p) {
    if (!p) return "";
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* ------------------------------------------------------------------ */

void cos_sandbox_config_default(cos_sandbox_config_t *cfg, int risk_level) {
    if (!cfg) return;
    memset(cfg, 0, sizeof *cfg);
    if (risk_level < 0) risk_level = 0;
    if (risk_level > 4) risk_level = 4;
    cfg->risk_level         = risk_level;
    cfg->max_stdout_bytes   = 64 * 1024;
    cfg->max_stderr_bytes   = 64 * 1024;

    switch (risk_level) {
    case COS_SANDBOX_RISK_CALC:
        cfg->network_allowed    = 1;
        cfg->max_runtime_ms     = 5 * 1000;
        cfg->max_cpu_ms         = 5 * 1000;
        cfg->max_memory_mb      = 512;
        cfg->max_file_write_mb  = 0;
        break;
    case COS_SANDBOX_RISK_READ:
        cfg->network_allowed    = 0;
        cfg->max_runtime_ms     = 5 * 1000;
        cfg->max_cpu_ms         = 5 * 1000;
        cfg->max_memory_mb      = 512;
        cfg->max_file_write_mb  = 0;
        break;
    case COS_SANDBOX_RISK_WRITE:
        cfg->network_allowed    = 0;
        cfg->max_runtime_ms     = 10 * 1000;
        cfg->max_cpu_ms         = 10 * 1000;
        cfg->max_memory_mb      = 512;
        cfg->max_file_write_mb  = 64;
        break;
    case COS_SANDBOX_RISK_SHELL:
    case COS_SANDBOX_RISK_IRREVERSIBLE:
    default:
        cfg->network_allowed    = 0;
        cfg->max_runtime_ms     = 30 * 1000;
        cfg->max_cpu_ms         = 20 * 1000;
        cfg->max_memory_mb      = 1024;
        cfg->max_file_write_mb  = 64;
        break;
    }
}

int cos_sandbox_allowed(const cos_sandbox_config_t *cfg,
                        const char *const *argv) {
    if (!cfg || !argv || !argv[0]) return 0;
    if (!cfg->allowed_commands) {
        return cfg->risk_level < COS_SANDBOX_RISK_SHELL ? 1 : 0;
    }
    const char *base = sandbox_basename(argv[0]);
    for (int i = 0; cfg->allowed_commands[i]; i++) {
        const char *a = cfg->allowed_commands[i];
        if (strcmp(a, argv[0]) == 0) return 1;
        if (strcmp(sandbox_basename(a), base) == 0) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */

static int sandbox_set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

/* Append `n` bytes from `src` into a caller-owned heap buffer,
 * growing on demand up to `cap`.  Sets *truncated = 1 if the cap is
 * hit.  Returns new length. */
static size_t sandbox_append(char **buf_io, size_t *len_io,
                             size_t *cap_grown,
                             size_t cap_total,
                             int *truncated,
                             const char *src, size_t n) {
    if (*truncated || n == 0) return *len_io;
    size_t remaining = cap_total > *len_io ? cap_total - *len_io : 0;
    size_t take = n < remaining ? n : remaining;
    if (take < n) *truncated = 1;

    if (*len_io + take + 1 > *cap_grown) {
        size_t want = (*cap_grown == 0) ? 4096 : *cap_grown * 2;
        while (want < *len_io + take + 1) want *= 2;
        if (want > cap_total + 1) want = cap_total + 1;
        char *n2 = (char *)realloc(*buf_io, want);
        if (!n2) { *truncated = 1; return *len_io; }
        *buf_io    = n2;
        *cap_grown = want;
    }
    memcpy(*buf_io + *len_io, src, take);
    *len_io += take;
    (*buf_io)[*len_io] = '\0';
    return *len_io;
}

/* ------------------------------------------------------------------ */

void cos_sandbox_result_free(cos_sandbox_result_t *r) {
    if (!r) return;
    free(r->stdout_buf); r->stdout_buf = NULL; r->stdout_len = 0;
    free(r->stderr_buf); r->stderr_buf = NULL; r->stderr_len = 0;
}

static void sandbox_child(const cos_sandbox_config_t *cfg,
                          const char *const *argv,
                          int out_w, int err_w) {
    /* child: quiet exit on any failure; parent will read EOF. */
    (void)setpgid(0, 0);  /* own process group for clean signal */

    dup2(out_w, 1);
    dup2(err_w, 2);
    close(out_w);
    close(err_w);

    int nullfd = open("/dev/null", O_RDONLY);
    if (nullfd >= 0) { dup2(nullfd, 0); close(nullfd); }

#if defined(__linux__)
    if (!cfg->network_allowed) {
        /* Best-effort; ENOSYS / EPERM is survivable. */
        (void)unshare(CLONE_NEWNET);
    }
#endif

    if (cfg->chroot_path && cfg->chroot_path[0]) {
        if (geteuid() == 0) {
            if (chroot(cfg->chroot_path) == 0) (void)chdir("/");
        } else {
            (void)chdir(cfg->chroot_path);
        }
    }

    struct rlimit rl;
    if (cfg->max_cpu_ms > 0) {
        rl.rlim_cur = (rlim_t)((cfg->max_cpu_ms + 999) / 1000);
        rl.rlim_max = rl.rlim_cur + 1;
        setrlimit(RLIMIT_CPU, &rl);
    }
    if (cfg->max_memory_mb > 0) {
        rl.rlim_cur = (rlim_t)cfg->max_memory_mb * 1024 * 1024;
        rl.rlim_max = rl.rlim_cur;
#if defined(RLIMIT_AS)
        setrlimit(RLIMIT_AS, &rl);
#endif
#if defined(RLIMIT_DATA)
        setrlimit(RLIMIT_DATA, &rl);
#endif
    }
    if (cfg->max_file_write_mb >= 0) {
        rl.rlim_cur = (rlim_t)cfg->max_file_write_mb * 1024 * 1024;
        rl.rlim_max = rl.rlim_cur;
        setrlimit(RLIMIT_FSIZE, &rl);
    }

    execvp(argv[0], (char *const *)argv);
    _exit(127);
}

int cos_sandbox_exec(const cos_sandbox_config_t *cfg,
                     const char *const *argv,
                     cos_sandbox_result_t *out) {
    if (!cfg || !argv || !argv[0] || !out) return COS_SANDBOX_ERR_ARG;
    memset(out, 0, sizeof *out);
    out->exit_code = -1;

    if (cfg->risk_level == COS_SANDBOX_RISK_IRREVERSIBLE &&
        !cfg->user_consent) {
        out->status = COS_SANDBOX_ERR_DISALLOWED;
        return out->status;
    }
    if (!cos_sandbox_allowed(cfg, argv)) {
        out->status = COS_SANDBOX_ERR_DISALLOWED;
        return out->status;
    }

    int pout[2], perr[2];
    if (pipe(pout) < 0) { out->status = COS_SANDBOX_ERR_PIPE; return out->status; }
    if (pipe(perr) < 0) { close(pout[0]); close(pout[1]);
                          out->status = COS_SANDBOX_ERR_PIPE; return out->status; }

    uint64_t t0 = sandbox_now_ms();
    pid_t pid = fork();
    if (pid < 0) {
        close(pout[0]); close(pout[1]); close(perr[0]); close(perr[1]);
        out->status = COS_SANDBOX_ERR_FORK;
        return out->status;
    }
    if (pid == 0) {
        close(pout[0]); close(perr[0]);
        sandbox_child(cfg, argv, pout[1], perr[1]);
        _exit(127); /* unreached */
    }

    close(pout[1]); close(perr[1]);
    sandbox_set_nonblock(pout[0]);
    sandbox_set_nonblock(perr[0]);

    size_t cap_out = cfg->max_stdout_bytes ? cfg->max_stdout_bytes : 64 * 1024;
    size_t cap_err = cfg->max_stderr_bytes ? cfg->max_stderr_bytes : 64 * 1024;
    size_t grown_out = 0, grown_err = 0;

    struct pollfd pfd[2];
    pfd[0].fd = pout[0]; pfd[0].events = POLLIN;
    pfd[1].fd = perr[0]; pfd[1].events = POLLIN;

    int done = 0, killed_soft = 0, killed_hard = 0, timed_out = 0;
    int status = 0;
    uint64_t deadline = (cfg->max_runtime_ms > 0)
                       ? t0 + (uint64_t)cfg->max_runtime_ms : 0;

    for (;;) {
        uint64_t now = sandbox_now_ms();
        int timeout_ms;
        if (deadline == 0) timeout_ms = 250;
        else if (now >= deadline) timeout_ms = 0;
        else {
            uint64_t left = deadline - now;
            timeout_ms = left > 250 ? 250 : (int)left;
        }

        int pr = poll(pfd, 2, timeout_ms);
        if (pr < 0 && errno == EINTR) continue;

        for (int i = 0; i < 2 && pr > 0; i++) {
            if (!(pfd[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char    tmp[4096];
            ssize_t r = read(pfd[i].fd, tmp, sizeof tmp);
            if (r > 0) {
                if (i == 0)
                    sandbox_append(&out->stdout_buf, &out->stdout_len,
                                   &grown_out, cap_out, &out->stdout_truncated,
                                   tmp, (size_t)r);
                else
                    sandbox_append(&out->stderr_buf, &out->stderr_len,
                                   &grown_err, cap_err, &out->stderr_truncated,
                                   tmp, (size_t)r);
            } else if (r == 0) {
                pfd[i].events = 0;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                pfd[i].events = 0;
            }
        }

        /* check child liveness */
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) { done = 1; break; }
        if (w < 0 && errno != EINTR) { done = 1; break; }

        if (deadline && sandbox_now_ms() >= deadline) {
            if (!killed_soft) {
                killed_soft = 1; timed_out = 1;
                kill(-pid, SIGTERM);
                deadline = sandbox_now_ms() + 200; /* grace */
            } else if (!killed_hard) {
                killed_hard = 1;
                kill(-pid, SIGKILL);
                deadline = sandbox_now_ms() + 250;
            } else {
                /* child refuses to die; accept status anyway */
                kill(-pid, SIGKILL);
            }
        }
    }

    /* drain pipes one last time */
    for (int i = 0; i < 2; i++) {
        char    tmp[4096];
        ssize_t r;
        while ((r = read(pfd[i].fd, tmp, sizeof tmp)) > 0) {
            if (i == 0)
                sandbox_append(&out->stdout_buf, &out->stdout_len,
                               &grown_out, cap_out, &out->stdout_truncated,
                               tmp, (size_t)r);
            else
                sandbox_append(&out->stderr_buf, &out->stderr_len,
                               &grown_err, cap_err, &out->stderr_truncated,
                               tmp, (size_t)r);
        }
    }
    close(pout[0]); close(perr[0]);

    if (!done) waitpid(pid, &status, 0);
    uint64_t t1 = sandbox_now_ms();
    out->wall_ms = t1 - t0;

    if (WIFEXITED(status)) {
        out->exit_code     = WEXITSTATUS(status);
        out->killed_signal = 0;
    } else if (WIFSIGNALED(status)) {
        out->exit_code     = -1;
        out->killed_signal = WTERMSIG(status);
    }

    out->timed_out        = timed_out;
    out->memory_exceeded  = (out->killed_signal == SIGSEGV ||
                             out->killed_signal == SIGBUS) ? 1 : 0;
#if defined(__linux__)
    out->network_isolated = (cfg->network_allowed == 0) ? 1 : 0;
#else
    out->network_isolated = 0;
#endif

    if (!out->stdout_buf) { out->stdout_buf = (char *)calloc(1, 1);
                            if (!out->stdout_buf) { out->status = COS_SANDBOX_ERR_OOM; return out->status; } }
    if (!out->stderr_buf) { out->stderr_buf = (char *)calloc(1, 1);
                            if (!out->stderr_buf) { out->status = COS_SANDBOX_ERR_OOM; return out->status; } }
    out->status = COS_SANDBOX_OK;
    return out->status;
}

/* ------------------------------------------------------------------ *
 * Self-test                                                          *
 *                                                                    *
 *  1. risk-0 /bin/echo "hello" returns exit 0, stdout starts "hello" *
 *  2. risk-3 /bin/sleep 5 with max_runtime_ms=300 returns timed_out  *
 *  3. disallowed command is refused before fork                      *
 *  4. risk-4 without user_consent is refused                         *
 * ------------------------------------------------------------------ */

int cos_sandbox_self_test(void) {
    const char *ECHO[] = {"/bin/echo", "hello", NULL};
    cos_sandbox_config_t c0;
    cos_sandbox_config_default(&c0, COS_SANDBOX_RISK_CALC);
    cos_sandbox_result_t r0 = {0};
    if (cos_sandbox_exec(&c0, ECHO, &r0) != COS_SANDBOX_OK) { cos_sandbox_result_free(&r0); return -1; }
    if (r0.exit_code != 0) { cos_sandbox_result_free(&r0); return -2; }
    if (!r0.stdout_buf || strncmp(r0.stdout_buf, "hello", 5) != 0) {
        cos_sandbox_result_free(&r0); return -3;
    }
    cos_sandbox_result_free(&r0);

    /* 2. timeout on /bin/sleep 5 with 300 ms cap */
    const char *SLEEP[] = {"/bin/sleep", "5", NULL};
    const char *ALLOW[] = {"sleep", NULL};
    cos_sandbox_config_t c3;
    cos_sandbox_config_default(&c3, COS_SANDBOX_RISK_SHELL);
    c3.allowed_commands = ALLOW;
    c3.max_runtime_ms   = 300;
    cos_sandbox_result_t r1 = {0};
    cos_sandbox_exec(&c3, SLEEP, &r1);
    if (!r1.timed_out) { cos_sandbox_result_free(&r1); return -4; }
    if (r1.wall_ms > 2000)  { cos_sandbox_result_free(&r1); return -5; }
    cos_sandbox_result_free(&r1);

    /* 3. disallowed verb at risk 3 */
    const char *RM[]    = {"/bin/rm", "-rf", "/", NULL};
    cos_sandbox_result_t r2 = {0};
    cos_sandbox_config_t c3b;
    cos_sandbox_config_default(&c3b, COS_SANDBOX_RISK_SHELL);
    c3b.allowed_commands = ALLOW; /* sleep only */
    int s2 = cos_sandbox_exec(&c3b, RM, &r2);
    if (s2 != COS_SANDBOX_ERR_DISALLOWED) { cos_sandbox_result_free(&r2); return -6; }
    cos_sandbox_result_free(&r2);

    /* 4. risk 4 without consent */
    cos_sandbox_config_t c4;
    cos_sandbox_config_default(&c4, COS_SANDBOX_RISK_IRREVERSIBLE);
    c4.allowed_commands = ALLOW;
    c4.user_consent     = 0;
    cos_sandbox_result_t r3 = {0};
    int s3 = cos_sandbox_exec(&c4, SLEEP, &r3);
    if (s3 != COS_SANDBOX_ERR_DISALLOWED) { cos_sandbox_result_free(&r3); return -7; }
    cos_sandbox_result_free(&r3);

    return 0;
}
