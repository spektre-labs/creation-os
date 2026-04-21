/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bitnet_spawn.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
int cos_bitnet_spawn_capture(const char *exe, const char *prompt, char *out, size_t cap)
{
    (void)exe;
    (void)prompt;
    (void)out;
    (void)cap;
    return -1;
}

int cos_bitnet_spawn_argv_capture(char *const argv[], char *out, size_t cap, int merge_stderr)
{
    (void)argv;
    (void)out;
    (void)cap;
    (void)merge_stderr;
    return -1;
}
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int append_env_argv(char **argv, int *argc, int max, const char *envname)
{
    const char *v = getenv(envname);
    if (!v || !v[0])
        return 0;
    if (*argc + 1 >= max)
        return -1;
    argv[(*argc)++] = (char *)v;
    return 0;
}

static void strip_ansi_inplace(char *s)
{
    char *w = s;
    for (const char *r = s; *r != '\0';) {
        if (*r == '\033' && r[1] == '[') {
            r += 2;
            while (*r != '\0' && *r != 'm')
                ++r;
            if (*r == 'm')
                ++r;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static int read_all(int fd, char *buf, size_t cap)
{
    size_t got = 0;
    while (got + 1u < cap) {
        ssize_t n = read(fd, buf + got, cap - 1u - got);
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        got += (size_t)n;
    }
    buf[got] = '\0';
    return 0;
}

static int run_spawn(char *const argv[], const char *stdin_payload, size_t stdin_len,
                     int merge_stderr, char *out, size_t cap)
{
    if (!argv || !argv[0] || !argv[0][0] || !out || cap < 2u)
        return -1;

    int pout[2] = {-1, -1};
    int pin[2] = {-1, -1};
    posix_spawn_file_actions_t fa;
    pid_t pid = 0;

    if (pipe(pout) != 0)
        return -1;

    if (posix_spawn_file_actions_init(&fa) != 0) {
        close(pout[0]);
        close(pout[1]);
        return -1;
    }

    (void)posix_spawn_file_actions_addclose(&fa, pout[0]);
    (void)posix_spawn_file_actions_adddup2(&fa, pout[1], STDOUT_FILENO);
    if (merge_stderr)
        (void)posix_spawn_file_actions_adddup2(&fa, pout[1], STDERR_FILENO);
    (void)posix_spawn_file_actions_addclose(&fa, pout[1]);

    const int use_stdin = (stdin_payload != NULL && stdin_len > 0u);
    if (use_stdin) {
        if (pipe(pin) != 0)
            goto fail;
        (void)posix_spawn_file_actions_adddup2(&fa, pin[0], STDIN_FILENO);
        (void)posix_spawn_file_actions_addclose(&fa, pin[0]);
        (void)posix_spawn_file_actions_addclose(&fa, pin[1]);
    }

    if (posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ) != 0)
        goto fail;

    close(pout[1]);
    pout[1] = -1;

    if (use_stdin) {
        close(pin[0]);
        pin[0] = -1;
        size_t w = 0;
        while (w < stdin_len) {
            ssize_t n = write(pin[1], stdin_payload + w, stdin_len - w);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            w += (size_t)n;
        }
        close(pin[1]);
        pin[1] = -1;
    }

    if (read_all(pout[0], out, cap) != 0) {
        (void)waitpid(pid, NULL, 0);
        goto fail_after_spawn;
    }
    strip_ansi_inplace(out);
    close(pout[0]);
    pout[0] = -1;

    int st = 0;
    if (waitpid(pid, &st, 0) < 0) {
        (void)posix_spawn_file_actions_destroy(&fa);
        return -1;
    }
    (void)posix_spawn_file_actions_destroy(&fa);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
        return -1;
    return 0;

fail_after_spawn:
    if (pout[0] >= 0)
        close(pout[0]);
    if (pout[1] >= 0)
        close(pout[1]);
    if (pin[0] >= 0)
        close(pin[0]);
    if (pin[1] >= 0)
        close(pin[1]);
    (void)posix_spawn_file_actions_destroy(&fa);
    return -1;

fail:
    (void)posix_spawn_file_actions_destroy(&fa);
    if (pout[0] >= 0)
        close(pout[0]);
    if (pout[1] >= 0)
        close(pout[1]);
    if (pin[0] >= 0)
        close(pin[0]);
    if (pin[1] >= 0)
        close(pin[1]);
    return -1;
}

int cos_bitnet_spawn_argv_capture(char *const argv[], char *out, size_t cap, int merge_stderr)
{
    return run_spawn(argv, NULL, 0u, merge_stderr != 0, out, cap);
}

int cos_bitnet_spawn_capture(const char *exe, const char *prompt, char *out, size_t cap)
{
    if (!exe || !exe[0] || !prompt || !out || cap < 2u)
        return -1;

    int pin_dummy[2] = {-1, -1};
    (void)pin_dummy;

    const int use_stdin = getenv("CREATION_OS_BITNET_STDIN") && getenv("CREATION_OS_BITNET_STDIN")[0] == '1';

    char *argv[40];
    int argc = 0;
    const int max = (int)(sizeof(argv) / sizeof(argv[0]) - 1);
    argv[argc++] = (char *)exe;

    const char *model = getenv("CREATION_OS_BITNET_MODEL");
    if (model != NULL && model[0] != '\0') {
        const char *perf = getenv("CREATION_OS_BITNET_PERF");
        if (perf == NULL || perf[0] != '1') {
            if (argc + 1 >= max)
                return -1;
            argv[argc++] = "--no-perf";
        }
        if (argc + 2 >= max)
            return -1;
        argv[argc++] = "-m";
        argv[argc++] = (char *)model;

        char np_buf[32];
        {
            const char *np = getenv("CREATION_OS_BITNET_N_PREDICT");
            if (np != NULL && np[0] != '\0')
                (void)snprintf(np_buf, sizeof np_buf, "%s", np);
            else
                (void)snprintf(np_buf, sizeof np_buf, "128");
        }
        if (argc + 2 >= max)
            return -1;
        argv[argc++] = "-n";
        argv[argc++] = np_buf;

        const char *threads = getenv("CREATION_OS_BITNET_THREADS");
        if (threads != NULL && threads[0] != '\0') {
            if (argc + 2 >= max)
                return -1;
            argv[argc++] = "-t";
            argv[argc++] = (char *)threads;
        }
        const char *ngl = getenv("CREATION_OS_BITNET_NGL");
        if (ngl != NULL && ngl[0] != '\0') {
            if (argc + 2 >= max)
                return -1;
            argv[argc++] = "-ngl";
            argv[argc++] = (char *)ngl;
        }
    }

    for (int i = 0; i < 10; i++) {
        char name[40];
        (void)snprintf(name, sizeof name, "CREATION_OS_BITNET_ARG%d", i);
        if (append_env_argv(argv, &argc, max, name) != 0)
            return -1;
    }
    if (!use_stdin) {
        if (argc + 3 >= max)
            return -1;
        argv[argc++] = "-p";
        argv[argc++] = (char *)prompt;
    }
    argv[argc] = NULL;

    if (use_stdin)
        return run_spawn(argv, prompt, strlen(prompt), 0, out, cap);
    return run_spawn(argv, NULL, 0u, 0, out, cap);
}
#endif
