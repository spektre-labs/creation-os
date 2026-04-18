/*
 * v164 σ-Plugin — implementation.
 *
 * Deterministic, offline simulation of a plugin host.  No
 * dlopen, no fork, no network.  The four "official" plugins are
 * compiled in and behave via fixed fixtures parameterized by a
 * SplitMix64 stream for reproducible jitter.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "plugin.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------- */
/* Deterministic PRNG                                            */
/* ------------------------------------------------------------- */

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ------------------------------------------------------------- */
/* Helpers                                                       */
/* ------------------------------------------------------------- */

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float geomean(const float *xs, int n) {
    if (n <= 0) return 1.0f;
    double log_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        double v = xs[i] < 1e-6f ? 1e-6 : (double)xs[i];
        log_sum += log(v);
    }
    return (float)exp(log_sum / n);
}

const char *cos_v164_status_name(cos_v164_status_t s) {
    switch (s) {
        case COS_V164_OK:              return "ok";
        case COS_V164_TIMED_OUT:       return "timed_out";
        case COS_V164_CAP_REFUSED:     return "cap_refused";
        case COS_V164_CRASHED:         return "crashed";
        case COS_V164_DISABLED:        return "disabled";
        case COS_V164_UNKNOWN_PLUGIN:  return "unknown";
        default:                       return "?";
    }
}

/* ------------------------------------------------------------- */
/* Official baked manifests                                      */
/* ------------------------------------------------------------- */

static void make_manifest(cos_v164_manifest_t *m,
                          const char *name,
                          const char *version,
                          const char *entry,
                          const char *provides,
                          uint32_t caps,
                          int timeout_ms,
                          int memory_mb,
                          float sigma_impact,
                          bool official) {
    memset(m, 0, sizeof(*m));
    safe_copy(m->name,      sizeof(m->name),      name);
    safe_copy(m->version,   sizeof(m->version),   version);
    safe_copy(m->entry,     sizeof(m->entry),     entry);
    safe_copy(m->provides,  sizeof(m->provides),  provides);
    m->required_caps = caps;
    m->timeout_ms    = timeout_ms;
    m->memory_mb     = memory_mb;
    m->sigma_impact  = sigma_impact;
    m->official      = official;
    m->enabled       = true;
}

/* ------------------------------------------------------------- */
/* Registry management                                           */
/* ------------------------------------------------------------- */

void cos_v164_registry_init(cos_v164_registry_t *r, uint64_t seed) {
    memset(r, 0, sizeof(*r));
    r->seed = seed ? seed : 0x16401964ABCDEF01ULL;

    cos_v164_manifest_t m;

    make_manifest(&m, "web-search", "0.1.0", "libwebsearch.dylib",
                  "tool.web_search",
                  COS_V164_CAP_NETWORK,
                  1500, 64, 0.04f, true);
    cos_v164_registry_install(r, &m);

    make_manifest(&m, "calculator", "0.2.0", "libcalc.dylib",
                  "tool.calculator",
                  0u,
                  100, 16, 0.01f, true);
    cos_v164_registry_install(r, &m);

    make_manifest(&m, "file-reader", "0.1.0", "libfileread.dylib",
                  "tool.file_read",
                  COS_V164_CAP_FILE_READ,
                  500, 32, 0.02f, true);
    cos_v164_registry_install(r, &m);

    make_manifest(&m, "git", "0.3.0", "libgit.dylib",
                  "tool.git",
                  COS_V164_CAP_FILE_READ | COS_V164_CAP_FILE_WRITE |
                      COS_V164_CAP_SUBPROCESS,
                  3000, 128, 0.05f, true);
    cos_v164_registry_install(r, &m);

    /* seed σ_reputation with a benign baseline so history_len > 0. */
    uint64_t s = r->seed;
    for (int i = 0; i < r->n_plugins; ++i) {
        cos_v164_entry_t *e = &r->plugins[i];
        for (int k = 0; k < 3; ++k) {
            float v = 0.08f + 0.05f * frand(&s);
            e->history[e->history_len++] = v;
        }
        e->sigma_reputation = geomean(e->history, e->history_len);
    }
}

int cos_v164_registry_install(cos_v164_registry_t *r,
                              const cos_v164_manifest_t *m) {
    if (!r || !m) return -1;
    if (r->n_plugins >= COS_V164_MAX_PLUGINS) return -1;
    /* refuse duplicates by name */
    if (cos_v164_registry_get(r, m->name) != NULL) return -1;
    cos_v164_entry_t *e = &r->plugins[r->n_plugins++];
    memset(e, 0, sizeof(*e));
    e->manifest = *m;
    /* a freshly installed plugin starts at σ_reputation = 0.30
     * (medium confidence until proven). */
    e->sigma_reputation = 0.30f;
    return 0;
}

int cos_v164_registry_set_enabled(cos_v164_registry_t *r,
                                  const char *name, bool on) {
    cos_v164_entry_t *e = cos_v164_registry_get(r, name);
    if (!e) return -1;
    e->manifest.enabled = on;
    return 0;
}

cos_v164_entry_t *cos_v164_registry_get(cos_v164_registry_t *r,
                                        const char *name) {
    if (!r || !name) return NULL;
    for (int i = 0; i < r->n_plugins; ++i) {
        if (strcmp(r->plugins[i].manifest.name, name) == 0)
            return &r->plugins[i];
    }
    return NULL;
}

/* ------------------------------------------------------------- */
/* Simulated plugin `process` implementations                    */
/* ------------------------------------------------------------- */

static void run_web_search(uint64_t *s,
                           const cos_v164_input_t *in,
                           cos_v164_output_t *out) {
    (void)in;
    float jitter = frand(s);
    int latency = 300 + (int)(1000.0f * jitter);  /* 300..1300 ms */
    out->runtime_ms = latency;
    /* σ = 0.10 baseline + entropy_like jitter */
    out->sigma_plugin = clampf(0.10f + 0.20f * jitter, 0.0f, 1.0f);
    snprintf(out->response, COS_V164_MAX_MSG,
             "web_search: top result for prompt [%.24s...]",
             in->prompt);
    out->sigma_override = out->sigma_plugin > 0.25f;
}

static void run_calculator(uint64_t *s,
                           const cos_v164_input_t *in,
                           cos_v164_output_t *out) {
    (void)s;
    out->runtime_ms = 5;
    /* deterministic echo: "2+2" -> "4" demo; for other inputs,
     * return a symbolic placeholder. */
    if (strstr(in->prompt, "2+2")) {
        safe_copy(out->response, COS_V164_MAX_MSG, "4");
        out->sigma_plugin = 0.02f;
    } else {
        snprintf(out->response, COS_V164_MAX_MSG,
                 "calculator: eval(%.48s) = <result>", in->prompt);
        out->sigma_plugin = 0.05f;
    }
    out->sigma_override = false;
}

static void run_file_reader(uint64_t *s,
                            const cos_v164_input_t *in,
                            cos_v164_output_t *out) {
    (void)s;
    out->runtime_ms = 12;
    snprintf(out->response, COS_V164_MAX_MSG,
             "file_reader: stat+head(%.40s)", in->prompt);
    out->sigma_plugin = 0.08f;
    out->sigma_override = false;
}

static void run_git(uint64_t *s,
                    const cos_v164_input_t *in,
                    cos_v164_output_t *out) {
    (void)in;
    float jitter = frand(s);
    out->runtime_ms = 80 + (int)(400.0f * jitter);
    snprintf(out->response, COS_V164_MAX_MSG,
             "git: exec(status/diff/log) jitter=%.2f", (double)jitter);
    out->sigma_plugin = 0.12f + 0.05f * jitter;
    out->sigma_override = false;
}

static void run_unknown_default(uint64_t *s,
                                const cos_v164_input_t *in,
                                cos_v164_output_t *out) {
    (void)s; (void)in;
    /* Third-party plugins without a baked impl return a neutral
     * "ok-noop" response so the host can still exercise the ABI. */
    out->runtime_ms   = 3;
    out->sigma_plugin = 0.22f;
    out->sigma_override = false;
    safe_copy(out->response, COS_V164_MAX_MSG, "third-party-plugin-noop");
}

/* ------------------------------------------------------------- */
/* Sandbox + dispatch                                            */
/* ------------------------------------------------------------- */

cos_v164_output_t cos_v164_registry_run(cos_v164_registry_t *r,
                                        const char *name,
                                        const cos_v164_input_t *in) {
    cos_v164_output_t out;
    memset(&out, 0, sizeof(out));

    cos_v164_entry_t *e = cos_v164_registry_get(r, name);
    if (!e) {
        out.status = COS_V164_UNKNOWN_PLUGIN;
        out.sigma_plugin = 1.0f;
        out.sigma_override = true;
        safe_copy(out.response, COS_V164_MAX_MSG, "plugin not found");
        safe_copy(out.note, COS_V164_MAX_MSG, "registry lookup failed");
        return out;
    }
    if (!e->manifest.enabled) {
        out.status = COS_V164_DISABLED;
        out.sigma_plugin = 1.0f;
        out.sigma_override = true;
        safe_copy(out.response, COS_V164_MAX_MSG, "plugin disabled");
        safe_copy(out.note, COS_V164_MAX_MSG, "hot-swap off");
        return out;
    }

    /* MODEL_WEIGHTS is *never* granted by the host. */
    if (e->manifest.required_caps & COS_V164_CAP_MODEL_WEIGHTS) {
        out.status = COS_V164_CAP_REFUSED;
        out.sigma_plugin = 1.0f;
        out.sigma_override = true;
        safe_copy(out.response, COS_V164_MAX_MSG, "cap refused: model weights");
        safe_copy(out.note, COS_V164_MAX_MSG, "host policy: weights are sealed");
        return out;
    }

    /* Sandbox: host must have granted every required cap. */
    uint32_t missing = e->manifest.required_caps & ~in->granted_caps;
    if (missing != 0) {
        out.status = COS_V164_CAP_REFUSED;
        out.sigma_plugin = 1.0f;
        out.sigma_override = true;
        snprintf(out.response, COS_V164_MAX_MSG,
                 "cap refused: missing=0x%08x", missing);
        safe_copy(out.note, COS_V164_MAX_MSG,
                  "sandbox enforced manifest vs grant");
        /* bump history: caller requested an over-privileged call */
        if (e->history_len < COS_V164_MAX_HISTORY)
            e->history[e->history_len++] = 1.0f;
        else {
            memmove(e->history, e->history + 1,
                    (COS_V164_MAX_HISTORY - 1) * sizeof(float));
            e->history[COS_V164_MAX_HISTORY - 1] = 1.0f;
        }
        e->n_failures++;
        e->n_invocations++;
        e->sigma_reputation = geomean(e->history, e->history_len);
        return out;
    }

    /* Dispatch. */
    uint64_t s = r->seed ^ (uint64_t)e->n_invocations * 0x9E3779B97F4A7C15ULL;
    if      (strcmp(e->manifest.name, "web-search") == 0)  run_web_search(&s,  in, &out);
    else if (strcmp(e->manifest.name, "calculator") == 0)  run_calculator(&s,  in, &out);
    else if (strcmp(e->manifest.name, "file-reader") == 0) run_file_reader(&s, in, &out);
    else if (strcmp(e->manifest.name, "git") == 0)         run_git(&s,         in, &out);
    else                                                   run_unknown_default(&s, in, &out);

    /* Timeout enforcement (simulated wall clock). */
    if (out.runtime_ms > e->manifest.timeout_ms) {
        out.status = COS_V164_TIMED_OUT;
        out.sigma_plugin   = 1.0f;
        out.sigma_override = true;
        safe_copy(out.note, COS_V164_MAX_MSG, "sandbox timeout");
        e->n_failures++;
    } else {
        out.status = COS_V164_OK;
        safe_copy(out.note, COS_V164_MAX_MSG, "sandbox ok");
    }

    /* Update σ_reputation ring buffer. */
    if (e->history_len < COS_V164_MAX_HISTORY) {
        e->history[e->history_len++] = out.sigma_plugin;
    } else {
        memmove(e->history, e->history + 1,
                (COS_V164_MAX_HISTORY - 1) * sizeof(float));
        e->history[COS_V164_MAX_HISTORY - 1] = out.sigma_plugin;
    }
    e->n_invocations++;
    e->sigma_reputation = geomean(e->history, e->history_len);
    return out;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v164_registry_to_json(const cos_v164_registry_t *r,
                                 char *buf, size_t cap) {
    if (!r || !buf || cap < 2) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v164\",\"n_plugins\":%d,\"plugins\":[",
                     r->n_plugins);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < r->n_plugins; ++i) {
        const cos_v164_entry_t *e = &r->plugins[i];
        n = snprintf(buf + used, cap - used,
                     "%s{\"name\":\"%s\",\"version\":\"%s\","
                     "\"provides\":\"%s\",\"enabled\":%s,"
                     "\"required_caps\":%u,\"timeout_ms\":%d,"
                     "\"memory_mb\":%d,\"sigma_impact\":%.4f,"
                     "\"sigma_reputation\":%.4f,"
                     "\"n_invocations\":%d,\"n_failures\":%d,"
                     "\"official\":%s}",
                     i == 0 ? "" : ",",
                     e->manifest.name, e->manifest.version,
                     e->manifest.provides,
                     e->manifest.enabled ? "true" : "false",
                     (unsigned)e->manifest.required_caps,
                     e->manifest.timeout_ms, e->manifest.memory_mb,
                     (double)e->manifest.sigma_impact,
                     (double)e->sigma_reputation,
                     e->n_invocations, e->n_failures,
                     e->manifest.official ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v164_self_test(void) {
    cos_v164_registry_t r;
    cos_v164_registry_init(&r, 0x164A1B2C3D4E5F60ULL);
    if (r.n_plugins < 4) return 1;
    if (!cos_v164_registry_get(&r, "calculator")) return 2;

    /* cap enforcement: file-reader without FILE_READ should refuse. */
    cos_v164_input_t in;
    memset(&in, 0, sizeof(in));
    safe_copy(in.prompt, COS_V164_MAX_MSG, "read /etc/hosts");
    in.granted_caps = 0;
    cos_v164_output_t o = cos_v164_registry_run(&r, "file-reader", &in);
    if (o.status != COS_V164_CAP_REFUSED) return 3;

    /* grant cap, retry → ok */
    in.granted_caps = COS_V164_CAP_FILE_READ;
    o = cos_v164_registry_run(&r, "file-reader", &in);
    if (o.status != COS_V164_OK) return 4;

    /* calculator 2+2 → "4" */
    safe_copy(in.prompt, COS_V164_MAX_MSG, "2+2");
    o = cos_v164_registry_run(&r, "calculator", &in);
    if (o.status != COS_V164_OK) return 5;
    if (strcmp(o.response, "4") != 0) return 6;

    /* disable hot-swap */
    if (cos_v164_registry_set_enabled(&r, "git", false) != 0) return 7;
    o = cos_v164_registry_run(&r, "git", &in);
    if (o.status != COS_V164_DISABLED) return 8;

    /* unknown plugin */
    o = cos_v164_registry_run(&r, "does-not-exist", &in);
    if (o.status != COS_V164_UNKNOWN_PLUGIN) return 9;

    /* install refuses duplicate */
    cos_v164_manifest_t dup;
    memset(&dup, 0, sizeof(dup));
    safe_copy(dup.name, sizeof(dup.name), "calculator");
    if (cos_v164_registry_install(&r, &dup) == 0) return 10;

    return 0;
}
