/*
 * v251 σ-Marketplace — kernel registry.
 *
 *   Creation OS kernels are modular.  v251 makes them
 *   shareable, discoverable, and quality-scored in a
 *   typed, σ-audited registry.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Registry URL (canonical, non-empty):
 *       registry.creation-os.dev
 *
 *   Published kernels (v0 fixture: 5 kernels):
 *     medical-v1 · legal-v1 · finance-v1 ·
 *     science-v1 · teach-pro-v1
 *   Every entry carries:
 *     - name, version (semver), deps (≥ 1 id),
 *     - 4 quality axes in [0, 1]:
 *         σ_calibration · benchmark_score ·
 *         user_rating   · audit_trail
 *     - σ_quality = mean of the four axes
 *     - σ_quality ∈ [0, 1]
 *
 *   Install fixture (exactly 1 action):
 *     install medical-v1 → dependencies auto-resolved
 *     (medical-v1 requires {v115, v111}).
 *     After install: kernel is in the local index AND
 *     dependencies have `installed == true`.
 *
 *   Composition fixture (exactly 1 pair):
 *     compose medical-v1 + legal-v1 → "medical-legal"
 *     σ_compatibility ∈ [0, 1]; a hard τ_compat = 0.50
 *     gate rejects incompatible pairs.  v0 fixture
 *     passes (σ_compatibility = 0.18).
 *
 *   Publish contract:
 *     Every new community kernel MUST:
 *       merge_gate_green = true
 *       sigma_profile_attached = true
 *       docs_attached = true
 *       scsl_license_attached = true
 *     Missing any of these → publish refused.
 *
 *   σ_marketplace (surface hygiene):
 *       σ_marketplace = 1 −
 *         (kernels_ok + install_ok + compose_ok +
 *          publish_contract_ok) /
 *         (5 + 1 + 1 + 4)
 *   v0 requires `σ_marketplace == 0.0`.
 *
 *   Contracts (v0):
 *     1. registry URL non-empty; canonical name in state.
 *     2. Exactly 5 kernels in canonical order; every
 *        `σ_quality ∈ [0, 1]` and equal to the mean of
 *        the four declared axes (±1e-4 slack).
 *     3. Install fixture: kernel installed,
 *        `deps_resolved == n_deps`, every dep has
 *        `installed == true`.
 *     4. Composition fixture: `σ_compatibility ∈ [0, 1]`,
 *        `σ_compatibility < τ_compat`, `compose_ok`.
 *     5. Publish contract has exactly 4 required items,
 *        all `required == true` AND `met == true`.
 *     6. σ_marketplace ∈ [0, 1] AND σ_marketplace == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v251.1 (named, not implemented): live
 *     `registry.creation-os.dev`, signed package manifests,
 *     `cos search` / `cos install` / `cos publish` CLI,
 *     live quality-score ingestion from v247 harness runs.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V251_MARKETPLACE_H
#define COS_V251_MARKETPLACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V251_N_KERNELS  5
#define COS_V251_MAX_DEPS   4
#define COS_V251_N_PUBLISH  4

typedef struct {
    char  name[24];
    char  version[12];
    int   deps[COS_V251_MAX_DEPS];
    int   n_deps;

    float sigma_calibration;
    float benchmark_score;
    float user_rating;
    float audit_trail;
    float sigma_quality;
} cos_v251_kernel_t;

typedef struct {
    char  kernel_name[24];
    int   deps_resolved;
    int   n_deps;
    bool  installed;
} cos_v251_install_t;

typedef struct {
    char   left[24];
    char   right[24];
    char   composed[48];
    float  sigma_compatibility;
    float  tau_compat;
    bool   compose_ok;
} cos_v251_compose_t;

typedef struct {
    char  item[32];
    bool  required;
    bool  met;
} cos_v251_publish_t;

typedef struct {
    char                  registry_url[48];

    cos_v251_kernel_t     kernels  [COS_V251_N_KERNELS];
    cos_v251_install_t    install;
    cos_v251_compose_t    compose;
    cos_v251_publish_t    publish  [COS_V251_N_PUBLISH];

    int                   n_kernels_ok;
    bool                  install_ok;
    bool                  compose_ok;
    int                   n_publish_met;

    float                 sigma_marketplace;

    bool                  chain_valid;
    uint64_t              terminal_hash;
    uint64_t              seed;
} cos_v251_state_t;

void   cos_v251_init(cos_v251_state_t *s, uint64_t seed);
void   cos_v251_run (cos_v251_state_t *s);

size_t cos_v251_to_json(const cos_v251_state_t *s,
                         char *buf, size_t cap);

int    cos_v251_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V251_MARKETPLACE_H */
