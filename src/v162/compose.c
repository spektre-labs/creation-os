/*
 * v162 σ-Compose — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "compose.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void copy_bounded(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    size_t n = 0;
    if (src) { while (src[n] && n + 1 < cap) { dst[n] = src[n]; ++n; } }
    dst[n] = '\0';
}

/* ---------- baked manifest fixture ----------------------------- */

typedef struct {
    const char *id;
    const char *provides;
    const char *requires_[COS_V162_MAX_DEPS];
    int         latency;
    int         memory;
    const char *channels[COS_V162_MAX_CHANNELS];
} v162_tpl_t;

static const v162_tpl_t TPL[] = {
    /* Core (no dependencies). */
    {"v101", "sigma_channels",  {NULL}, 0,  50,  {"sigma_entropy", "sigma_n_effective", NULL}},
    {"v106", "http_server",     {"v101", NULL}, 5,  40,  {NULL}},
    {"v108", "dashboard",       {"v106", NULL}, 2,  20,  {NULL}},
    {"v111", "sigma_gate",      {"v101", NULL}, 1,  5,   {"sigma_product", NULL}},
    {"v115", "episodic_memory", {"v106", NULL}, 3,  80,  {"sigma_recall", NULL}},
    {"v117", "paged_kv",        {"v101", NULL}, 1,  30,  {NULL}},
    {"v119", "planner",         {"v111", NULL}, 8,  25,  {"sigma_plan", NULL}},
    {"v124", "continual_lora",  {"v111", "v115", NULL}, 12, 40, {"sigma_rsi", NULL}},
    {"v128", "swarm_node",      {"v106", "v111", NULL}, 6, 30, {"sigma_swarm", NULL}},
    {"v135", "uncertainty",     {"v111", NULL}, 2,  10,  {"sigma_uncertainty", NULL}},
    {"v140", "meta",            {"v111", "v135", NULL}, 3, 15, {"sigma_meta", NULL}},
    {"v150", "swarm_debate",    {"v128", "v106", NULL}, 50, 60, {"sigma_collective", "sigma_consensus", NULL}},
    {"v152", "corpus_qa",       {"v115", "v118", NULL}, 20, 100, {"sigma_corpus", NULL}},
    {"v118", "vision_encoder",  {"v101", NULL}, 15, 200, {NULL}},
    {"v153", "identity",        {"v111", "v124", NULL}, 4, 10, {"sigma_identity", NULL}},
    /* Observability (v159/v160) live outside profiles but are
     * reachable from custom roots if wanted. */
    {"v159", "self_heal",       {"v106", NULL}, 2,  8,   {"sigma_health", NULL}},
    {"v160", "telemetry",       {"v106", NULL}, 1,  5,   {NULL}},
};
#define V162_N_TPL ((int)(sizeof(TPL)/sizeof(TPL[0])))

/* ---------- profile roots -------------------------------------- */

typedef struct {
    cos_v162_profile_kind_t kind;
    const char *name;
    const char *roots[COS_V162_MAX_ROOTS];
} v162_profile_tpl_t;

static const v162_profile_tpl_t PROFILE_TPL[] = {
    {COS_V162_PROFILE_LEAN,       "lean",
        {"v106", "v111", NULL}},
    {COS_V162_PROFILE_RESEARCHER, "researcher",
        {"v106", "v111", "v115", "v118", "v119", "v152", NULL}},
    {COS_V162_PROFILE_SOVEREIGN,  "sovereign",
        {"v106", "v108", "v111", "v115", "v117", "v118", "v119",
         "v124", "v128", "v135", "v140", "v150", "v152", "v153", NULL}},
    {COS_V162_PROFILE_CUSTOM,     "custom", {NULL}},
};

/* ---------- helpers -------------------------------------------- */

const cos_v162_manifest_t *cos_v162_composer_lookup(
    const cos_v162_composer_t *c, const char *kernel) {
    if (!c || !kernel) return NULL;
    for (int i = 0; i < c->n_manifests; ++i) {
        if (strcmp(c->manifests[i].id, kernel) == 0)
            return &c->manifests[i];
    }
    return NULL;
}

static int manifest_index(const cos_v162_composer_t *c, const char *kernel) {
    if (!c || !kernel) return -1;
    for (int i = 0; i < c->n_manifests; ++i) {
        if (strcmp(c->manifests[i].id, kernel) == 0) return i;
    }
    return -1;
}

bool cos_v162_composer_is_enabled(const cos_v162_composer_t *c,
                                  const char *kernel) {
    if (!c || !kernel) return false;
    for (int i = 0; i < c->n_enabled; ++i) {
        if (strcmp(c->enabled_ids[i], kernel) == 0) return true;
    }
    return false;
}

static int count_strs(const char *const *arr) {
    int n = 0;
    while (arr && arr[n]) ++n;
    return n;
}

/* ---------- init ----------------------------------------------- */

void cos_v162_composer_init(cos_v162_composer_t *c) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < V162_N_TPL && c->n_manifests < COS_V162_MAX_KERNELS; ++i) {
        cos_v162_manifest_t *m = &c->manifests[c->n_manifests++];
        copy_bounded(m->id,       TPL[i].id,       sizeof(m->id));
        copy_bounded(m->provides, TPL[i].provides, sizeof(m->provides));
        m->n_requires = count_strs(TPL[i].requires_);
        for (int j = 0; j < m->n_requires; ++j) {
            copy_bounded(m->requires[j], TPL[i].requires_[j],
                         sizeof(m->requires[j]));
        }
        m->added_latency_ms = TPL[i].latency;
        m->added_memory_mb  = TPL[i].memory;
        m->n_channels = count_strs(TPL[i].channels);
        for (int j = 0; j < m->n_channels; ++j) {
            copy_bounded(m->channels[j], TPL[i].channels[j],
                         sizeof(m->channels[j]));
        }
        m->enabled = false;
    }
    for (int i = 0; i < (int)(sizeof(PROFILE_TPL)/sizeof(PROFILE_TPL[0])) &&
                    c->n_profiles < COS_V162_MAX_PROFILES; ++i) {
        cos_v162_profile_t *p = &c->profiles[c->n_profiles++];
        p->kind = PROFILE_TPL[i].kind;
        copy_bounded(p->name, PROFILE_TPL[i].name, sizeof(p->name));
        p->n_roots = count_strs(PROFILE_TPL[i].roots);
        for (int j = 0; j < p->n_roots; ++j) {
            copy_bounded(p->roots[j], PROFILE_TPL[i].roots[j],
                         sizeof(p->roots[j]));
        }
    }
}

/* ---------- cycle detection ------------------------------------ */

static bool dfs_has_cycle(const cos_v162_composer_t *c, int idx,
                          int *state /*0=white,1=gray,2=black*/ ,
                          char *msg, size_t cap) {
    if (idx < 0) return false;
    if (state[idx] == 1) {
        snprintf(msg, cap, "cycle on %s", c->manifests[idx].id);
        return true;
    }
    if (state[idx] == 2) return false;
    state[idx] = 1;
    const cos_v162_manifest_t *m = &c->manifests[idx];
    for (int i = 0; i < m->n_requires; ++i) {
        int ch = manifest_index(c, m->requires[i]);
        if (ch < 0) continue;
        if (dfs_has_cycle(c, ch, state, msg, cap)) return true;
    }
    state[idx] = 2;
    return false;
}

/* ---------- closure BFS ---------------------------------------- */

static int add_enabled(cos_v162_composer_t *c, const char *id) {
    if (cos_v162_composer_is_enabled(c, id)) return 0;
    if (c->n_enabled >= COS_V162_MAX_KERNELS) return -1;
    copy_bounded(c->enabled_ids[c->n_enabled++], id,
                 sizeof(c->enabled_ids[0]));
    const cos_v162_manifest_t *m = cos_v162_composer_lookup(c, id);
    if (m) {
        c->total_latency_ms += m->added_latency_ms;
        c->total_memory_mb  += m->added_memory_mb;
    }
    int idx = manifest_index(c, id);
    if (idx >= 0) c->manifests[idx].enabled = true;
    return 1;
}

static int closure(cos_v162_composer_t *c, const char *root) {
    int idx = manifest_index(c, root);
    if (idx < 0) return -1;
    /* Recursive DFS add.  Bounded (≤ MAX_KERNELS). */
    if (cos_v162_composer_is_enabled(c, root)) return 0;
    const cos_v162_manifest_t *m = &c->manifests[idx];
    for (int i = 0; i < m->n_requires; ++i) {
        int rc = closure(c, m->requires[i]);
        if (rc < 0) return rc;
    }
    return add_enabled(c, root);
}

int cos_v162_composer_select(cos_v162_composer_t *c,
                             cos_v162_profile_kind_t kind) {
    if (!c) return -1;
    c->n_enabled = 0;
    c->total_latency_ms = 0;
    c->total_memory_mb  = 0;
    for (int i = 0; i < c->n_manifests; ++i) c->manifests[i].enabled = false;
    c->cycle_detected   = false;
    c->cycle_msg[0]     = '\0';
    c->active_profile   = kind;

    /* Cycle check first (over the *whole* declared graph). */
    int state[COS_V162_MAX_KERNELS] = {0};
    for (int i = 0; i < c->n_manifests; ++i) {
        if (dfs_has_cycle(c, i, state, c->cycle_msg,
                          sizeof(c->cycle_msg))) {
            c->cycle_detected = true;
            return -1;
        }
    }

    /* Locate profile. */
    const cos_v162_profile_t *p = NULL;
    for (int i = 0; i < c->n_profiles; ++i) {
        if (c->profiles[i].kind == kind) { p = &c->profiles[i]; break; }
    }
    if (!p) return -1;

    for (int i = 0; i < p->n_roots; ++i) {
        int rc = closure(c, p->roots[i]);
        if (rc < 0) return -1;
    }
    return c->n_enabled;
}

int cos_v162_composer_set_custom(cos_v162_composer_t *c, const char *roots) {
    if (!c || !roots) return -1;
    cos_v162_profile_t *p = NULL;
    for (int i = 0; i < c->n_profiles; ++i) {
        if (c->profiles[i].kind == COS_V162_PROFILE_CUSTOM) {
            p = &c->profiles[i]; break;
        }
    }
    if (!p) return -1;
    p->n_roots = 0;
    char  tmp[256];
    copy_bounded(tmp, roots, sizeof(tmp));
    char *ctx = NULL;
    for (char *tok = strtok_r(tmp, ",", &ctx); tok && p->n_roots < COS_V162_MAX_ROOTS;
         tok = strtok_r(NULL, ",", &ctx)) {
        while (*tok == ' ') ++tok;
        copy_bounded(p->roots[p->n_roots++], tok, sizeof(p->roots[0]));
    }
    return p->n_roots;
}

/* ---------- hot-swap ------------------------------------------- */

int cos_v162_composer_enable(cos_v162_composer_t *c, const char *kernel) {
    if (!c || !kernel) return -1;
    if (manifest_index(c, kernel) < 0) return -1;
    int before = c->n_enabled;
    if (closure(c, kernel) < 0) return -1;
    (void)before;
    if (c->n_events < (int)(sizeof(c->events)/sizeof(c->events[0]))) {
        cos_v162_event_t *e = &c->events[c->n_events++];
        e->kind = COS_V162_EVENT_ENABLE;
        copy_bounded(e->kernel, kernel, sizeof(e->kernel));
        const cos_v162_manifest_t *m = cos_v162_composer_lookup(c, kernel);
        e->added_latency_ms = m ? m->added_latency_ms : 0;
        e->added_memory_mb  = m ? m->added_memory_mb  : 0;
    }
    return 0;
}

int cos_v162_composer_disable(cos_v162_composer_t *c, const char *kernel) {
    if (!c || !kernel) return -1;
    if (!cos_v162_composer_is_enabled(c, kernel)) return -1;
    /* Reject if another enabled kernel depends on this one. */
    for (int i = 0; i < c->n_manifests; ++i) {
        const cos_v162_manifest_t *m = &c->manifests[i];
        if (!m->enabled) continue;
        if (strcmp(m->id, kernel) == 0) continue;
        for (int j = 0; j < m->n_requires; ++j) {
            if (strcmp(m->requires[j], kernel) == 0) return -2;
        }
    }
    /* Drop from enabled_ids and recompute σ budget. */
    int idx = -1;
    for (int i = 0; i < c->n_enabled; ++i) {
        if (strcmp(c->enabled_ids[i], kernel) == 0) { idx = i; break; }
    }
    if (idx < 0) return -1;
    const cos_v162_manifest_t *m = cos_v162_composer_lookup(c, kernel);
    if (m) {
        c->total_latency_ms -= m->added_latency_ms;
        c->total_memory_mb  -= m->added_memory_mb;
    }
    for (int i = idx; i < c->n_enabled - 1; ++i) {
        memcpy(c->enabled_ids[i], c->enabled_ids[i + 1],
               sizeof(c->enabled_ids[0]));
    }
    --c->n_enabled;
    int mi = manifest_index(c, kernel);
    if (mi >= 0) c->manifests[mi].enabled = false;

    if (c->n_events < (int)(sizeof(c->events)/sizeof(c->events[0]))) {
        cos_v162_event_t *e = &c->events[c->n_events++];
        e->kind = COS_V162_EVENT_DISABLE;
        copy_bounded(e->kernel, kernel, sizeof(e->kernel));
        e->added_latency_ms = m ? -m->added_latency_ms : 0;
        e->added_memory_mb  = m ? -m->added_memory_mb  : 0;
    }
    return 0;
}

/* ---------- JSON ----------------------------------------------- */

static int jprintf(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
    if (!buf || *off >= cap) return 0;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= cap - *off) { *off = cap - 1; return 0; }
    *off += (size_t)n;
    return 1;
}

static const char *profile_name(cos_v162_profile_kind_t k) {
    switch (k) {
        case COS_V162_PROFILE_LEAN:       return "lean";
        case COS_V162_PROFILE_RESEARCHER: return "researcher";
        case COS_V162_PROFILE_SOVEREIGN:  return "sovereign";
        case COS_V162_PROFILE_CUSTOM:     return "custom";
    }
    return "unknown";
}

size_t cos_v162_composer_to_json(const cos_v162_composer_t *c,
                                 char *buf, size_t cap) {
    if (!c || !buf || cap == 0) return 0;
    size_t off = 0;
    jprintf(buf, cap, &off,
        "{\"kernel\":\"v162\",\"profile\":\"%s\","
        "\"n_manifests\":%d,\"n_enabled\":%d,"
        "\"total_latency_ms\":%d,\"total_memory_mb\":%d,"
        "\"cycle_detected\":%s,\"cycle_msg\":\"%s\",\"enabled\":[",
        profile_name(c->active_profile),
        c->n_manifests, c->n_enabled,
        c->total_latency_ms, c->total_memory_mb,
        c->cycle_detected ? "true" : "false",
        c->cycle_msg);
    for (int i = 0; i < c->n_enabled; ++i) {
        if (i) jprintf(buf, cap, &off, ",");
        jprintf(buf, cap, &off, "\"%s\"", c->enabled_ids[i]);
    }
    jprintf(buf, cap, &off, "],\"manifests\":[");
    for (int i = 0; i < c->n_manifests; ++i) {
        const cos_v162_manifest_t *m = &c->manifests[i];
        if (i) jprintf(buf, cap, &off, ",");
        jprintf(buf, cap, &off,
            "{\"id\":\"%s\",\"provides\":\"%s\",\"enabled\":%s,"
            "\"added_latency_ms\":%d,\"added_memory_mb\":%d,\"requires\":[",
            m->id, m->provides,
            m->enabled ? "true" : "false",
            m->added_latency_ms, m->added_memory_mb);
        for (int j = 0; j < m->n_requires; ++j) {
            if (j) jprintf(buf, cap, &off, ",");
            jprintf(buf, cap, &off, "\"%s\"", m->requires[j]);
        }
        jprintf(buf, cap, &off, "],\"channels\":[");
        for (int j = 0; j < m->n_channels; ++j) {
            if (j) jprintf(buf, cap, &off, ",");
            jprintf(buf, cap, &off, "\"%s\"", m->channels[j]);
        }
        jprintf(buf, cap, &off, "]}");
    }
    jprintf(buf, cap, &off, "],\"events\":[");
    for (int i = 0; i < c->n_events; ++i) {
        const cos_v162_event_t *e = &c->events[i];
        if (i) jprintf(buf, cap, &off, ",");
        jprintf(buf, cap, &off,
          "{\"kind\":\"%s\",\"kernel\":\"%s\","
          "\"delta_latency_ms\":%d,\"delta_memory_mb\":%d}",
          e->kind == COS_V162_EVENT_ENABLE ? "enable" : "disable",
          e->kernel, e->added_latency_ms, e->added_memory_mb);
    }
    jprintf(buf, cap, &off, "]}");
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- self-test ------------------------------------------ */

int cos_v162_self_test(void) {
    cos_v162_composer_t c;

    /* C1: init loads manifests + 4 profiles. */
    cos_v162_composer_init(&c);
    if (c.n_manifests < 10) return 1;
    if (c.n_profiles != 4)  return 1;

    /* C2: lean profile resolves to {v101, v106, v111}. */
    int n_lean = cos_v162_composer_select(&c, COS_V162_PROFILE_LEAN);
    if (n_lean < 2) return 2;
    if (!cos_v162_composer_is_enabled(&c, "v101")) return 2;
    if (!cos_v162_composer_is_enabled(&c, "v106")) return 2;
    if (!cos_v162_composer_is_enabled(&c, "v111")) return 2;
    int lean_count = c.n_enabled;

    /* C3: sovereign profile resolves more kernels than lean. */
    int n_sov = cos_v162_composer_select(&c, COS_V162_PROFILE_SOVEREIGN);
    if (n_sov <= lean_count) return 3;
    if (!cos_v162_composer_is_enabled(&c, "v150")) return 3;
    if (!cos_v162_composer_is_enabled(&c, "v124")) return 3;
    /* v128 is a required dep of v150 → must be pulled in transitively. */
    if (!cos_v162_composer_is_enabled(&c, "v128")) return 3;

    /* C4: custom profile picks exactly the declared roots + deps. */
    cos_v162_composer_set_custom(&c, "v150,v140");
    int n_custom = cos_v162_composer_select(&c, COS_V162_PROFILE_CUSTOM);
    if (n_custom < 1) return 4;
    if (!cos_v162_composer_is_enabled(&c, "v150")) return 4;
    if (!cos_v162_composer_is_enabled(&c, "v140")) return 4;
    /* v140 requires v135 → must be in closure. */
    if (!cos_v162_composer_is_enabled(&c, "v135")) return 4;

    /* C5: enable v159 at runtime (hot-swap). */
    cos_v162_composer_select(&c, COS_V162_PROFILE_LEAN);
    int prev = c.n_enabled;
    if (cos_v162_composer_enable(&c, "v159") != 0) return 5;
    if (!cos_v162_composer_is_enabled(&c, "v159")) return 5;
    if (c.n_enabled <= prev) return 5;

    /* C6: disable must respect deps — can't disable v101 while
     * v106 still depends on it. */
    int rc = cos_v162_composer_disable(&c, "v101");
    if (rc != -2) return 6;
    /* But disabling v159 (leaf) must succeed. */
    if (cos_v162_composer_disable(&c, "v159") != 0) return 6;
    if (cos_v162_composer_is_enabled(&c, "v159")) return 6;

    /* C7: determinism — the same profile selection twice produces
     * the same enabled list in the same order. */
    cos_v162_composer_t a, b;
    cos_v162_composer_init(&a); cos_v162_composer_select(&a, COS_V162_PROFILE_RESEARCHER);
    cos_v162_composer_init(&b); cos_v162_composer_select(&b, COS_V162_PROFILE_RESEARCHER);
    if (a.n_enabled != b.n_enabled) return 7;
    for (int i = 0; i < a.n_enabled; ++i) {
        if (strcmp(a.enabled_ids[i], b.enabled_ids[i]) != 0) return 7;
    }

    return 0;
}
