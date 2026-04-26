/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "cross_model_sigma.h"

#include "bitnet_server.h"
#include "text_similarity.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/wait.h>

static int cm_parallel_on(void)
{
    const char *e = getenv("COS_CROSS_MODEL_SIGMA_PARALLEL");
    return (e == NULL || e[0] != '0' || e[1] != '\0');
}

static const char *cm_model(int k)
{
    static const char *def[3] = { "gemma3:4b", "llama3.2:3b", "qwen3.5:4b" };
    const char        *e        = NULL;
    if (k == 0)
        e = getenv("COS_CROSS_MODEL_A");
    else if (k == 1)
        e = getenv("COS_CROSS_MODEL_B");
    else
        e = getenv("COS_CROSS_MODEL_C");
    if (e != NULL && e[0] != '\0')
        return e;
    return def[k];
}

static int cm_pipe_read(int rd, char **out)
{
    *out = NULL;
    uint32_t n = 0;
    if (read(rd, &n, sizeof n) != (ssize_t)sizeof n)
        return -1;
    if (n == 0u)
        return 0;
    if (n > 4000000u)
        n = 4000000u;
    char *b = malloc((size_t)n + 1u);
    if (b == NULL)
        return -1;
    size_t g = 0;
    while (g < (size_t)n) {
        ssize_t w = read(rd, b + g, (size_t)n - g);
        if (w <= 0) {
            free(b);
            return -1;
        }
        g += (size_t)w;
    }
    b[g] = '\0';
    *out = b;
    return 0;
}

static pid_t cm_fork_one(int pair[2], int port, const char *prompt,
                         const char *sysp, const char *model, int max_tok)
{
    if (pipe(pair) != 0)
        return (pid_t)-1;
    pid_t pid = fork();
    if (pid < 0) {
        close(pair[0]);
        close(pair[1]);
        return (pid_t)-1;
    }
    if (pid == 0) {
        close(pair[0]);
        char *r = cos_bitnet_query_model(port, prompt, sysp, model, 0.7f,
                                         max_tok);
        const char *s = (r != NULL) ? r : "";
        uint32_t    n = (uint32_t)strlen(s);
        (void)write(pair[1], &n, sizeof n);
        if (n > 0u)
            (void)write(pair[1], s, (size_t)n);
        free(r);
        close(pair[1]);
        _exit(0);
    }
    close(pair[1]);
    return pid;
}

float cos_cross_model_sigma(int port, const char *prompt,
                            const char *system_prompt)
{
    if (prompt == NULL || prompt[0] == '\0')
        return 0.5f;

    const char *sysp = (system_prompt != NULL && system_prompt[0] != '\0')
                           ? system_prompt
                           : "Answer briefly.";
    enum { k_np = 60 };
    char *       r0 = NULL, *r1 = NULL, *r2 = NULL;
    int          ok = 0;

    if (cm_parallel_on()) {
        int   p0[2], p1[2], p2[2];
        pid_t c0 = cm_fork_one(p0, port, prompt, sysp, cm_model(0), k_np);
        if (c0 < (pid_t)0) {
            ok = -1;
        } else {
            pid_t c1 = cm_fork_one(p1, port, prompt, sysp, cm_model(1), k_np);
            if (c1 < (pid_t)0) {
                (void)waitpid(c0, NULL, 0);
                close(p0[0]);
                ok = -1;
            } else {
                pid_t c2 = cm_fork_one(p2, port, prompt, sysp, cm_model(2),
                                       k_np);
                if (c2 < (pid_t)0) {
                    (void)waitpid(c0, NULL, 0);
                    (void)waitpid(c1, NULL, 0);
                    close(p0[0]);
                    close(p1[0]);
                    ok = -1;
                } else {
                    int s0 = 0, s1 = 0, s2 = 0;
                    (void)waitpid(c0, &s0, 0);
                    (void)waitpid(c1, &s1, 0);
                    (void)waitpid(c2, &s2, 0);
                    if (!(WIFEXITED(s0) && WEXITSTATUS(s0) == 0
                          && WIFEXITED(s1) && WEXITSTATUS(s1) == 0
                          && WIFEXITED(s2) && WEXITSTATUS(s2) == 0)
                        || cm_pipe_read(p0[0], &r0) != 0
                        || cm_pipe_read(p1[0], &r1) != 0
                        || cm_pipe_read(p2[0], &r2) != 0) {
                        free(r0);
                        free(r1);
                        free(r2);
                        r0 = r1 = r2 = NULL;
                        ok = -1;
                    }
                    close(p0[0]);
                    close(p1[0]);
                    close(p2[0]);
                }
            }
        }
    } else {
        r0 = cos_bitnet_query_model(port, prompt, sysp, cm_model(0), 0.7f,
                                    k_np);
        r1 = cos_bitnet_query_model(port, prompt, sysp, cm_model(1), 0.7f,
                                    k_np);
        r2 = cos_bitnet_query_model(port, prompt, sysp, cm_model(2), 0.7f,
                                    k_np);
        if (r0 == NULL || r1 == NULL || r2 == NULL || r0[0] == '\0'
            || r1[0] == '\0' || r2[0] == '\0')
            ok = -1;
    }

    if (ok != 0 || r0 == NULL || r1 == NULL || r2 == NULL) {
        free(r0);
        free(r1);
        free(r2);
        return 0.85f;
    }

    char s0[256], s1[256], s2[256];
    cos_text_first_sentence(r0, s0, sizeof s0);
    cos_text_first_sentence(r1, s1, sizeof s1);
    cos_text_first_sentence(r2, s2, sizeof s2);
    float j01 = cos_text_jaccard(s0, s1);
    float j02 = cos_text_jaccard(s0, s2);
    float j12 = cos_text_jaccard(s1, s2);
    if (j01 < 0.f)
        j01 = 0.f;
    if (j01 > 1.f)
        j01 = 1.f;
    if (j02 < 0.f)
        j02 = 0.f;
    if (j02 > 1.f)
        j02 = 1.f;
    if (j12 < 0.f)
        j12 = 0.f;
    if (j12 > 1.f)
        j12 = 1.f;
    float mean = (j01 + j02 + j12) / 3.0f;
    float sig  = 1.0f - mean;
    if (sig < 0.f)
        sig = 0.f;
    if (sig > 1.f)
        sig = 1.f;
    free(r0);
    free(r1);
    free(r2);
    return sig;
}
