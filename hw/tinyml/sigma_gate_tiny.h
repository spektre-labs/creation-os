/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * TinyMCU σ-gate — Q16 state matching `src/inference/cos_sigma_mirror.h` /
 * `python/cos/sigma_gate_core.py`.
 *
 * Does **not** replace or modify `python/cos/sigma_gate.h` (packaging hook).
 * Intended for ESP32 / AVR / STM32 sketches with zero heap in the gate path.
 */
#ifndef SIGMA_GATE_TINY_H
#define SIGMA_GATE_TINY_H

#include <stdint.h>

#define SIGMA_TINY_Q16     ((int32_t)65536)
#define SIGMA_TINY_K_CRIT  ((int32_t)8323) /* round(0.127 * 65536) */

typedef struct {
    int32_t sigma;
    int32_t d_sigma;
    int32_t k_eff;
} sigma_state_t;

typedef enum {
    SIGMA_ACCEPT = 0,
    SIGMA_RETHINK = 1,
    SIGMA_ABSTAIN = 2,
} sigma_verdict_t;

/* Map [0,1] float literals to Q16 at compile time (MCU toolchains: double OK). */
#define SIGMA_Q16(x) ((int32_t)((double)(x) * 65536.0 + 0.5))

static inline void sigma_state_init(sigma_state_t *st)
{
    if (!st)
        return;
    st->sigma   = 0;
    st->d_sigma = 0;
    st->k_eff   = SIGMA_TINY_Q16;
}

static inline void sigma_update(sigma_state_t *st,
                               int32_t new_sigma_q16,
                               int32_t k_raw_q16)
{
    int32_t delta;
    int64_t ke;
    if (!st)
        return;
    delta       = new_sigma_q16 - st->sigma;
    st->d_sigma = delta;
    st->sigma   = new_sigma_q16;
    ke = ((int64_t)(SIGMA_TINY_Q16 - (int64_t)st->sigma) * (int64_t)k_raw_q16)
        >> 16;
    if (ke < 0)
        ke = 0;
    if (ke > SIGMA_TINY_Q16 - 1)
        ke = SIGMA_TINY_Q16 - 1;
    st->k_eff = (int32_t)ke;
}

static inline sigma_verdict_t sigma_gate(const sigma_state_t *st)
{
    if (!st)
        return SIGMA_ABSTAIN;
    if (st->k_eff < SIGMA_TINY_K_CRIT)
        return SIGMA_ABSTAIN;
    if (st->d_sigma > 0)
        return SIGMA_RETHINK;
    return SIGMA_ACCEPT;
}

#endif /* SIGMA_GATE_TINY_H */
