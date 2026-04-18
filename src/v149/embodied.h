/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v149 σ-Embodied — digital twin, σ-gated action, sim-to-real gap.
 *
 * The v144–v148 sovereign stack runs in text space.  v149 extends
 * Creation OS into a physical-ish substrate: a deterministic
 * rigid-body sim that stands in for MuJoCo's `mj_step` call
 * (MuJoCo 3.x is CPU-only, M3-friendly, but a heavy binary
 * dependency we do not want on merge-gate).  The sim in v149.0 is
 * a 3-DOF arm state (x, y, z) plus a 3-DOF held-object state;
 * mj_step is replaced with a pure-C linear physics that exposes
 * every σ-gate the real MuJoCo call will exercise in v149.1.
 *
 * Three σ measurements, all weights-free and byte-identical across
 * runs:
 *
 *   σ_embodied = ‖s_pred − s_actual_sim‖ / ‖s_actual_sim‖
 *     — world-model (v139) prediction error on the simulated
 *       physics.  Low σ_embodied ⇒ the model understands the
 *       sim step it just executed; high σ_embodied ⇒ the model
 *       is surprised and the v121 planner should pause / replan.
 *
 *   σ_gap = ‖s_actual_sim − s_actual_real‖ / ‖s_actual_real‖
 *     — sim-to-real drift.  The "real" digital twin applies a
 *       configurable bias + Gaussian noise on top of the sim
 *       step.  Low σ_gap ⇒ transfer safe; high σ_gap ⇒ sim
 *       predictions cannot be trusted on hardware.
 *
 *   σ_safe = user-supplied τ that gates action execution.  If
 *       σ_embodied > σ_safe, cos_v149_step_gated() refuses to
 *       commit the action and returns the last safe state.
 *
 * v149.0 is deterministic (SplitMix64 + Box–Muller) and never
 * opens a file.  v149.1 wires real MuJoCo XML scenes, a v108
 * WebGL visualiser (3-D sim + σ-overlay), POST /v1/embodied/step
 * on v106 HTTP, and v140 counterfactual propagation for pre-
 * flight do-calculus on arm motions.
 */
#ifndef COS_V149_EMBODIED_H
#define COS_V149_EMBODIED_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V149_STATE_DIM   6       /* arm(3) + object(3)         */
#define COS_V149_ACTIONS     6       /* ±x ±y ±z unit directions   */

typedef enum {
    COS_V149_MOVE_RIGHT = 0,
    COS_V149_MOVE_LEFT,
    COS_V149_MOVE_UP,
    COS_V149_MOVE_DOWN,
    COS_V149_MOVE_FORWARD,
    COS_V149_MOVE_BACK
} cos_v149_action_id_t;

typedef struct cos_v149_action {
    cos_v149_action_id_t  id;
    float                 magnitude;  /* default 0.05, clamped     */
} cos_v149_action_t;

typedef struct cos_v149_sim {
    float  state[COS_V149_STATE_DIM];       /* sim (ground truth)  */
    float  real_state[COS_V149_STATE_DIM];  /* real twin           */
    float  real_bias;                       /* actuator gain loss  */
    float  real_noise_sigma;                /* Gaussian σ on real  */
    uint64_t prng;
    int    t;                               /* step count          */
} cos_v149_sim_t;

typedef struct cos_v149_step_report {
    int   step_index;
    int   action_id;
    float magnitude;

    float s_pred   [COS_V149_STATE_DIM];
    float s_sim    [COS_V149_STATE_DIM];
    float s_real   [COS_V149_STATE_DIM];

    float norm_sim;
    float norm_real;
    float sigma_embodied;      /* against sim        */
    float sigma_gap;           /* sim vs real        */

    int   executed;            /* 0 iff σ-gate refused */
    int   gated_out;           /* !executed           */
    int   clamped_magnitude;
} cos_v149_step_report_t;

/* Initialise a sim with a given seed, bias, and noise floor.
 * Default arm/object start positions match the unit test. */
void  cos_v149_sim_init(cos_v149_sim_t *s, uint64_t seed,
                        float real_bias, float real_noise_sigma);

/* Deterministic sim physics: applies the action's unit delta
 * scaled by magnitude to the arm, and propagates an object that
 * sits beneath the arm (gravity-free in v149.0). */
void  cos_v149_sim_step(cos_v149_sim_t *s,
                        const cos_v149_action_t *a,
                        float out_state[COS_V149_STATE_DIM]);

/* "Real" twin step: same rule with bias + noise. */
void  cos_v149_real_step(cos_v149_sim_t *s,
                         const cos_v149_action_t *a,
                         float out_state[COS_V149_STATE_DIM]);

/* v139-style world-model prediction.  v149 uses the *nominal* sim
 * physics as its learned A, i.e. pred = s + a.  A real v149.1
 * plumbs cos_v139_predict() here; the σ-gate math is identical. */
void  cos_v149_predict_step(const float prior[COS_V149_STATE_DIM],
                            const cos_v149_action_t *a,
                            float out_pred[COS_V149_STATE_DIM]);

/* σ_embodied against sim (pred vs actual sim). */
float cos_v149_sigma_embodied(const float pred[COS_V149_STATE_DIM],
                              const float sim [COS_V149_STATE_DIM]);

/* σ_gap (sim vs real). */
float cos_v149_sigma_gap(const float sim [COS_V149_STATE_DIM],
                         const float real[COS_V149_STATE_DIM]);

/* One step with the safe-action gate: if predicted σ_embodied
 * (computed on sim using pred) exceeds σ_safe, the action is
 * refused, *s is left unchanged, and out->executed = 0.  Always
 * fills *out regardless. */
int   cos_v149_step_gated(cos_v149_sim_t *s,
                          const cos_v149_action_t *a,
                          float sigma_safe,
                          cos_v149_step_report_t *out);

/* Run N steps of a given plan (array of actions) through both
 * sim and real.  Writes a per-step report array (caller-owned)
 * and returns steps actually executed (may be fewer than n if
 * a step was σ-gated and `stop_on_gate` is non-zero). */
int   cos_v149_rollout(cos_v149_sim_t *s,
                       const cos_v149_action_t *plan, int n,
                       float sigma_safe, int stop_on_gate,
                       cos_v149_step_report_t *out);

int   cos_v149_step_to_json(const cos_v149_step_report_t *r,
                            char *buf, size_t cap);
int   cos_v149_sim_to_json (const cos_v149_sim_t *s,
                            char *buf, size_t cap);

int   cos_v149_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
