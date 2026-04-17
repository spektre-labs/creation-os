/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * creation_os_v101 — σ-BitNet-Bridge CLI.
 *
 * Two top-level modes matched to the lm-evaluation-harness interface:
 *
 *   creation_os_v101 --ll   --gguf <path> --ctx <text> --cont <text>
 *       → emits a single JSON object:
 *         {"loglikelihood": <float>, "is_greedy": <bool>,
 *          "n_ctx_tokens": <int>,    "n_cont_tokens": <int>,
 *          "sigma_mean":   <float>}
 *
 *   creation_os_v101 --gen  --gguf <path> --ctx <text>
 *                    [--until "s1,s2"] [--max-tokens N]
 *                    [--sigma-threshold F]
 *       → emits a single JSON object:
 *         {"text": "<generated>",
 *          "sigma_profile": [e,m,tk,tl,ls,pm,ne,std],
 *          "abstained": <bool>}
 *
 * Default (no mode flag): runs `cos_v101_self_test()` + prints a one-line
 * banner.  This is what the merge-gate's `check-v101` target calls — it
 * works regardless of whether the GGUF exists, so CI stays green.
 *
 * Everything else (sampling, σ-channels, tokenization) lives in
 * src/v101/bridge_*.c.
 */
#include "bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_escape(const char *s, FILE *fp)
{
    fputc('"', fp);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  fputs("\\\"", fp); break;
        case '\\': fputs("\\\\", fp); break;
        case '\n': fputs("\\n",  fp); break;
        case '\r': fputs("\\r",  fp); break;
        case '\t': fputs("\\t",  fp); break;
        case '\b': fputs("\\b",  fp); break;
        case '\f': fputs("\\f",  fp); break;
        default:
            if (c < 0x20) fprintf(fp, "\\u%04x", c);
            else          fputc(c, fp);
        }
    }
    fputc('"', fp);
}

static int split_until(const char *csv, const char **out, int max_out)
{
    if (!csv || !*csv) return 0;
    static char buf[1024];
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    int n = 0;
    char *saveptr = NULL;
    for (char *tok = strtok_r(buf, ",", &saveptr); tok && n < max_out;
         tok = strtok_r(NULL, ",", &saveptr)) {
        out[n++] = tok;
    }
    return n;
}

static void usage(void)
{
    fprintf(stderr,
            "usage: creation_os_v101 --self-test\n"
            "       creation_os_v101 --banner\n"
            "       creation_os_v101 --ll  --gguf PATH --ctx CTX --cont CONT\n"
            "       creation_os_v101 --gen --gguf PATH --ctx CTX [--until LIST]\n"
            "                                [--max-tokens N] [--sigma-threshold F]\n"
            "                                [--n-ctx N]\n"
            "       creation_os_v101 --stdin --gguf PATH [--n-ctx N]\n"
            "           persistent JSON-lines mode: one request per input line, one\n"
            "           response per output line.  Request shapes:\n"
            "             {\"op\":\"ll\", \"ctx\":TEXT, \"cont\":TEXT}\n"
            "             {\"op\":\"gen\",\"ctx\":TEXT,\"max_tokens\":N,\n"
            "              \"until\":[...],\"sigma_threshold\":F}\n"
            "           The model is loaded once; every subsequent call reuses the\n"
            "           same llama.cpp context, which is how v102 avoids a 3 s\n"
            "           per-sample reload cost under the lm-evaluation-harness.\n");
}

/* Minimal JSON field extractor: finds "key": and returns the next string
 * or number.  Not a full JSON parser — accepts the exact shapes emitted by
 * benchmarks/v102/creation_os_backend.py, which never uses nested objects.
 */
static const char *json_find_key(const char *s, const char *key)
{
    size_t klen = strlen(key);
    const char *p = s;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            p += klen + 2;
            while (*p == ' ' || *p == '\t' || *p == ':') p++;
            return p;
        }
        p++;
    }
    return NULL;
}

/* Decode a JSON string value starting at `p` (expected to point at `"`).
 * Unescapes \\, \", \n, \r, \t, \/, \b, \f and \uXXXX (basic BMP only).
 * Returns pointer just past the closing `"`, or NULL on parse error.
 */
static const char *json_decode_string(const char *p, char *out, int max_out)
{
    if (*p != '"') return NULL;
    p++;
    int w = 0;
    while (*p && *p != '"') {
        if (w + 1 >= max_out) return NULL;
        if (*p == '\\') {
            p++;
            switch (*p) {
            case '"': out[w++] = '"'; p++; break;
            case '\\': out[w++] = '\\'; p++; break;
            case '/':  out[w++] = '/';  p++; break;
            case 'n':  out[w++] = '\n'; p++; break;
            case 'r':  out[w++] = '\r'; p++; break;
            case 't':  out[w++] = '\t'; p++; break;
            case 'b':  out[w++] = '\b'; p++; break;
            case 'f':  out[w++] = '\f'; p++; break;
            case 'u': {
                if (!p[1] || !p[2] || !p[3] || !p[4]) return NULL;
                unsigned int cp = 0;
                for (int k = 1; k <= 4; k++) {
                    char c = p[k]; int d;
                    if      (c >= '0' && c <= '9') d = c - '0';
                    else if (c >= 'a' && c <= 'f') d = 10 + c - 'a';
                    else if (c >= 'A' && c <= 'F') d = 10 + c - 'A';
                    else return NULL;
                    cp = (cp << 4) | (unsigned)d;
                }
                p += 5;
                /* encode as UTF-8 (BMP only) */
                if (cp < 0x80) {
                    if (w + 1 >= max_out) return NULL;
                    out[w++] = (char)cp;
                } else if (cp < 0x800) {
                    if (w + 2 >= max_out) return NULL;
                    out[w++] = (char)(0xC0 | (cp >> 6));
                    out[w++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    if (w + 3 >= max_out) return NULL;
                    out[w++] = (char)(0xE0 | (cp >> 12));
                    out[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    out[w++] = (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: return NULL;
            }
        } else {
            out[w++] = *p++;
        }
    }
    if (*p != '"') return NULL;
    out[w] = 0;
    return p + 1;
}

static int stdin_loop(cos_v101_bridge_t *b)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    /* one-line banner so the client knows the bridge is up */
    printf("{\"ready\":true,\"n_vocab\":%d}\n", cos_v101_bridge_n_vocab(b));
    fflush(stdout);

    enum { SCRATCH = 32768 };
    char *ctx  = (char *)malloc(SCRATCH);
    char *cont = (char *)malloc(SCRATCH);
    char *gen_buf = (char *)malloc(SCRATCH);
    if (!ctx || !cont || !gen_buf) { free(ctx); free(cont); free(gen_buf); return 10; }

    while ((n = getline(&line, &cap, stdin)) > 0) {
        /* strip trailing newline */
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) { line[--n] = 0; }
        if (n == 0) continue;

        const char *op_p = json_find_key(line, "op");
        if (!op_p) { printf("{\"error\":\"missing op\"}\n"); fflush(stdout); continue; }
        char op[16] = {0};
        const char *after = json_decode_string(op_p, op, sizeof(op));
        if (!after) { printf("{\"error\":\"bad op\"}\n"); fflush(stdout); continue; }

        if (strcmp(op, "ll") == 0) {
            const char *ctx_p = json_find_key(line, "ctx");
            const char *cont_p = json_find_key(line, "cont");
            if (!ctx_p || !cont_p) { printf("{\"error\":\"ll needs ctx+cont\"}\n"); fflush(stdout); continue; }
            if (!json_decode_string(ctx_p, ctx, SCRATCH) || !json_decode_string(cont_p, cont, SCRATCH)) {
                printf("{\"error\":\"json decode\"}\n"); fflush(stdout); continue;
            }
            double ll = 0.0; int is_greedy = 0;
            int n_ctx_t = 0, n_cont_t = 0; float sigma_mean = 0.f;
            int r = cos_v101_bridge_loglikelihood(b, ctx, cont,
                                                  &ll, &is_greedy,
                                                  &n_ctx_t, &n_cont_t, &sigma_mean);
            if (r != COS_V101_OK) {
                printf("{\"error\":\"ll rc=%d\"}\n", r);
            } else {
                printf("{\"loglikelihood\":%.8f,\"is_greedy\":%s,"
                       "\"n_ctx_tokens\":%d,\"n_cont_tokens\":%d,"
                       "\"sigma_mean\":%.6f}\n",
                       ll, is_greedy ? "true" : "false",
                       n_ctx_t, n_cont_t, (double)sigma_mean);
            }
            fflush(stdout);
        } else if (strcmp(op, "gen") == 0) {
            const char *ctx_p = json_find_key(line, "ctx");
            if (!ctx_p || !json_decode_string(ctx_p, ctx, SCRATCH)) {
                printf("{\"error\":\"gen needs ctx\"}\n"); fflush(stdout); continue;
            }
            /* simplified: no until/max_tokens/σ extraction; defaults. */
            int max_tokens = 256;
            float sigma_threshold = 0.0f;
            const char *mt_p = json_find_key(line, "max_tokens");
            if (mt_p) max_tokens = atoi(mt_p);
            const char *st_p = json_find_key(line, "sigma_threshold");
            if (st_p) sigma_threshold = (float)atof(st_p);

            float profile[COS_V101_SIGMA_CHANNELS] = {0};
            int abstained = 0;
            int r = cos_v101_bridge_generate(b, ctx, NULL, 0, max_tokens, sigma_threshold,
                                             gen_buf, SCRATCH, profile, &abstained);
            if (r < 0) {
                printf("{\"error\":\"gen rc=%d\"}\n", r);
            } else {
                printf("{\"text\":");
                json_escape(gen_buf, stdout);
                printf(",\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
                       "\"abstained\":%s}\n",
                       (double)profile[0], (double)profile[1], (double)profile[2], (double)profile[3],
                       (double)profile[4], (double)profile[5], (double)profile[6], (double)profile[7],
                       abstained ? "true" : "false");
            }
            fflush(stdout);
        } else if (strcmp(op, "quit") == 0) {
            break;
        } else {
            printf("{\"error\":\"unknown op\"}\n"); fflush(stdout);
        }
    }

    free(line);
    free(ctx); free(cont); free(gen_buf);
    return 0;
}

int main(int argc, char **argv)
{
    const char *gguf = NULL;
    const char *ctx  = NULL;
    const char *cont = NULL;
    const char *until_csv = NULL;
    int   max_tokens = 128;
    float sigma_threshold = 0.0f;
    uint32_t n_ctx_override = 0;
    int   mode_ll = 0;
    int   mode_gen = 0;
    int   mode_self = 1;
    int   mode_banner = 0;
    int   mode_stdin = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--ll"))                { mode_ll = 1; mode_self = 0; }
        else if (!strcmp(a, "--gen"))               { mode_gen = 1; mode_self = 0; }
        else if (!strcmp(a, "--stdin"))             { mode_stdin = 1; mode_self = 0; }
        else if (!strcmp(a, "--self-test"))         { mode_self = 1; mode_ll = 0; mode_gen = 0; mode_stdin = 0; }
        else if (!strcmp(a, "--banner"))            { mode_banner = 1; mode_self = 0; }
        else if (!strcmp(a, "--gguf") && i + 1 < argc)            { gguf = argv[++i]; }
        else if (!strcmp(a, "--ctx") && i + 1 < argc)             { ctx = argv[++i]; }
        else if (!strcmp(a, "--cont") && i + 1 < argc)            { cont = argv[++i]; }
        else if (!strcmp(a, "--until") && i + 1 < argc)           { until_csv = argv[++i]; }
        else if (!strcmp(a, "--max-tokens") && i + 1 < argc)      { max_tokens = atoi(argv[++i]); }
        else if (!strcmp(a, "--sigma-threshold") && i + 1 < argc) { sigma_threshold = (float)atof(argv[++i]); }
        else if (!strcmp(a, "--n-ctx") && i + 1 < argc)           { n_ctx_override = (uint32_t)atoi(argv[++i]); }
        else if (!strcmp(a, "--help") || !strcmp(a, "-h"))        { usage(); return 0; }
        else { fprintf(stderr, "unknown flag: %s\n", a); usage(); return 2; }
    }

    if (mode_banner) {
        printf("creation_os_v101 σ-BitNet-Bridge "
#ifdef COS_V101_BITNET_REAL
               "(real mode)\n"
#else
               "(stub mode — build with COS_V101_BITNET_REAL=1 for real inference)\n"
#endif
        );
        return 0;
    }

    if (mode_self) {
        return cos_v101_self_test();
    }

    if ((mode_ll && (!gguf || !ctx || !cont)) ||
        (mode_gen && (!gguf || !ctx)) ||
        (mode_stdin && !gguf)) {
        usage();
        return 2;
    }

    cos_v101_bridge_t *b = cos_v101_bridge_open(gguf, n_ctx_override);
    if (!b) {
        fprintf(stderr, "cos_v101: failed to open model\n");
        return 3;
    }

    int rc = 0;

    if (mode_stdin) {
        rc = stdin_loop(b);
        cos_v101_bridge_close(b);
        return rc;
    }

    if (mode_ll) {
        double ll = 0.0; int is_greedy = 0;
        int n_ctx_t = 0, n_cont_t = 0; float sigma_mean = 0.f;
        int r = cos_v101_bridge_loglikelihood(b, ctx, cont,
                                              &ll, &is_greedy,
                                              &n_ctx_t, &n_cont_t,
                                              &sigma_mean);
        if (r != COS_V101_OK) {
            fprintf(stderr, "cos_v101: loglikelihood rc=%d\n", r);
            rc = 4;
        } else {
            printf("{\"loglikelihood\":%.8f,\"is_greedy\":%s,"
                   "\"n_ctx_tokens\":%d,\"n_cont_tokens\":%d,"
                   "\"sigma_mean\":%.6f}\n",
                   ll,
                   is_greedy ? "true" : "false",
                   n_ctx_t, n_cont_t, (double)sigma_mean);
        }
    } else if (mode_gen) {
        const char *until[16];
        int n_until = split_until(until_csv, until, 16);
        int out_buf_sz = max_tokens * 16 + 512;
        if (out_buf_sz < 4096) out_buf_sz = 4096;
        char *buf = (char *)calloc((size_t)out_buf_sz, 1);
        float profile[COS_V101_SIGMA_CHANNELS] = {0};
        int abstained = 0;
        int r = cos_v101_bridge_generate(b, ctx, until, n_until,
                                         max_tokens, sigma_threshold,
                                         buf, out_buf_sz,
                                         profile, &abstained);
        if (r < 0) {
            fprintf(stderr, "cos_v101: generate rc=%d\n", r);
            rc = 5;
        } else {
            printf("{\"text\":");
            json_escape(buf, stdout);
            printf(",\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
                   "\"abstained\":%s}\n",
                   (double)profile[0], (double)profile[1], (double)profile[2], (double)profile[3],
                   (double)profile[4], (double)profile[5], (double)profile[6], (double)profile[7],
                   abstained ? "true" : "false");
        }
        free(buf);
    }

    cos_v101_bridge_close(b);
    return rc;
}
