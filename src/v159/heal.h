/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v159 σ-Heal — self-healing infrastructure kernel.
 *
 * Creation OS is a cognitive architecture. v159 makes it
 * self-healing: a health-monitor loop probes six component
 * signals at a fixed cadence, diagnoses failures against a
 * declared dependency graph, emits a σ_health per component and
 * a σ_heal for the system as a whole, executes a bounded
 * self-repair action (restart | flush | rollback | restore |
 * refetch | reboot-bridge), and produces an auditable heal
 * receipt.  A predictive pass runs a 3-day σ slope detector on
 * top of v131-temporal to schedule preemptive repairs *before*
 * a failing component breaks.
 *
 * v159.0 ships the loop, the dependency graph, the RCA, the
 * σ_health aggregator, the self-repair action executor
 * (deterministic in-process stubs — no real restart, no real
 * SQLite, no real download), and a deterministic 7-day
 * synthetic slope oracle for the predictive pass.  v159.1
 * promotes the stubs to real shell-outs + a real systemd-style
 * daemon wrapper + a /v1/health history endpoint on v106.
 *
 * Pure C11 + libm.  No network.  No filesystem writes.  Fully
 * deterministic in (seed, tick-count).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V159_HEAL_H
#define COS_V159_HEAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V159_MAX_COMPONENTS 8
#define COS_V159_MAX_NAME       32
#define COS_V159_MAX_MSG        96
#define COS_V159_MAX_HISTORY    64
#define COS_V159_MAX_SLOPE_DAYS 7

typedef enum {
    COS_V159_COMP_HTTP_SERVER     = 0,  /* v106 /health */
    COS_V159_COMP_SIGMA_CHANNELS  = 1,  /* v101 σ producing values */
    COS_V159_COMP_MEMORY_SQLITE   = 2,  /* v115 memory */
    COS_V159_COMP_KV_CACHE        = 3,  /* v117 paged KV */
    COS_V159_COMP_ADAPTER_VERSION = 4,  /* v124 continual LoRA */
    COS_V159_COMP_MERGE_GATE      = 5,  /* make check-smoke */
    COS_V159_COMP_BRIDGE          = 6,  /* v101 bitnet bridge */
    COS_V159_COMP_WEIGHTS         = 7,  /* GGUF file on disk */
} cos_v159_component_t;

typedef enum {
    COS_V159_OK                = 0,
    COS_V159_DEGRADED          = 1,
    COS_V159_FAIL              = 2,
} cos_v159_health_t;

typedef enum {
    COS_V159_ACT_NONE           = 0,
    COS_V159_ACT_RESTART        = 1,  /* HTTP restart */
    COS_V159_ACT_FLUSH_KV       = 2,  /* KV-cache flush */
    COS_V159_ACT_ROLLBACK_LORA  = 3,  /* v124 rollback */
    COS_V159_ACT_RESTORE_SQLITE = 4,  /* SQLite restore */
    COS_V159_ACT_REFETCH_GGUF   = 5,  /* GGUF re-download */
    COS_V159_ACT_RESTART_BRIDGE = 6,  /* bridge crash recovery */
    COS_V159_ACT_PREEMPTIVE     = 7,  /* 3-day slope preemption */
} cos_v159_action_t;

typedef struct {
    cos_v159_component_t id;
    char                 name[COS_V159_MAX_NAME];
    float                sigma_health;      /* [0,1], higher = worse */
    cos_v159_health_t    status;
    bool                 failed_this_tick;
    float                slope_3d;          /* σ_health day_7 - day_5 */
    bool                 degrading;         /* monotone rise ≥ 3 ticks */
    char                 detail[COS_V159_MAX_MSG];
} cos_v159_component_state_t;

typedef struct {
    uint64_t                   timestamp;       /* synthetic tick # */
    cos_v159_component_t       component;
    char                       component_name[COS_V159_MAX_NAME];
    char                       failure[COS_V159_MAX_MSG];
    cos_v159_action_t          action;
    char                       action_name[COS_V159_MAX_NAME];
    bool                       succeeded;
    float                      sigma_before;
    float                      sigma_after;
    char                       root_cause[COS_V159_MAX_MSG];
    bool                       predictive;      /* true iff preemptive */
} cos_v159_heal_receipt_t;

typedef struct {
    uint64_t                   seed;
    int                        n_components;
    cos_v159_component_state_t components[COS_V159_MAX_COMPONENTS];
    float                      sigma_heal;      /* system-level geomean */
    int                        n_failing;
    int                        n_receipts;
    cos_v159_heal_receipt_t    receipts[COS_V159_MAX_HISTORY];
    /* slope history — σ_health samples per component for the last
     * COS_V159_MAX_SLOPE_DAYS ticks (day-0 = oldest, day-6 = newest). */
    float                      slope_history[COS_V159_MAX_COMPONENTS]
                                              [COS_V159_MAX_SLOPE_DAYS];
    int                        slope_filled;    /* how many days present */
} cos_v159_daemon_t;

/* Initialize the daemon with component names + healthy σ baselines. */
void cos_v159_daemon_init(cos_v159_daemon_t *d, uint64_t seed);

/* Run a single health tick.  `scenario` ∈ {0..5}:
 *   0 — all green (no failures injected)
 *   1 — HTTP server down          → RESTART
 *   2 — KV-cache degenerate       → FLUSH_KV
 *   3 — adapter corrupted         → ROLLBACK_LORA
 *   4 — SQLite corrupted          → RESTORE_SQLITE
 *   5 — GGUF weights missing      → REFETCH_GGUF (cascade: bridge + HTTP)
 *
 * Returns the number of heal receipts produced this tick.
 */
int cos_v159_daemon_tick(cos_v159_daemon_t *d, int scenario);

/* Run the predictive slope detector.  Synthesizes a 7-day
 * σ_health trajectory for the `target` component with a monotone
 * rise, then asserts `degrading=true` and schedules a
 * PREEMPTIVE receipt before the component actually fails. */
int cos_v159_daemon_predict(cos_v159_daemon_t *d,
                            cos_v159_component_t target);

/* System-level σ_heal = geomean of per-component σ_health. */
float cos_v159_daemon_sigma_heal(const cos_v159_daemon_t *d);

/* JSON serializer for the daemon's full state (components +
 * receipts).  Returns bytes written (excluding NUL), 0 on
 * failure. */
size_t cos_v159_daemon_to_json(const cos_v159_daemon_t *d,
                               char *buf, size_t cap);

/* Human-readable action label. */
const char *cos_v159_action_name(cos_v159_action_t a);

/* Human-readable component label. */
const char *cos_v159_component_name(cos_v159_component_t c);

/* Exhaustive self-test.  Returns 0 on pass, nonzero contract id
 * (H1..H7) on failure. */
int cos_v159_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V159_HEAL_H */
