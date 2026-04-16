/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "schema_validator.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *slurp_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 1 << 20) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    if (out_len)
        *out_len = n;
    return buf;
}

static void skip_ws(const char **pp)
{
    while (*pp && isspace((unsigned char)**pp))
        (*pp)++;
}

static int json_obj_has_key(const char *j, const char *key)
{
    char pat[128];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return 0;
    const char *p = strstr(j, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    skip_ws(&p);
    return *p == ':';
}

static int extract_const_string(const char *schema, const char *prop_key, char *out, size_t cap)
{
    char anchor[160];
    if (snprintf(anchor, sizeof anchor, "\"%s\"", prop_key) >= (int)sizeof anchor)
        return -1;
    const char *props = strstr(schema, "\"properties\"");
    const char *scan = props ? props : schema;
    const char *p = strstr(scan, anchor);
    if (!p)
        return -1;
    const char *c = strstr(p, "\"const\"");
    if (!c)
        return -1;
    c = strstr(c, ":");
    if (!c)
        return -1;
    c++;
    skip_ws(&c);
    if (*c != '"')
        return -1;
    c++;
    size_t w = 0;
    while (*c && *c != '"') {
        if (*c == '\\' && c[1]) {
            c++;
            if (w + 1 >= cap)
                return -1;
            out[w++] = *c++;
            continue;
        }
        if (w + 1 >= cap)
            return -1;
        out[w++] = *c++;
    }
    out[w] = '\0';
    return (int)w;
}

static int extract_required_keys(const char *schema, char keys[8][64], int *nkeys)
{
    const char *rq = strstr(schema, "\"required\"");
    if (!rq)
        return -1;
    const char *lb = strchr(rq, '[');
    if (!lb)
        return -1;
    const char *rb = strchr(lb, ']');
    if (!rb)
        return -1;
    int n = 0;
    const char *p = lb + 1;
    while (p < rb && n < 8) {
        while (p < rb && *p != '"')
            p++;
        if (p >= rb)
            break;
        p++;
        size_t i = 0;
        while (p < rb && *p != '"' && i + 1 < 64) {
            if (*p == '\\' && p + 1 < rb) {
                p++;
                keys[n][i++] = *p++;
                continue;
            }
            keys[n][i++] = *p++;
        }
        keys[n][i] = '\0';
        if (i > 0)
            n++;
        if (p < rb && *p == '"')
            p++;
    }
    *nkeys = n;
    return n > 0 ? 0 : -1;
}

static int tool_name_matches_const(const char *tool, const char *schema)
{
    char expect[128];
    if (extract_const_string(schema, "name", expect, sizeof expect) < 0)
        return 1;
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"name\"") >= (int)sizeof pat)
        return 0;
    const char *p = strstr(tool, pat);
    if (!p)
        return 0;
    p = strchr(p, ':');
    if (!p)
        return 0;
    p++;
    skip_ws(&p);
    if (*p != '"')
        return 0;
    p++;
    char got[128];
    size_t w = 0;
    while (*p && *p != '"' && w + 1 < sizeof got) {
        if (*p == '\\' && p[1]) {
            p++;
            got[w++] = *p++;
            continue;
        }
        got[w++] = *p++;
    }
    got[w] = '\0';
    return !strcmp(got, expect);
}

static int arguments_has_required(const char *tool, const char *schema)
{
    char inner_keys[8][64];
    int nk = 0;
    const char *args = strstr(schema, "\"arguments\"");
    if (!args)
        return 1;
    const char *props = strstr(args, "\"properties\"");
    if (!props)
        return 1;
    const char *req = strstr(props, "\"required\"");
    if (!req)
        return 1;
    const char *lb = strchr(req, '[');
    const char *rb = lb ? strchr(lb, ']') : NULL;
    if (!lb || !rb)
        return 1;
    nk = 0;
    const char *ip = lb + 1;
    while (ip < rb && nk < 8) {
        while (ip < rb && *ip != '"')
            ip++;
        if (ip >= rb)
            break;
        ip++;
        size_t i = 0;
        while (ip < rb && *ip != '"' && i + 1 < 64) {
            if (*ip == '\\' && ip + 1 < rb) {
                ip++;
                inner_keys[nk][i++] = *ip++;
                continue;
            }
            inner_keys[nk][i++] = *ip++;
        }
        inner_keys[nk][i] = '\0';
        if (i > 0)
            nk++;
        if (ip < rb && *ip == '"')
            ip++;
    }
    if (nk == 0)
        return 1;

    const char *at = strstr(tool, "\"arguments\"");
    if (!at)
        return 0;
    const char *brace = strchr(at, '{');
    if (!brace)
        return 0;
    const char *end = strchr(brace, '}');
    if (!end)
        return 0;
    size_t frag_len = (size_t)(end - brace + 1);
    char *frag = (char *)malloc(frag_len + 1u);
    if (!frag)
        return 0;
    memcpy(frag, brace, frag_len);
    frag[frag_len] = '\0';
    int ok = 1;
    for (int i = 0; i < nk; i++) {
        if (!json_obj_has_key(frag, inner_keys[i])) {
            ok = 0;
            break;
        }
    }
    free(frag);
    return ok;
}

int cos_schema_validate_tool_json(const char *tool_json, const char *schema_path)
{
    if (!tool_json || !schema_path)
        return 0;
    char *schema = slurp_file(schema_path, NULL);
    if (!schema)
        return 0;

    char keys[8][64];
    int nkeys = 0;
    if (extract_required_keys(schema, keys, &nkeys) != 0) {
        free(schema);
        return 0;
    }
    int ok = 1;
    for (int i = 0; i < nkeys; i++) {
        if (!json_obj_has_key(tool_json, keys[i])) {
            ok = 0;
            break;
        }
    }
    if (ok)
        ok = tool_name_matches_const(tool_json, schema);
    if (ok)
        ok = arguments_has_required(tool_json, schema);

    free(schema);
    return ok;
}
