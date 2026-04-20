/*
 * σ-TinyML — embedded sensor anomaly σ in 12 bytes of RAM.
 *
 * On a Raspberry Pi 5 or iPhone we run the full BitNet+σ pipeline.
 * On an ESP32 we can't — no LLM fits in 520 KiB of SRAM.  But we
 * can still run σ: a streaming anomaly detector over raw sensor
 * readings.
 *
 * The primitive maintains a running mean and variance (Welford's
 * online algorithm — numerically stable and O(1) per sample) and
 * emits σ ∈ [0, 1] per measurement based on how many standard
 * deviations the current reading is from the running mean.
 *
 *     z = |value − μ| / (σ_std + eps)
 *     σ = clamp(z / Z_ALERT, 0, 1)
 *
 * With Z_ALERT = 3.0 (three-sigma rule) a reading exactly on the
 * mean returns σ = 0; a reading three stddevs away returns σ = 1;
 * intermediate values interpolate linearly.
 *
 * State footprint:
 *     mean           float    4 B
 *     m2             float    4 B
 *     count          uint16_t 2 B
 *     last_sigma_q   uint16_t 2 B       (σ · 65535, rounded)
 *     ────────────────────────────
 *     total                  12 B
 *
 * 12 bytes is the size of three float variables.  Even an ATtiny
 * can run this over 100 sensors.
 *
 * Contracts (v0):
 *   1. sizeof(cos_sigma_tinyml_state_t) == 12 (static_assert below).
 *   2. First sample (count == 0) returns σ = 0 and seeds mean.
 *   3. Second sample (count == 1) returns σ = 0 (no variance yet).
 *   4. From sample 3 onwards σ is the z-score ÷ Z_ALERT, clamped.
 *   5. NaN / infinite input → σ = 1.0 and state UNCHANGED.
 *   6. reset() zeros mean/m2/count/last_sigma_q.
 *   7. last_sigma_q always equals round(observed_sigma · 65535).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TINYML_H
#define COS_SIGMA_TINYML_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SIGMA_TINYML_Z_ALERT 3.0f
#define COS_SIGMA_TINYML_EPS     1e-6f

typedef struct {
    float    mean;
    float    m2;
    uint16_t count;
    uint16_t last_sigma_q;     /* σ · 65535, rounded */
} cos_sigma_tinyml_state_t;

/* Static size contract: the promise is "12 bytes per sensor channel". */
_Static_assert(sizeof(cos_sigma_tinyml_state_t) == 12,
               "tinyml state must stay 12 bytes");

void  cos_sigma_tinyml_init (cos_sigma_tinyml_state_t *s);
void  cos_sigma_tinyml_reset(cos_sigma_tinyml_state_t *s);

/* Ingest one sample; returns σ ∈ [0, 1]. */
float cos_sigma_tinyml_observe(cos_sigma_tinyml_state_t *s, float value);

/* True if the most recent σ exceeded ``threshold``. */
bool  cos_sigma_tinyml_is_anomalous(const cos_sigma_tinyml_state_t *s,
                                    float threshold);

/* Dequantise last_sigma_q back to float. */
float cos_sigma_tinyml_last_sigma(const cos_sigma_tinyml_state_t *s);

int   cos_sigma_tinyml_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_TINYML_H */
