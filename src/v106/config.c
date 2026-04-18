/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v106 config — tiny TOML-ish loader.
 *
 * We do **not** ship a full TOML parser.  The config shape is fixed
 * and tiny (three sections, < 10 keys).  Accepted syntax:
 *
 *     # comment
 *     [section]
 *     key = "string value"   # trailing comments ok
 *     key = 123              # integer
 *     key = 1.5              # double
 *     key = true             # accepted as "true"/"false" string
 *
 * Anything else (arrays, inline tables, multi-line strings) is simply
 * ignored — we do not aspire to be a TOML host.  Keys unknown to the
 * struct are logged on stderr and skipped, so a typo is visible
 * without being fatal.
 */
#include "server.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#endif

void cos_v106_config_defaults(cos_v106_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->host,       sizeof cfg->host,       "%s", "127.0.0.1");
    cfg->port = 8080;
    cfg->tau_abstain = 0.7;
    snprintf(cfg->aggregator, sizeof cfg->aggregator, "%s", "product");
    cfg->gguf_path[0] = '\0';
    cfg->n_ctx = 2048;
    snprintf(cfg->model_id,   sizeof cfg->model_id,   "%s", "creation-os");
    snprintf(cfg->web_root,   sizeof cfg->web_root,   "%s", "web");
}

int cos_v106_default_config_path(char *out, size_t cap)
{
    if (!out || cap < 1) return -1;
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif
    if (!home || !*home) return -1;
    int n = snprintf(out, cap, "%s/.creation-os/config.toml", home);
    return (n <= 0 || (size_t)n >= cap) ? -1 : 0;
}

int cos_v106_ensure_parent_dir(const char *path)
{
    if (!path || !*path) return -1;
    char tmp[1024];
    int n = snprintf(tmp, sizeof tmp, "%s", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return -1;
    char *slash = strrchr(tmp, '/');
    if (!slash) return 0;  /* no parent */
    *slash = '\0';
    if (tmp[0] == '\0') return 0;
    struct stat st;
    if (stat(tmp, &st) == 0) {
        return (st.st_mode & S_IFDIR) ? 0 : -1;
    }
    /* Walk up and mkdir piece by piece.  Best-effort; fails cleanly
     * on the first unrecoverable permission error. */
    char build[1024];
    build[0] = '\0';
    size_t bp = 0;
    for (char *p = tmp; ; p++) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            if (tmp[0] != '\0') {
                int m = snprintf(build + bp, sizeof(build) - bp, "%s%s",
                                 (bp == 0 && tmp[0] == '/') ? "" : "",
                                 tmp);
                if (m < 0 || (size_t)m >= sizeof(build) - bp) return -1;
                bp += (size_t)m;
                if (stat(build, &st) != 0) {
#ifdef _WIN32
                    if (mkdir(build) != 0 && errno != EEXIST) return -1;
#else
                    if (mkdir(build, 0755) != 0 && errno != EEXIST) return -1;
#endif
                }
                build[bp++] = '/';
                build[bp] = '\0';
            }
            *p = saved;
            if (saved == '\0') break;
            /* Reset: we already wrote this full prefix. */
            break;
        }
    }
    return 0;
}

/* --- small string helpers ------------------------------------------ */

static char *lstrip(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rstrip(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

/* Strip surrounding quotes from `s` in-place.  Accepts "..." or '...'.
 * Returns the stripped string (same pointer, content may move up). */
static char *unquote(char *s)
{
    size_t n = strlen(s);
    if (n >= 2 && ((s[0] == '"' && s[n - 1] == '"') ||
                   (s[0] == '\'' && s[n - 1] == '\''))) {
        s[n - 1] = '\0';
        memmove(s, s + 1, n - 1);
    }
    return s;
}

/* Expand a leading "~/" to $HOME if present.  Writes to `out` (cap).
 * If no tilde, just copies.  Returns bytes written or -1. */
static int expand_tilde(const char *in, char *out, size_t cap)
{
    if (!in || !out || cap < 2) return -1;
    if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
#else
        const char *home = getenv("HOME");
#endif
        if (!home) home = "";
        int n = snprintf(out, cap, "%s%s", home, in + 1);
        return (n <= 0 || (size_t)n >= cap) ? -1 : n;
    }
    int n = snprintf(out, cap, "%s", in);
    return (n <= 0 || (size_t)n >= cap) ? -1 : n;
}

int cos_v106_config_load(cos_v106_config_t *cfg, const char *path)
{
    if (!cfg || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[1024];
    char section[64] = "";
    int line_no = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        /* Strip inline `#` comment (ignore `#` inside quotes for our
         * purposes — config values are short and we don't expect
         * quoted pounds). */
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *p = lstrip(line);
        rstrip(p);
        if (!*p) continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (!end) continue;
            *end = '\0';
            p++;
            char *s = lstrip(p);
            rstrip(s);
            snprintf(section, sizeof section, "%s", s);
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        rstrip(key);
        val = lstrip(val);
        rstrip(val);
        unquote(val);

        /* Dispatch by (section, key). */
        if (strcmp(section, "server") == 0) {
            if (strcmp(key, "host") == 0) {
                snprintf(cfg->host, sizeof cfg->host, "%s", val);
            } else if (strcmp(key, "port") == 0) {
                cfg->port = (int)strtol(val, NULL, 10);
            } else {
                fprintf(stderr, "cos_v106 config: unknown [server].%s at %s:%d\n",
                        key, path, line_no);
            }
        } else if (strcmp(section, "sigma") == 0) {
            if (strcmp(key, "tau_abstain") == 0) {
                cfg->tau_abstain = strtod(val, NULL);
            } else if (strcmp(key, "aggregator") == 0) {
                snprintf(cfg->aggregator, sizeof cfg->aggregator, "%s", val);
            } else {
                fprintf(stderr, "cos_v106 config: unknown [sigma].%s at %s:%d\n",
                        key, path, line_no);
            }
        } else if (strcmp(section, "model") == 0) {
            if (strcmp(key, "gguf_path") == 0) {
                char expanded[1024];
                if (expand_tilde(val, expanded, sizeof expanded) > 0) {
                    snprintf(cfg->gguf_path, sizeof cfg->gguf_path, "%s", expanded);
                }
            } else if (strcmp(key, "n_ctx") == 0) {
                cfg->n_ctx = (int)strtol(val, NULL, 10);
            } else if (strcmp(key, "model_id") == 0) {
                snprintf(cfg->model_id, sizeof cfg->model_id, "%s", val);
            } else {
                fprintf(stderr, "cos_v106 config: unknown [model].%s at %s:%d\n",
                        key, path, line_no);
            }
        } else if (strcmp(section, "web") == 0) {
            if (strcmp(key, "web_root") == 0) {
                char expanded[1024];
                if (expand_tilde(val, expanded, sizeof expanded) > 0) {
                    snprintf(cfg->web_root, sizeof cfg->web_root, "%s", expanded);
                }
            } else {
                fprintf(stderr, "cos_v106 config: unknown [web].%s at %s:%d\n",
                        key, path, line_no);
            }
        } else if (*section) {
            fprintf(stderr, "cos_v106 config: unknown section [%s] at %s:%d\n",
                    section, path, line_no);
        }
    }
    fclose(f);
    return 0;
}
