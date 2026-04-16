/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "model_registry.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **pp)
{
    while (*pp && isspace((unsigned char)**pp))
        (*pp)++;
}

static cos_model_entry_t *alloc_push(cos_model_registry_t *reg)
{
    if (reg->n >= COS_MODEL_MAX_ENTRIES)
        return NULL;
    cos_model_entry_t *e = &reg->entries[reg->n];
    memset(e, 0, sizeof(*e));
    reg->n++;
    return e;
}

static void set_entry(cos_model_entry_t *e,
    const char *name,
    const char *binary,
    const char *model_path,
    int is_local,
    const char *api_base,
    float cost_per_token)
{
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
    if (name)
        (void)snprintf(e->name, sizeof e->name, "%s", name);
    if (binary)
        (void)snprintf(e->binary, sizeof e->binary, "%s", binary);
    if (model_path)
        (void)snprintf(e->model_path, sizeof e->model_path, "%s", model_path);
    e->is_local = is_local ? 1 : 0;
    if (api_base)
        (void)snprintf(e->api_base, sizeof e->api_base, "%s", api_base);
    e->cost_per_token = cost_per_token;
}

void cos_model_registry_init_defaults(cos_model_registry_t *reg)
{
    if (!reg)
        return;
    memset(reg, 0, sizeof(*reg));
    const char *cli = getenv("COS_BITNET_CLI");
    const char *mdl = getenv("COS_BITNET_MODEL");
    if (!cli || !*cli)
        cli = "external/BitNet/build/bin/llama-cli";
    if (!mdl || !*mdl)
        mdl = "models/bitnet-2b4t/model.gguf";
    cos_model_entry_t *b = alloc_push(reg);
    set_entry(b, "bitnet-2b4t", cli, mdl, 1, "", 0.0f);

    const char *fb = getenv("COS_V33_FALLBACK_NAME");
    const char *fb_bin = getenv("COS_V33_FALLBACK_CLI");
    const char *fb_mdl = getenv("COS_V33_FALLBACK_MODEL");
    const char *fb_api = getenv("COS_V33_FALLBACK_API_BASE");
    if (fb && *fb) {
        cos_model_entry_t *e = alloc_push(reg);
        int local = (fb_bin && *fb_bin) ? 1 : 0;
        float cpt = 0.0f;
        const char *cpt_env = getenv("COS_V33_FALLBACK_COST_PER_TOKEN");
        if (cpt_env && *cpt_env) {
            errno = 0;
            char *end = NULL;
            double v = strtod(cpt_env, &end);
            if (end != cpt_env && errno == 0)
                cpt = (float)v;
        }
        set_entry(e, fb, fb_bin ? fb_bin : "", fb_mdl ? fb_mdl : "", local,
            fb_api ? fb_api : "", cpt);
    }
}

const cos_model_entry_t *cos_model_find(const cos_model_registry_t *reg, const char *name)
{
    if (!reg || !name)
        return NULL;
    for (size_t i = 0; i < reg->n; i++) {
        if (!strcmp(reg->entries[i].name, name))
            return &reg->entries[i];
    }
    return NULL;
}

int cos_model_has_named_fallback(const cos_model_registry_t *reg, const char *fallback_name)
{
    const cos_model_entry_t *e = cos_model_find(reg, fallback_name);
    if (!e)
        return 0;
    if (e->is_local)
        return e->binary[0] != '\0' && e->model_path[0] != '\0';
    return e->api_base[0] != '\0';
}

static int extract_json_string_in_object(const char *obj, const char *key, char *out, size_t cap)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(obj, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    if (*p != '"')
        return -1;
    p++;
    size_t w = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            if (w + 1 >= cap)
                return -1;
            out[w++] = *p++;
            continue;
        }
        if (w + 1 >= cap)
            return -1;
        out[w++] = *p++;
    }
    out[w] = '\0';
    return (int)w;
}

static int extract_json_number_in_object(const char *obj, const char *key, float *out)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(obj, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    errno = 0;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p || errno == ERANGE)
        return -1;
    *out = (float)v;
    return 0;
}

static int extract_json_bool_in_object(const char *obj, const char *key, int *out)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(obj, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    if (!strncmp(p, "true", 4)) {
        *out = 1;
        return 0;
    }
    if (!strncmp(p, "false", 5)) {
        *out = 0;
        return 0;
    }
    return -1;
}

static int upsert(cos_model_registry_t *reg, cos_model_entry_t *tmp)
{
    for (size_t i = 0; i < reg->n; i++) {
        if (!strcmp(reg->entries[i].name, tmp->name)) {
            reg->entries[i] = *tmp;
            return 0;
        }
    }
    if (reg->n >= COS_MODEL_MAX_ENTRIES)
        return -1;
    reg->entries[reg->n++] = *tmp;
    return 0;
}

int cos_model_registry_load(const char *path, cos_model_registry_t *reg)
{
    if (!reg)
        return -1;
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 1 << 20) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    const char *p = buf;
    while ((p = strstr(p, "\"name\"")) != NULL) {
        const char *start = p;
        while (start > buf && *start != '{')
            start--;
        if (*start != '{')
            break;
        const char *end = strchr(p, '}');
        if (!end)
            break;
        char obj[2048];
        size_t len = (size_t)(end - start + 1);
        if (len >= sizeof obj)
            break;
        memcpy(obj, start, len);
        obj[len] = '\0';

        cos_model_entry_t tmp;
        memset(&tmp, 0, sizeof tmp);
        if (extract_json_string_in_object(obj, "name", tmp.name, sizeof tmp.name) < 0) {
            p = end + 1;
            continue;
        }
        (void)extract_json_string_in_object(obj, "binary", tmp.binary, sizeof tmp.binary);
        (void)extract_json_string_in_object(obj, "model_path", tmp.model_path, sizeof tmp.model_path);
        (void)extract_json_string_in_object(obj, "api_base", tmp.api_base, sizeof tmp.api_base);
        int il = tmp.is_local;
        if (extract_json_bool_in_object(obj, "is_local", &il) == 0)
            tmp.is_local = il;
        float cpt = 0.0f;
        if (extract_json_number_in_object(obj, "cost_per_token", &cpt) == 0)
            tmp.cost_per_token = cpt;
        (void)upsert(reg, &tmp);
        p = end + 1;
    }
    free(buf);
    return 0;
}
