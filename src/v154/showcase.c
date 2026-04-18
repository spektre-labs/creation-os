/*
 * v154 σ-Showcase — deterministic end-to-end pipeline orchestrator.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "showcase.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------- deterministic RNG ----------------------------------- */

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

/* ---------- scenario templates ---------------------------------- */

typedef struct {
    const char *name;
    const char *kernel;
    float       sigma_base;
} v154_stage_template_t;

/* Research-assistant pipeline (8 stages). */
static const v154_stage_template_t RESEARCH_STAGES[] = {
    { "ingest_pdf",         "v118", 0.10f },
    { "corpus_lookup",      "v152", 0.18f },
    { "symbolic_check",     "v135", 0.22f },
    { "multi_step_reason",  "v111", 0.26f },
    { "swarm_evaluate",     "v150", 0.20f },
    { "causal_attribute",   "v140", 0.24f },
    { "identity_calibrate", "v153", 0.15f },
    { "persist_memory",     "v115", 0.12f },
};

/* Self-improving-coder pipeline (6 stages). */
static const v154_stage_template_t CODER_STAGES[] = {
    { "code_generate",      "v151", 0.20f },
    { "sandbox_execute",    "v113", 0.16f },
    { "reflect_weakest",    "v147", 0.22f },
    { "speculative_decode", "v119", 0.18f },
    { "continual_style",    "v124", 0.24f },
    { "skill_archive",      "v145", 0.14f },
};

/* Collaborative-research pipeline (4 stages). */
static const v154_stage_template_t COLLAB_STAGES[] = {
    { "mesh_discover",      "v128", 0.18f },
    { "federated_share",    "v129", 0.22f },
    { "swarm_debate",       "v150", 0.24f },
    { "codec_compress",     "v130", 0.16f },
};

typedef struct {
    const v154_stage_template_t *stages;
    int                          n_stages;
    const char                  *title;
    const char                  *user_query;
} v154_scenario_def_t;

static const v154_scenario_def_t SCENARIO_DEF[] = {
    {
        RESEARCH_STAGES, (int)(sizeof(RESEARCH_STAGES)/sizeof(RESEARCH_STAGES[0])),
        "research-assistant",
        "analyze this paper: are its claims supported by evidence",
    },
    {
        CODER_STAGES, (int)(sizeof(CODER_STAGES)/sizeof(CODER_STAGES[0])),
        "self-improving-coder",
        "write a function that counts vowels and prove it",
    },
    {
        COLLAB_STAGES, (int)(sizeof(COLLAB_STAGES)/sizeof(COLLAB_STAGES[0])),
        "collaborative-research",
        "two peers co-investigate and converge a finding",
    },
};

const char *cos_v154_scenario_name(cos_v154_scenario_t s) {
    switch (s) {
        case COS_V154_SCENARIO_RESEARCH: return "research";
        case COS_V154_SCENARIO_CODER:    return "coder";
        case COS_V154_SCENARIO_COLLAB:   return "collab";
    }
    return "unknown";
}

/* Geometric mean of non-zero floats, n > 0. */
static float geomean(const float *xs, int n) {
    if (n <= 0) return 0.0f;
    double acc = 0.0;
    int    k   = 0;
    for (int i = 0; i < n; ++i) {
        float v = xs[i] > 1e-6f ? xs[i] : 1e-6f;
        acc += log((double)v);
        ++k;
    }
    return (float)exp(acc / (double)k);
}

bool cos_v154_run(cos_v154_scenario_t scenario,
                  uint64_t            seed,
                  float               tau_abstain,
                  cos_v154_report_t  *out) {
    if (!out) return false;
    if ((int)scenario < 0 ||
        (int)scenario >= (int)(sizeof(SCENARIO_DEF)/sizeof(SCENARIO_DEF[0])))
        return false;
    memset(out, 0, sizeof(*out));

    const v154_scenario_def_t *def = &SCENARIO_DEF[scenario];
    out->scenario    = scenario;
    out->seed        = seed ? seed : 0x5154DEFA511CULL;
    out->tau_abstain = tau_abstain > 0.0f ? tau_abstain
                                          : COS_V154_TAU_ABSTAIN_DEF;
    copy_bounded(out->title,      def->title,      sizeof(out->title));
    copy_bounded(out->user_query, def->user_query, sizeof(out->user_query));
    out->n_stages            = def->n_stages;
    out->abstain_stage_index = -1;

    uint64_t prng = out->seed;
    float    sigmas[COS_V154_MAX_STAGES];
    float    sigma_in = 0.10f;
    float    max_sig  = -1.0f;
    float    min_sig  = 2.0f;
    double   sum_sig  = 0.0;

    for (int i = 0; i < def->n_stages; ++i) {
        const v154_stage_template_t *tmpl = &def->stages[i];
        cos_v154_stage_t            *st   = &out->stages[i];
        float jitter = (urand(&prng) - 0.5f) * 0.06f; /* ±0.03 */
        float sig    = clamp01(tmpl->sigma_base + jitter);
        st->index    = i;
        copy_bounded(st->name,   tmpl->name,   sizeof(st->name));
        copy_bounded(st->kernel, tmpl->kernel, sizeof(st->kernel));
        st->sigma_in    = sigma_in;
        st->sigma_out   = sig;
        st->delta_sigma = sig - sigma_in;
        st->abstained   = sig > out->tau_abstain;
        if (st->abstained && out->abstain_stage_index < 0)
            out->abstain_stage_index = i;
        snprintf(st->note, sizeof(st->note),
                 "%s/%s σ_in=%.3f σ_out=%.3f%s",
                 def->title, tmpl->name, sigma_in, sig,
                 st->abstained ? " [ABSTAIN]" : "");
        sigmas[i] = sig;
        if (sig > max_sig) max_sig = sig;
        if (sig < min_sig) min_sig = sig;
        sum_sig += (double)sig;
        sigma_in = sig; /* chain: downstream stage sees upstream σ */
    }

    out->sigma_product = geomean(sigmas, def->n_stages);
    out->mean_sigma    = (float)(sum_sig / (double)def->n_stages);
    out->max_sigma     = max_sig < 0.0f ? 0.0f : max_sig;
    out->min_sigma     = min_sig > 1.0f ? 0.0f : min_sig;
    out->abstained     = out->sigma_product > out->tau_abstain ||
                         out->abstain_stage_index >= 0;

    if (out->abstained) {
        snprintf(out->final_message, sizeof(out->final_message),
                 "abstained: σ_product=%.3f τ=%.3f stage=%d",
                 out->sigma_product, out->tau_abstain,
                 out->abstain_stage_index);
    } else {
        snprintf(out->final_message, sizeof(out->final_message),
                 "emitted: σ_product=%.3f τ=%.3f stages=%d",
                 out->sigma_product, out->tau_abstain, out->n_stages);
    }
    return true;
}

static int append(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
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

size_t cos_v154_report_to_json(const cos_v154_report_t *r,
                               char                    *buf,
                               size_t                   cap) {
    if (!r || !buf || cap == 0) return 0;
    size_t off = 0;
    append(buf, cap, &off, "{");
    append(buf, cap, &off, "\"kernel\":\"v154\"");
    append(buf, cap, &off, ",\"scenario\":\"%s\"", cos_v154_scenario_name(r->scenario));
    append(buf, cap, &off, ",\"title\":\"%s\"",    r->title);
    append(buf, cap, &off, ",\"user_query\":\"%s\"", r->user_query);
    append(buf, cap, &off, ",\"seed\":%llu", (unsigned long long)r->seed);
    append(buf, cap, &off, ",\"tau_abstain\":%.4f", r->tau_abstain);
    append(buf, cap, &off, ",\"n_stages\":%d", r->n_stages);
    append(buf, cap, &off, ",\"sigma_product\":%.4f", r->sigma_product);
    append(buf, cap, &off, ",\"mean_sigma\":%.4f", r->mean_sigma);
    append(buf, cap, &off, ",\"max_sigma\":%.4f", r->max_sigma);
    append(buf, cap, &off, ",\"min_sigma\":%.4f", r->min_sigma);
    append(buf, cap, &off, ",\"abstained\":%s", r->abstained ? "true" : "false");
    append(buf, cap, &off, ",\"abstain_stage_index\":%d", r->abstain_stage_index);
    append(buf, cap, &off, ",\"final_message\":\"%s\"", r->final_message);
    append(buf, cap, &off, ",\"stages\":[");
    for (int i = 0; i < r->n_stages; ++i) {
        const cos_v154_stage_t *s = &r->stages[i];
        if (i) append(buf, cap, &off, ",");
        append(buf, cap, &off,
               "{\"index\":%d,\"name\":\"%s\",\"kernel\":\"%s\","
               "\"sigma_in\":%.4f,\"sigma_out\":%.4f,\"delta_sigma\":%.4f,"
               "\"abstained\":%s}",
               s->index, s->name, s->kernel,
               s->sigma_in, s->sigma_out, s->delta_sigma,
               s->abstained ? "true" : "false");
    }
    append(buf, cap, &off, "]}");
    if (off < cap) buf[off] = '\0';
    return off;
}

int cos_v154_self_test(void) {
    cos_v154_report_t r;

    /* S1: every scenario runs to completion with the default τ. */
    for (int s = 0; s < 3; ++s) {
        if (!cos_v154_run((cos_v154_scenario_t)s, 154, 0.0f, &r)) return 1;
        if (r.n_stages < 4) return 1;
        if (r.sigma_product <= 0.0f || r.sigma_product >= 1.0f) return 1;
    }

    /* S2: default τ = 0.60 never abstains the research pipeline
     * (every baseline σ is ≤ 0.26 + 0.03 jitter = 0.29 < 0.60). */
    cos_v154_run(COS_V154_SCENARIO_RESEARCH, 154, 0.60f, &r);
    if (r.abstained) return 2;

    /* S3: a very tight τ = 0.05 *does* abstain (some σ ≥ 0.10). */
    cos_v154_run(COS_V154_SCENARIO_RESEARCH, 154, 0.05f, &r);
    if (!r.abstained) return 3;
    if (r.abstain_stage_index < 0) return 3;

    /* S4: determinism — same (scenario, seed, τ) yields identical
     * sigma_product and identical final_message. */
    cos_v154_report_t a, b;
    cos_v154_run(COS_V154_SCENARIO_CODER, 77, 0.60f, &a);
    cos_v154_run(COS_V154_SCENARIO_CODER, 77, 0.60f, &b);
    if (a.sigma_product != b.sigma_product) return 4;
    if (strcmp(a.final_message, b.final_message) != 0) return 4;
    if (memcmp(a.stages, b.stages,
               sizeof(cos_v154_stage_t) * (size_t)a.n_stages) != 0) return 4;

    /* S5: chain invariant — stage i's sigma_in equals stage (i-1)'s
     * sigma_out (the pipeline actually wires σ forward, no cheating). */
    cos_v154_run(COS_V154_SCENARIO_COLLAB, 12, 0.60f, &r);
    for (int i = 1; i < r.n_stages; ++i) {
        if (r.stages[i].sigma_in != r.stages[i - 1].sigma_out) return 5;
    }

    /* S6: JSON round-trip writes at least the field "kernel":"v154". */
    char json[4096];
    size_t n = cos_v154_report_to_json(&r, json, sizeof(json));
    if (n == 0) return 6;
    if (!strstr(json, "\"kernel\":\"v154\"")) return 6;
    if (!strstr(json, "\"scenario\":\"collab\"")) return 6;
    if (!strstr(json, "\"stages\":[")) return 6;
    return 0;
}
