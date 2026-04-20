/*
 * σ-Plugin registry + dlopen loader.  See plugin.h for contracts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#define _POSIX_C_SOURCE 200809L

#include "plugin.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    cos_sigma_plugin_t plugin;
    int                live;
} plug_slot_t;

static plug_slot_t g_registry[COS_PLUGIN_REGISTRY_CAP];
static int         g_count = 0;

/* ------------------------------------------------------------------ */

void cos_sigma_plugin_registry_reset(void) {
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        plug_slot_t *s = &g_registry[i];
        if (s->live && s->plugin._lib_handle && !s->plugin._static) {
            dlclose(s->plugin._lib_handle);
        }
        memset(s, 0, sizeof *s);
    }
    g_count = 0;
}

static int plug_find_by_name(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        if (!g_registry[i].live) continue;
        if (g_registry[i].plugin.name &&
            strcmp(g_registry[i].plugin.name, name) == 0)
            return i;
    }
    return -1;
}

static int plug_first_free(void) {
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++)
        if (!g_registry[i].live) return i;
    return -1;
}

static int plug_validate(const cos_sigma_plugin_t *p) {
    if (!p) return COS_PLUGIN_ERR_ARG;
    if (p->abi_version != COS_SIGMA_PLUGIN_ABI) return COS_PLUGIN_ERR_ABI;
    if (!p->name || !p->name[0])                return COS_PLUGIN_ERR_ARG;
    if (!p->version || !p->version[0])          return COS_PLUGIN_ERR_ARG;
    if (p->n_tools < 0 || p->n_tools > COS_PLUGIN_MAX_TOOLS)
        return COS_PLUGIN_ERR_ARG;
    for (int i = 0; i < p->n_tools; i++) {
        if (!p->tools[i].name || !p->tools[i].name[0]) return COS_PLUGIN_ERR_ARG;
        if (!p->tools[i].run)                          return COS_PLUGIN_ERR_ARG;
        if (p->tools[i].risk_level < 0 || p->tools[i].risk_level > 4)
            return COS_PLUGIN_ERR_ARG;
    }
    return COS_PLUGIN_OK;
}

int cos_sigma_plugin_register(const cos_sigma_plugin_t *plugin) {
    int rc = plug_validate(plugin);
    if (rc != COS_PLUGIN_OK) return rc;
    if (plug_find_by_name(plugin->name) >= 0) return COS_PLUGIN_ERR_DUP;
    int slot = plug_first_free();
    if (slot < 0) return COS_PLUGIN_ERR_FULL;
    g_registry[slot].plugin         = *plugin;
    g_registry[slot].plugin._static = 1;
    g_registry[slot].plugin._lib_handle = NULL;
    g_registry[slot].live           = 1;
    g_count++;
    return COS_PLUGIN_OK;
}

int cos_sigma_plugin_load(const char *path) {
    if (!path || !path[0]) return COS_PLUGIN_ERR_ARG;

    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) return COS_PLUGIN_ERR_DLOPEN;

    cos_sigma_plugin_entry_fn entry =
        (cos_sigma_plugin_entry_fn)dlsym(h, "cos_sigma_plugin_entry");
    if (!entry) { dlclose(h); return COS_PLUGIN_ERR_DLSYM; }

    cos_sigma_plugin_t p; memset(&p, 0, sizeof p);
    int rc = entry(&p);
    if (rc != COS_PLUGIN_OK) { dlclose(h); return rc; }

    rc = plug_validate(&p);
    if (rc != COS_PLUGIN_OK) { dlclose(h); return rc; }
    if (plug_find_by_name(p.name) >= 0) { dlclose(h); return COS_PLUGIN_ERR_DUP; }

    int slot = plug_first_free();
    if (slot < 0) { dlclose(h); return COS_PLUGIN_ERR_FULL; }

    g_registry[slot].plugin             = p;
    g_registry[slot].plugin._lib_handle = h;
    g_registry[slot].plugin._static     = 0;
    g_registry[slot].live               = 1;
    g_count++;
    return COS_PLUGIN_OK;
}

int cos_sigma_plugin_remove(const char *name) {
    int i = plug_find_by_name(name);
    if (i < 0) return COS_PLUGIN_ERR_NOTFOUND;
    plug_slot_t *s = &g_registry[i];
    if (s->plugin._lib_handle && !s->plugin._static)
        dlclose(s->plugin._lib_handle);
    memset(s, 0, sizeof *s);
    g_count--;
    return COS_PLUGIN_OK;
}

int cos_sigma_plugin_count(void) { return g_count; }

const cos_sigma_plugin_t *cos_sigma_plugin_at(int idx) {
    if (idx < 0) return NULL;
    int seen = 0;
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        if (!g_registry[i].live) continue;
        if (seen == idx) return &g_registry[i].plugin;
        seen++;
    }
    return NULL;
}

/* Canonical JSON listing. */
int cos_sigma_plugin_list_json(char *out, size_t out_cap) {
    if (!out || out_cap < 16) return -1;
    size_t j = 0;
    int n = snprintf(out + j, out_cap - j, "{\"count\":%d,\"plugins\":[", g_count);
    if (n < 0 || (size_t)n >= out_cap - j) return -1;
    j += (size_t)n;

    int first = 1;
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        if (!g_registry[i].live) continue;
        const cos_sigma_plugin_t *p = &g_registry[i].plugin;
        n = snprintf(out + j, out_cap - j,
                     "%s{\"name\":\"%s\",\"version\":\"%s\","
                     "\"tools\":%d,\"has_substrate\":%s,"
                     "\"has_custom_sigma\":%s,\"has_codex\":%s,"
                     "\"origin\":\"%s\"}",
                     first ? "" : ",",
                     p->name ? p->name : "",
                     p->version ? p->version : "",
                     p->n_tools,
                     p->substrate       ? "true"  : "false",
                     p->custom_sigma    ? "true"  : "false",
                     (p->codex_extension && p->codex_extension[0]) ? "true" : "false",
                     p->_static ? "static" : "dlopen");
        if (n < 0 || (size_t)n >= out_cap - j) return -1;
        j += (size_t)n;
        first = 0;
    }
    n = snprintf(out + j, out_cap - j, "]}");
    if (n < 0 || (size_t)n >= out_cap - j) return -1;
    j += (size_t)n;
    return (int)j;
}

/* ------------------------------------------------------------------ *
 *  Self-test                                                         *
 * ------------------------------------------------------------------ */

static int st_tool_fn(const char *const *argv, char **out_stdout, char **out_stderr) {
    (void)argv; (void)out_stderr;
    if (out_stdout) {
        *out_stdout = (char *)malloc(6);
        if (*out_stdout) { memcpy(*out_stdout, "hello", 6); }
    }
    return 0;
}

static float st_sigma_fn(void *ctx) { (void)ctx; return 0.25f; }

int cos_sigma_plugin_self_test(void) {
    cos_sigma_plugin_registry_reset();

    cos_sigma_plugin_t p1; memset(&p1, 0, sizeof p1);
    p1.abi_version = COS_SIGMA_PLUGIN_ABI;
    p1.name        = "st-sample";
    p1.version     = "0.1.0";
    p1.author      = "self-test";
    p1.n_tools     = 1;
    p1.tools[0].name       = "echo";
    p1.tools[0].doc        = "print hello";
    p1.tools[0].risk_level = 0;
    p1.tools[0].run        = st_tool_fn;
    p1.custom_sigma        = st_sigma_fn;
    p1.codex_extension     = "be kind";

    if (cos_sigma_plugin_register(&p1) != COS_PLUGIN_OK) return -1;
    if (cos_sigma_plugin_count() != 1) return -2;

    /* Duplicate name must be refused. */
    if (cos_sigma_plugin_register(&p1) != COS_PLUGIN_ERR_DUP) return -3;

    /* ABI mismatch must be refused. */
    cos_sigma_plugin_t p_bad = p1;
    p_bad.abi_version = COS_SIGMA_PLUGIN_ABI + 1;
    p_bad.name = "st-bad";
    if (cos_sigma_plugin_register(&p_bad) != COS_PLUGIN_ERR_ABI) return -4;

    /* Lookup + remove round-trip. */
    const cos_sigma_plugin_t *rp = cos_sigma_plugin_at(0);
    if (!rp || strcmp(rp->name, "st-sample") != 0) return -5;
    if (cos_sigma_plugin_remove("st-sample") != COS_PLUGIN_OK) return -6;
    if (cos_sigma_plugin_count() != 0) return -7;
    if (cos_sigma_plugin_remove("st-sample") != COS_PLUGIN_ERR_NOTFOUND) return -8;

    /* Fill registry; next register must get FULL. */
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        char  nm[32];
        snprintf(nm, sizeof nm, "st-fill-%02d", i);
        cos_sigma_plugin_t p; memset(&p, 0, sizeof p);
        p.abi_version = COS_SIGMA_PLUGIN_ABI;
        p.name = strdup(nm);
        p.version = "0.0.1";
        p.n_tools = 0;
        if (cos_sigma_plugin_register(&p) != COS_PLUGIN_OK) {
            free((void *)p.name);
            return -10 - i;
        }
    }
    cos_sigma_plugin_t p_overflow; memset(&p_overflow, 0, sizeof p_overflow);
    p_overflow.abi_version = COS_SIGMA_PLUGIN_ABI;
    p_overflow.name        = "st-overflow";
    p_overflow.version     = "0.0.1";
    if (cos_sigma_plugin_register(&p_overflow) != COS_PLUGIN_ERR_FULL) return -9;

    /* Clean up strdup'd names. */
    for (int i = 0; i < COS_PLUGIN_REGISTRY_CAP; i++) {
        if (g_registry[i].live && g_registry[i].plugin.name) {
            free((void *)g_registry[i].plugin.name);
        }
    }
    cos_sigma_plugin_registry_reset();
    return 0;
}
