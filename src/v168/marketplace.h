/*
 * v168 σ-Marketplace — kernel / skill / plugin marketplace.
 *
 * v145 skill library is local.  v164 plugin is local.  v168
 * turns both into shareable artifacts with a σ-reputation
 * surface that warns users before installing something
 * uncertain.
 *
 * Artifacts are one of:
 *   - SKILL   — LoRA adapter + template + test.jsonl + meta
 *   - KERNEL  — C source + header + test + manifest
 *   - PLUGIN  — v164 plugin manifest + compiled .dylib
 *
 * Each artifact carries:
 *   - SHA256-of-manifest (hex)          ← integrity
 *   - semver version string             ← compatibility
 *   - σ_reputation ∈ [0, 1]             ← quality signal
 *   - tests_passed / tests_total        ← drives σ_reputation
 *   - user_reports_negative count       ← drives σ_reputation
 *   - v143_benchmark_delta_pct          ← positive if better
 *
 * σ_reputation model:
 *   fail_rate = (tests_total - tests_passed) / max(tests_total, 1)
 *   neg_rate  = user_reports_negative / max(user_reports_total, 1)
 *   bench_pen = max(0, -benchmark_delta) * 0.01
 *   σ_reputation = clamp(0.6*fail_rate + 0.3*neg_rate + 0.1*bench_pen,
 *                        0, 1)
 *
 * σ-gated install:
 *   if σ_reputation > 0.50 → refuse install without
 *   `force=true` override, emit a compliance-grade warning.
 *
 * v168.0 (this file) ships:
 *   - a baked registry of 6 artifacts (2 skills, 2 kernels,
 *     2 plugins) with varied σ_reputation scores so all paths
 *     (install, warn, refuse) are exercised
 *   - `cos market search TERM`    → filter-by-substring
 *   - `cos market install ID`     → σ-gated install
 *   - `cos market install ID --force` → override
 *   - `cos market publish ID ...` → append a new (tested)
 *     artifact to the registry
 *   - `.cos-skill` fixture structure validator
 *
 * v168.1 (named, not shipped):
 *   - real registry over HTTPS (registry.creation-os.dev)
 *   - real SHA256 computed from a packed .cos-skill tarball
 *   - user feedback ingestion pipeline
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V168_MARKETPLACE_H
#define COS_V168_MARKETPLACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V168_MAX_ARTIFACTS    16
#define COS_V168_MAX_NAME         48
#define COS_V168_MAX_MSG         160
#define COS_V168_SHA_HEX          16   /* 8 bytes hex for determinism demo */

typedef enum {
    COS_V168_KIND_SKILL   = 0,
    COS_V168_KIND_KERNEL  = 1,
    COS_V168_KIND_PLUGIN  = 2,
} cos_v168_kind_t;

typedef enum {
    COS_V168_OK               = 0,
    COS_V168_REFUSED          = 1,  /* σ too high, no force */
    COS_V168_NOT_FOUND        = 2,
    COS_V168_ALREADY_INSTALLED = 3,
    COS_V168_FIXTURE_INVALID  = 4,
    COS_V168_FORCED           = 5,  /* installed despite high σ */
} cos_v168_status_t;

typedef struct {
    char            id[COS_V168_MAX_NAME];        /* "spektre-labs/skill-algebra" */
    char            name[COS_V168_MAX_NAME];
    char            version[COS_V168_MAX_NAME];   /* "0.3.1" */
    cos_v168_kind_t kind;
    char            sha_hex[COS_V168_SHA_HEX + 1];
    int             tests_passed;
    int             tests_total;
    int             user_reports_total;
    int             user_reports_negative;
    float           benchmark_delta_pct;          /* +% is better */
    float           sigma_reputation;
    bool            installed;
    char            author[COS_V168_MAX_NAME];
} cos_v168_artifact_t;

typedef struct {
    int                   n_artifacts;
    cos_v168_artifact_t   artifacts[COS_V168_MAX_ARTIFACTS];
    float                 sigma_install_threshold;  /* default 0.50 */
    int                   n_installs;
} cos_v168_registry_t;

typedef struct {
    cos_v168_status_t status;
    char              id[COS_V168_MAX_NAME];
    float             sigma_reputation;
    bool              forced;
    char              note[COS_V168_MAX_MSG];
} cos_v168_install_receipt_t;

/* Bake the 6 default artifacts, recompute σ_reputation, mark
 * none as installed.  Threshold defaults to 0.50. */
void cos_v168_init(cos_v168_registry_t *r);

/* Filter artifacts whose id OR name contains `term` (case-sensitive).
 * Caller passes a bool array of length n_artifacts. */
int  cos_v168_search(const cos_v168_registry_t *r,
                     const char *term,
                     bool out_mask[COS_V168_MAX_ARTIFACTS]);

/* σ-gated install (or forced). */
cos_v168_install_receipt_t cos_v168_install(cos_v168_registry_t *r,
                                            const char *id,
                                            bool force);

/* Publish a new tested artifact; returns a pointer to the
 * appended entry (caller should not free).  Returns NULL on
 * full registry / duplicate id. */
cos_v168_artifact_t *cos_v168_publish(cos_v168_registry_t *r,
                                      const cos_v168_artifact_t *in);

/* Recompute σ_reputation for a single artifact from its
 * tests/reports/benchmark stats. */
float cos_v168_recompute_sigma(const cos_v168_artifact_t *a);

/* Validate a .cos-skill fixture directory's claimed fileset
 * (passed as a NUL-separated list).  Returns 0 on valid,
 * non-zero on missing files. */
int  cos_v168_validate_cos_skill(const char *files_nul, size_t len);

/* JSON emitters. */
size_t cos_v168_to_json(const cos_v168_registry_t *r,
                        char *buf, size_t cap);
size_t cos_v168_install_to_json(const cos_v168_install_receipt_t *r,
                                char *buf, size_t cap);

const char *cos_v168_kind_name(cos_v168_kind_t k);
const char *cos_v168_status_name(cos_v168_status_t s);

int cos_v168_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V168_MARKETPLACE_H */
