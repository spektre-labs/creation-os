/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v293 σ-Hagia-Sofia — continuous use = longevity.
 *
 *   Hagia Sofia has stood for 1500 years because
 *   someone was always using it: cathedral →
 *   mosque → museum → mosque.  Code lives the same
 *   way: a kernel in continuous use survives
 *   vendor changes, license pivots, and ecosystem
 *   fashion.  v293 types longevity discipline as
 *   four v0 manifests: adoption is measured,
 *   kernels serve multiple domains, community
 *   owns the long tail, and re-purposing keeps
 *   the stones in service.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Adoption metrics (exactly 3 canonical
 *   metrics, all tracked):
 *     `daily_users` · `api_calls` ·
 *     `sigma_evaluations`, each `tracked == true`.
 *     Contract: 3 canonical metrics in order; every
 *     metric tracked — v293 names the instrument
 *     panel for `cos status --adoption`.
 *
 *   Multi-purpose kernels (exactly 3 canonical
 *   domains where the σ-gate applies):
 *     `llm` · `sensor` · `organization`, each
 *     `sigma_gate_applicable == true`.
 *     Contract: 3 canonical domains in order;
 *     σ-gate applicable to every domain — one
 *     function, many rooms, same as Hagia Sofia.
 *
 *   Community longevity (exactly 3 canonical
 *   properties, all hold):
 *     `open_source_agpl` · `community_maintainable`
 *     · `vendor_independent`, each
 *     `holds == true`.
 *     Contract: 3 canonical properties in order;
 *     every property holds — the building outlasts
 *     its original caretakers because caretakers
 *     are not a single vendor.
 *
 *   Adaptive re-purposing lifecycle (exactly 3
 *   canonical phases, all alive):
 *     `active_original_purpose` → `use_count = 100`;
 *     `declining_usage`         → `use_count = 20`,
 *                                  `warning_issued
 *                                     = true`;
 *     `repurposed`              → `use_count = 80`,
 *                                  `new_domain_found
 *                                     = true`.
 *     Each: `phase`, `use_count ≥ 0`,
 *           `alive == true`.
 *     Contract: 3 canonical phases in order; every
 *     phase alive; the `declining_usage` row
 *     carries `warning_issued`; the `repurposed`
 *     row carries `new_domain_found` — the lifecycle
 *     has a documented re-use, not a retirement.
 *
 *   σ_hagia (surface hygiene):
 *       σ_hagia = 1 −
 *         (metric_rows_ok + metric_all_tracked_ok +
 *          domain_rows_ok + domain_all_applicable_ok +
 *          community_rows_ok +
 *          community_all_hold_ok +
 *          lifecycle_rows_ok +
 *          lifecycle_all_alive_ok +
 *          lifecycle_warning_ok +
 *          lifecycle_new_domain_ok) /
 *         (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 1 + 1)
 *   v0 requires `σ_hagia == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 adoption metrics canonical; all tracked.
 *     2. 3 multi-purpose domains canonical; σ-gate
 *        applicable to every domain.
 *     3. 3 community properties canonical; all hold.
 *     4. 3 lifecycle phases canonical; all alive;
 *        `declining_usage` warns;
 *        `repurposed` finds a new domain.
 *     5. σ_hagia ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v293.1 (named, not implemented): live adoption
 *     telemetry driving `cos status --adoption`,
 *     automatic warnings when a kernel's use_count
 *     falls below threshold, and a re-purposing
 *     registry that tracks how a single kernel
 *     evolves across domains over time.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V293_HAGIA_SOFIA_H
#define COS_V293_HAGIA_SOFIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V293_N_METRIC     3
#define COS_V293_N_DOMAIN     3
#define COS_V293_N_COMMUNITY  3
#define COS_V293_N_LIFECYCLE  3

typedef struct {
    char  name[20];
    bool  tracked;
} cos_v293_metric_t;

typedef struct {
    char  domain[14];
    bool  sigma_gate_applicable;
} cos_v293_domain_t;

typedef struct {
    char  property[24];
    bool  holds;
} cos_v293_community_t;

typedef struct {
    char  phase[24];
    int   use_count;
    bool  alive;
    bool  warning_issued;
    bool  new_domain_found;
} cos_v293_lifecycle_t;

typedef struct {
    cos_v293_metric_t     metric    [COS_V293_N_METRIC];
    cos_v293_domain_t     domain    [COS_V293_N_DOMAIN];
    cos_v293_community_t  community [COS_V293_N_COMMUNITY];
    cos_v293_lifecycle_t  lifecycle [COS_V293_N_LIFECYCLE];

    int   n_metric_rows_ok;
    bool  metric_all_tracked_ok;

    int   n_domain_rows_ok;
    bool  domain_all_applicable_ok;

    int   n_community_rows_ok;
    bool  community_all_hold_ok;

    int   n_lifecycle_rows_ok;
    bool  lifecycle_all_alive_ok;
    bool  lifecycle_warning_ok;
    bool  lifecycle_new_domain_ok;

    float sigma_hagia;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v293_state_t;

void   cos_v293_init(cos_v293_state_t *s, uint64_t seed);
void   cos_v293_run (cos_v293_state_t *s);

size_t cos_v293_to_json(const cos_v293_state_t *s,
                         char *buf, size_t cap);

int    cos_v293_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V293_HAGIA_SOFIA_H */
