/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "semantic_entropy.h"

#include "bitnet_server.h"
#include "inference_cache.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int se_streq_ci(const char *a, const char *b) {
    if (a == NULL || b == NULL)
        return 0;
    for (; *a && *b; ++a, ++b) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char)(ca + 32u);
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char)(cb + 32u);
        if (ca != cb)
            return 0;
    }
    return *a == *b;
}

#define COS_SE_MAX_SAMPLES 8
#define COS_SE_WORD_CAP    256
#define COS_SE_TEXT_SNIP   4096

static float se_env_float(const char *name, float def) {
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

static int se_use_bsc_metric(void) {
    const char *m = getenv("COS_SEMANTIC_ENTROPY_METRIC");
    return (m != NULL && se_streq_ci(m, "bsc")) ? 1 : 0;
}

float cos_text_similarity_jaccard(const char *a, const char *b) {
    char        bufa[COS_SE_TEXT_SNIP], bufb[COS_SE_TEXT_SNIP];
    const char *wa[COS_SE_WORD_CAP], *wb[COS_SE_WORD_CAP];
    int         na = 0, nb = 0;

    strncpy(bufa, a != NULL ? a : "", sizeof(bufa) - 1);
    bufa[sizeof(bufa) - 1] = '\0';
    strncpy(bufb, b != NULL ? b : "", sizeof(bufb) - 1);
    bufb[sizeof(bufb) - 1] = '\0';

    for (char *tok = strtok(bufa, " \t\n\r.,!?;:\"'()[]{}");
         tok != NULL && na < COS_SE_WORD_CAP; tok = strtok(NULL, " \t\n\r.,!?;:\"'()[]{}")) {
        for (char *p = tok; *p; ++p) {
            if (*p >= 'A' && *p <= 'Z')
                *p = (char)(*p + 32);
        }
        wa[na++] = tok;
    }
    for (char *tok = strtok(bufb, " \t\n\r.,!?;:\"'()[]{}");
         tok != NULL && nb < COS_SE_WORD_CAP; tok = strtok(NULL, " \t\n\r.,!?;:\"'()[]{}")) {
        for (char *p = tok; *p; ++p) {
            if (*p >= 'A' && *p <= 'Z')
                *p = (char)(*p + 32);
        }
        wb[nb++] = tok;
    }
    if (na == 0 && nb == 0)
        return 1.0f;
    int inter = 0;
    for (int i = 0; i < na; i++) {
        for (int j = 0; j < nb; j++) {
            if (strcmp(wa[i], wb[j]) == 0) {
                inter++;
                break;
            }
        }
    }
    int uni = na + nb - inter;
    if (uni <= 0)
        return 0.0f;
    return (float)inter / (float)uni;
}

static float se_pair_sim(const char *a, const char *b) {
    if (se_use_bsc_metric()) {
        uint64_t va[COS_INF_W], vb[COS_INF_W];
        cos_inference_bsc_encode_prompt(a != NULL ? a : "", va);
        cos_inference_bsc_encode_prompt(b != NULL ? b : "", vb);
        float d = cos_inference_hamming_norm(va, vb, COS_INF_W);
        float s = 1.0f - d;
        if (s < 0.0f)
            s = 0.0f;
        if (s > 1.0f)
            s = 1.0f;
        return s;
    }
    return cos_text_similarity_jaccard(a, b);
}

static int se_uf_find(int *parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i         = parent[i];
    }
    return i;
}

static void se_uf_union(int *parent, int a, int b) {
    a = se_uf_find(parent, a);
    b = se_uf_find(parent, b);
    if (a != b)
        parent[b] = a;
}

static int se_count_clusters(int n, const char **texts, float tau_merge) {
    if (n <= 0)
        return 1;
    int parent[COS_SE_MAX_SAMPLES];
    for (int i = 0; i < n; i++)
        parent[i] = i;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (se_pair_sim(texts[i], texts[j]) >= tau_merge)
                se_uf_union(parent, i, j);
        }
    }

    int roots[COS_SE_MAX_SAMPLES];
    int nr = 0;
    for (int i = 0; i < n; i++) {
        int r = se_uf_find(parent, i);
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

static void se_snap_env(int port, const char *model, char *buf_port, char *buf_model,
                        const char **old_port, const char **old_bcm, const char **old_oll) {
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

static void se_restore_env(int port_set, int model_set, const char *old_port,
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

static int se_parallel_samples(void)
{
    const char *e = getenv("COS_SEMANTIC_ENTROPY_PARALLEL");
    return (e == NULL || e[0] != '0' || e[1] != '\0');
}

static int se_pipe_read_dup(int rd, char **out)
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

static pid_t se_fork_complete_pipe(int pair[2], const char *prompt,
                                   const char *system_prompt, float temp,
                                   int max_tok, int seed)
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
        par.system_prompt = system_prompt;
        par.temperature   = temp;
        par.n_predict       = max_tok;
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

static int se_default_temps(int n, float *out) {
    static const float t3[3] = { 0.3f, 0.7f, 1.0f };
    static const float t2[2] = { 0.3f, 0.9f };
    static const float t4[4] = { 0.2f, 0.5f, 0.8f, 1.0f };
    if (n == 3) {
        memcpy(out, t3, sizeof(t3));
        return 0;
    }
    if (n == 2) {
        memcpy(out, t2, sizeof(t2));
        return 0;
    }
    if (n == 4) {
        memcpy(out, t4, sizeof(t4));
        return 0;
    }
    if (n < 2 || n > COS_SE_MAX_SAMPLES)
        return -1;
    for (int i = 0; i < n; i++)
        out[i] = 0.3f + (0.7f * (float)i / (float)(n > 1 ? (n - 1) : 1));
    return 0;
}

float cos_semantic_entropy_ex(const char *prompt, const char *system_prompt,
                              int port, const char *model, int n_samples,
                              int *out_n_clusters) {
    if (out_n_clusters != NULL)
        *out_n_clusters = 1;

    if (prompt == NULL || prompt[0] == '\0' || n_samples < 2
        || n_samples > COS_SE_MAX_SAMPLES)
        return 0.5f;

    float temps[COS_SE_MAX_SAMPLES];
    if (se_default_temps(n_samples, temps) != 0)
        return 0.5f;

    char        buf_port[32], buf_model[512];
    const char *old_port, *old_bcm, *old_oll;
    int         port_set = (port > 0) ? 1 : 0;
    int         model_set =
        (model != NULL && model[0] != '\0') ? 1 : 0;
    se_snap_env(port, model, buf_port, buf_model, &old_port, &old_bcm, &old_oll);

    const char *env_ntok = getenv("COS_SEMANTIC_ENTROPY_MAX_TOKENS");
    int         max_tok   = (env_ntok != NULL && env_ntok[0] != '\0')
                              ? atoi(env_ntok)
                              : 100;
    if (max_tok < 16)
        max_tok = 16;
    if (max_tok > 512)
        max_tok = 512;

    char *dup[COS_SE_MAX_SAMPLES];
    memset(dup, 0, sizeof(dup));

    int ok = 0;
    if (se_parallel_samples() && n_samples >= 2 && n_samples <= COS_SE_MAX_SAMPLES) {
        int   pr[COS_SE_MAX_SAMPLES][2];
        pid_t ch[COS_SE_MAX_SAMPLES];
        int   started = 0;
        for (int i = 0; i < n_samples; ++i) {
            ch[i] = se_fork_complete_pipe(pr[i], prompt, system_prompt, temps[i],
                                          max_tok, 9001 + i * 17);
            if (ch[i] < (pid_t)0) {
                for (int j = 0; j < i; ++j) {
                    (void)waitpid(ch[j], NULL, 0);
                    close(pr[j][0]);
                }
                ok = -1;
                break;
            }
            started++;
        }
        if (started == n_samples) {
            for (int i = 0; i < n_samples; ++i) {
                int st = 0;
                (void)waitpid(ch[i], &st, 0);
                if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0)
                    || se_pipe_read_dup(pr[i][0], &dup[i]) != 0
                    || dup[i] == NULL || dup[i][0] == '\0') {
                    ok = -1;
                }
                close(pr[i][0]);
            }
        }
    } else {
        for (int i = 0; i < n_samples; i++) {
            cos_bitnet_server_params_t par;
            memset(&par, 0, sizeof(par));
            par.system_prompt = system_prompt;
            par.temperature   = temps[i];
            par.n_predict     = max_tok;
            par.seed          = 9001 + i * 17;

            cos_bitnet_server_result_t outr;
            memset(&outr, 0, sizeof(outr));
            if (cos_bitnet_server_complete(prompt, &par, &outr) != 0
                || outr.text == NULL) {
                ok = -1;
                break;
            }
            dup[i] = strdup(outr.text);
            if (dup[i] == NULL) {
                ok = -1;
                break;
            }
        }
    }

    se_restore_env(port_set, model_set, old_port, old_bcm, old_oll);

    if (ok != 0) {
        for (int i = 0; i < n_samples; i++) {
            free(dup[i]);
            dup[i] = NULL;
        }
        return 0.85f;
    }

    const char *tx[COS_SE_MAX_SAMPLES];
    for (int i = 0; i < n_samples; i++)
        tx[i] = dup[i] != NULL ? dup[i] : "";

    float tau_merge = se_env_float("COS_SEMANTIC_ENTROPY_SIM", 0.6f);
    int   nc        = se_count_clusters(n_samples, tx, tau_merge);
    for (int i = 0; i < n_samples; i++) {
        free(dup[i]);
        dup[i] = NULL;
    }

    if (out_n_clusters != NULL)
        *out_n_clusters = nc;

    float sigma = 1.0f - 1.0f / (float)nc;
    if (sigma < 0.0f)
        sigma = 0.0f;
    if (sigma > 1.0f)
        sigma = 1.0f;
    return sigma;
}

float cos_semantic_entropy(const char *prompt, const char *system_prompt,
                           int port, const char *model, int n_samples) {
    return cos_semantic_entropy_ex(prompt, system_prompt, port, model, n_samples,
                                   NULL);
}
