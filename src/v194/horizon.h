/*
 * v194 σ-Horizon — multi-horizon goal tree + σ-degradation monitor.
 *
 *   Long-horizon agents today collapse after ~35 minutes
 *   because they never measure their own degradation.  v194
 *   makes that measurement explicit: a three-tier goal tree
 *     (strategic  — week+,
 *      tactical   — day,
 *      operational— now)
 *   plus a per-tick σ_product monitor on the operational tier
 *   that triggers staged recovery when σ trends upward.
 *
 *   Recovery ladder (ordered by cost):
 *       1. KV-cache flush        (v117)
 *       2. context summarize+resume (v172)
 *       3. recommend break point + save progress (v115)
 *
 *   Checkpoint discipline: every operational task writes a
 *   deterministic checkpoint (hash-chained) so a crash never
 *   loses work; v181 audit holds the full trace.
 *
 *   Merge-gate contracts exercised on the fixture:
 *
 *     1. Goal tree has exactly 1 strategic, 3 tactical, 12
 *        operational nodes (1 → 3 → 4 fan-out), with each
 *        operational checkpoint-chained.
 *     2. σ_product on the 30-tick operational trajectory
 *        monotonically climbs over a 10-tick window → a
 *        `degradation_detected` flag must fire.
 *     3. On detection, the recovery ladder actually runs:
 *        ladder step `1 → 2 → 3` is recorded in order, and
 *        σ_product after step 3 is strictly lower than at the
 *        detection tick.
 *     4. Checkpoint hash chain replays byte-identically and
 *        survives a simulated crash (the run-from-checkpoint
 *        resume produces the same terminal hash).
 *     5. Byte-deterministic output.
 *
 * v194.0 (this file) ships a closed-form simulation:
 *   - sigma trajectory over 30 ticks injects a monotone
 *     10-tick climb centred near tick 12;
 *   - three recovery functions are closed-form operators on
 *     σ (flush = −0.10, summarize = −0.20, break = −0.35).
 *
 * v194.1 (named, not shipped):
 *   - live v115 memory persistence of the tree;
 *   - `cos goals` CLI walker over `~/.creation-os/goals.jsonl`;
 *   - real v117 + v172 + v181 wiring.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V194_HORIZON_H
#define COS_V194_HORIZON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V194_N_STRATEGIC      1
#define COS_V194_N_TACTICAL       3
#define COS_V194_N_OPERATIONAL   12
#define COS_V194_N_TICKS         30
#define COS_V194_WINDOW          10
#define COS_V194_STR_MAX         48

typedef enum {
    COS_V194_TIER_STRATEGIC   = 0,
    COS_V194_TIER_TACTICAL    = 1,
    COS_V194_TIER_OPERATIONAL = 2
} cos_v194_tier_t;

typedef enum {
    COS_V194_STAT_PENDING   = 0,
    COS_V194_STAT_ACTIVE    = 1,
    COS_V194_STAT_DONE      = 2,
    COS_V194_STAT_BLOCKED   = 3
} cos_v194_status_t;

typedef enum {
    COS_V194_ACT_FLUSH_KV     = 1,   /* v117 */
    COS_V194_ACT_SUMMARIZE    = 2,   /* v172 */
    COS_V194_ACT_BREAK_POINT  = 3    /* v115 */
} cos_v194_action_t;

typedef struct {
    int       id;
    int       parent_id;             /* -1 for root */
    int       tier;                  /* cos_v194_tier_t */
    char      name[COS_V194_STR_MAX];
    int       status;                /* cos_v194_status_t */
    float     sigma;
    uint64_t  created_tick;
    uint64_t  updated_tick;
    uint64_t  checkpoint_hash;       /* 0 for non-operational */
} cos_v194_goal_t;

typedef struct {
    int       tick;
    float     sigma_product;
    int       action_taken;          /* 0 = none, cos_v194_action_t otherwise */
    float     sigma_after;
    bool      degradation_detected;
} cos_v194_step_t;

typedef struct {
    /* Goal tree. */
    int               n_goals;
    cos_v194_goal_t   goals[COS_V194_N_STRATEGIC +
                            COS_V194_N_TACTICAL +
                            COS_V194_N_OPERATIONAL];

    /* Operational trajectory. */
    int               n_ticks;
    cos_v194_step_t   steps[COS_V194_N_TICKS];

    /* Monitoring. */
    int               window;
    float             tau_degrade_slope;        /* σ/tick to trigger */
    bool              degradation_detected;
    int               detection_tick;
    int               recovery_ladder[3];       /* ordered 1,2,3 */
    int               recovery_steps_run;

    /* Checkpoint chain. */
    bool              checkpoint_chain_valid;
    uint64_t          terminal_hash;
    bool              crash_recovery_matches;   /* resume reproduces hash */

    uint64_t          seed;
} cos_v194_state_t;

void   cos_v194_init(cos_v194_state_t *s, uint64_t seed);
void   cos_v194_build(cos_v194_state_t *s);
void   cos_v194_run(cos_v194_state_t *s);
bool   cos_v194_verify_checkpoints(const cos_v194_state_t *s);

size_t cos_v194_to_json(const cos_v194_state_t *s,
                         char *buf, size_t cap);

int    cos_v194_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V194_HORIZON_H */
