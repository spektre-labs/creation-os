/*
 * v159 σ-Heal — self-healing infrastructure kernel.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "heal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------- deterministic RNG ---------------------------------- */

static uint64_t splitmix(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float urand(uint64_t *s) {
    return (float)((splitmix(s) >> 40) / (float)(1ULL << 24));
}

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void copy_bounded(char *dst, const char *src, size_t cap) {
    if (!dst || cap == 0) return;
    size_t n = 0;
    if (src) {
        while (src[n] && n + 1 < cap) { dst[n] = src[n]; ++n; }
    }
    dst[n] = '\0';
}

/* ---------- static labels -------------------------------------- */

static const char *COMPONENT_NAMES[COS_V159_MAX_COMPONENTS] = {
    [COS_V159_COMP_HTTP_SERVER]     = "v106_http",
    [COS_V159_COMP_SIGMA_CHANNELS]  = "v101_sigma",
    [COS_V159_COMP_MEMORY_SQLITE]   = "v115_memory",
    [COS_V159_COMP_KV_CACHE]        = "v117_kv",
    [COS_V159_COMP_ADAPTER_VERSION] = "v124_adapter",
    [COS_V159_COMP_MERGE_GATE]      = "merge_gate",
    [COS_V159_COMP_BRIDGE]          = "v101_bridge",
    [COS_V159_COMP_WEIGHTS]         = "gguf_weights",
};

/* Healthy baseline σ_health per component. */
static const float HEALTHY_SIGMA[COS_V159_MAX_COMPONENTS] = {
    [COS_V159_COMP_HTTP_SERVER]     = 0.05f,
    [COS_V159_COMP_SIGMA_CHANNELS]  = 0.08f,
    [COS_V159_COMP_MEMORY_SQLITE]   = 0.07f,
    [COS_V159_COMP_KV_CACHE]        = 0.12f,
    [COS_V159_COMP_ADAPTER_VERSION] = 0.10f,
    [COS_V159_COMP_MERGE_GATE]      = 0.06f,
    [COS_V159_COMP_BRIDGE]          = 0.09f,
    [COS_V159_COMP_WEIGHTS]         = 0.04f,
};

static const char *ACTION_NAMES[] = {
    [COS_V159_ACT_NONE]           = "none",
    [COS_V159_ACT_RESTART]        = "restart_http",
    [COS_V159_ACT_FLUSH_KV]       = "flush_kv",
    [COS_V159_ACT_ROLLBACK_LORA]  = "rollback_lora",
    [COS_V159_ACT_RESTORE_SQLITE] = "restore_sqlite",
    [COS_V159_ACT_REFETCH_GGUF]   = "refetch_gguf",
    [COS_V159_ACT_RESTART_BRIDGE] = "restart_bridge",
    [COS_V159_ACT_PREEMPTIVE]     = "preemptive_repair",
};

const char *cos_v159_action_name(cos_v159_action_t a) {
    if ((int)a < 0 || (int)a >= (int)(sizeof(ACTION_NAMES)/sizeof(ACTION_NAMES[0])))
        return "invalid";
    const char *s = ACTION_NAMES[a];
    return s ? s : "invalid";
}

const char *cos_v159_component_name(cos_v159_component_t c) {
    if ((int)c < 0 || c >= COS_V159_MAX_COMPONENTS) return "invalid";
    const char *s = COMPONENT_NAMES[c];
    return s ? s : "invalid";
}

/* ---------- init ----------------------------------------------- */

void cos_v159_daemon_init(cos_v159_daemon_t *d, uint64_t seed) {
    if (!d) return;
    memset(d, 0, sizeof(*d));
    d->seed = seed ? seed : 0x5159DAEA011ULL;
    d->n_components = COS_V159_MAX_COMPONENTS;
    for (int i = 0; i < COS_V159_MAX_COMPONENTS; ++i) {
        cos_v159_component_state_t *c = &d->components[i];
        c->id            = (cos_v159_component_t)i;
        copy_bounded(c->name, COMPONENT_NAMES[i], sizeof(c->name));
        c->sigma_health  = HEALTHY_SIGMA[i];
        c->status        = COS_V159_OK;
        c->failed_this_tick = false;
        c->slope_3d      = 0.0f;
        c->degrading     = false;
        snprintf(c->detail, sizeof(c->detail), "%s healthy", c->name);
    }
    d->sigma_heal = cos_v159_daemon_sigma_heal(d);
}

/* ---------- heal-action planner -------------------------------- */

typedef struct {
    cos_v159_component_t comp;
    cos_v159_action_t    action;
    const char          *failure;
    const char          *root_cause;
    /* Cascade-failure list (NULL-terminated).  A failing weights
     * file brings down the bridge and the HTTP server too. */
    const cos_v159_component_t *cascade;
    int                  cascade_len;
    float                cascade_sigma;   /* σ_health injected on cascade */
} v159_failure_template_t;

static const cos_v159_component_t CASCADE_WEIGHTS[] = {
    COS_V159_COMP_BRIDGE,
    COS_V159_COMP_HTTP_SERVER,
};

static const v159_failure_template_t FAILURES[] = {
    /* scenario 0 is "all green" and has no template. */
    [1] = {
        .comp       = COS_V159_COMP_HTTP_SERVER,
        .action     = COS_V159_ACT_RESTART,
        .failure    = "v106 /health unreachable (connect refused)",
        .root_cause = "http server process died",
    },
    [2] = {
        .comp       = COS_V159_COMP_KV_CACHE,
        .action     = COS_V159_ACT_FLUSH_KV,
        .failure    = "v117 paged KV-cache hit-rate < 0.05",
        .root_cause = "kv-cache degenerate; distribution flat",
    },
    [3] = {
        .comp       = COS_V159_COMP_ADAPTER_VERSION,
        .action     = COS_V159_ACT_ROLLBACK_LORA,
        .failure    = "v124 continual LoRA adapter σ_rsi > τ_sovereign",
        .root_cause = "last continual cycle regressed TruthfulQA",
    },
    [4] = {
        .comp       = COS_V159_COMP_MEMORY_SQLITE,
        .action     = COS_V159_ACT_RESTORE_SQLITE,
        .failure    = "v115 SQLite db corrupted (PRAGMA integrity_check)",
        .root_cause = "last checkpoint truncated mid-write",
    },
    [5] = {
        .comp        = COS_V159_COMP_WEIGHTS,
        .action      = COS_V159_ACT_REFETCH_GGUF,
        .failure     = "GGUF file missing or sha256 mismatch",
        .root_cause  = "weights on disk corrupted; bridge + http cascade failed",
        .cascade     = CASCADE_WEIGHTS,
        .cascade_len = (int)(sizeof(CASCADE_WEIGHTS)/sizeof(CASCADE_WEIGHTS[0])),
        .cascade_sigma = 0.92f,
    },
};

#define V159_N_FAILURES ((int)(sizeof(FAILURES)/sizeof(FAILURES[0])))

static void push_receipt(cos_v159_daemon_t *d,
                         cos_v159_component_t c,
                         cos_v159_action_t a,
                         const char *failure,
                         const char *root_cause,
                         float sigma_before,
                         float sigma_after,
                         bool succeeded,
                         bool predictive,
                         uint64_t tick) {
    if (d->n_receipts >= COS_V159_MAX_HISTORY) return;
    cos_v159_heal_receipt_t *r = &d->receipts[d->n_receipts++];
    r->timestamp    = tick;
    r->component    = c;
    copy_bounded(r->component_name, COMPONENT_NAMES[c],
                 sizeof(r->component_name));
    copy_bounded(r->failure,    failure    ? failure    : "",
                 sizeof(r->failure));
    r->action       = a;
    copy_bounded(r->action_name, cos_v159_action_name(a),
                 sizeof(r->action_name));
    copy_bounded(r->root_cause, root_cause ? root_cause : "",
                 sizeof(r->root_cause));
    r->succeeded    = succeeded;
    r->sigma_before = sigma_before;
    r->sigma_after  = sigma_after;
    r->predictive   = predictive;
}

int cos_v159_daemon_tick(cos_v159_daemon_t *d, int scenario) {
    if (!d) return 0;
    /* Reset per-tick state. */
    for (int i = 0; i < d->n_components; ++i) {
        d->components[i].failed_this_tick = false;
    }
    int receipts_this_tick = 0;

    /* synthesize a small deterministic σ jitter on every healthy
     * component so σ_heal moves (but stays low). */
    uint64_t prng = d->seed + (uint64_t)scenario * 1009ULL
                  + (uint64_t)d->n_receipts * 31ULL;
    for (int i = 0; i < d->n_components; ++i) {
        cos_v159_component_state_t *c = &d->components[i];
        float jitter = (urand(&prng) - 0.5f) * 0.01f;
        c->sigma_health = clamp01(HEALTHY_SIGMA[i] + jitter);
        c->status       = COS_V159_OK;
        snprintf(c->detail, sizeof(c->detail), "%s healthy σ=%.3f",
                 c->name, c->sigma_health);
    }

    if (scenario <= 0 || scenario >= V159_N_FAILURES) {
        d->sigma_heal = cos_v159_daemon_sigma_heal(d);
        return 0;
    }

    const v159_failure_template_t *t = &FAILURES[scenario];

    /* Inject failure σ on the target. */
    cos_v159_component_state_t *prim = &d->components[t->comp];
    float sigma_before = 0.92f; /* failure σ */
    prim->sigma_health      = sigma_before;
    prim->status            = COS_V159_FAIL;
    prim->failed_this_tick  = true;
    copy_bounded(prim->detail, t->failure, sizeof(prim->detail));

    /* Inject cascade. */
    for (int i = 0; i < t->cascade_len; ++i) {
        cos_v159_component_state_t *cc = &d->components[t->cascade[i]];
        cc->sigma_health     = t->cascade_sigma;
        cc->status           = COS_V159_FAIL;
        cc->failed_this_tick = true;
        snprintf(cc->detail, sizeof(cc->detail),
                 "%s cascade-failed (root=%s)",
                 cc->name, prim->name);
    }

    /* Execute the self-heal action: in v159.0 we simulate
     * deterministic success, drop the σ_health back to healthy
     * baseline, and emit receipts. */
    float sigma_after = HEALTHY_SIGMA[t->comp] + 0.02f;
    push_receipt(d, t->comp, t->action,
                 t->failure, t->root_cause,
                 sigma_before, sigma_after,
                 /*succeeded*/ true, /*predictive*/ false,
                 (uint64_t)(d->n_receipts + 1));
    prim->sigma_health = sigma_after;
    prim->status       = COS_V159_OK;
    snprintf(prim->detail, sizeof(prim->detail),
             "%s healed via %s", prim->name,
             cos_v159_action_name(t->action));
    ++receipts_this_tick;

    /* Cascade: explicit per-component receipts — one RESTART on
     * HTTP and one RESTART_BRIDGE on the bridge, so the receipt
     * log names every component that crossed σ > τ. */
    for (int i = 0; i < t->cascade_len; ++i) {
        cos_v159_component_state_t *cc = &d->components[t->cascade[i]];
        cos_v159_action_t cascade_action =
            (t->cascade[i] == COS_V159_COMP_BRIDGE)
              ? COS_V159_ACT_RESTART_BRIDGE
              : COS_V159_ACT_RESTART;
        float cc_before = cc->sigma_health;
        float cc_after  = HEALTHY_SIGMA[t->cascade[i]] + 0.02f;
        push_receipt(d, t->cascade[i], cascade_action,
                     cc->detail, t->root_cause,
                     cc_before, cc_after,
                     /*succeeded*/ true, /*predictive*/ false,
                     (uint64_t)(d->n_receipts + 1));
        cc->sigma_health = cc_after;
        cc->status       = COS_V159_OK;
        snprintf(cc->detail, sizeof(cc->detail),
                 "%s healed via %s (cascade)",
                 cc->name, cos_v159_action_name(cascade_action));
        ++receipts_this_tick;
    }

    d->sigma_heal = cos_v159_daemon_sigma_heal(d);
    return receipts_this_tick;
}

int cos_v159_daemon_predict(cos_v159_daemon_t *d,
                            cos_v159_component_t target) {
    if (!d) return 0;
    if ((int)target < 0 || target >= COS_V159_MAX_COMPONENTS) return 0;

    /* Synthesize a deterministic 7-day monotone σ rise from the
     * healthy baseline at day 0 to a pre-fail σ=0.55 at day 6. */
    float base = HEALTHY_SIGMA[target];
    float end  = 0.55f;
    for (int day = 0; day < COS_V159_MAX_SLOPE_DAYS; ++day) {
        float t = (float)day / (float)(COS_V159_MAX_SLOPE_DAYS - 1);
        d->slope_history[target][day] = base + t * (end - base);
    }
    d->slope_filled = COS_V159_MAX_SLOPE_DAYS;

    /* 3-day slope = σ[6] - σ[3] (last 3 days).  Rising ≥ 0.05 ⇒
     * degrading ⇒ emit a predictive receipt. */
    float slope = d->slope_history[target][6] - d->slope_history[target][3];
    d->components[target].slope_3d  = slope;
    d->components[target].degrading = (slope >= 0.05f);

    if (!d->components[target].degrading) return 0;

    char msg[COS_V159_MAX_MSG];
    snprintf(msg, sizeof(msg),
             "component %s σ_health rose %.3f over last 3d; preemptive repair scheduled",
             COMPONENT_NAMES[target], slope);

    push_receipt(d, target, COS_V159_ACT_PREEMPTIVE,
                 msg, "3-day monotone σ rise on v131 slope",
                 d->slope_history[target][6],
                 HEALTHY_SIGMA[target] + 0.02f,
                 /*succeeded*/ true, /*predictive*/ true,
                 (uint64_t)(d->n_receipts + 1));
    /* Set the component back to baseline after the preemptive fix. */
    d->components[target].sigma_health = HEALTHY_SIGMA[target] + 0.02f;
    d->components[target].status       = COS_V159_OK;
    d->sigma_heal = cos_v159_daemon_sigma_heal(d);
    return 1;
}

float cos_v159_daemon_sigma_heal(const cos_v159_daemon_t *d) {
    if (!d || d->n_components <= 0) return 0.0f;
    double acc = 0.0;
    int    k   = 0;
    int    failing = 0;
    for (int i = 0; i < d->n_components; ++i) {
        float v = d->components[i].sigma_health > 1e-6f
                ? d->components[i].sigma_health : 1e-6f;
        acc += log((double)v);
        ++k;
        if (d->components[i].sigma_health > 0.5f) ++failing;
    }
    ((cos_v159_daemon_t*)d)->n_failing = failing;
    return (float)exp(acc / (double)k);
}

/* ---------- JSON ----------------------------------------------- */

static int ja(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
    if (*off >= cap) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= cap - *off) { *off = cap - 1; return 0; }
    *off += (size_t)n;
    return 1;
}

size_t cos_v159_daemon_to_json(const cos_v159_daemon_t *d,
                               char *buf, size_t cap) {
    if (!d || !buf || cap == 0) return 0;
    size_t off = 0;
    ja(buf, cap, &off, "{\"kernel\":\"v159\"");
    ja(buf, cap, &off, ",\"seed\":%llu", (unsigned long long)d->seed);
    ja(buf, cap, &off, ",\"sigma_heal\":%.4f", d->sigma_heal);
    ja(buf, cap, &off, ",\"n_failing\":%d", d->n_failing);
    ja(buf, cap, &off, ",\"components\":[");
    for (int i = 0; i < d->n_components; ++i) {
        const cos_v159_component_state_t *c = &d->components[i];
        if (i) ja(buf, cap, &off, ",");
        ja(buf, cap, &off,
           "{\"id\":%d,\"name\":\"%s\",\"sigma_health\":%.4f,"
           "\"status\":%d,\"slope_3d\":%.4f,\"degrading\":%s,"
           "\"detail\":\"%s\"}",
           c->id, c->name, c->sigma_health, (int)c->status,
           c->slope_3d, c->degrading ? "true" : "false",
           c->detail);
    }
    ja(buf, cap, &off, "],\"receipts\":[");
    for (int i = 0; i < d->n_receipts; ++i) {
        const cos_v159_heal_receipt_t *r = &d->receipts[i];
        if (i) ja(buf, cap, &off, ",");
        ja(buf, cap, &off,
           "{\"timestamp\":%llu,\"component\":\"%s\",\"failure\":\"%s\","
           "\"action\":\"%s\",\"succeeded\":%s,"
           "\"sigma_before\":%.4f,\"sigma_after\":%.4f,"
           "\"root_cause\":\"%s\",\"predictive\":%s}",
           (unsigned long long)r->timestamp, r->component_name,
           r->failure, r->action_name,
           r->succeeded ? "true" : "false",
           r->sigma_before, r->sigma_after,
           r->root_cause,
           r->predictive ? "true" : "false");
    }
    ja(buf, cap, &off, "]}");
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- self-test ------------------------------------------ */

int cos_v159_self_test(void) {
    cos_v159_daemon_t d;

    /* H1: init ⇒ 8 components, σ_heal < 0.2 (all healthy). */
    cos_v159_daemon_init(&d, 159);
    if (d.n_components != COS_V159_MAX_COMPONENTS) return 1;
    if (d.sigma_heal >= 0.20f) return 1;

    /* H2: all-green tick ⇒ 0 receipts, σ_heal stays < 0.25. */
    int rc0 = cos_v159_daemon_tick(&d, 0);
    if (rc0 != 0) return 2;
    if (d.sigma_heal >= 0.25f) return 2;

    /* H3: HTTP-down scenario ⇒ 1 receipt, action=RESTART,
     * σ_heal recovers to a healthy range after repair. */
    int rc1 = cos_v159_daemon_tick(&d, 1);
    if (rc1 != 1) return 3;
    const cos_v159_heal_receipt_t *last = &d.receipts[d.n_receipts - 1];
    if (last->action != COS_V159_ACT_RESTART) return 3;
    if (!last->succeeded) return 3;
    if (d.sigma_heal >= 0.25f) return 3;

    /* H4: SQLite-corrupted ⇒ RESTORE_SQLITE action. */
    cos_v159_daemon_init(&d, 159);
    cos_v159_daemon_tick(&d, 4);
    if (d.receipts[d.n_receipts - 1].action != COS_V159_ACT_RESTORE_SQLITE)
        return 4;

    /* H5: weights missing ⇒ 3 receipts (primary + 2 cascade:
     * bridge RESTART_BRIDGE + http RESTART). */
    cos_v159_daemon_init(&d, 159);
    int rc_cascade = cos_v159_daemon_tick(&d, 5);
    if (rc_cascade != 3) return 5;
    /* Scan receipts for the three expected actions. */
    bool saw_refetch = false, saw_bridge = false, saw_restart = false;
    for (int i = 0; i < d.n_receipts; ++i) {
        switch (d.receipts[i].action) {
            case COS_V159_ACT_REFETCH_GGUF:   saw_refetch = true; break;
            case COS_V159_ACT_RESTART_BRIDGE: saw_bridge  = true; break;
            case COS_V159_ACT_RESTART:        saw_restart = true; break;
            default: break;
        }
    }
    if (!(saw_refetch && saw_bridge && saw_restart)) return 5;

    /* H6: predictive pass ⇒ a monotone 3-day σ rise produces a
     * PREEMPTIVE receipt and the component's σ drops back to
     * baseline. */
    cos_v159_daemon_init(&d, 159);
    int npr = cos_v159_daemon_predict(&d, COS_V159_COMP_KV_CACHE);
    if (npr != 1) return 6;
    const cos_v159_heal_receipt_t *p = &d.receipts[d.n_receipts - 1];
    if (!p->predictive) return 6;
    if (p->action != COS_V159_ACT_PREEMPTIVE) return 6;
    if (d.components[COS_V159_COMP_KV_CACHE].sigma_health >= 0.20f) return 6;
    if (!d.components[COS_V159_COMP_KV_CACHE].degrading) return 6;

    /* H7: determinism — same seed + same scenarios ⇒ identical
     * JSON receipts. */
    cos_v159_daemon_t a, b;
    cos_v159_daemon_init(&a, 77);
    cos_v159_daemon_init(&b, 77);
    cos_v159_daemon_tick(&a, 2);
    cos_v159_daemon_tick(&b, 2);
    char ja2[8192], jb[8192];
    cos_v159_daemon_to_json(&a, ja2, sizeof(ja2));
    cos_v159_daemon_to_json(&b, jb,  sizeof(jb));
    if (strcmp(ja2, jb) != 0) return 7;

    return 0;
}
