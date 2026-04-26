/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * echo "A. B!" | ./cos verify  — per-sentence σ via semantic entropy (HTTP).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_verify_claims.h"

#include "bitnet_server.h"
#include "reinforce.h"
#include "semantic_entropy.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { VBUF_CAP = 262144, SENT_MAX = 512, LINE_MAX = 4096 };

static int read_all_stdin(char *buf, size_t cap)
{
    size_t n = 0;
    for (;;) {
        if (n + 4096 >= cap)
            return -1;
        size_t room = cap - n - 1u;
        size_t r    = fread(buf + n, 1, room, stdin);
        if (r == 0)
            break;
        n += r;
        buf[n] = '\0';
    }
    return (int)n;
}

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    size_t n = fread(buf, 1, cap - 1u, fp);
    fclose(fp);
    buf[n] = '\0';
    return (int)n;
}

enum { COS_VERIFY_JSON_TEXT_MAX = 12000 };

static int json_escape_str(const char *in, char *out, size_t cap)
{
    size_t w = 0;
    for (; *in && w + 6 < cap; ++in) {
        unsigned char c = (unsigned char)*in;
        if (c == '"' || c == '\\') {
            out[w++] = '\\';
            out[w++] = (char)c;
        } else if (c < 0x20u)
            w += (size_t)snprintf(out + w, cap - w, "\\u%04x", (unsigned)c);
        else
            out[w++] = (char)c;
    }
    out[w] = '\0';
    return *in ? -1 : 0;
}

static int next_sentence(const char *in, int *pos, char *out, size_t outcap)
{
    size_t w = 0;
    int    p = *pos;
    while (in[p] == ' ' || in[p] == '\t' || in[p] == '\n' || in[p] == '\r')
        p++;
    if (in[p] == '\0') {
        *pos = p;
        return 0;
    }
    for (; in[p] != '\0' && w + 2 < outcap; ++p) {
        unsigned char c = (unsigned char)in[p];
        out[w++] = (char)c;
        if ((c == '.' || c == '!' || c == '?')
            && (in[p + 1] == '\0' || in[p + 1] == ' ' || in[p + 1] == '\t'
                || in[p + 1] == '\n' || in[p + 1] == '\r')) {
            ++p;
            break;
        }
    }
    while (w > 0 && (out[w - 1] == ' ' || out[w - 1] == '\t'))
        w--;
    out[w] = '\0';
    *pos   = p;
    return (out[0] != '\0') ? 1 : 0;
}

static int ollama_port(void)
{
    const char *pe = getenv("COS_BITNET_SERVER_PORT");
    if (pe != NULL && pe[0] != '\0')
        return atoi(pe);
    pe = getenv("COS_OLLAMA_PORT");
    if (pe != NULL && pe[0] != '\0')
        return atoi(pe);
    return 11434;
}

int cos_verify_claims_main(int argc, char **argv)
{
    char        buf[VBUF_CAP];
    const char *path = NULL;
    int         i;

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fputs("usage: cos verify [--file PATH] [\"single claim\"]\n"
                  "  With stdin pipe: split into sentences; score each via "
                  "semantic-entropy (3 samples).\n"
                  "  Requires llama-server / Ollama reachable (see "
                  "COS_BITNET_SERVER_PORT, default 11434).\n",
                  stdout);
            return 0;
        }
    }

    int n;
    if (path != NULL) {
        n = read_file(path, buf, sizeof buf);
        if (n < 0) {
            perror("cos verify");
            return 2;
        }
    } else if (!isatty(fileno(stdin))) {
        n = read_all_stdin(buf, sizeof buf);
        if (n < 0) {
            fputs("cos verify: input too large\n", stderr);
            return 2;
        }
    } else {
        /* single claim from argv remainder (skip --file if present) */
        size_t w = 0;
        for (i = 0; i < argc; ++i) {
            if (strcmp(argv[i], "--file") == 0) {
                if (i + 1 < argc)
                    ++i;
                continue;
            }
            size_t L = strlen(argv[i]);
            if (w + L + 2 >= sizeof buf)
                break;
            if (w > 0)
                buf[w++] = ' ';
            memcpy(buf + w, argv[i], L);
            w += L;
        }
        buf[w] = '\0';
        n      = (int)w;
        if (n == 0) {
            fputs("cos verify: no text (pipe stdin, use --file, or pass a "
                  "claim argument)\n",
                  stderr);
            return 2;
        }
    }

    if (n == 0) {
        fputs("cos verify: empty input\n", stderr);
        return 2;
    }

    if (!cos_bitnet_server_is_healthy()) {
        fputs("cos verify: HTTP backend not healthy on configured port — "
              "start Ollama/llama-server (default localhost:11434) or set "
              "COS_BITNET_SERVER_PORT.\n",
              stderr);
        return 3;
    }

    const char *model = getenv("COS_BITNET_CHAT_MODEL");
    if (model == NULL || model[0] == '\0')
        model = "gemma3:4b";

    int   port = ollama_port();
    float tau_accept  = 0.40f;
    float tau_rethink = 0.60f;
    int   pos   = 0;
    int   n_ok  = 0;
    int   n_bad = 0;
    char  sent[SENT_MAX];

    while (next_sentence(buf, &pos, sent, sizeof sent) == 1) {
        if (sent[0] == '\0')
            continue;
        int   ncl = 3;
        float se  = cos_semantic_entropy_ex(sent, NULL, port, model, 3, &ncl);
        cos_sigma_action_t g =
            cos_sigma_reinforce(se, tau_accept, tau_rethink);
        int pass = (g == COS_SIGMA_ACTION_ACCEPT);
        if (pass)
            n_ok++;
        else
            n_bad++;
        const char *sym = pass ? "✓" : "✗";
        printf("%s σ=%.2f  %s\n", sym, (double)se, sent);
    }

    printf("\nVerdict: %d/%d ACCEPT | %d/%d REJECT\n", n_ok, n_ok + n_bad,
           n_bad, n_ok + n_bad);
    return n_bad > 0 ? 1 : 0;
}

int cos_verify_claims_sentences_json(const char *text, char *out, size_t outcap)
{
    if (text == NULL || out == NULL || outcap < 32u)
        return -1;
    size_t tl = strlen(text);
    if (tl >= (size_t)COS_VERIFY_JSON_TEXT_MAX)
        return -2;

    char work[COS_VERIFY_JSON_TEXT_MAX];
    memcpy(work, text, tl + 1u);

    const char *model = getenv("COS_BITNET_CHAT_MODEL");
    if (model == NULL || model[0] == '\0')
        model = "gemma3:4b";
    int   port         = ollama_port();
    float tau_accept   = 0.40f;
    float tau_rethink  = 0.60f;

    size_t w = (size_t)snprintf(out, outcap, "{\"sentences\":[");
    if (w >= outcap)
        return -1;

    int  pos   = 0;
    int  first = 1;
    char sent[SENT_MAX];

    while (next_sentence(work, &pos, sent, sizeof sent) == 1) {
        if (sent[0] == '\0')
            continue;
        char esc[2048];
        if (json_escape_str(sent, esc, sizeof esc) != 0)
            return -1;
        int   ncl = 3;
        float se  = cos_semantic_entropy_ex(sent, NULL, port, model, 3, &ncl);
        cos_sigma_action_t g = cos_sigma_reinforce(se, tau_accept, tau_rethink);
        const char          *lab = cos_sigma_action_label(g);
        int nw = snprintf(out + w, outcap - w,
                          "%s{\"text\":\"%s\",\"sigma\":%.4f,\"action\":\"%s\"}",
                          first ? "" : ",", esc, (double)se, lab);
        if (nw < 0 || (size_t)nw >= outcap - w)
            return -1;
        w += (size_t)nw;
        first = 0;
    }

    if (first) {
        /* No sentence extracted — score whole trimmed text once */
        const char *p = text;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p == '\0')
            return snprintf(out, outcap, "{\"sentences\":[]}") < 0 ? -1 : 0;
        char esc[2048];
        if (json_escape_str(p, esc, sizeof esc) != 0)
            return -1;
        int   ncl = 3;
        float se  = cos_semantic_entropy_ex(p, NULL, port, model, 3, &ncl);
        cos_sigma_action_t g   = cos_sigma_reinforce(se, tau_accept, tau_rethink);
        const char          *lab = cos_sigma_action_label(g);
        if (snprintf(out, outcap,
                     "{\"sentences\":[{\"text\":\"%s\",\"sigma\":%.4f,\"action\":\"%s\"}]}",
                     esc, (double)se, lab)
            < 0)
            return -1;
        return 0;
    }

    if (snprintf(out + w, outcap - w, "]}") < 0 || w + 2u >= outcap)
        return -1;
    return 0;
}
