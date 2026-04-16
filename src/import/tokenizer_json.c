/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "tokenizer_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COS_TJ_MAX_BYTES ((size_t)(64u * 1024u * 1024u))

static int skip_ws(const char *s, size_t n, size_t *i)
{
    while (*i < n && isspace((unsigned char)s[*i]))
        (*i)++;
    return (*i < n) ? 0 : -1;
}

static int expect(const char *s, size_t n, size_t *i, char c)
{
    if (*i >= n || s[*i] != c)
        return -1;
    (*i)++;
    return 0;
}

static int parse_string_token(const char *s, size_t n, size_t *i)
{
    if (*i >= n || s[*i] != '"')
        return -1;
    (*i)++;
    while (*i < n) {
        unsigned char c = (unsigned char)s[*i];
        if (c == '"')
            break;
        if (c == '\\') {
            (*i)++;
            if (*i >= n)
                return -1;
        }
        (*i)++;
    }
    if (*i >= n || s[*i] != '"')
        return -1;
    (*i)++;
    return 0;
}

static int parse_number_skip(const char *s, size_t n, size_t *i)
{
    if (*i >= n)
        return -1;
    if (s[*i] == '-')
        (*i)++;
    if (*i >= n || !isdigit((unsigned char)s[*i]))
        return -1;
    while (*i < n && isdigit((unsigned char)s[*i]))
        (*i)++;
    return 0;
}

static const char *find_sub(const char *s, size_t n, const char *needle)
{
    const size_t m = strlen(needle);
    if (m == 0 || m > n)
        return NULL;
    for (size_t i = 0; i + m <= n; i++) {
        if (memcmp(s + i, needle, m) == 0)
            return s + i;
    }
    return NULL;
}

int cos_tokenizer_json_count_vocab(const char *path, uint64_t *out_n)
{
    if (!path || !out_n)
        return -1;
    *out_n = 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long szl = ftell(f);
    if (szl < 0 || (size_t)szl > COS_TJ_MAX_BYTES) {
        fclose(f);
        return -1;
    }
    size_t sz = (size_t)szl;
    rewind(f);
    char *buf = (char *)malloc(sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, sz, f) != sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[sz] = '\0';

    const char *p = find_sub(buf, sz, "\"vocab\"");
    if (!p) {
        free(buf);
        return -1;
    }
    size_t i = (size_t)(p - buf);
    if (memcmp(buf + i, "\"vocab\"", 7) != 0) {
        free(buf);
        return -1;
    }
    i += 7u;
    if (skip_ws(buf, sz, &i) != 0) {
        free(buf);
        return -1;
    }
    if (expect(buf, sz, &i, ':') != 0) {
        free(buf);
        return -1;
    }
    if (skip_ws(buf, sz, &i) != 0) {
        free(buf);
        return -1;
    }
    if (expect(buf, sz, &i, '{') != 0) {
        free(buf);
        return -1;
    }

    uint64_t count = 0;
    for (;;) {
        if (skip_ws(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        if (i < sz && buf[i] == '}')
            break;
        if (parse_string_token(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        if (skip_ws(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        if (expect(buf, sz, &i, ':') != 0) {
            free(buf);
            return -1;
        }
        if (skip_ws(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        if (i < sz && buf[i] == '"') {
            free(buf);
            return -1;
        }
        if (parse_number_skip(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        count++;
        if (skip_ws(buf, sz, &i) != 0) {
            free(buf);
            return -1;
        }
        if (i < sz && buf[i] == ',') {
            i++;
            continue;
        }
        if (i < sz && buf[i] == '}')
            break;
        free(buf);
        return -1;
    }

    free(buf);
    *out_n = count;
    return 0;
}
