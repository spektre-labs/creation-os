/*
 * v176 σ-Simulator — synthetic worlds + dream training for
 * embodied learning.
 *
 *   v149 uses MuJoCo.  v176 *generates* MuJoCo-like worlds
 *   automatically, curricula them from easy→hard via v141,
 *   measures sim-to-sim transfer, and runs `dream` rollouts
 *   in the v139 latent world model so LoRA can improve
 *   without a single real MuJoCo step.
 *
 * v176.0 (this file) ships:
 *   - procedural generation of 6 worlds (room size, object
 *     count, friction, mass) each with a `realism_sigma`
 *     derived from parameter plausibility
 *   - a curriculum of 5 worlds ordered by difficulty
 *     σ_difficulty (easy first) using v141 weakness
 *   - a closed-form train→test transfer model over pairs
 *     (σ_transfer) that flags overfit worlds
 *   - 1000 latent "dream" rollouts with σ per rollout; the
 *     aggregate σ_dream is asserted ≤ σ_real (dreams are the
 *     safer learning signal when gated)
 *
 * v176.1 (named, not shipped):
 *   - real MuJoCo XML emission + headless simulate
 *   - real v141 curriculum hook
 *   - DreamerV3-style neural world model backing the rollouts
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V176_SIMULATOR_H
#define COS_V176_SIMULATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V176_N_WORLDS      6
#define COS_V176_CURRICULUM    5
#define COS_V176_DREAM_ROLLOUTS 1000
#define COS_V176_MAX_STR      96

typedef struct {
    int   id;
    char  name[COS_V176_MAX_STR];
    float room_size_m;            /* 2.0 .. 20.0 */
    int   n_objects;              /* 0 .. 32 */
    float friction;               /* 0.05 .. 1.5 */
    float mass_kg;                /* 0.1 .. 50.0 */
    float sigma_realism;          /* ∈ [0,1]; 0 = perfectly plausible */
    float sigma_difficulty;       /* harder worlds → higher σ */
} cos_v176_world_t;

typedef struct {
    int   train_world_id;
    int   test_world_id;
    float sigma_train;
    float sigma_test;
    float sigma_transfer;         /* |σ_test − σ_train| normalised */
    bool  overfit;
} cos_v176_transfer_t;

typedef struct {
    int   n_rollouts;
    float sigma_dream_mean;       /* mean over rollouts */
    float sigma_dream_var;        /* variance */
    float sigma_real;             /* σ after an equivalent real step */
    bool  dream_helped;           /* sigma_dream_mean ≤ sigma_real + slack */
} cos_v176_dream_t;

typedef struct {
    cos_v176_world_t    worlds[COS_V176_N_WORLDS];
    int                 curriculum_order[COS_V176_CURRICULUM];

    int                 n_transfer;
    cos_v176_transfer_t transfers[COS_V176_N_WORLDS];

    cos_v176_dream_t    dream;

    float               tau_realism;      /* default 0.35 */
    float               tau_transfer;     /* default 0.30 */
    float               dream_slack;      /* default 0.02 */
    uint64_t            seed;
} cos_v176_state_t;

void cos_v176_init(cos_v176_state_t *s, uint64_t seed);

/* Generate 6 worlds and populate σ_realism and σ_difficulty. */
void cos_v176_generate_worlds(cos_v176_state_t *s);

/* Produce a 5-world curriculum ordered easy → hard. */
void cos_v176_build_curriculum(cos_v176_state_t *s);

/* Measure σ_transfer for {w_i → w_{i+1}} pairs. */
void cos_v176_measure_transfer(cos_v176_state_t *s);

/* Run 1000 latent dreams and compare to σ_real. */
void cos_v176_dream_train(cos_v176_state_t *s);

size_t cos_v176_to_json(const cos_v176_state_t *s, char *buf, size_t cap);

int cos_v176_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V176_SIMULATOR_H */
