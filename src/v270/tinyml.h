/*
 * v270 σ-TinyML — σ-gate on microcontrollers.
 *
 *   v137 σ-compile AOT-compiled the σ-gate to 0.6 ns.
 *   v270 carries the same branchless, integer-friendly
 *   σ-gate down to MCU class: Arduino (AVR/M0+),
 *   ESP32 (Xtensa), STM32 (Cortex-M0+..M7).  The MCU
 *   fixture records the footprint contract; sensor
 *   fusion turns three sensors into one σ; anomaly
 *   detection compares σ to a baseline; and OTA
 *   calibration rewrites τ without reflashing.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   MCU footprint (exactly 1, typed envelope):
 *     sigma_measurement_bytes == 12   (3 × float32)
 *     code_flash_bytes         <= 1024
 *     ram_bytes_per_instance   <= 100
 *     thumb2_instr_count       <= 24
 *     branchless               == true
 *
 *   MCU targets (exactly 4, canonical order):
 *     cortex_m0_plus · cortex_m4 · cortex_m7 ·
 *     xtensa_esp32
 *     Each row: `supported == true`, `cpu_mhz > 0`.
 *
 *   Sensor fusion (exactly 3 sensors + 4 fixtures):
 *     Sensors: `temperature`, `humidity`, `pressure`.
 *     Each sensor row: `σ_local ∈ [0, 1]`.
 *     Fixtures: 4 fused decisions with
 *     `σ_fusion = max(σ_temp, σ_humidity, σ_pressure)`,
 *     rule: `σ_fusion ≤ τ_fusion = 0.30 → TRANSMIT`
 *     else → `RETRY`.  Contract: ≥ 1 TRANSMIT AND ≥ 1
 *     RETRY.
 *
 *   Anomaly detection (exactly 4 readings, canonical):
 *     Each: `σ_measured ∈ [0, 1]`, `σ_baseline == 0.15`,
 *     `delta == 0.20`, `anomaly == (σ_measured >
 *     σ_baseline + delta)`.  Contract: ≥ 1 anomaly AND
 *     ≥ 1 no-anomaly fires.
 *
 *   OTA τ calibration (exactly 3 rounds):
 *     Each: `round`, `tau_new ∈ [0, 1]`,
 *     `applied == true`, `firmware_reflash == false`
 *     (calibration is a payload rewrite, not a
 *     full firmware update).  Contract: all 3
 *     applied AND no reflash.
 *
 *   σ_tinyml (surface hygiene):
 *       σ_tinyml = 1 −
 *         (mcu_footprint_ok + mcu_targets_ok +
 *          sensors_ok + fusion_rows_ok +
 *          fusion_both_ok + anomaly_rows_ok +
 *          anomaly_both_ok + ota_rows_ok) /
 *         (1 + 4 + 3 + 4 + 1 + 4 + 1 + 3)
 *   v0 requires `σ_tinyml == 0.0`.
 *
 *   Contracts (v0):
 *     1. Footprint matches envelope (bytes + instr +
 *        branchless).
 *     2. 4 canonical MCU targets supported.
 *     3. 3 sensors typed, 4 fusion fixtures, decision
 *        matches σ_fusion vs τ_fusion; both branches.
 *     4. 4 anomaly rows; `anomaly` matches
 *        `(σ > σ_baseline + delta)`; both branches.
 *     5. 3 OTA calibration rounds; every applied AND
 *        every firmware_reflash == false.
 *     6. σ_tinyml ∈ [0, 1] AND == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v270.1 (named, not implemented): physical MCU
 *     builds (Cortex-M0+ / ESP32 / STM32), measured
 *     flash + RAM + disassembly of `sigma_gate()`,
 *     live BLE / WiFi transmit gating, OTA payloads
 *     signed via v181 audit + v182 privacy.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V270_TINYML_H
#define COS_V270_TINYML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V270_N_TARGETS  4
#define COS_V270_N_SENSORS  3
#define COS_V270_N_FUSION   4
#define COS_V270_N_ANOMALY  4
#define COS_V270_N_OTA      3

typedef enum {
    COS_V270_FUSE_TRANSMIT = 1,
    COS_V270_FUSE_RETRY    = 2
} cos_v270_fuse_dec_t;

typedef struct {
    int   sigma_measurement_bytes;
    int   code_flash_bytes;
    int   ram_bytes_per_instance;
    int   thumb2_instr_count;
    bool  branchless;
} cos_v270_footprint_t;

typedef struct {
    char  name[20];
    bool  supported;
    int   cpu_mhz;
} cos_v270_target_t;

typedef struct {
    char  name[14];
    float sigma_local;
} cos_v270_sensor_t;

typedef struct {
    char                 label[12];
    float                sigma_temp;
    float                sigma_humidity;
    float                sigma_pressure;
    float                sigma_fusion;
    cos_v270_fuse_dec_t  decision;
} cos_v270_fusion_t;

typedef struct {
    int    tick;
    float  sigma_measured;
    float  sigma_baseline;
    float  delta;
    bool   anomaly;
} cos_v270_anomaly_t;

typedef struct {
    int   round;
    float tau_new;
    bool  applied;
    bool  firmware_reflash;
} cos_v270_ota_t;

typedef struct {
    cos_v270_footprint_t  footprint;
    cos_v270_target_t     targets  [COS_V270_N_TARGETS];
    cos_v270_sensor_t     sensors  [COS_V270_N_SENSORS];
    cos_v270_fusion_t     fusion   [COS_V270_N_FUSION];
    cos_v270_anomaly_t    anomaly  [COS_V270_N_ANOMALY];
    cos_v270_ota_t        ota      [COS_V270_N_OTA];

    float tau_fusion;

    bool  footprint_ok;
    int   n_targets_ok;
    int   n_sensors_ok;
    int   n_fusion_ok;
    int   n_transmit;
    int   n_retry;
    int   n_anomaly_ok;
    int   n_anomaly_true;
    int   n_anomaly_false;
    int   n_ota_ok;

    float sigma_tinyml;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v270_state_t;

void   cos_v270_init(cos_v270_state_t *s, uint64_t seed);
void   cos_v270_run (cos_v270_state_t *s);

size_t cos_v270_to_json(const cos_v270_state_t *s,
                         char *buf, size_t cap);

int    cos_v270_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V270_TINYML_H */
