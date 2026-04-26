/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "semantic_sigma.h"

#include "bitnet_server.h"
#include "text_similarity.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

#define COS_SS_MAX_SAMPLES 8

static float ss_clamp01(float x) {
    if (x < 0.0f)
        return 0.0f;
    if (x > 1.0f)
        return 1.0f;
    return x;
}

static float ss_env_float(const char *name, float def) {
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
        return def;
    float x = (float)strtod(v, NULL);
    if (x < 0.0f)
        return def;
    if (x > 1.0f)
        return 1.0f;
    return x;
}

static void ss_snap_env(int port, const char *model, char *buf_port,
                        char *buf_model, const char **old_port,
                        const char **old_bcm, const char **old_oll) {
    *old_port = getenv("COS_BITNET_SERVER_PORT");
    *old_bcm  = getenv("COS_BITNET_CHAT_MODEL");
    *old_oll  = getenv("COS_OLLAMA_MODEL");
    if (port > 0) {
        snprintf(buf_port, 32, "%d", port);
        setenv("COS_BITNET_SERVER_PORT", buf_port, 1);
    }
    if (model != NULL && model[0] != '\0') {
        snprintf(buf_model, 512, "%s", model);
        setenv("COS_BITNET_CHAT_MODEL", buf_model, 1);
        setenv("COS_OLLAMA_MODEL", buf_model, 1);
    }
}

static void ss_restore_env(int port_set, int model_set, const char *old_port,
                           const char *old_bcm, const char *old_oll) {
    if (port_set) {
        if (old_port != NULL)
            setenv("COS_BITNET_SERVER_PORT", old_port, 1);
        else
            unsetenv("COS_BITNET_SERVER_PORT");
    }
    if (model_set) {
        if (old_bcm != NULL)
            setenv("COS_BITNET_CHAT_MODEL", old_bcm, 1);
        else
            unsetenv("COS_BITNET_CHAT_MODEL");
        if (old_oll != NULL)
            setenv("COS_OLLAMA_MODEL", old_oll, 1);
        else
            unsetenv("COS_OLLAMA_MODEL");
    }
}

static int ss_uf_find(int *parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i         = parent[i];
    }
    return i;
}

static void ss_uf_union(int *parent, int a, int b) {
    a = ss_uf_find(parent, a);
    b = ss_uf_find(parent, b);
    if (a != b)
        parent[b] = a;
}

static int ss_cluster_count_from_sims(float s01, float s02, float s12,
                                        float tau) {
    int parent[3] = { 0, 1, 2 };
    if (s01 >= tau)
        ss_uf_union(parent, 0, 1);
    if (s02 >= tau)
        ss_uf_union(parent, 0, 2);
    if (s12 >= tau)
        ss_uf_union(parent, 1, 2);
    int roots[3], nr = 0;
    for (int i = 0; i < 3; i++) {
        int r = ss_uf_find(parent, i);
        int dup = 0;
        for (int k = 0; k < nr; k++) {
            if (roots[k] == r) {
                dup = 1;
                break;
            }
        }
        if (!dup)
            roots[nr++] = r;
    }
    return nr > 0 ? nr : 1;
}

int cos_semantic_sigma_compute_texts(const char *t0, const char *t1,
                                       const char *t2,
                                       cos_semantic_sigma_result *out) {
    if (out == NULL)
        return -1;
    memset(out, 0, sizeof(*out));
    out->n_samples = 3;

    const char *a = (t0 != NULL) ? t0 : "";
    const char *b = (t1 != NULL) ? t1 : "";
    const char *c = (t2 != NULL) ? t2 : "";

    float s01 = cos_text_jaccard(a, b);
    float s02 = cos_text_jaccard(a, c);
    float s12 = cos_text_jaccard(b, c);
    if (s01 < 0.0f)
        s01 = 0.0f;
    if (s01 > 1.0f)
        s01 = 1.0f;
    if (s02 < 0.0f)
        s02 = 0.0f;
    if (s02 > 1.0f)
        s02 = 1.0f;
    if (s12 < 0.0f)
        s12 = 0.0f;
    if (s12 > 1.0f)
        s12 = 1.0f;

    out->similarities[0] = s01;
    out->similarities[1] = s02;
    out->similarities[2] = s12;

    float mean_sim =
        (double)(s01 + s02 + s12) / 3.0f;
    out->sigma           = ss_clamp01(1.0f - mean_sim);
    float tau_c          = ss_env_float("COS_SEMANTIC_SIGMA_CLUSTER_SIM", 0.6f);
    out->n_clusters      = ss_cluster_count_from_sims(s01, s02, s12, tau_c);
    return 0;
}

static int ss_parallel_semantic_sigma(void)
{
    const char *e = getenv("COS_SEMANTIC_SIGMA_PARALLEL");
    return (e == NULL || e[0] != '0' || e[1] != '\0');
}

/** Parallel T=0.1 / T=1.5 pair via pthread (default off; fork is default). */
static int ss_use_pthread_semantic_sigma(void)
{
    const char *e = getenv("COS_SEMANTIC_SIGMA_USE_PTHREAD");
    return (e != NULL && e[0] == '1' && e[1] == '\0');
}

typedef struct {
    int         port;
    const char *prompt;
    const char *sysp;
    float       temp;
    int         max_tokens;
    char       *result;
} ss_inference_arg_t;

static void *ss_inference_thread(void *arg)
{
    ss_inference_arg_t *a = (ss_inference_arg_t *)arg;
    a->result =
        cos_bitnet_query_temp_with_options(a->port, a->prompt, a->sysp, a->temp,
                                           a->max_tokens);
    return NULL;
}

static int ss_pipe_read_dup(int rd, char **out)
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

static pid_t ss_fork_query_pipe(int pair[2], int port, const char *prompt,
                                const char *sysp, float temp, int max_tok)
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
        char *r =
            cos_bitnet_query_temp_with_options(port, prompt, sysp, temp, max_tok);
        uint32_t n =
            (r != NULL && r[0] != '\0') ? (uint32_t)strlen(r) : 0u;
        if (n > 4000000u)
            n = 4000000u;
        (void)write(pair[1], &n, sizeof n);
        if (n > 0u && r != NULL)
            (void)write(pair[1], r, n);
        free(r);
        close(pair[1]);
        _exit(0);
    }
    close(pair[1]);
    return pid;
}

static pid_t ss_fork_complete_pipe(int pair[2], const char *prompt,
                                   const char *sysp, float temp, int np,
                                   int seed)
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
        cos_bitnet_server_params_t par;
        memset(&par, 0, sizeof(par));
        par.system_prompt = sysp;
        par.temperature   = temp;
        par.n_predict       = np;
        par.seed            = seed;
        cos_bitnet_server_result_t br;
        memset(&br, 0, sizeof(br));
        const char *txt = "";
        if (cos_bitnet_server_complete(prompt, &par, &br) == 0
            && br.text != NULL)
            txt = br.text;
        uint32_t n = (uint32_t)strlen(txt);
        if (n > 4000000u)
            n = 4000000u;
        (void)write(pair[1], &n, sizeof n);
        if (n > 0u)
            (void)write(pair[1], txt, n);
        close(pair[1]);
        _exit(0);
    }
    close(pair[1]);
    return pid;
}

static int ss_default_temps(int n, float *out) {
    /* Wide spread (curl-style) for three independent samples. */
    static const float t3[3] = { 0.1f, 0.7f, 1.5f };
    if (n == 3) {
        memcpy(out, t3, sizeof(t3));
        return 0;
    }
    if (n < 2 || n > COS_SS_MAX_SAMPLES)
        return -1;
    for (int i = 0; i < n; i++)
        out[i] = 0.3f + (0.7f * (float)i / (float)(n > 1 ? (n - 1) : 1));
    return 0;
}

float cos_semantic_sigma_ex(const char *prompt, const char *system_prompt,
                            int port, const char *model, int n_samples,
                            const char *primary_answer,
                            cos_semantic_sigma_result *out) {
    cos_semantic_sigma_result local;
    if (out == NULL)
        out = &local;
    memset(out, 0, sizeof(*out));

    if (prompt == NULL || prompt[0] == '\0' || n_samples != 3) {
        out->sigma     = 0.5f;
        out->n_samples = n_samples;
        return out->sigma;
    }

    char        buf_port[32], buf_model[512];
    const char *old_port, *old_bcm, *old_oll;
    int         port_set  = (port > 0) ? 1 : 0;
    int         model_set = (model != NULL && model[0] != '\0') ? 1 : 0;
    ss_snap_env(port, model, buf_port, buf_model, &old_port, &old_bcm, &old_oll);

    const char *env_ntok = getenv("COS_SEMANTIC_SIGMA_MAX_TOKENS");
    int         max_tok  = (env_ntok != NULL && env_ntok[0] != '\0')
                             ? atoi(env_ntok)
                             : 100;
    if (max_tok < 16)
        max_tok = 16;
    if (max_tok > 512)
        max_tok = 512;

    const char *texts[3];
    char       *dup0 = NULL, *dup1 = NULL, *dup2 = NULL;
    int         ok = 0;

    static const char k_semantic_extra_sysp[] =
        "Give only the direct answer. Maximum one sentence. No explanation.";
    enum { k_semantic_extra_np = 60 };

    if (primary_answer != NULL && primary_answer[0] != '\0') {
        dup0 = strdup(primary_answer);
        if (dup0 == NULL)
            ok = -1;
        texts[0] = dup0;
        if (ok == 0 && ss_use_pthread_semantic_sigma()) {
            pthread_t        t1, t2;
            ss_inference_arg_t a1 = { port, prompt, k_semantic_extra_sysp, 0.1f,
                                      k_semantic_extra_np, NULL };
            ss_inference_arg_t a2 = { port, prompt, k_semantic_extra_sysp, 1.5f,
                                      k_semantic_extra_np, NULL };
            if (pthread_create(&t1, NULL, ss_inference_thread, &a1) != 0
                || pthread_create(&t2, NULL, ss_inference_thread, &a2) != 0) {
                ok = -1;
            } else {
                (void)pthread_join(t1, NULL);
                (void)pthread_join(t2, NULL);
                dup1 = a1.result;
                dup2 = a2.result;
                if (dup1 == NULL || dup1[0] == '\0' || dup2 == NULL
                    || dup2[0] == '\0') {
                    free(dup1);
                    dup1 = NULL;
                    free(dup2);
                    dup2 = NULL;
                    ok = -1;
                }
            }
        } else if (ok == 0 && ss_parallel_semantic_sigma()) {
            int   pr1[2], pr2[2];
            pid_t c1 = ss_fork_query_pipe(pr1, port, prompt, k_semantic_extra_sysp,
                                         0.1f, k_semantic_extra_np);
            if (c1 < (pid_t)0) {
                ok = -1;
            } else {
                pid_t c2 = ss_fork_query_pipe(
                    pr2, port, prompt, k_semantic_extra_sysp, 1.5f,
                    k_semantic_extra_np);
                if (c2 < (pid_t)0) {
                    (void)waitpid(c1, NULL, 0);
                    close(pr1[0]);
                    ok = -1;
                } else {
                    int st1 = 0, st2 = 0;
                    (void)waitpid(c1, &st1, 0);
                    (void)waitpid(c2, &st2, 0);
                    if (!(WIFEXITED(st1) && WEXITSTATUS(st1) == 0
                          && WIFEXITED(st2) && WEXITSTATUS(st2) == 0)
                        || ss_pipe_read_dup(pr1[0], &dup1) != 0
                        || ss_pipe_read_dup(pr2[0], &dup2) != 0
                        || dup1 == NULL || dup1[0] == '\0' || dup2 == NULL
                        || dup2[0] == '\0') {
                        free(dup1);
                        dup1 = NULL;
                        free(dup2);
                        dup2 = NULL;
                        ok = -1;
                    }
                    close(pr1[0]);
                    close(pr2[0]);
                }
            }
        } else if (ok == 0) {
            dup1 = cos_bitnet_query_temp_with_options(
                port, prompt, k_semantic_extra_sysp, 0.1f, k_semantic_extra_np);
            if (dup1 == NULL || dup1[0] == '\0')
                ok = -1;
            if (ok == 0) {
                dup2 = cos_bitnet_query_temp_with_options(
                    port, prompt, k_semantic_extra_sysp, 1.5f,
                    k_semantic_extra_np);
                if (dup2 == NULL || dup2[0] == '\0') {
                    free(dup1);
                    dup1 = NULL;
                    ok     = -1;
                }
            }
        }
        texts[1] = dup1 != NULL ? dup1 : "";
        texts[2] = dup2 != NULL ? dup2 : "";
    } else {
        float temps[3];
        if (ss_default_temps(3, temps) != 0) {
            ok = -1;
        } else if (ss_parallel_semantic_sigma()) {
            int   pr0[2], pr1[2], pr2[2];
            pid_t c0 = ss_fork_complete_pipe(pr0, prompt, k_semantic_extra_sysp,
                                             temps[0], k_semantic_extra_np,
                                             13001 + 0 * 17);
            if (c0 < (pid_t)0) {
                ok = -1;
            } else {
                pid_t c1 = ss_fork_complete_pipe(
                    pr1, prompt, k_semantic_extra_sysp, temps[1],
                    k_semantic_extra_np, 13001 + 1 * 17);
                if (c1 < (pid_t)0) {
                    (void)waitpid(c0, NULL, 0);
                    close(pr0[0]);
                    ok = -1;
                } else {
                    pid_t c2 = ss_fork_complete_pipe(
                        pr2, prompt, k_semantic_extra_sysp, temps[2],
                        k_semantic_extra_np, 13001 + 2 * 17);
                    if (c2 < (pid_t)0) {
                        (void)waitpid(c0, NULL, 0);
                        (void)waitpid(c1, NULL, 0);
                        close(pr0[0]);
                        close(pr1[0]);
                        ok = -1;
                    } else {
                        int s0 = 0, s1 = 0, s2 = 0;
                        (void)waitpid(c0, &s0, 0);
                        (void)waitpid(c1, &s1, 0);
                        (void)waitpid(c2, &s2, 0);
                        if (!(WIFEXITED(s0) && WEXITSTATUS(s0) == 0
                              && WIFEXITED(s1) && WEXITSTATUS(s1) == 0
                              && WIFEXITED(s2) && WEXITSTATUS(s2) == 0)
                            || ss_pipe_read_dup(pr0[0], &dup0) != 0
                            || ss_pipe_read_dup(pr1[0], &dup1) != 0
                            || ss_pipe_read_dup(pr2[0], &dup2) != 0
                            || dup0 == NULL || dup0[0] == '\0' || dup1 == NULL
                            || dup1[0] == '\0' || dup2 == NULL || dup2[0] == '\0')
                            ok = -1;
                        close(pr0[0]);
                        close(pr1[0]);
                        close(pr2[0]);
                    }
                }
            }
        } else {
            for (int i = 0; i < 3 && ok == 0; i++) {
                cos_bitnet_server_params_t par;
                memset(&par, 0, sizeof(par));
                par.system_prompt = k_semantic_extra_sysp;
                par.temperature   = temps[i];
                par.n_predict     = k_semantic_extra_np;
                par.seed          = 13001 + i * 17;

                cos_bitnet_server_result_t br;
                memset(&br, 0, sizeof(br));
                if (cos_bitnet_server_complete(prompt, &par, &br) != 0
                    || br.text == NULL) {
                    ok = -1;
                    break;
                }
                char *d = strdup(br.text);
                if (d == NULL) {
                    ok = -1;
                    break;
                }
                if (i == 0)
                    dup0 = d;
                else if (i == 1)
                    dup1 = d;
                else
                    dup2 = d;
            }
        }
        texts[0] = dup0 != NULL ? dup0 : "";
        texts[1] = dup1 != NULL ? dup1 : "";
        texts[2] = dup2 != NULL ? dup2 : "";
    }

    ss_restore_env(port_set, model_set, old_port, old_bcm, old_oll);

    if (ok != 0) {
        free(dup0);
        free(dup1);
        free(dup2);
        out->sigma     = 0.85f;
        out->n_samples = 3;
        return out->sigma;
    }

    {
        char fs0[512], fs1[512], fs2[512];
        cos_text_first_sentence(texts[0], fs0, sizeof fs0);
        cos_text_first_sentence(texts[1], fs1, sizeof fs1);
        cos_text_first_sentence(texts[2], fs2, sizeof fs2);
        (void)cos_semantic_sigma_compute_texts(fs0, fs1, fs2, out);
    }
    out->n_samples = 3;

    free(dup0);
    free(dup1);
    free(dup2);
    return out->sigma;
}

float cos_semantic_sigma(const char *prompt, int port, const char *model,
                         int n_samples) {
    return cos_semantic_sigma_ex(prompt, NULL, port, model, n_samples, NULL,
                                 NULL);
}
