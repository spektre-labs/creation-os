/*
 * v168 σ-Marketplace — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "marketplace.h"

#include <stdio.h>
#include <string.h>

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

const char *cos_v168_kind_name(cos_v168_kind_t k) {
    switch (k) {
        case COS_V168_KIND_SKILL:  return "skill";
        case COS_V168_KIND_KERNEL: return "kernel";
        case COS_V168_KIND_PLUGIN: return "plugin";
        default:                   return "?";
    }
}

const char *cos_v168_status_name(cos_v168_status_t s) {
    switch (s) {
        case COS_V168_OK:                return "ok";
        case COS_V168_REFUSED:           return "refused";
        case COS_V168_NOT_FOUND:         return "not_found";
        case COS_V168_ALREADY_INSTALLED: return "already_installed";
        case COS_V168_FIXTURE_INVALID:   return "fixture_invalid";
        case COS_V168_FORCED:            return "forced";
        default:                         return "?";
    }
}

/* ------------------------------------------------------------- */
/* Deterministic 8-byte hex "sha" for demo                       */
/* ------------------------------------------------------------- */

static void fake_sha(const char *id, const char *version,
                     char out_hex[COS_V168_SHA_HEX + 1]) {
    /* FNV-1a 64-bit, lower 32 bits as 8-hex. */
    uint64_t h = 0xCBF29CE484222325ULL;
    for (const char *p = id;      p && *p; ++p) { h ^= (uint8_t)*p; h *= 0x100000001B3ULL; }
    for (const char *p = version; p && *p; ++p) { h ^= (uint8_t)*p; h *= 0x100000001B3ULL; }
    uint64_t low = h & 0xFFFFFFFFULL;
    snprintf(out_hex, COS_V168_SHA_HEX + 1, "%08llx%08llx",
             (unsigned long long)((h >> 32) & 0xFFFFULL),
             (unsigned long long)(low & 0xFFFFFFFFULL));
    /* belt-and-suspenders: ensure length is exactly 16 hex. */
    for (int i = 0; i < COS_V168_SHA_HEX; ++i) if (out_hex[i] == '\0') out_hex[i] = '0';
    out_hex[COS_V168_SHA_HEX] = '\0';
}

/* ------------------------------------------------------------- */
/* σ_reputation                                                  */
/* ------------------------------------------------------------- */

float cos_v168_recompute_sigma(const cos_v168_artifact_t *a) {
    if (!a) return 1.0f;
    float fail_rate = 0.0f;
    if (a->tests_total > 0) {
        int failed = a->tests_total - a->tests_passed;
        if (failed < 0) failed = 0;
        fail_rate = (float)failed / (float)a->tests_total;
    } else {
        /* no tests → pessimistic */
        fail_rate = 0.6f;
    }
    float neg_rate = 0.0f;
    if (a->user_reports_total > 0) {
        neg_rate = (float)a->user_reports_negative /
                   (float)a->user_reports_total;
    }
    float bench_pen = 0.0f;
    if (a->benchmark_delta_pct < 0.0f) {
        bench_pen = -a->benchmark_delta_pct * 0.01f;
    }
    float sigma = 0.6f * fail_rate + 0.3f * neg_rate + 0.1f * bench_pen;
    return clampf(sigma, 0.0f, 1.0f);
}

/* ------------------------------------------------------------- */
/* Init                                                          */
/* ------------------------------------------------------------- */

static void push_artifact(cos_v168_registry_t *r,
                          const char *id, const char *name,
                          const char *version, cos_v168_kind_t kind,
                          int tp, int tt, int urt, int urn,
                          float bench, const char *author) {
    if (r->n_artifacts >= COS_V168_MAX_ARTIFACTS) return;
    cos_v168_artifact_t *a = &r->artifacts[r->n_artifacts++];
    memset(a, 0, sizeof(*a));
    safe_copy(a->id,      sizeof(a->id),      id);
    safe_copy(a->name,    sizeof(a->name),    name);
    safe_copy(a->version, sizeof(a->version), version);
    safe_copy(a->author,  sizeof(a->author),  author);
    a->kind                  = kind;
    a->tests_passed          = tp;
    a->tests_total           = tt;
    a->user_reports_total    = urt;
    a->user_reports_negative = urn;
    a->benchmark_delta_pct   = bench;
    a->installed             = false;
    fake_sha(a->id, a->version, a->sha_hex);
    a->sigma_reputation = cos_v168_recompute_sigma(a);
}

void cos_v168_init(cos_v168_registry_t *r) {
    memset(r, 0, sizeof(*r));
    r->sigma_install_threshold = 0.50f;

    /* healthy artifacts (σ ≈ 0.0..0.15) */
    push_artifact(r, "spektre-labs/skill-algebra",  "skill-algebra",
                  "0.3.1", COS_V168_KIND_SKILL,
                  100, 100, 50, 2, +3.2f, "spektre-labs");
    push_artifact(r, "spektre-labs/kernel-fast-math","kernel-fast-math",
                  "0.2.0", COS_V168_KIND_KERNEL,
                  40, 40, 30, 1, +1.1f, "spektre-labs");
    push_artifact(r, "spektre-labs/plugin-calculator","plugin-calculator",
                  "1.0.0", COS_V168_KIND_PLUGIN,
                  20, 20, 80, 3, 0.0f, "spektre-labs");

    /* medium-reputation (σ ≈ 0.2..0.45) — will install but
     * warn-worthy */
    push_artifact(r, "community/skill-geo",          "skill-geo",
                  "0.1.0", COS_V168_KIND_SKILL,
                  35, 50, 20, 4,  -1.8f, "community");
    push_artifact(r, "community/plugin-web-fetch",   "plugin-web-fetch",
                  "0.2.3", COS_V168_KIND_PLUGIN,
                  18, 24, 40, 6,  -0.5f, "community");

    /* high-σ artifact (σ > 0.50) — must be refused unless forced */
    push_artifact(r, "random/kernel-experimental",   "kernel-experimental",
                  "0.0.1", COS_V168_KIND_KERNEL,
                   2, 20, 10, 6,  -8.0f, "random");
}

/* ------------------------------------------------------------- */
/* Search                                                        */
/* ------------------------------------------------------------- */

int cos_v168_search(const cos_v168_registry_t *r,
                    const char *term,
                    bool out_mask[COS_V168_MAX_ARTIFACTS]) {
    if (!r || !out_mask) return -1;
    int hits = 0;
    for (int i = 0; i < r->n_artifacts; ++i) {
        bool m = false;
        if (!term || !*term) m = true;
        else {
            m = (strstr(r->artifacts[i].id,   term) != NULL) ||
                (strstr(r->artifacts[i].name, term) != NULL);
        }
        out_mask[i] = m;
        if (m) hits++;
    }
    return hits;
}

static cos_v168_artifact_t *find_by_id(cos_v168_registry_t *r, const char *id) {
    if (!r || !id) return NULL;
    for (int i = 0; i < r->n_artifacts; ++i)
        if (strcmp(r->artifacts[i].id, id) == 0) return &r->artifacts[i];
    return NULL;
}

/* ------------------------------------------------------------- */
/* Install + publish                                             */
/* ------------------------------------------------------------- */

cos_v168_install_receipt_t cos_v168_install(cos_v168_registry_t *r,
                                            const char *id,
                                            bool force) {
    cos_v168_install_receipt_t out;
    memset(&out, 0, sizeof(out));
    if (id) safe_copy(out.id, sizeof(out.id), id);

    cos_v168_artifact_t *a = find_by_id(r, id);
    if (!a) {
        out.status = COS_V168_NOT_FOUND;
        safe_copy(out.note, sizeof(out.note), "not in registry");
        return out;
    }
    out.sigma_reputation = a->sigma_reputation;

    if (a->installed) {
        out.status = COS_V168_ALREADY_INSTALLED;
        snprintf(out.note, sizeof(out.note),
                 "already at v%s (σ=%.3f)",
                 a->version, (double)a->sigma_reputation);
        return out;
    }

    if (a->sigma_reputation > r->sigma_install_threshold && !force) {
        out.status = COS_V168_REFUSED;
        snprintf(out.note, sizeof(out.note),
                 "σ_reputation=%.3f > threshold=%.2f — use --force to override",
                 (double)a->sigma_reputation,
                 (double)r->sigma_install_threshold);
        return out;
    }

    a->installed = true;
    r->n_installs++;
    if (a->sigma_reputation > r->sigma_install_threshold && force) {
        out.status = COS_V168_FORCED;
        out.forced = true;
        snprintf(out.note, sizeof(out.note),
                 "forced install despite σ=%.3f (> %.2f)",
                 (double)a->sigma_reputation,
                 (double)r->sigma_install_threshold);
    } else {
        out.status = COS_V168_OK;
        snprintf(out.note, sizeof(out.note),
                 "installed v%s, σ=%.3f",
                 a->version, (double)a->sigma_reputation);
    }
    return out;
}

cos_v168_artifact_t *cos_v168_publish(cos_v168_registry_t *r,
                                      const cos_v168_artifact_t *in) {
    if (!r || !in) return NULL;
    if (r->n_artifacts >= COS_V168_MAX_ARTIFACTS) return NULL;
    if (find_by_id(r, in->id)) return NULL;
    cos_v168_artifact_t *a = &r->artifacts[r->n_artifacts++];
    *a = *in;
    a->installed = false;
    fake_sha(a->id, a->version, a->sha_hex);
    a->sigma_reputation = cos_v168_recompute_sigma(a);
    return a;
}

/* ------------------------------------------------------------- */
/* Fixture validation                                            */
/* ------------------------------------------------------------- */

int cos_v168_validate_cos_skill(const char *files_nul, size_t len) {
    if (!files_nul || len == 0) return -1;
    static const char *required[] = {
        "adapter.safetensors",
        "template.txt",
        "test.jsonl",
        "meta.toml",
        "README.md",
    };
    const int N = (int)(sizeof(required) / sizeof(required[0]));
    int found[5] = {0};
    size_t off = 0;
    while (off < len) {
        const char *s = files_nul + off;
        size_t n = strnlen(s, len - off);
        if (n == 0) break;
        for (int j = 0; j < N; ++j) {
            if (strcmp(s, required[j]) == 0) { found[j] = 1; break; }
        }
        off += n + 1;
    }
    for (int j = 0; j < N; ++j) if (!found[j]) return j + 1;
    return 0;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v168_to_json(const cos_v168_registry_t *r,
                        char *buf, size_t cap) {
    if (!r || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v168\",\"n_artifacts\":%d,"
                     "\"n_installs\":%d,"
                     "\"sigma_install_threshold\":%.4f,\"artifacts\":[",
                     r->n_artifacts, r->n_installs,
                     (double)r->sigma_install_threshold);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;

    for (int i = 0; i < r->n_artifacts; ++i) {
        const cos_v168_artifact_t *a = &r->artifacts[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":\"%s\",\"name\":\"%s\",\"version\":\"%s\","
            "\"kind\":\"%s\",\"sha\":\"%s\",\"author\":\"%s\","
            "\"tests_passed\":%d,\"tests_total\":%d,"
            "\"user_reports_total\":%d,\"user_reports_negative\":%d,"
            "\"benchmark_delta_pct\":%.4f,"
            "\"sigma_reputation\":%.4f,\"installed\":%s}",
            i == 0 ? "" : ",",
            a->id, a->name, a->version, cos_v168_kind_name(a->kind),
            a->sha_hex, a->author,
            a->tests_passed, a->tests_total,
            a->user_reports_total, a->user_reports_negative,
            (double)a->benchmark_delta_pct,
            (double)a->sigma_reputation,
            a->installed ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v168_install_to_json(const cos_v168_install_receipt_t *r,
                                char *buf, size_t cap) {
    if (!r || !buf) return 0;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v168\",\"event\":\"install\",\"id\":\"%s\","
        "\"status\":\"%s\",\"sigma_reputation\":%.4f,"
        "\"forced\":%s,\"note\":\"%s\"}",
        r->id, cos_v168_status_name(r->status),
        (double)r->sigma_reputation,
        r->forced ? "true" : "false", r->note);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v168_self_test(void) {
    cos_v168_registry_t r;
    cos_v168_init(&r);
    if (r.n_artifacts != 6) return 1;

    /* find healthy artifact + install → OK */
    cos_v168_install_receipt_t ok =
        cos_v168_install(&r, "spektre-labs/skill-algebra", false);
    if (ok.status != COS_V168_OK) return 2;
    if (ok.sigma_reputation > 0.1f) return 3;  /* 100/100, positive bench */

    /* already installed */
    cos_v168_install_receipt_t dup =
        cos_v168_install(&r, "spektre-labs/skill-algebra", false);
    if (dup.status != COS_V168_ALREADY_INSTALLED) return 4;

    /* high-σ must refuse without force */
    cos_v168_install_receipt_t ref =
        cos_v168_install(&r, "random/kernel-experimental", false);
    if (ref.status != COS_V168_REFUSED) return 5;
    if (ref.sigma_reputation <= 0.5f) return 6;

    /* forced install */
    cos_v168_install_receipt_t frc =
        cos_v168_install(&r, "random/kernel-experimental", true);
    if (frc.status != COS_V168_FORCED) return 7;
    if (!frc.forced)                    return 8;

    /* not found */
    cos_v168_install_receipt_t nf =
        cos_v168_install(&r, "nobody/nothing", false);
    if (nf.status != COS_V168_NOT_FOUND) return 9;

    /* search */
    bool mask[COS_V168_MAX_ARTIFACTS];
    int hits = cos_v168_search(&r, "skill", mask);
    if (hits < 2) return 10;

    /* publish */
    cos_v168_artifact_t new_one = {0};
    safe_copy(new_one.id,      sizeof(new_one.id),      "spektre-labs/skill-geo-pro");
    safe_copy(new_one.name,    sizeof(new_one.name),    "skill-geo-pro");
    safe_copy(new_one.version, sizeof(new_one.version), "0.4.0");
    safe_copy(new_one.author,  sizeof(new_one.author),  "spektre-labs");
    new_one.kind = COS_V168_KIND_SKILL;
    new_one.tests_passed = 60; new_one.tests_total = 60;
    new_one.user_reports_total = 10; new_one.user_reports_negative = 0;
    new_one.benchmark_delta_pct = 1.0f;
    cos_v168_artifact_t *added = cos_v168_publish(&r, &new_one);
    if (!added) return 11;
    if (r.n_artifacts != 7) return 12;
    if (added->sigma_reputation > 0.1f) return 13;
    if (strlen(added->sha_hex) != COS_V168_SHA_HEX) return 14;

    /* fixture validator: OK case */
    const char ok_files[] =
        "adapter.safetensors\0template.txt\0test.jsonl\0meta.toml\0README.md\0";
    if (cos_v168_validate_cos_skill(ok_files, sizeof(ok_files)) != 0) return 15;

    /* fixture validator: missing README */
    const char missing[] =
        "adapter.safetensors\0template.txt\0test.jsonl\0meta.toml\0";
    if (cos_v168_validate_cos_skill(missing, sizeof(missing)) == 0) return 16;

    return 0;
}
