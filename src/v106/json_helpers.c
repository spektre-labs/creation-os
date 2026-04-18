/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v106 — tiny JSON extraction helpers.
 *
 * The server parses only what it needs from request bodies:
 *   - chat-completion: the last `{"role":"user", ...}` .content.
 *   - completions   : the `prompt` key.
 *   - optional      : `model`, `stream`, `max_tokens`, `temperature`.
 *
 * The helpers below are deliberately naive: linear scan, first match
 * wins for `_get_string/_int/_double`.  They do not attempt to parse
 * nested objects or arrays.  For the flat-ish request bodies OpenAI
 * clients send, that is enough.  For `extract_last_user`, we do a
 * second-pass seek on `"role":"user"` with whitespace tolerance.
 */
#include "server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Locate the first occurrence of `"key"` followed by optional
 * whitespace and `:` in `body`.  Returns pointer to the first
 * character AFTER the colon, or NULL if not found. */
static const char *find_key(const char *body, const char *key)
{
    if (!body || !key) return NULL;
    size_t klen = strlen(key);
    for (const char *p = body; *p; p++) {
        if (*p != '"') continue;
        if (strncmp(p + 1, key, klen) != 0) continue;
        if (p[1 + klen] != '"') continue;
        const char *q = p + 2 + klen;
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q != ':') continue;
        q++;
        while (*q && isspace((unsigned char)*q)) q++;
        return q;
    }
    return NULL;
}

/* Decode a JSON string literal starting at `p` (must point AT the
 * opening quote) into `out`.  Handles \", \\, \n, \r, \t, \/, \".
 * Returns bytes written (excl. NUL) or -1 if the literal is not
 * well-formed or overflows `cap`. */
static int decode_string_at(const char *p, char *out, size_t cap)
{
    if (!p || *p != '"' || cap < 1) return -1;
    p++;
    size_t w = 0;
    while (*p && *p != '"') {
        if (w + 1 >= cap) return -1;
        if (*p == '\\' && p[1]) {
            p++;
            char c;
            switch (*p) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case '\\': c = '\\'; break;
            case '"':  c = '"';  break;
            case '/':  c = '/';  break;
            case 'u': {
                /* Best-effort \uXXXX → UTF-8.  For BMP code points
                 * only.  Surrogate pairs are decoded as two chars. */
                if (!p[1] || !p[2] || !p[3] || !p[4]) return -1;
                unsigned v = 0;
                for (int i = 1; i <= 4; i++) {
                    char h = p[i];
                    v <<= 4;
                    if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
                    else return -1;
                }
                p += 4;
                if (v < 0x80) {
                    if (w + 1 >= cap) return -1;
                    out[w++] = (char)v;
                } else if (v < 0x800) {
                    if (w + 2 >= cap) return -1;
                    out[w++] = (char)(0xC0 | (v >> 6));
                    out[w++] = (char)(0x80 | (v & 0x3F));
                } else {
                    if (w + 3 >= cap) return -1;
                    out[w++] = (char)(0xE0 | (v >> 12));
                    out[w++] = (char)(0x80 | ((v >> 6) & 0x3F));
                    out[w++] = (char)(0x80 | (v & 0x3F));
                }
                p++;
                continue;
            }
            default: c = *p; break;
            }
            out[w++] = c;
            p++;
            continue;
        }
        out[w++] = *p++;
    }
    if (*p != '"') return -1;
    out[w] = '\0';
    return (int)w;
}

int cos_v106_json_get_string(const char *body, const char *key, char *out, size_t cap)
{
    if (!body || !key || !out || cap < 1) return -1;
    const char *v = find_key(body, key);
    if (!v) return -1;
    if (*v != '"') return -1;
    return decode_string_at(v, out, cap);
}

long cos_v106_json_get_int(const char *body, const char *key, long fallback)
{
    const char *v = find_key(body, key);
    if (!v) return fallback;
    while (*v && isspace((unsigned char)*v)) v++;
    if (*v == '"') v++;  /* tolerate quoted numbers */
    char *end = NULL;
    long x = strtol(v, &end, 10);
    return (end == v) ? fallback : x;
}

double cos_v106_json_get_double(const char *body, const char *key, double fallback)
{
    const char *v = find_key(body, key);
    if (!v) return fallback;
    while (*v && isspace((unsigned char)*v)) v++;
    if (*v == '"') v++;
    char *end = NULL;
    double x = strtod(v, &end);
    return (end == v) ? fallback : x;
}

int cos_v106_json_stream_flag(const char *body)
{
    const char *v = find_key(body, "stream");
    if (!v) return 0;
    while (*v && isspace((unsigned char)*v)) v++;
    return (strncmp(v, "true", 4) == 0 &&
            !isalnum((unsigned char)v[4]) && v[4] != '_');
}

int cos_v106_json_extract_last_user(const char *body, char *out, size_t cap)
{
    if (!body || !out || cap < 1) return -1;
    /* Scan for "role":"user" occurrences and keep the last one.
     * This tolerates spaces inside the object separator. */
    const char *last = NULL;
    const char *p = body;
    while ((p = strstr(p, "\"role\"")) != NULL) {
        const char *q = p + 6;
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q != ':') { p++; continue; }
        q++;
        while (*q && isspace((unsigned char)*q)) q++;
        if (strncmp(q, "\"user\"", 6) != 0) { p++; continue; }
        last = p;
        p = q + 6;
    }
    if (!last) return -1;
    /* From `last`, seek forward to the NEXT "content" key. */
    const char *c = strstr(last, "\"content\"");
    if (!c) return -1;
    c += 9;
    while (*c && isspace((unsigned char)*c)) c++;
    if (*c != ':') return -1;
    c++;
    while (*c && isspace((unsigned char)*c)) c++;
    if (*c != '"') return -1;
    return decode_string_at(c, out, cap);
}
