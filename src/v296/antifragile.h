/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v296 σ-Antifragile — stronger from stress.
 *
 *   Nassim Taleb: antifragile systems do not merely
 *   survive stress — they get stronger from it.
 *   Entropy is fuel, not damage.  v296 types four v0
 *   manifests for that discipline: the stress-to-
 *   adaptation loop lowers σ across cycles, σ
 *   volatility classifies the regime, the σ-vaccine
 *   pre-exposes the system to controlled noise, and
 *   a barbell allocation keeps the extremes while
 *   rejecting the middle compromise.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Stress → adaptation (exactly 3 canonical cycles):
 *     `cycle_1` stress=1.0 σ_after=0.50;
 *     `cycle_2` stress=2.0 σ_after=0.35;
 *     `cycle_3` stress=3.0 σ_after=0.25.
 *     Contract: 3 rows in order; `stress_level`
 *     strictly monotonically increasing AND
 *     `sigma_after` strictly monotonically
 *     DECREASING — more pressure, less noise:
 *     antifragile, not merely robust.
 *
 *   σ-volatility tracking (exactly 3 canonical regimes):
 *     `unstable`    σ_mean=0.40 σ_std=0.20
 *                   trend=NONE        UNSTABLE;
 *     `stable`      σ_mean=0.30 σ_std=0.02
 *                   trend=NONE        STABLE;
 *     `antifragile` σ_mean=0.30 σ_std=0.08
 *                   trend=DECREASING  ANTIFRAGILE.
 *     Contract: 3 DISTINCT classifications in order;
 *     `ANTIFRAGILE iff σ_std > std_stability=0.03 AND
 *     trend == DECREASING`; `STABLE iff σ_std ≤
 *     std_stability`; `UNSTABLE iff σ_std >
 *     std_unstable=0.15 AND trend == NONE` —
 *     volatile is not the same as unstable, and
 *     stable is not yet antifragile.
 *
 *   Controlled exposure / σ-vaccine (exactly 3
 *   canonical rows):
 *     `dose_small`   noise=0.10 vaccine survived;
 *     `dose_medium`  noise=0.30 vaccine survived;
 *     `real_attack`  noise=0.80 survived
 *                              because_trained=true.
 *     Contract: 3 rows in order; noise strictly
 *     monotonically increasing; all `survived=true`;
 *     exactly 2 rows with `is_vaccine=true`; exactly
 *     1 row with `is_vaccine=false` AND
 *     `because_trained=true` — the real attack
 *     survives because the vaccinations came first.
 *
 *   Barbell strategy (exactly 3 canonical allocations):
 *     `safe_mode`         share=0.90 τ=0.15 kept;
 *     `experimental_mode` share=0.10 τ=0.70 kept;
 *     `middle_compromise` share=0.00 τ=0.40
 *                                     REJECTED.
 *     Contract: 3 rows in order; `share_safe +
 *     share_exp == 1.0 ± 1e-3`; `share_middle == 0.0
 *     ± 1e-3`; `τ_safe < τ_exp`; `kept polarity`:
 *     `safe_mode` and `experimental_mode` kept AND
 *     `middle_compromise` rejected — extremes
 *     together, middle out.
 *
 *   σ_antifragile (surface hygiene):
 *       σ_antifragile = 1 −
 *         (stress_rows_ok +
 *          stress_monotone_ok +
 *          vol_rows_ok + vol_classify_ok +
 *          vac_rows_ok + vac_noise_order_ok +
 *          vac_survived_ok + vac_vaccine_count_ok +
 *          vac_trained_ok +
 *          bb_rows_ok + bb_share_sum_ok +
 *          bb_middle_zero_ok + bb_tau_order_ok +
 *          bb_kept_polarity_ok) /
 *         (3 + 1 + 3 + 1 + 3 + 1 + 1 + 1 + 1 +
 *          3 + 1 + 1 + 1 + 1)
 *   v0 requires `σ_antifragile == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 stress cycles; stress ↑ AND σ ↓.
 *     2. 3 volatility regimes; 3 distinct
 *        classifications; ANTIFRAGILE iff σ_std >
 *        0.03 AND trend=DECREASING.
 *     3. 3 vaccine rows; noise strictly increasing;
 *        all survived; exactly 2 vaccines; exactly 1
 *        real attack survived because trained.
 *     4. 3 barbell rows; safe + exp = 1.0;
 *        middle = 0.0; τ_safe < τ_exp; extremes
 *        kept, middle rejected.
 *     5. σ_antifragile ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v296.1 (named, not implemented): live σ-volatility
 *     telemetry classifying the running system per
 *     minute, a scheduled σ-vaccine pipeline that
 *     injects controlled adversarial noise during
 *     low-traffic windows, and a barbell allocator
 *     that routes 10 % of production traffic through
 *     an experimental τ.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V296_ANTIFRAGILE_H
#define COS_V296_ANTIFRAGILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V296_N_STRESS  3
#define COS_V296_N_VOL     3
#define COS_V296_N_VAC     3
#define COS_V296_N_BB      3

#define COS_V296_STD_STABILITY 0.03f
#define COS_V296_STD_UNSTABLE  0.15f

typedef struct {
    char  label[10];
    float stress_level;
    float sigma_after;
} cos_v296_stress_t;

typedef struct {
    char  regime[12];
    float sigma_mean;
    float sigma_std;
    char  trend[12];
    char  classification[12];
} cos_v296_vol_t;

typedef struct {
    char  label[14];
    float noise;
    bool  is_vaccine;
    bool  survived;
    bool  because_trained;
} cos_v296_vac_t;

typedef struct {
    char  allocation[20];
    float share;
    float tau;
    bool  kept;
} cos_v296_bb_t;

typedef struct {
    cos_v296_stress_t  stress[COS_V296_N_STRESS];
    cos_v296_vol_t     vol   [COS_V296_N_VOL];
    cos_v296_vac_t     vac   [COS_V296_N_VAC];
    cos_v296_bb_t      bb    [COS_V296_N_BB];

    int   n_stress_rows_ok;
    bool  stress_monotone_ok;

    int   n_vol_rows_ok;
    bool  vol_classify_ok;

    int   n_vac_rows_ok;
    bool  vac_noise_order_ok;
    bool  vac_survived_ok;
    bool  vac_vaccine_count_ok;
    bool  vac_trained_ok;

    int   n_bb_rows_ok;
    bool  bb_share_sum_ok;
    bool  bb_middle_zero_ok;
    bool  bb_tau_order_ok;
    bool  bb_kept_polarity_ok;

    float sigma_antifragile;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v296_state_t;

void   cos_v296_init(cos_v296_state_t *s, uint64_t seed);
void   cos_v296_run (cos_v296_state_t *s);

size_t cos_v296_to_json(const cos_v296_state_t *s,
                         char *buf, size_t cap);

int    cos_v296_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V296_ANTIFRAGILE_H */
