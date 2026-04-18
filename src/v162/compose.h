/*
 * v162 σ-Compose — kernel composition for the user.
 *
 * Creation OS ships 163+ kernels.  v162 makes the set
 * composable: every kernel publishes a manifest (what it
 * provides, what it requires, its latency/σ-impact), profiles
 * select a subset (lean / researcher / sovereign / custom), a
 * dependency resolver completes the closure and rejects
 * cycles, and a hot-swap command adds or drops a kernel at
 * runtime without restarting the server.
 *
 * v162.0 (this file) is fully in-process:
 *   - a small baked manifest table for a representative subset
 *     of kernels (one per architectural tier);
 *   - four profiles with declared roots (lean/researcher/
 *     sovereign/custom);
 *   - a BFS closure resolver + DFS cycle checker;
 *   - a hot-swap simulator that records the enable/disable
 *     event + the σ-impact delta.
 *
 * v162.1 (named, not shipped) reads `kernels/vNN.manifest.toml`
 * from disk for every kernel and drives real process-level
 * enable/disable via a v106 /v1/compose endpoint; `cos enable
 * v150` / `cos disable v150` round-trips into v108's dashboard.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V162_COMPOSE_H
#define COS_V162_COMPOSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V162_MAX_KERNELS   64
#define COS_V162_MAX_DEPS      8
#define COS_V162_MAX_CHANNELS  4
#define COS_V162_MAX_NAME      32
#define COS_V162_MAX_PROFILES  4
#define COS_V162_MAX_ROOTS     16

typedef struct {
    char     id[COS_V162_MAX_NAME];          /* "v106", "v150", … */
    char     provides[COS_V162_MAX_NAME];    /* short capability tag */
    int      n_requires;
    char     requires[COS_V162_MAX_DEPS][COS_V162_MAX_NAME];
    int      added_latency_ms;
    int      added_memory_mb;
    int      n_channels;
    char     channels[COS_V162_MAX_CHANNELS][COS_V162_MAX_NAME];
    bool     enabled;
} cos_v162_manifest_t;

typedef enum {
    COS_V162_PROFILE_LEAN       = 0,
    COS_V162_PROFILE_RESEARCHER = 1,
    COS_V162_PROFILE_SOVEREIGN  = 2,
    COS_V162_PROFILE_CUSTOM     = 3,
} cos_v162_profile_kind_t;

typedef struct {
    cos_v162_profile_kind_t kind;
    char     name[COS_V162_MAX_NAME];
    int      n_roots;
    char     roots[COS_V162_MAX_ROOTS][COS_V162_MAX_NAME];
} cos_v162_profile_t;

typedef enum {
    COS_V162_EVENT_ENABLE  = 0,
    COS_V162_EVENT_DISABLE = 1,
} cos_v162_event_kind_t;

typedef struct {
    cos_v162_event_kind_t kind;
    char   kernel[COS_V162_MAX_NAME];
    int    added_latency_ms;
    int    added_memory_mb;
} cos_v162_event_t;

typedef struct {
    int                 n_manifests;
    cos_v162_manifest_t manifests[COS_V162_MAX_KERNELS];

    int                 n_profiles;
    cos_v162_profile_t  profiles[COS_V162_MAX_PROFILES];

    /* Resolver output for the last selected profile. */
    int                 n_enabled;
    char                enabled_ids[COS_V162_MAX_KERNELS][COS_V162_MAX_NAME];
    bool                cycle_detected;
    char                cycle_msg[96];
    int                 total_latency_ms;
    int                 total_memory_mb;

    /* Hot-swap event log. */
    int                 n_events;
    cos_v162_event_t    events[COS_V162_MAX_KERNELS * 2];

    cos_v162_profile_kind_t active_profile;
} cos_v162_composer_t;

/* Initialize with baked manifests + the four profiles. */
void cos_v162_composer_init(cos_v162_composer_t *c);

/* Select a profile; resolves the closure into `enabled_ids`.
 * Returns number of enabled kernels, or -1 on cycle / unknown
 * root / capacity overflow.  For `CUSTOM`, the caller must have
 * populated profile[CUSTOM].roots beforehand (helper below). */
int cos_v162_composer_select(cos_v162_composer_t *c,
                             cos_v162_profile_kind_t kind);

/* Set custom profile roots by comma-separated string e.g.
 * "v106,v111,v115,v150,v153".  Returns number of roots parsed.
 */
int cos_v162_composer_set_custom(cos_v162_composer_t *c, const char *roots);

/* Runtime hot-swap: enable or disable a kernel and update
 * enabled_ids + total σ-impact budget.  Returns 0 on success,
 * -1 on unknown kernel, -2 on attempt to disable a kernel that
 * is currently depended on by another enabled kernel.  Disable
 * respects the dependency graph. */
int cos_v162_composer_enable(cos_v162_composer_t *c, const char *kernel);
int cos_v162_composer_disable(cos_v162_composer_t *c, const char *kernel);

/* Query. */
bool cos_v162_composer_is_enabled(const cos_v162_composer_t *c,
                                  const char *kernel);
const cos_v162_manifest_t *cos_v162_composer_lookup(
    const cos_v162_composer_t *c, const char *kernel);

/* JSON serializer — current resolver state + enabled list +
 * event log. */
size_t cos_v162_composer_to_json(const cos_v162_composer_t *c,
                                 char *buf, size_t cap);

int cos_v162_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V162_COMPOSE_H */
