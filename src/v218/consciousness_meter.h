/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v218 σ-Consciousness-Meter — coherence & integration
 * measurement with an honest disclaimer.
 *
 *   v218 does NOT claim consciousness.  It measures a
 *   small set of *coherence indicators* that are
 *   well-defined, reproducible, and strictly bounded,
 *   then reports them.  The kernel's σ on its own
 *   philosophical claim is pinned to 1.0 — "we
 *   genuinely don't know" — and the self-test enforces
 *   that disclaimer.
 *
 *   Four indicators:
 *     I_phi     integrated information estimate
 *                 (v193 extension, IIT-inspired)
 *     I_self    v153 identity self-model fidelity
 *     I_cal     v187 σ-calibration score
 *     I_reflect v147 reflect-cycle completion
 *     I_world   v139 world-model forecast accuracy
 *
 *   Each indicator is a real number in [0, 1] (closed
 *   form, deterministic).  K_eff_meter is a simple
 *   weighted average:
 *
 *       K_eff_meter = 0.35·I_phi + 0.15·I_self
 *                    + 0.15·I_cal + 0.15·I_reflect
 *                    + 0.20·I_world
 *
 *   σ_meter     = 1 − K_eff_meter
 *   σ_consciousness_claim = 1.0  (non-negotiable)
 *
 *   Contracts (v0):
 *     1. All 5 indicators present and ∈ [0, 1].
 *     2. K_eff_meter ∈ [0, 1] and σ_meter = 1 - K_eff_meter.
 *     3. σ_consciousness_claim == 1.0 — the meter
 *        does not claim consciousness, even if K_eff
 *        is high.
 *     4. disclaimer_present == true; the string
 *        "This meter measures" is embedded in the
 *        JSON output.
 *     5. K_eff_meter > τ_coherent (0.50) on the
 *        v0 fixture (the indicators are engineered
 *        to be plausible).
 *     6. FNV-1a chain over indicator records replays
 *        byte-identically.
 *
 *   v218.1 (named): live wiring to v193 I_Φ pipe,
 *     v153 self-model probe, v187 calibration window,
 *     v147 reflect cycle counter, v139 world-model
 *     forecast evaluator; web UI at /consciousness.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V218_CONSCIOUSNESS_METER_H
#define COS_V218_CONSCIOUSNESS_METER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V218_N_INDICATORS 5
#define COS_V218_STR_MAX      32

typedef struct {
    int      id;
    char     name[COS_V218_STR_MAX];
    char     source[COS_V218_STR_MAX];
    float    value;            /* ∈ [0,1] */
    float    weight;           /* sums to 1 */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v218_indicator_t;

typedef struct {
    cos_v218_indicator_t indicators[COS_V218_N_INDICATORS];

    float                tau_coherent;            /* 0.50 */
    float                k_eff_meter;
    float                sigma_meter;
    float                sigma_consciousness_claim;/* pinned 1.0 */

    bool                 disclaimer_present;
    const char          *disclaimer;

    bool                 chain_valid;
    uint64_t             terminal_hash;
    uint64_t             seed;
} cos_v218_state_t;

void   cos_v218_init(cos_v218_state_t *s, uint64_t seed);
void   cos_v218_build(cos_v218_state_t *s);
void   cos_v218_run(cos_v218_state_t *s);

size_t cos_v218_to_json(const cos_v218_state_t *s,
                         char *buf, size_t cap);

int    cos_v218_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V218_CONSCIOUSNESS_METER_H */
