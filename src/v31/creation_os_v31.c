/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v31 — optional “purge lab” wrapper (POSIX)
 *
 * Intent: do NOT rewrite BitNet kernels. Optionally spawn an upstream-built
 * inference binary and add Creation OS σ-style telemetry on signals we control.
 *
 * Non-goals:
 * - not merge-gate
 * - not a weight download bootstrap
 * - not a claim that chat works in CI without external artifacts
 */
#if defined(_WIN32)
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "creation_os_v31: POSIX-only.\n");
    return 2;
}
#else
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

static float sigma_logit_entropy(const float *logprobs, int n)
{
    if (!logprobs || n <= 1)
        return 0.0f;
    float max_lp = -1e9f;
    for (int i = 0; i < n; i++)
        if (logprobs[i] > max_lp)
            max_lp = logprobs[i];
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
        sum += expf(logprobs[i] - max_lp);
    if (!(sum > 0.0f))
        return 0.0f;
    float h = 0.0f;
    for (int i = 0; i < n; i++) {
        float p = expf(logprobs[i] - max_lp) / sum;
        if (p > 1e-10f)
            h -= p * logf(p);
    }
    float denom = logf((float)n);
    if (!(denom > 0.0f))
        return 0.0f;
    return h / denom;
}

static float sigma_top_margin_from_logprobs(const float *logprobs, int n)
{
    if (!logprobs || n < 2)
        return 0.0f;
    float m1 = -1e9f, m2 = -1e9f;
    for (int i = 0; i < n; i++) {
        float v = logprobs[i];
        if (v > m1) {
            m2 = m1;
            m1 = v;
        } else if (v > m2) {
            m2 = v;
        }
    }
    float gap = m1 - m2;
    return 1.0f / (1.0f + gap);
}

typedef struct {
    int in_fd;
    int out_fd;
    pid_t pid;
} bitnet_proc_t;

static void bitnet_shutdown(bitnet_proc_t *p)
{
    if (!p)
        return;
    if (p->in_fd >= 0)
        close(p->in_fd);
    if (p->out_fd >= 0)
        close(p->out_fd);
    if (p->pid > 0) {
        int st = 0;
        (void)waitpid(p->pid, &st, 0);
    }
    free(p);
}

static bitnet_proc_t *bitnet_spawn(const char *cli_path, const char *model_path)
{
    int in_pipe[2] = {-1, -1};
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        perror("pipe");
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return NULL;
    }

    if (pid == 0) {
        /* child */
        if (dup2(in_pipe[0], STDIN_FILENO) < 0 || dup2(out_pipe[1], STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(127);
        }
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);

        /* Keep stderr attached for upstream logs. */
        execlp(cli_path, cli_path, "-m", model_path, "-n", "64", (char *)NULL);
        perror("execlp");
        _exit(127);
    }

    /* parent */
    close(in_pipe[0]);
    close(out_pipe[1]);

    bitnet_proc_t *p = (bitnet_proc_t *)calloc(1, sizeof(*p));
    if (!p) {
        close(in_pipe[1]);
        close(out_pipe[0]);
        return NULL;
    }
    p->in_fd = in_pipe[1];
    p->out_fd = out_pipe[0];
    p->pid = pid;
    return p;
}

static void bitnet_send_line(bitnet_proc_t *p, const char *s)
{
    if (!p || p->in_fd < 0 || !s)
        return;
    /* NOTE: upstream CLIs vary; this is a lab hook, not a stable protocol. */
    dprintf(p->in_fd, "%s\n", s);
}

static int self_test_math(void)
{
    float lp[] = {-0.5f, -1.2f, -2.0f, -3.5f, -4.0f};
    float h = sigma_logit_entropy(lp, (int)(sizeof(lp) / sizeof(lp[0])));
    if (h < 0.0f || h > 1.0f) {
        fprintf(stderr, "[v31 self-test] FAIL entropy out of range: %f\n", h);
        return 1;
    }
    float m = sigma_top_margin_from_logprobs(lp, (int)(sizeof(lp) / sizeof(lp[0])));
    if (!(m >= 0.0f && m <= 1.0f)) {
        fprintf(stderr, "[v31 self-test] FAIL margin out of range: %f\n", m);
        return 1;
    }
    fprintf(stderr, "[v31 self-test] OK math (entropy=%.6f margin=%.6f)\n", h, m);
    return 0;
}

static int self_test_external_optional(const char *cli, const char *model)
{
    if (!cli || !*cli || !model || !*model)
        return 0;
    if (access(cli, X_OK) != 0) {
        fprintf(stderr, "[v31 self-test] SKIP external cli not executable: %s\n", cli);
        return 0;
    }
    if (access(model, R_OK) != 0) {
        fprintf(stderr, "[v31 self-test] SKIP model not readable: %s\n", model);
        return 0;
    }
    fprintf(stderr, "[v31 self-test] NOTE external spawn is best-effort; upstream flags differ.\n");
    return 0;
}

static void print_help(const char *argv0)
{
    printf("usage: %s [--self-test] [--help]\n", argv0);
    printf("\n");
    printf("Optional POSIX lab wrapper around an upstream inference binary.\n");
    printf("\n");
    printf("Environment:\n");
    printf("  COS_BITNET_CLI   path to upstream llama-cli-like binary (optional)\n");
    printf("  COS_BITNET_MODEL path to GGUF weights (optional)\n");
    printf("\n");
    printf("This binary is NOT part of merge-gate.\n");
}

int main(int argc, char **argv)
{
    const char *cli = getenv("COS_BITNET_CLI");
    const char *model = getenv("COS_BITNET_MODEL");

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test"))
            return (self_test_math() == 0 && self_test_external_optional(cli, model) == 0) ? 0 : 2;
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_help(argv[0]);
            return 0;
        }
    }

    if (!cli || !*cli || !model || !*model) {
        fprintf(stderr, "creation_os_v31: set COS_BITNET_CLI and COS_BITNET_MODEL to run chat mode.\n");
        fprintf(stderr, "Try: %s --self-test\n", argv[0]);
        return 2;
    }

    bitnet_proc_t *bn = bitnet_spawn(cli, model);
    if (!bn) {
        fprintf(stderr, "FAIL: spawn\n");
        return 2;
    }

    fprintf(stderr, "creation_os_v31: spawned upstream process (lab). Type lines; Ctrl-D to exit.\n");
    char line[4096];
    char out[65536];
    while (fgets(line, sizeof(line), stdin)) {
        bitnet_send_line(bn, line);
        ssize_t n = read(bn->out_fd, out, sizeof(out) - 1);
        if (n > 0) {
            out[n] = '\0';
            fputs(out, stdout);
            fflush(stdout);
        }
    }

    bitnet_shutdown(bn);
    return 0;
}
#endif
