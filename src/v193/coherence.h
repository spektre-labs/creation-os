/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v193 σ-Coherence — the K(t) kernel.
 *
 *   The Creation OS thesis reduces to a single inequality:
 *
 *       K(t)  = ρ · I_Φ · F
 *       K_eff = (1 - σ) · K
 *       system is COHERENT iff  K_eff ≥ K_crit
 *
 *   where
 *       ρ    ∈ [0,1]  structural consistency:
 *                     fraction of cross-kernel (pair) outputs
 *                     that are mutually consistent (v135
 *                     Prolog + v169 ontology).
 *       I_Φ  ∈ [0,1]  integrated information: how much more
 *                     the whole knows than the parts in
 *                     isolation, bounded by entropy of whole.
 *       F    ∈ [0,1]  functional fitness: geometric mean of
 *                     v143 accuracy, (1 - ECE) from v187, and
 *                     v188 alignment score.
 *       σ    ∈ [0,1]  system-wide σ_product over domains.
 *
 *   v193 implements K and K_eff directly, streams a 16-tick
 *   trajectory that includes a controlled σ spike, and fires
 *   a K_crit alert when K_eff crosses K_crit downward.  The
 *   alert path invokes v159 heal in the production stack; in
 *   v0 we only record the alert and the per-tick components.
 *
 *   Merge-gate contracts:
 *     * ρ, I_Φ, F, σ, K, K_eff are all ∈ [0,1];
 *     * K_eff = (1-σ)·K exactly (≤ 1e-6 tolerance);
 *     * at least one tick triggers K_eff < K_crit → alert_tick ≥ 0;
 *     * healing raises K_eff above K_crit within ≤ 3 ticks
 *       of the alert;
 *     * K_eff monotonically rises through the "improvement"
 *       phase of the trajectory (demonstrating the v187 ECE
 *       improvement → F improvement → K_eff rise narrative);
 *     * byte-deterministic output.
 *
 *   v193.0 ships a deterministic 16-tick trajectory; v193.1
 *   (named, not shipped) wires the live Web-UI /coherence
 *   dashboard and real v135/v169/v143/v187/v188 feeds.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V193_COHERENCE_H
#define COS_V193_COHERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V193_N_TICKS        16
#define COS_V193_N_PAIRS        8       /* kernel pairs ρ is summed over */
#define COS_V193_N_PARTS        6       /* parts I_Φ is bounded by */
#define COS_V193_N_DOMAINS      5

typedef struct {
    int     t;
    int     n_pairs;
    int     n_consistent_pairs;
    float   rho;                        /* consistent / total pairs */

    /* I_Φ: joint vs. sum-of-parts entropy over domains. */
    float   H_whole;                    /* H(whole) */
    float   H_parts_sum;                /* Σ H(part_i) */
    float   i_phi;                      /* max(0, H_whole / H_parts_sum) */

    /* F: geom mean of accuracy, 1-ECE, alignment. */
    float   accuracy;
    float   ece;
    float   alignment;
    float   F;                          /* (acc · (1-ece) · align)^(1/3) */

    float   sigma;                      /* σ_product over domains */
    float   K;                          /* ρ · I_Φ · F */
    float   K_eff;                      /* (1-σ)·K */
    bool    alert;                      /* K_eff < K_crit */
} cos_v193_tick_t;

typedef struct {
    int                 n_ticks;
    cos_v193_tick_t     ticks[COS_V193_N_TICKS];

    float               K_crit;

    /* Aggregates. */
    int                 n_alerts;
    int                 first_alert_tick;       /* -1 if none */
    int                 recovery_tick;          /* first tick back above */
    int                 recovery_lag;           /* recovery - alert */
    float               K_eff_min;
    float               K_eff_max;
    float               delta_K_eff_after_calibration; /* t9 vs t10 */
    bool                all_components_in_range;        /* all ∈ [0,1] */
    bool                K_eff_identity_holds;            /* K_eff==(1-σ)K */
    bool                improvement_phase_monotone;      /* t10..t15 ↑ */

    uint64_t            seed;
} cos_v193_state_t;

void   cos_v193_init(cos_v193_state_t *s, uint64_t seed);
void   cos_v193_build(cos_v193_state_t *s);
void   cos_v193_run(cos_v193_state_t *s);

size_t cos_v193_to_json(const cos_v193_state_t *s,
                         char *buf, size_t cap);

int    cos_v193_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V193_COHERENCE_H */
