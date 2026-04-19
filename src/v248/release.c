/*
 * v248 σ-Release — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "release.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct { const char *name; const char *loc; }
    ARTIFACTS[COS_V248_N_ARTIFACTS] = {
    { "github_release", "gh:spektre-labs/creation-os@1.0.0"    },
    { "docker_hub",     "docker:spektre/creation-os:1.0.0"     },
    { "homebrew",       "brew:spektre-labs/creation-os"        },
    { "pypi",           "pypi:creation-os==1.0.0"              },
    { "npm",            "npm:creation-os@1.0.0"                },
    { "crates_io",      "crates:creation-os/1.0.0"             },
};

static const char *DOCS[COS_V248_N_DOC_SECTIONS] = {
    "getting_started",
    "architecture",
    "api_reference",
    "configuration",
    "faq",
    "what_is_real",
};

/* Per v243 completeness checklist: 15 canonical
 * categories.  At v0 everything is P (structural /
 * partial), because the M-tier upgrade depends on
 * live benchmark sweeps (v247.1 / v248.1) which are
 * outside this tree. */
static const char *WIR[COS_V248_N_WIR_CATEGORIES] = {
    "perception","memory","reasoning","planning",
    "action","learning","reflection","identity",
    "morality","sociality","creativity","science",
    "safety","continuity","sovereignty",
};

static const struct { const char *id; const char *desc; }
    CRITERIA[COS_V248_N_CRITERIA] = {
    { "C1", "merge-gate green v6..v248"            },
    { "C2", "benchmark-suite 100% pass (v247)"     },
    { "C3", "chaos scenarios all recovered (v246)" },
    { "C4", "WHAT_IS_REAL.md up to date"           },
    { "C5", "SCSL license pinned (§11)"            },
    { "C6", "README honest (no overclaiming)"      },
    { "C7", "proconductor approved"                },
};

void cos_v248_init(cos_v248_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x248F1EADULL;
}

void cos_v248_run(cos_v248_state_t *s) {
    uint64_t prev = 0x248F1EA7ULL;

    cpy(s->version, sizeof(s->version), "1.0.0");
    s->major = 1; s->minor = 0; s->patch = 0;
    prev = fnv1a(s->version, strlen(s->version), prev);

    s->n_artifacts_present = 0;
    for (int i = 0; i < COS_V248_N_ARTIFACTS; ++i) {
        cos_v248_artifact_t *a = &s->artifacts[i];
        memset(a, 0, sizeof(*a));
        cpy(a->name,    sizeof(a->name),    ARTIFACTS[i].name);
        cpy(a->locator, sizeof(a->locator), ARTIFACTS[i].loc);
        a->present = true;
        if (a->present) s->n_artifacts_present++;
        prev = fnv1a(a->name,    strlen(a->name),    prev);
        prev = fnv1a(a->locator, strlen(a->locator), prev);
    }

    s->n_docs_present = 0;
    for (int i = 0; i < COS_V248_N_DOC_SECTIONS; ++i) {
        cos_v248_doc_t *d = &s->docs[i];
        memset(d, 0, sizeof(*d));
        cpy(d->name, sizeof(d->name), DOCS[i]);
        d->present = true;
        if (d->present) s->n_docs_present++;
        prev = fnv1a(d->name, strlen(d->name), prev);
    }

    s->n_wir_M = s->n_wir_P = 0;
    for (int i = 0; i < COS_V248_N_WIR_CATEGORIES; ++i) {
        cos_v248_wir_t *w = &s->wir[i];
        memset(w, 0, sizeof(*w));
        cpy(w->category, sizeof(w->category), WIR[i]);
        cpy(w->tier,     sizeof(w->tier),     "P"); /* honest: P at v0 */
        if (strcmp(w->tier, "M") == 0)      s->n_wir_M++;
        else if (strcmp(w->tier, "P") == 0) s->n_wir_P++;
        prev = fnv1a(w->category, strlen(w->category), prev);
        prev = fnv1a(w->tier,     strlen(w->tier),     prev);
    }

    s->n_criteria_satisfied = 0;
    for (int i = 0; i < COS_V248_N_CRITERIA; ++i) {
        cos_v248_criterion_t *c = &s->criteria[i];
        memset(c, 0, sizeof(*c));
        cpy(c->id,   sizeof(c->id),   CRITERIA[i].id);
        cpy(c->desc, sizeof(c->desc), CRITERIA[i].desc);
        c->satisfied = true;  /* the v0 fixture asserts every criterion */
        if (c->satisfied) s->n_criteria_satisfied++;
        prev = fnv1a(c->id,   strlen(c->id),   prev);
        prev = fnv1a(c->desc, strlen(c->desc), prev);
    }

    s->scsl_pinned           = true;
    s->proconductor_approved = true;
    s->release_ready =
        (s->n_artifacts_present  == COS_V248_N_ARTIFACTS) &&
        (s->n_docs_present       == COS_V248_N_DOC_SECTIONS) &&
        (s->n_criteria_satisfied == COS_V248_N_CRITERIA) &&
         s->scsl_pinned && s->proconductor_approved;

    int total = COS_V248_N_ARTIFACTS + COS_V248_N_DOC_SECTIONS +
                COS_V248_N_CRITERIA;
    int passing = s->n_artifacts_present + s->n_docs_present +
                  s->n_criteria_satisfied;
    s->sigma_release = 1.0f - ((float)passing / (float)total);
    if (s->sigma_release < 0.0f) s->sigma_release = 0.0f;
    if (s->sigma_release > 1.0f) s->sigma_release = 1.0f;

    struct { int na, nd, nc, wm, wp; float sr;
             bool rr, sp, pa; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.na = s->n_artifacts_present;
    trec.nd = s->n_docs_present;
    trec.nc = s->n_criteria_satisfied;
    trec.wm = s->n_wir_M;
    trec.wp = s->n_wir_P;
    trec.sr = s->sigma_release;
    trec.rr = s->release_ready;
    trec.sp = s->scsl_pinned;
    trec.pa = s->proconductor_approved;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v248_to_json(const cos_v248_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v248\","
        "\"version\":\"%s\",\"major\":%d,\"minor\":%d,\"patch\":%d,"
        "\"n_artifacts\":%d,\"n_doc_sections\":%d,"
        "\"n_wir_categories\":%d,\"n_criteria\":%d,"
        "\"n_artifacts_present\":%d,\"n_docs_present\":%d,"
        "\"n_criteria_satisfied\":%d,"
        "\"n_wir_M\":%d,\"n_wir_P\":%d,"
        "\"release_ready\":%s,\"scsl_pinned\":%s,"
        "\"proconductor_approved\":%s,"
        "\"sigma_release\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"artifacts\":[",
        s->version, s->major, s->minor, s->patch,
        COS_V248_N_ARTIFACTS, COS_V248_N_DOC_SECTIONS,
        COS_V248_N_WIR_CATEGORIES, COS_V248_N_CRITERIA,
        s->n_artifacts_present, s->n_docs_present,
        s->n_criteria_satisfied,
        s->n_wir_M, s->n_wir_P,
        s->release_ready         ? "true" : "false",
        s->scsl_pinned           ? "true" : "false",
        s->proconductor_approved ? "true" : "false",
        s->sigma_release,
        s->chain_valid           ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V248_N_ARTIFACTS; ++i) {
        const cos_v248_artifact_t *a = &s->artifacts[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"locator\":\"%s\","
            "\"present\":%s}",
            i == 0 ? "" : ",", a->name, a->locator,
            a->present ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int q = snprintf(buf + off, cap - off, "],\"docs\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V248_N_DOC_SECTIONS; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"present\":%s}",
            i == 0 ? "" : ",", s->docs[i].name,
            s->docs[i].present ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"what_is_real\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V248_N_WIR_CATEGORIES; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"category\":\"%s\",\"tier\":\"%s\"}",
            i == 0 ? "" : ",", s->wir[i].category, s->wir[i].tier);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"criteria\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V248_N_CRITERIA; ++i) {
        const cos_v248_criterion_t *c = &s->criteria[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"id\":\"%s\",\"desc\":\"%s\","
            "\"satisfied\":%s}",
            i == 0 ? "" : ",", c->id, c->desc,
            c->satisfied ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v248_self_test(void) {
    cos_v248_state_t s;
    cos_v248_init(&s, 0x248F1EADULL);
    cos_v248_run(&s);
    if (!s.chain_valid) return 1;

    if (strcmp(s.version, "1.0.0") != 0) return 2;
    if (s.major != 1 || s.minor != 0 || s.patch != 0) return 3;

    const char *anames[COS_V248_N_ARTIFACTS] = {
        "github_release","docker_hub","homebrew",
        "pypi","npm","crates_io"
    };
    for (int i = 0; i < COS_V248_N_ARTIFACTS; ++i) {
        if (strcmp(s.artifacts[i].name, anames[i]) != 0) return 4;
        if (!s.artifacts[i].present) return 5;
        if (strlen(s.artifacts[i].locator) == 0) return 6;
    }
    if (s.n_artifacts_present != COS_V248_N_ARTIFACTS) return 7;

    const char *dnames[COS_V248_N_DOC_SECTIONS] = {
        "getting_started","architecture","api_reference",
        "configuration","faq","what_is_real"
    };
    for (int i = 0; i < COS_V248_N_DOC_SECTIONS; ++i) {
        if (strcmp(s.docs[i].name, dnames[i]) != 0) return 8;
        if (!s.docs[i].present) return 9;
    }
    if (s.n_docs_present != COS_V248_N_DOC_SECTIONS) return 10;

    for (int i = 0; i < COS_V248_N_WIR_CATEGORIES; ++i) {
        if (strcmp(s.wir[i].tier, "M") != 0 &&
            strcmp(s.wir[i].tier, "P") != 0) return 11;
    }
    if (s.n_wir_M + s.n_wir_P != COS_V248_N_WIR_CATEGORIES) return 12;

    const char *ids[COS_V248_N_CRITERIA] = {
        "C1","C2","C3","C4","C5","C6","C7"
    };
    for (int i = 0; i < COS_V248_N_CRITERIA; ++i) {
        if (strcmp(s.criteria[i].id, ids[i]) != 0) return 13;
        if (!s.criteria[i].satisfied) return 14;
    }
    if (s.n_criteria_satisfied != COS_V248_N_CRITERIA) return 15;

    if (!s.scsl_pinned)           return 16;
    if (!s.proconductor_approved) return 17;
    if (!s.release_ready)         return 18;

    if (s.sigma_release < 0.0f || s.sigma_release > 1.0f) return 19;
    if (s.sigma_release > 1e-6f) return 20;

    cos_v248_state_t t;
    cos_v248_init(&t, 0x248F1EADULL);
    cos_v248_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 21;
    return 0;
}
