/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "config_persist.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int cfg_dir(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!buf || cap < 32)
        return -1;
    if (h == NULL || h[0] == '\0')
        h = "/tmp";
    if (snprintf(buf, cap, "%s/.cos", h) >= (int)cap)
        return -1;
    return 0;
}

static int cfg_path(char *buf, size_t cap)
{
    if (cfg_dir(buf, cap) != 0)
        return -1;
    {
        size_t n = strlen(buf);
        if (n + 16 >= cap)
            return -1;
        snprintf(buf + n, cap - n, "/config.json");
    }
    return 0;
}

static float json_float_after(const char *json, const char *key, int *found)
{
    char pat[80];
    const char *p;

    if (found)
        *found = 0;
    if (!json || !key)
        return NAN;
    if (snprintf(pat, sizeof pat, "\"%s\":", key) >= (int)sizeof pat)
        return NAN;
    p = strstr(json, pat);
    if (p == NULL)
        return NAN;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t')
        ++p;
    if (found)
        *found = 1;
    return (float)strtod(p, NULL);
}

static int json_int_after(const char *json, const char *key, int *found)
{
    float f = json_float_after(json, key, found);
    if (f != f || (found && !*found))
        return 0;
    return (int)(f + (f >= 0.f ? 0.5f : -0.5f));
}

void cos_omega_runtime_config_default(cos_omega_runtime_config_t *c)
{
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    cos_evolver_config_default(&c->ev);
    {
        const char *m = getenv("COS_BITNET_CHAT_MODEL");
        if (m != NULL && m[0] != '\0')
            snprintf(c->preferred_model, sizeof c->preferred_model, "%s", m);
        else
            snprintf(c->preferred_model, sizeof c->preferred_model, "%s",
                     "default");
    }
    c->last_episode   = 0;
    c->last_kappa     = 0.f;
    c->ignition_count = 0;
}

int cos_omega_runtime_config_load(cos_omega_runtime_config_t *c)
{
    char   path[768];
    FILE  *fp;
    long   sz;
    char  *buf;
    float  ta, tr, tsl, tsh;

    if (!c)
        return -1;
    cos_omega_runtime_config_default(c);
    if (cfg_path(path, sizeof path) != 0)
        return -1;
    fp = fopen(path, "r");
    if (!fp)
        return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    sz = ftell(fp);
    if (sz < 4 || sz > 65536L) {
        fclose(fp);
        return -1;
    }
    rewind(fp);
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    if (fread(buf, 1u, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    buf[sz] = '\0';

    {
        int fk;
        ta = json_float_after(buf, "tau_accept", &fk);
        if (fk && ta > 0.01f && ta < 0.99f)
            c->ev.tau_accept = ta;
    }
    {
        int fk;
        tr = json_float_after(buf, "tau_rethink", &fk);
        if (fk && tr > 0.02f && tr < 0.99f)
            c->ev.tau_rethink = tr;
    }
    {
        int fk;
        tsl = json_float_after(buf, "semantic_temp_low", &fk);
        if (fk && tsl > 1e-4f && tsl < 1.f)
            c->ev.semantic_temp_low = tsl;
    }
    {
        int fk;
        tsh = json_float_after(buf, "semantic_temp_high", &fk);
        if (fk && tsh > 0.5f && tsh < 4.f)
            c->ev.semantic_temp_high = tsh;
    }
    {
        int fk;
        int mt = json_int_after(buf, "semantic_max_tokens", &fk);
        if (fk && mt >= 8 && mt <= 8192)
            c->ev.semantic_max_tokens = mt;
    }
    {
        int fk;
        int pt = json_int_after(buf, "preferred_tier", &fk);
        if (fk && pt >= 0 && pt <= 16)
            c->ev.preferred_tier = pt;
    }
    {
        const char *pk = "\"preferred_model\":\"";
        const char *p = strstr(buf, pk);
        if (p) {
            char  tmp[96];
            size_t i = 0;
            p += strlen(pk);
            while (*p && *p != '"' && i + 1 < sizeof tmp)
                tmp[i++] = *p++;
            tmp[i] = '\0';
            if (i > 0)
                snprintf(c->preferred_model, sizeof c->preferred_model, "%s",
                         tmp);
        }
    }
    {
        int fk;
        c->last_episode = json_int_after(buf, "last_episode", &fk);
        (void)fk;
    }
    {
        int fk;
        float lk = json_float_after(buf, "last_kappa", &fk);
        if (fk)
            c->last_kappa = lk;
    }
    {
        int fk;
        c->ignition_count = json_int_after(buf, "ignition_count", &fk);
        (void)fk;
    }
    free(buf);
    return 0;
}

int cos_omega_runtime_config_save(const cos_omega_runtime_config_t *c)
{
    char         dir[512], path[768], tmp[768];
    FILE        *fp;
    const cos_evolver_config_t *e;

    if (!c)
        return -1;
    if (cfg_dir(dir, sizeof dir) != 0)
        return -1;
#if defined(__unix__) || defined(__APPLE__)
    if (mkdir(dir, 0700) != 0 && errno != EEXIST)
        return -1;
#endif
    if (cfg_path(path, sizeof path) != 0)
        return -1;
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
        return -1;
    fp = fopen(tmp, "w");
    if (!fp)
        return -1;
    e = &c->ev;
    {
        char esc[256];
        size_t i, j;
        for (i = 0, j = 0; c->preferred_model[i] && j + 2 < sizeof esc; i++) {
            if (c->preferred_model[i] == '"' || c->preferred_model[i] == '\\')
                esc[j++] = '\\';
            esc[j++] = c->preferred_model[i];
        }
        esc[j] = '\0';
        fprintf(fp,
                "{\n  \"tau_accept\":%.6f,\n  \"tau_rethink\":%.6f,\n"
                "  \"semantic_temp_low\":%.6f,\n  \"semantic_temp_high\":%.6f,\n"
                "  \"semantic_max_tokens\":%d,\n  \"preferred_tier\":%d,\n"
                "  \"preferred_model\":\"%s\",\n"
                "  \"last_episode\":%d,\n  \"last_kappa\":%.6f,\n"
                "  \"ignition_count\":%d\n}\n",
                (double)e->tau_accept, (double)e->tau_rethink,
                (double)e->semantic_temp_low, (double)e->semantic_temp_high,
                e->semantic_max_tokens, e->preferred_tier, esc,
                c->last_episode, (double)c->last_kappa, c->ignition_count);
    }
    if (fclose(fp) != 0)
        return -1;
    if (rename(tmp, path) != 0)
        return -1;
    return 0;
}
